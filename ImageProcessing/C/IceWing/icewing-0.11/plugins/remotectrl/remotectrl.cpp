/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "main/plugin.h"

#define SOCK_PORT		4208
#define SOCK_COM_LOAD	"control"
#define SOCK_COM_SAVE	"getSettings"

#ifdef WITH_XCF
#define LOG4CPP_FIX_ERROR_COLLISION
#include "xcf/xcf.hpp"
#define XCFNAME			"xcf"
#endif

/* All parameter of one plugin instance */
typedef struct ctrlPlugin {
	plugDefinition def;		/* iceWing base class, a plugin */

	/* Command line arguments */
	int sockport;			/* Port number for socket commuication */
	char *xcfname;			/* XCF server name */

	int sockfd;				/* socket() file descriptor */
} ctrlPlugin;

static BOOL cb_opts_load (void *data, char *buffer, int size)
{
	char **string = (char**)data, *pos;

	while (**string == '\n') (*string)++;
	pos = *string;
	while (*pos && *pos != '\n') pos++;

	if (pos != *string) {
		if (pos-(*string) >= size)
			pos = (*string)+size-1;
		if (pos-*string > 0)
			memcpy (buffer, *string, pos-*string);
		buffer[pos-*string] = '\0';
		*string = pos;
		return TRUE;
	} else
		return FALSE;
}

typedef struct ctrlSaveData {
	char *out;
	char *outpos;
	int len;
} ctrlSaveData;
static BOOL cb_opts_save (void *data, char *string)
{
	ctrlSaveData *dat = (ctrlSaveData*)data;
	int len;

	if (!string) return TRUE;

	len = strlen (string);
	if (dat->outpos - dat->out + len+1 >= dat->len) {
		char *out = dat->out;
		dat->len += MAX(len+1,1000);
		dat->out = (char*)realloc (dat->out, dat->len);
		dat->outpos = dat->outpos - out + dat->out;
	}
	strcpy (dat->outpos, string);
	dat->outpos += len;

	return TRUE;
}

#ifdef WITH_XCF
/*********************************************************************
  XCF function to remote-control the GUI
  in : a config-string in the form
           '\"Tracking.Dist Init\" = 50\n\"GrabImage1.Interlace:\" = 0'"
  out: --- (not used)
*********************************************************************/
static void xcf_controlFunc (std::string& in, std::string& out)
{
	char *str = (char*)in.c_str();
	opts_load (cb_opts_load, &str);
}

/*********************************************************************
  XCF function to get the GUI settings
  in : --- (not used)
  out: the complete GUI settings
*********************************************************************/
static void xcf_getSettingsFunc (std::string& in, std::string& out)
{
	ctrlSaveData dat;
	dat.out = dat.outpos = NULL;
	dat.len = 0;
	opts_save (cb_opts_save, &dat);
	out = std::string(dat.out);
	free (dat.out);
}
#endif /* WITH_XCF */

/*********************************************************************
  Send one packet (len + buf (of length len)) to socket s.
*********************************************************************/
static ssize_t sock_send (int s, const void *buf, size_t len)
{
	size_t total = 0;
	int cnt;
	uint32_t len_n;

	len_n = htonl(len);
	do {
		cnt = send (s, &len_n, sizeof(len_n), 0);
	} while (cnt == -1 && errno == EINTR);
	if (cnt != sizeof(len_n))
		return -1;

	while (total < len) {
		do {
			cnt = send (s, (char*)buf+total, len - total, 0);
		} while (cnt == -1 && errno == EINTR);
		if (cnt < 0)
			return -1;
		total += cnt;
	}
	return total;
}

/*********************************************************************
  Receive one packet (string length + string) from socket s and save
  the string to *buf (of size *len).
*********************************************************************/
static ssize_t sock_recv (int s, char **buf, int *len)
{
	uint32_t packetlen;
	size_t total = 0;
	int cnt;

	do {
		cnt = recv (s, &packetlen, sizeof(packetlen), 0);
	} while (cnt == -1 && errno == EINTR);
	if (cnt < 0)
		return -1;
	else if (cnt != sizeof(packetlen))
		return 0;
	packetlen = ntohl (packetlen);

	if (*len < (int)packetlen) {
		*len = packetlen;
		*buf = (char*)realloc (*buf, packetlen);
	}
	while (total < packetlen) {
		do {
			cnt = recv (s, (*buf)+total, packetlen - total, 0);
		} while (cnt == -1 && errno == EINTR);
		if (cnt <= 0)
			return cnt;
		total += cnt;
	}
	return total;
}

/*********************************************************************
  Continously handle all requests on socket plug->sockfd.
*********************************************************************/
static void sock_listen (ctrlPlugin *plug)
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	char *buffer;
	int bufferlen;
	int fd, newfd, cnt;
	fd_set all_fds;		/* All open file descriptors */
	fd_set read_fds;	/* Temp file descriptor list for select() */
	int fdmax;			/* Max. file descriptor number */

	bufferlen = 200;
	buffer = (char*)malloc (bufferlen);

	FD_ZERO (&all_fds);
	FD_SET (plug->sockfd, &all_fds);
	fdmax = plug->sockfd;

	while (1) {
		/* Check all open file descriptors */
		read_fds = all_fds;
		if (select (fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			iw_warning_errno ("select()");
			continue;
		}

		for (fd = 0; fd <= fdmax; fd++) {
			if (!FD_ISSET(fd, &read_fds))
				continue;

			if (fd == plug->sockfd) {
				/* Accept and add new connection */
				if ((newfd = accept (plug->sockfd, (struct sockaddr *)&addr,
									 &addrlen)) == -1) {
					iw_warning_errno ("Unable to accept() request");
					continue;
				}
				FD_SET (newfd, &all_fds);
				if (newfd > fdmax)
					fdmax = newfd;
				iw_debug (3, "Got connection from %s on socket %d",
						  inet_ntoa(addr.sin_addr), newfd);
				continue;
			}

			/* Handle data from a client */
			buffer[0] = '\0';
			cnt = sock_recv (fd, &buffer, &bufferlen);

			if (cnt <= 0) {
				/* Error or connection closed by client */
				if (cnt == -1)
					iw_warning_errno ("Unable to recv() command");
				close (fd);
				FD_CLR (fd, &all_fds);
				iw_debug (4, "Closed connection on socket %d", fd);
			} else if (!strncmp (SOCK_COM_LOAD, buffer, sizeof(SOCK_COM_LOAD)-1)) {
				char *str = buffer+sizeof(SOCK_COM_LOAD)-1;
				opts_load (cb_opts_load, &str);
			} else if (!strncmp (SOCK_COM_SAVE, buffer, sizeof(SOCK_COM_SAVE)-1)) {
				ctrlSaveData dat;
				dat.out = dat.outpos = NULL;
				dat.len = 0;
				opts_save (cb_opts_save, &dat);
				cnt = strlen (dat.out) + 1;
				if (sock_send (fd, dat.out, cnt) != cnt)
					iw_warning_errno ("Unable to send() settings");
				free (dat.out);
			} else {
				if (bufferlen > 20) {
					buffer[17] = '.';
					buffer[18] = '.';
					buffer[19] = '.';
					buffer[20] = '\0';
				}
				iw_warning ("Received unknown command '%s'", buffer);
			}
		} /* for */
	} /* while (1) */
}

/*********************************************************************
  Show help message and exit.
*********************************************************************/
static void help (ctrlPlugin *plug)
{
	fprintf (stderr,"\n%s plugin for %s, (c) 2006-2009 by Frank Loemker\n"
			 "Provide the XCF/INET socket function control(char) to control the\n"
			 "GUI via XCF/INET sockets and the function getSettings(void) to get\n"
			 "the current widget settings. Can be used with 'icewing-control' to\n"
			 "remote control iceWing.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-p [port]]"
#ifdef WITH_XCF
			 " [-x [name]]"
#endif
			 "\n"
			 "-p       use socket communication via a port, default port: %d\n",
			 plug->def.name, ICEWING_NAME, plug->def.name, SOCK_PORT);
#ifdef WITH_XCF
	fprintf (stderr,
			 "-x       use XCF communication via server name, default: %s\n\n"
			 "If neither -p nor -x is given, '-p' is used for communication.\n",
			 XCFNAME);
#endif
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para'    : command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
static void ctrl_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	ctrlPlugin *plug = (ctrlPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;

	plug->xcfname = NULL;
	plug->sockport = -1;
	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, "-P:pio -X:xro -H:H");
		switch (ch) {
			case 'p':
				if (arg != IW_ARG_NO_NUMBER)
					plug->sockport = (int)(long)arg;
				else
					plug->sockport = SOCK_PORT;
				break;
#ifdef WITH_XCF
			case 'x':
				if (arg)
					plug->xcfname = (char*)arg;
				else
					plug->xcfname = XCFNAME;
				break;
#endif
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	if (plug->sockport < 0 && !plug->xcfname)
		plug->sockport = SOCK_PORT;

#ifdef WITH_XCF
	if (plug->xcfname) {
		try {
			XCF::ServerPtr xcf_server = NULL;

			xcf_server = XCF::Server::create (plug->xcfname);
			xcf_server->registerMethod ("control", &xcf_controlFunc);
			xcf_server->registerMethod ("getSettings", &xcf_getSettingsFunc);
			xcf_server->run (true);
		} catch (const XCF::GenericException& ex) {
			iw_error ("XCF::GenericException: %s\n"
					  "\tCould not init Server", ex.reason.c_str());
		}
	}
#endif
	if (plug->sockport >= 0) {
		struct sockaddr_in addr;
		int reuse = 1;
		pthread_t thread;

		if ((plug->sockfd = socket (PF_INET, SOCK_STREAM, 0)) < 0)
			iw_error_errno ("Unable to open socket");

		if (setsockopt (plug->sockfd, SOL_SOCKET, SO_REUSEADDR,
						&reuse, sizeof(reuse)) < 0)
			iw_warning_errno ("Unable to set SO_REUSEADDR");

		addr.sin_family = AF_INET;
		addr.sin_port = htons (plug->sockport);
		addr.sin_addr.s_addr = htonl (INADDR_ANY);
		memset (&(addr.sin_zero), '\0', 8);
		if (bind (plug->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
			iw_error_errno ("Unable to bind() socket");
		if (plug->sockport == 0) {
			socklen_t addrlen = sizeof(addr);
			if (getsockname (plug->sockfd,
							 (struct sockaddr *)&addr, &addrlen) < 0)
				iw_error_errno ("Unable to get port number");
			iw_warning ("Using port %d", ntohs(addr.sin_port));
		}

		if (listen (plug->sockfd, 5) < 0)
			iw_error_errno ("Unable to listen() on socket");

		pthread_create (&thread, NULL,
						(void*(*)(void*))sock_listen, plug);
	}
}

static plugDefinition plug_ctrl = {
	"remotectrl%d",
	PLUG_ABI_VERSION,
	ctrl_init,
	NULL,
	NULL,
	NULL
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
extern "C" plugDefinition *plug_get_info (int instCount, BOOL *append)
{
	ctrlPlugin *plug = (ctrlPlugin*)calloc (1, sizeof(ctrlPlugin));

	plug->def = plug_ctrl;
	plug->def.name = g_strdup_printf (plug_ctrl.name, instCount);

	*append = TRUE;
	return (plugDefinition*)plug;
}
