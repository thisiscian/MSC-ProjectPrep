/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 2006-2009
 * Frank Loemker, Applied Computer Science, Faculty of Technology,
 * Bielefeld University, Germany
 *
 * This file is part of iceWing, a graphical plugin shell.
 *
 * iceWing is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * iceWing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "gui/Grender.h"
#include "main/plugin.h"

typedef enum {
	SHM_NONE,
	SHM_GRABIMG,
	SHM_IWIMG,
	SHM_STRING
} shmType;

typedef struct shmData {
	shmType type;
	int shm_id;				/* Return value of shmget() */
	int ref_cnt;
} shmData;

/* Command line arguments */
typedef struct shmParameter {
	char *socket;			/* Name of the server communication socket */
	char *input_id;			/* Identifier to observe */
	shmType type;			/* Type of input_id */
	int shm_cnt;			/* Max. number of shm segments before blocking */
	char *output_id;		/* Identifier for providing the data */
	BOOL block;				/* Blocking / non blocking reading on the socket */
} shmParameter;

/* All parameter of one plugin instance */
typedef struct shmPlugin {
	plugDefinition def;		/* iceWing base class, a plugin */
	shmParameter para;		/* Command line arguments */

	int socket_fd;			/* Communication socket */

	/* Sender */
	struct sockaddr_un *clients; /* Array of all registered clients */
	int clients_cnt;		/* Number of entries in clients array */
	int clients_len;		/* Length of clients array */
	shmData **shm_adr;		/* Array of shared memory segments */

	/* Receiver */
	int pipe[2];			/* Signal the processing thread to not block on clean up */
	int time_wait;			/* Length of the usleep() between grabbing of images */
	BOOL contiune;			/* If time_wait < 0 -> Receive next image ? */

	BOOL do_reg;			/* Register with the Sender? */
	char *socket;			/* Receivers socket name */
	prevBuffer *b_image;
} shmPlugin;

/*********************************************************************
  Read from plug->socket_fd, but allow canceling by writing to
  plug->pipe[1].
*********************************************************************/
static ssize_t shm_recvfrom (shmPlugin *plug, void *buf, size_t len, int flags,
							 struct sockaddr *addr, socklen_t *addrlen)
{
	ssize_t cnt = -1;
	fd_set rfds;
	struct timeval tv = {0, 0}, *p_tv = &tv;
	int maxfd = plug->socket_fd;

	FD_ZERO (&rfds);
	FD_SET (plug->socket_fd, &rfds);
	if (plug->pipe[0] >= 0) {
		FD_SET (plug->pipe[0], &rfds);
		if (plug->pipe[0] > plug->socket_fd) maxfd = plug->pipe[0];
	}
	if (plug->para.block)
		p_tv = NULL;

	if (select(maxfd+1, &rfds, NULL, NULL, p_tv) > 0) {
		if (FD_ISSET(plug->socket_fd, &rfds)) {
			do {
				cnt = recvfrom (plug->socket_fd, buf, len, flags, addr, addrlen);
			} while (cnt == -1 && errno == EINTR);
		} else if (FD_ISSET(plug->pipe[0], &rfds)) {
			char ch;
			read (plug->pipe[0], &ch, 1);
		}
	}
	return cnt;
}

/*********************************************************************
  Called from the main loop for every iteration one time.
*********************************************************************/
static void cb_shm_main (plugDefinition *plug_d)
{
	shmPlugin *plug = (shmPlugin *)plug_d;
	long ms = plug->time_wait;

	if (ms > 0) {
		iw_usleep (ms*1000);
	} else if (ms < 0) {
		while (!plug->contiune && plug->time_wait < 0)
			iw_usleep (10000);
		plug->contiune = 0;
	}
}

/*********************************************************************
  Free the resources allocated during shm_xxx().
*********************************************************************/
static void shm_cleanup (plugDefinition *plug_d)
{
	shmPlugin *plug = (shmPlugin *)plug_d;
	int i;

	/* Prevent any new messages */
	if (plug->para.input_id && plug->para.socket)
		unlink (plug->para.socket);
	if (plug->para.output_id && plug->socket)
		unlink (plug->socket);

	/* Remove any remaining messages */
	if (plug->para.output_id) {
		shmData *shm_adr;
		int shm_id, cnt;
		plug->para.block = FALSE;
		write (plug->pipe[1], "A", 1);
		do {
			cnt = shm_recvfrom (plug, &shm_id, sizeof(shm_id), 0, NULL, NULL);
			if (cnt == sizeof(shm_id)) {
				shm_adr = shmat (shm_id, 0, 0);
				if (shm_adr != (shmData*)-1) {
					shm_adr->ref_cnt--;
					shmdt (shm_adr);
				}
			}
		} while (cnt == sizeof(shm_id));
	}
	if (plug->pipe[0] >= 0) close (plug->pipe[0]);
	if (plug->pipe[1] >= 0) close (plug->pipe[1]);

	if (plug->shm_adr) {
		for (i=0; i < plug->para.shm_cnt; i++)
			if (plug->shm_adr[i])
				shmdt (plug->shm_adr[i]);
	}
}

/*********************************************************************
  Show help message and exit.
*********************************************************************/
static void help (shmPlugin *plug, char *err, ...)
{
	va_list args;

	fprintf (stderr,"\n%s plugin for %s, (c) 2006-2009 by Frank Loemker\n"
			 "Distribute data via shared memory and local sockets to other\n"
			 "local processes (1 to n communication).\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     <-p socket> [-g ident | -i ident | -s ident] [-n num] [-o ident] [-b]\n"
			 "-p       name of socket for the communication channel,\n"
			 "         will be prefixed with '/tmp/'\n"
			 "-g       identifier to observe, must be of type grabImageData\n"
			 "-i       identifier to observe, must be of type iwImage\n"
			 "-s       identifier to observe, must be of type char[]\n"
			 "-n       max number of distributed items before the sender blocks,\n"
			 "         default: %d\n"
			 "-o       identifier for providing the received data\n"
			 "-b       do not block while reading from the socket\n"
			 "\n"
			 "Observed identifier (-g, -i, -s) can contain dependencies,\n"
			 "e.g. \"ident()\" or \"ident(plug1 plug2 ...)\".\n",
			 plug->def.name, ICEWING_NAME, plug->def.name, plug->para.shm_cnt);
	if (err) {
		fprintf (stderr, "\n");
		va_start (args, err);
		vfprintf (stderr, err, args);
		va_end (args);
		fprintf (stderr, "\n");
	}
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para'    : command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
static void shm_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	shmPlugin *plug = (shmPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;
	struct sockaddr_un addr;
	char *sockname;

	plug->do_reg = TRUE;
	plug->pipe[0] = plug->pipe[1] = -1;

	plug->para.socket = NULL;
	plug->para.input_id = NULL;
	plug->para.shm_cnt = 2;
	plug->para.output_id = NULL;
	plug->para.block = TRUE;
	plug->para.type = SHM_NONE;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg,
							"-P:Pr -G:Gr -I:Ir -S:Sr -N:Ni -O:Or -B:B -H:H");
		switch (ch) {
			case 'P':
				if (!plug->para.socket && arg) {
					char *s = (char*)arg;
					plug->para.socket = malloc (6+strlen(s));
					if (*s == '/')
						strcpy (plug->para.socket, "/tmp");
					else
						strcpy (plug->para.socket, "/tmp/");
					strcat (plug->para.socket, s);
				}
				break;
			case 'G':
			case 'I':
			case 'S':
				if (plug->para.type != SHM_NONE)
					help (plug, "Only one of -g, -i, and -s is allowed!");
				switch (ch) {
					case 'G':
						plug->para.type = SHM_GRABIMG;
						break;
					case 'I':
						plug->para.type = SHM_IWIMG;
						break;
					case 'S':
						plug->para.type = SHM_STRING;
						break;
				}
				plug->para.input_id = (char*)arg;
				break;
			case 'N':
				plug->para.shm_cnt = (int)(long)arg;
				if (plug->para.shm_cnt < 1 || plug->para.shm_cnt > 1000)
					help (plug, "Argument to -n must be between 1 and 1000!");
				break;
			case 'O':
				plug->para.output_id = (char*)arg;
				break;
			case 'B':
				plug->para.block = FALSE;
				break;
			case 'H':
			case '\0':
				help (plug, NULL);
			default:
				help (plug, "Unknown character %c!", ch);
		}
	}

	if ((plug->para.input_id && plug->para.output_id) ||
		(!plug->para.input_id && !plug->para.output_id))
		help (plug, "Exactly one of '-g|-i|-s' and '-o' must be specified!");
	if (!plug->para.socket)
		help (plug, "-p must be specified!");

	if (plug->para.output_id) {
		plug->socket = malloc (strlen(plug->para.socket) + 11);
		sprintf (plug->socket, "%s.%d", plug->para.socket, getpid());
		sockname = plug->socket;

		if (pipe (plug->pipe) < 0)
			iw_error ("Cannot create clean up pipe: %s", g_strerror(errno));
	} else {
		sockname = plug->para.socket;
		plug->para.block = FALSE;
	}

	unlink (sockname);
	if ((plug->socket_fd = socket (AF_UNIX, SOCK_DGRAM, 0)) < 0)
		iw_error ("Unable to create local socket '%s',\n"
				  "\terror: '%s'", sockname, strerror(errno));

	memset (&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy (addr.sun_path, sockname);
	if (bind (plug->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		iw_error ("Unable to bind local socket '%s',\n"
				  "\terror: '%s'", sockname, strerror(errno));

	if (chmod (sockname, 0777) < 0)
		iw_error ("Unable to set permissions on local socket '%s',\n"
				  "\terror: '%s'", sockname, strerror(errno));

	if (plug->para.input_id) {
		plug->shm_adr = calloc (plug->para.shm_cnt, sizeof(shmData*));
		plug_observ_data (plug_d, plug->para.input_id);
	} else {
		plug_observ_data (plug_d, "start");
		plug_function_register (plug_d, "mainCallback", (plugFunc)cb_shm_main);
	}
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int shm_init_options (plugDefinition *plug_d)
{
	shmPlugin *plug = (shmPlugin *)plug_d;
	int p;

	plug->time_wait = 0;
	plug->contiune = FALSE;

	if (!plug->para.output_id)
		return -1;

	p = opts_page_append (plug->def.name);

	opts_entscale_create (p, "Wait Time",
						  "Time in ms to wait between data receives  "
						  "-1: wait until the button is pressed",
						  &plug->time_wait, -1, 2000);
	opts_button_create (p, "Receive next",
						"Receive next data if 'Wait Time == -1'",
						&plug->contiune);

	plug->b_image = prev_new_window (plug_name(plug_d, ".Received"),
									 -1, -1, FALSE, FALSE);

	return p;
}

/*********************************************************************
  Allocate size+sizeof(shmData) bytes of shared memory,
  init a shmData-struct at the start of the memory,
  and return a pointer to the memory.
*********************************************************************/
static shmData* shm_get_mem (int size)
{
	shmData *ret = NULL;
	int shm_id;

	size += sizeof(shmData);
	shm_id= shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
	if (shm_id == -1) {
		iw_error ("Unable to get %d bytes shared memory", size);
	} else {
		ret = shmat (shm_id, NULL, 0);
		shmctl (shm_id, IPC_RMID, NULL);
		if (ret == (shmData*)-1)
			iw_error ("Unable to attach to shared memory segment");
		ret->shm_id = shm_id;
		ret->ref_cnt = 0;
	}
	return ret;
}

/*********************************************************************
  Get shared memory for a grabImageData (grabimg==TRUE) or iwImage
  and return a copy of the image gimg / gimg->img.
*********************************************************************/
static shmData* shm_img_duplicate (const grabImageData *gimg, BOOL grabimg)
{
	const iwImage *img = &gimg->img;
	int w = img->width, h = img->height;
	int planes = img->planes;
	int size;
	void *pos;
	shmData *ret = NULL;
	grabImageData *rgimg = NULL;
	iwImage *rimg;

	/* Get necessary size in bytes of the shared memory area */
	if (planes < 3) planes = 3;
	size = planes*sizeof(uchar*);
	if (grabimg) {
		size += sizeof(grabImageData);
		if (gimg->fname)
			size += strlen(gimg->fname) + 1;
	} else
		size += sizeof(iwImage);
	if (img->rowstride)
		size += img->rowstride*h;
	else
		size += img->planes*w*h*IW_TYPE_SIZE(img);
	if (img->ctab > IW_COLFORMAT_MAX && img->ctab != IW_INDEX)
		size += sizeof(img->ctab[0])*IW_CTAB_SIZE;

	/* Get memory and copy image */
	ret = shm_get_mem (size);
	pos = (void*)ret + sizeof(shmData);

	if (grabimg) {
		ret->type = SHM_GRABIMG;
		rgimg = pos;
		pos += sizeof(grabImageData);
		*rgimg = *gimg;
	} else {
		ret->type = SHM_IWIMG;
		rgimg = pos;
		pos += sizeof(iwImage);
		rgimg->img = gimg->img;
	}
	rimg = &rgimg->img;

	rimg->data = pos;
	pos += planes * sizeof(uchar*);

	if (img->rowstride) {
		rimg->data[0] = pos;
		pos += img->rowstride * h;
		memcpy (rimg->data[0], img->data[0], img->rowstride * h);
		rimg->data[0] = (uchar*)rimg->data[0]- (long)rgimg;
	} else {
		int i, count = w*h*IW_TYPE_SIZE(img);

		for (i=0; i<img->planes; i++) {
			rimg->data[i] = pos;
			pos += count;
			memcpy (rimg->data[i], img->data[i], count);
			rimg->data[i] = (uchar*)rimg->data[i] - (long)rgimg;
		}
	}
	rimg->data = (uchar**)((char*)rimg->data - (long)rgimg);

	if (img->ctab == IW_INDEX) {
		rimg->ctab = (iwColtab)-1;
	} else if (img->ctab > IW_COLFORMAT_MAX) {
		rimg->ctab = pos;
		pos += sizeof(img->ctab[0])*IW_CTAB_SIZE;
		memcpy (rimg->ctab, img->ctab,
				sizeof(img->ctab[0])*IW_CTAB_SIZE);
		rimg->ctab = (iwColtab)((char*)rimg->ctab - (long)rgimg);
	}

	if (grabimg && rgimg->fname) {
		rgimg->fname = pos;
		strcpy (rgimg->fname, gimg->fname);
		rgimg->fname = (char*)rgimg->fname - (long)rgimg;
	}

	return ret;
}

/*********************************************************************
  Extract a grabImageData or iwImage from shm and return it.
*********************************************************************/
static grabImageData *shm_get_img (shmData *shm)
{
	grabImageData *simg = (void*)shm+sizeof(shmData);
	grabImageData *gimg;
	iwImage *img;
	BOOL grabimg = shm->type == SHM_GRABIMG;
	int i, planes = simg->img.planes;
	uchar **sdata;

	if (planes < 3) planes = 3;
	planes = planes*sizeof(uchar*) + sizeof(shmData*);
	if (grabimg)
		planes += sizeof(grabImageData);
	else
		planes += sizeof(iwImage);

	gimg = calloc (1, planes);
	*((shmData**)gimg) = shm;
	gimg = (grabImageData*)((char*)gimg + sizeof(shmData*));
	if (grabimg) {
		*gimg = *simg;
		if (gimg->fname)
			gimg->fname = (char*)simg + (long)simg->fname;
		img = &gimg->img;
		img->data = (uchar**)((char*)gimg+sizeof(grabImageData));
	} else {
		gimg->img = simg->img;
		img = &gimg->img;
		img->data = (uchar**)((char*)gimg+sizeof(iwImage));
	}
	sdata = (uchar**)((char*)simg + (long)simg->img.data);
	if (img->rowstride) {
		img->data[0] = (uchar*)simg + (long)sdata[0];
	} else {
		for (i=0; i<img->planes; i++)
			img->data[i] = (uchar*)simg + (long)sdata[i];
	}
	if (img->ctab == (iwColtab)-1)
		img->ctab = IW_INDEX;
	else if (img->ctab > IW_COLFORMAT_MAX)
		img->ctab = (iwColtab)((char*)simg + (long)simg->img.ctab);
	return gimg;
}

/*********************************************************************
  Callback of plug_data_set(). Release the stored image, which was
  allocated by shm_get_img().
*********************************************************************/
static void shm_destroy_image (void *data)
{
	if (data) {
		shmData **shm = data - sizeof(shmData*);
		(*shm)->ref_cnt--;
		if ((*shm)->shm_id != -1)
			shmdt (*shm);
		free (shm);
	}
}

/*********************************************************************
  Callback of plug_data_set(). Release the stored string.
*********************************************************************/
static void shm_destroy_string (void *data)
{
	if (data) {
		shmData *shm = data - sizeof(shmData);
		shm->ref_cnt--;
		if (shm->shm_id != -1)
			shmdt (shm);
	}
}

/*********************************************************************
  Send data to all clients.
*********************************************************************/
static void shm_send_data (shmPlugin *plug, void *data)
{
	shmData **shm_adr = plug->shm_adr;
	char *str;
	int i, idx, cnt, pid;
	struct sockaddr_un addr;
	socklen_t addrlen;

	/* Check if new clients want to register */
	do {
		addrlen = sizeof(addr);
		cnt = shm_recvfrom (plug, &pid, sizeof(pid), 0,
							(struct sockaddr*)&addr, &addrlen);
		if (cnt == sizeof(pid)) {
			if (plug->clients_cnt >= plug->clients_len) {
				plug->clients_len += 5;
				plug->clients = realloc (plug->clients,
										 plug->clients_len*sizeof(struct sockaddr_un));
			}
			iw_debug (2, "Registering new client '%s'", addr.sun_path);
			plug->clients[plug->clients_cnt] = addr;
			plug->clients_cnt++;
		}
	} while (cnt == sizeof(pid));

	/* No client? -> Nothing to do */
	if (plug->clients_cnt <= 0)
		return;

	/* Get shm_adr entry without any active client */
	do {
		for (idx=0; idx < plug->para.shm_cnt; idx++) {
			if (!shm_adr[idx] || shm_adr[idx]->ref_cnt <= 0)
				break;
		}
		if (idx == plug->para.shm_cnt) {
			iw_usleep (5000);
			gui_check_exit (FALSE);
		}
	} while (idx == plug->para.shm_cnt);

	if (shm_adr[idx]) {
		/* FIXME: reuse memory */
		shmdt (shm_adr[idx]);
		shm_adr[idx] = NULL;
	}
	switch (plug->para.type) {
		case SHM_GRABIMG:
			shm_adr[idx] = shm_img_duplicate ((grabImageData*)data, TRUE);
			break;
		case SHM_IWIMG:
			shm_adr[idx] = shm_img_duplicate ((grabImageData*)data, FALSE);
			break;
		case SHM_STRING:
			str = (char*)data;
			shm_adr[idx] = shm_get_mem (strlen(str)+1);
			shm_adr[idx]->type = SHM_STRING;
			strcpy ((char*)shm_adr[idx]+sizeof(shmData), str);
			break;
		default:
			break;
	}

	/* Send the data to all registered clients */
	if (shm_adr[idx]) {
		for (i = 0; i < plug->clients_cnt; i++) {
			shm_adr[idx]->ref_cnt++;
			addr = plug->clients[i];
			do {
				cnt = sendto (plug->socket_fd, &shm_adr[idx]->shm_id,
							  sizeof(shm_adr[idx]->shm_id), 0,
							  (struct sockaddr *)&addr, sizeof(addr));
			} while (cnt == -1 && errno == EINTR);
			if (cnt != sizeof(shm_adr[idx]->shm_id)) {
				iw_debug (2, "'%s' not reachable, unregistering...",
						  addr.sun_path);
				shm_adr[idx]->ref_cnt--;
				plug->clients[i] = plug->clients[plug->clients_cnt-1];
				plug->clients_cnt--;
				i--;
			}
		}
	}
}

/*********************************************************************
  Receive data from the server.
*********************************************************************/
static void shm_receive_data (shmPlugin *plug)
{
	shmData *shm_adr;
	grabImageData *img;
	prevDataImage i;
	int shm_id, cnt, pid;

	/* Register with the sender by sending the pid */
	if (plug->do_reg) {
		struct sockaddr_un addr;

		memset (&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strcpy (addr.sun_path, plug->para.socket);
		pid = getpid();

		gui_check_exit (TRUE);
		do {
			cnt = sendto (plug->socket_fd, &pid, sizeof(pid), 0,
						  (struct sockaddr *)&addr, sizeof(addr));
			if (cnt != sizeof(pid)) {
				iw_warning ("Unable to register via socket '%s', retrying ...",
							plug->para.socket);
				if (!plug->para.block)
					return;
				sleep (1);
			}
		} while (cnt != sizeof(pid));
		gui_check_exit (FALSE);

		plug->do_reg = FALSE;
	}

	/* recv() may block, thus enable immediate exit */
	gui_check_exit (TRUE);
	cnt = shm_recvfrom (plug, &shm_id, sizeof(shm_id), 0, NULL, NULL);
	gui_check_exit (FALSE);

	if (cnt == sizeof(shm_id)) {
		shm_adr = shmat (shm_id, 0, 0);
		if (shm_adr == (shmData*)-1)
			iw_error ("Unable to attach to shared memory segment");
		switch (shm_adr->type) {
			case SHM_GRABIMG:
			case SHM_IWIMG:
				img = shm_get_img (shm_adr);

				i.i = &img->img;
				i.x = i.y = 0;
				prev_render_imgs (plug->b_image, &i, 1, RENDER_CLEAR,
								  img->img.width, img->img.height);
				prev_draw_buffer (plug->b_image);
#ifdef IW_DEBUG
				if (shm_adr->type == SHM_GRABIMG) {
					if (img->fname && img->fname[0]) {
						if (img->frame_number > 0)
							iw_debug (3, "Image %d (%s:%d) received, size: %dx%d",
									  img->img_number, img->fname, img->frame_number,
									  img->img.width, img->img.height);
						else
							iw_debug (3, "Image %d (%s) received, size: %dx%d",
									  img->img_number, img->fname,
									  img->img.width, img->img.height);
					} else
						iw_debug (3, "Image %d received, size: %dx%d",
								  img->img_number, img->img.width, img->img.height);
				} else
					iw_debug (3, "Image received, size: %dx%d",
							  img->img.width, img->img.height);
#endif
				plug_data_set (&plug->def, plug->para.output_id,
							   img, shm_destroy_image);
				break;
			case SHM_STRING:
				iw_debug (3, "String for ident %s received",
						  plug->para.output_id);
				plug_data_set (&plug->def, plug->para.output_id,
							   (char*)shm_adr+sizeof(shmData),
							   shm_destroy_string);
				break;
			default:
				break;
		}
	} else if (cnt == 0)
		iw_error ("Unable to read identifier from socket '%s'",
				  plug->socket);
}

/*********************************************************************
  Process the data that was registered for observation.
  ident : The id passed to plug_observ_data(), describes 'data'.
  data  : The currently available data,
          result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL shm_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	shmPlugin *plug = (shmPlugin *)plug_d;

	if (plug->para.input_id)
		shm_send_data (plug, data->data);
	else
		shm_receive_data (plug);

	return TRUE;
}

static plugDefinition plug_shm = {
	"shmData%d",
	PLUG_ABI_VERSION,
	shm_init,
	shm_init_options,
	shm_cleanup,
	shm_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *plug_get_info (int instCount, BOOL *append)
{
	shmPlugin *plug = (shmPlugin*)calloc (1, sizeof(shmPlugin));

	plug->def = plug_shm;
	plug->def.name = g_strdup_printf (plug_shm.name, instCount);

	*append = TRUE;
	return (plugDefinition*)plug;
}
