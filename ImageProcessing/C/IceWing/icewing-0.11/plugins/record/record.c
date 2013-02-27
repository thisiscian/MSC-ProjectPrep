/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2009
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tools/tools.h"
#include "main/image.h"
#include "main/plugin.h"
#include "gui/Gimage_i.h"
#include "gui/Ggui.h"
#include "gui/Gtools.h"
#include "gui/Goptions.h"
#include "gui/avi/avilib.h"
#ifdef WITH_JPEG
#include "gui/avi/jpegutils.h"
#endif

#include "gui/Gcolor.h"

#define EVEN_PROD(w,h)	((((w)+1)&(~1)) * (((h)+1)&(~1)))

#define REC_OFF			0
#define REC_EXCLUSIVE	1
#define REC_PARALLEL	2
#define REC_ONCE		3

/* Image is already converted, save directly */
#define REC_IMG_FORMAT_DATA		(IW_IMG_FORMAT_VECTOR + 100)
/* Close all movie files */
#define REC_IMG_FORMAT_CLOSE	(IW_IMG_FORMAT_VECTOR + 101)
/* Close all movie files and exit */
#define REC_IMG_FORMAT_EXIT		(IW_IMG_FORMAT_VECTOR + 102)

typedef struct recMovie {
	avi_t *AVI;
	iwMovie *movie;

	iwImgFormat format;		/* Format of AVI/movie */
	char name[PATH_MAX];	/* File name AVI/movie was opened with */
	int frame_ms;			/* Frame length in milliseconds */
	int rem_time;			/* Remaining time not used for last saved frame */
	unsigned int last_time;	/* Time in ms of last image */

	char *plug;				/* Key for hash table lookup: plugin name */
} recMovie;

typedef struct recImage {
	iwImgFormat format;		/* Format of data */
	struct timeval time;	/* The time image data was grabbed */
	char fname[PATH_MAX];	/* File name of image or NULL */
	int frame_number;		/* Image from movie file -> frame number, 0 otherwise */
	const char *pname;		/* Name of providing plugin (don't free this) */
	recMovie *movie;		/* The movie where data should be added */
	int dup_cnt;			/* Duplicate last frame dup_cnt times */
	int length;				/* Length of data */
	void *data;				/* The image data */
} recImage;

typedef struct recQEntry {
	struct recQEntry *next;
	struct recQEntry *prev;
	void *data;
} recQEntry;

typedef struct recQueue {
	recQEntry *first;
	recQEntry *last;
	int nentries;
	pthread_mutex_t in_use;
	pthread_cond_t changed;
} recQueue;

typedef struct recValues {
	int record;				/* Do recording / do only recording ? */
	int framedrop;			/* Min. queue length before waiting some ms */
	BOOL rgb;				/* Convert to rgb? */
	iwImgFormat format;
	BOOL avi_usefrate;		/* Drop/duplicate frames to obey frame rate? */
	float avi_framerate;
	int quality;
	char fname[PATH_MAX];	/* File name pattern */
	BOOL reset_counter;
} recValues;

typedef struct recParameter {	/* Command line arguments */
	BOOL grab;
	char *name;
} recParameter;

typedef struct recPlugin {		/* All parameter of one plugin instance */
	plugDefinition def;
	recValues values;
	recParameter para;
	pthread_t save_thread;

	recQueue *images;
	GHashTable *movie_hash;		/* Movies for all providing plugins */
	int img_num;				/* '%d' image file counter */
	void *first_plug;			/* REC_ONCE starting plugin */

	unsigned int last_time;		/* Time in ms of last image */
	iwImgFormat last_format;	/* Format of last queued image */
} recPlugin;

static recQueue *queue_init (void)
{
	recQueue *queue = malloc (sizeof(recQueue));
	queue->first = NULL;
	queue->last = NULL;
	queue->nentries = 0;
	pthread_mutex_init (&queue->in_use, NULL);
	pthread_cond_init (&queue->changed, NULL);

	return queue;
}

static void queue_push (recQueue *queue, void *data)
{
	recQEntry *entry = malloc (sizeof(recQEntry));
	entry->data = data;
	entry->next = NULL;

	pthread_mutex_lock (&queue->in_use);
	if (queue->first == NULL) {
		queue->first = entry;
		entry->prev = NULL;
	} else {
		entry->prev = queue->last;
		queue->last->next = entry;
	}
	queue->last = entry;
	queue->nentries++;

	pthread_cond_broadcast (&queue->changed);
	pthread_mutex_unlock (&queue->in_use);
}

static void *queue_pop (recQueue *queue)
{
	recQEntry *entry;
	void *data = NULL;

	pthread_mutex_lock (&queue->in_use);
	while (queue->first == NULL) {
		int ret;
		while ((ret = pthread_cond_wait (&queue->changed,
										 &queue->in_use)) != 0) {
		   iw_warning ("WaitCondition returned error %d", ret);
		}
	}
	entry = queue->first;
	queue->first = entry->next;
	if (queue->first == NULL)
		queue->last = NULL;
	else
		queue->first->prev = NULL;
	data = entry->data;
	queue->nentries--;
	pthread_mutex_unlock (&queue->in_use);

	free (entry);

	return data;
}

/*********************************************************************
  Close all movie files.
*********************************************************************/
static gboolean cb_close_avi (gpointer key, gpointer value, gpointer user_data)
{
	recMovie *m = value;

	if (m->AVI)
		AVI_close (m->AVI);
	if (m->movie)
		iw_movie_close (m->movie);
	free (m->plug);
	return TRUE;
}
static void record_close_avi (GHashTable *movie_hash)
{
	if (movie_hash) {
		g_hash_table_foreach_remove (movie_hash, cb_close_avi, NULL);
		g_hash_table_destroy (movie_hash);		
	}
}

/*********************************************************************
  Return a filename in 'dest' (length at least PATH_MAX) based on the
  pattern in plug->values.fname.
*********************************************************************/
static void record_get_filename (recPlugin *plug, struct timeval *time,
								 char *img_fname, int frame_num, const char *pname,
								 char *dest)
{
	static char hostname[256], *pw_name;
	static BOOL init = FALSE;
	int iargs[4];
	char *sargs[6], *pos;

	if (!init) {
		struct passwd *pass = getpwuid (getuid());
		if (pass && pass->pw_name)
			pw_name = pass->pw_name;
		else
			pw_name = "<unknown>";
		if (gethostname (hostname, 256) != 0)
			strcpy (hostname, "<unknown>");
		init = TRUE;
	}

	sargs[0] = pw_name;
	sargs[1] = hostname;
	sargs[2] = ".";
	sargs[3] = "file";
	sargs[4] =  "";
	iargs[0] = plug->img_num;
	iargs[1] = time->tv_sec;
	iargs[2] = time->tv_usec/1000;
	iargs[3] = frame_num;

	if (pname)
		sargs[5] = (char*)pname;
	else
		sargs[5] = "<unknown>";
	if (img_fname && *img_fname) {
		if ((pos = strrchr (img_fname, '/'))) {
			sargs[2] = img_fname;
			sargs[3] = pos+1;
			*pos = '\0';
		} else {
			sargs[3] = img_fname;
		}
		if ((pos = strrchr (sargs[3], '.'))) {
			sargs[4] = pos+1;
			*pos = '\0';
		}
	}

	gui_sprintf (dest, PATH_MAX, plug->values.fname,
				 "dTtF", iargs, "bhpfeP", sargs);

	plug->img_num++;
}

/*********************************************************************
  Continously save all grabbed images from the image queue.
*********************************************************************/
static void record_save (recPlugin *plug)
{
	int file = 0;
	recImage *img;
	char fname[PATH_MAX];
	iwImgStatus status;
	iwImgFileData *fdata = iw_img_data_create();

	iw_showtid (1, plug->def.name);
	while (1) {
		if ((img = queue_pop(plug->images))) {
			iwImgFormat format = img->format;

			if (format >= REC_IMG_FORMAT_CLOSE) {
				record_close_avi ((GHashTable*)img->data);
				free (img);
				if (format == REC_IMG_FORMAT_CLOSE)
					continue;
				else
					break;
			}

			if (img->movie && img->movie->AVI) {

				recMovie *m = img->movie;
				int cnt = 0;
				uchar *buf = NULL;

				for (; img->dup_cnt; img->dup_cnt--) {
					AVI_dup_frame (m->AVI);
					fprintf (stderr, "d");
				}
				if (m->format == IW_IMG_FORMAT_AVI_MJPEG) {
#ifdef WITH_JPEG
					uchar *planes[3];
					int w = AVI_video_width (m->AVI);
					int h = AVI_video_height (m->AVI);
					planes[0] = img->data;
					planes[1] = planes[0]+w*h;
					planes[2] = planes[1]+EVEN_PROD(w,h)/4;
					buf = gui_get_buffer (w*h*3/2);
					cnt = encode_jpeg_raw (buf, w*h*3/2, plug->values.quality,
										   LAV_NOT_INTERLACED, 0,
										   w, h, planes[0], planes[1], planes[2]);
#endif
				} else {
					cnt = img->length;
					buf = img->data;
				}
				if (cnt > 0) {
					if (AVI_write_frame_start (m->AVI, cnt) ||
						AVI_write_frame (m->AVI, buf, cnt)) {
						iw_warning ("Unable to write to '%s'", m->name);
					} else {
						AVI_write_frame_end (m->AVI);
						fprintf (stderr, "W %d|", plug->images->nentries);
					}
				} else {
					iw_warning ("Compressed image to 0 byte");
				}
				if (m->format == IW_IMG_FORMAT_AVI_MJPEG && buf)
					gui_release_buffer (buf);

			} else if (img->movie) {

				recMovie *m = img->movie;
				iw_img_data_set_movie (fdata, &m->movie,
									   plug->values.avi_framerate);
				iw_img_data_set_quality (fdata, plug->values.quality);
				status = iw_img_save ((iwImage*)img->data, m->format,
									  m->name, fdata);
				if (status != IW_IMG_STATUS_OK)
					iw_warning ("Error %d saving to %s", status, m->name);
				else
					fprintf (stderr, "W %d|", plug->images->nentries);

			} else {

				record_get_filename (plug, &img->time, img->fname,
									 img->frame_number, img->pname, fname);

				fprintf (stderr, "Saving '%s' (queue size %d)...\n",
						 fname, plug->images->nentries);

				if (format == REC_IMG_FORMAT_DATA) {
					if ((file = open(fname, O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK, 0666)) >= 0) {
						if (write (file, img->data, img->length) != img->length)
							iw_warning ("Unable to write to '%s'", fname);
						close (file);
					} else
						iw_warning ("Unable to open '%s'", fname);
				} else {
					iw_img_data_set_quality (fdata, plug->values.quality);
					status = iw_img_save ((iwImage*)img->data, format,
										  fname, fdata);
					if (status != IW_IMG_STATUS_OK)
						iw_warning ("Unable to save as '%s', error %d", fname, status);
				}

			}
			free (img->data);
			free (img);
		}
	}
	iw_img_data_free (fdata);
}

/*********************************************************************
  Push a "close all movie files" / "close all movie files and exit"
  onto the queue.
*********************************************************************/
static void record_push_close (recPlugin *plug, BOOL exit)
{
	recImage *img;
	iwImgFormat format = REC_IMG_FORMAT_CLOSE;
	if (exit) format = REC_IMG_FORMAT_EXIT;

	if (plug->last_format >= format)
		return;

	img = calloc (1, sizeof(recImage));
	img->format = format;
	img->data = plug->movie_hash;
	plug->movie_hash = NULL;
	plug->last_format = img->format;
	queue_push (plug->images, img);
}

/*********************************************************************
  Copy 'i' to 'recimg->data'/'recimg->length'. If 'rgb' convert from
  YUV to RGB.
*********************************************************************/
static void record_copy_img (recImage *recimg, BOOL rgb, iwImage *i)
{
	guchar *pos;
	int plane, x, p = i->planes;
	iwImage *i_copy;

	if (i->rowstride) {
		plane = i->rowstride*i->height;
		recimg->length = plane;
	} else {
		plane = IW_TYPE_SIZE(i)*i->width*i->height;
		recimg->length = plane*p;
	}

	recimg->length += sizeof(iwImage) + sizeof(uchar*)*p;
	recimg->data = malloc (recimg->length);
	i_copy = (iwImage*)recimg->data;
	*i_copy = *i;
	i_copy->data = (guchar**)((guchar*)i_copy + sizeof(iwImage));
	pos = (guchar*)i_copy->data + sizeof(guchar*)*p;

	if (p == 3 && rgb && i->type == IW_8U && !i->rowstride) {
		guchar *y = i->data[0], *u = i->data[1], *v = i->data[2];
		guchar *dy, *du, *dv;

		i_copy->data[0] = dy = pos;
		i_copy->data[1] = du = dy+i->width*i->height;
		i_copy->data[2] = dv = du+i->width*i->height;

		for (x=i->width*i->height; x>0; x--)
			prev_inline_yuvToRgbVis (*y++, *u++, *v++,
									 dy++, du++, dv++);
	} else {
		if (i->rowstride) {
			memcpy (pos, i->data[0], plane);
			i_copy->data[0] = pos;
		} else {
			for (x=0; x<p; x++) {
				memcpy (pos, i->data[x], plane);
				i_copy->data[x] = pos;
				pos += plane;
			}
		}
		if (rgb)
			iw_img_yuvToRgbVis (i_copy);
	}
}

/*********************************************************************
  Prepare the image 'gimg' as a new frame for an AVI movie of the
  format 'recimg->movie->format' in the file 'plug->values.fname'.
*********************************************************************/
static BOOL record_conv_avi (recPlugin *plug, recImage *recimg, const char *pname,
							 grabImageData *gimg)
{
	uchar *planes[3];
	long time;
	int colsize = 0;
	int cnt, w, h;
	recMovie *m = recimg->movie;
	iwImage *img = &gimg->img;

	if (IW_TYPE_SIZE(img) > 1) {
		iw_warning ("Unable to save images of type %d as avi frames",
					img->type);
		return FALSE;
	}

	/* Check if FFV1 or MPEG4 should be saved
	   (not handled natively in the record plugin) */
	if (m->movie ||
		m->format == IW_IMG_FORMAT_AVI_FFV1 || m->format == IW_IMG_FORMAT_AVI_MPEG4) {
		if (!m->movie) {
			record_get_filename (plug, &gimg->time, gimg->fname,
								 gimg->frame_number, pname, m->name);
			fprintf (stderr, "Saving frames in movie '%s' (Numbers: Queue length)...\n",
					 m->name);
		}
		record_copy_img (recimg, FALSE, img);
		return TRUE;
	}

	/* Handle RAW444, RAW420, and MJPEG */
	if (!m->AVI) {
		unsigned int codec;

#ifndef WITH_JPEG
		if (m->format == IW_IMG_FORMAT_AVI_MJPEG) {
			iw_warning ("Compiled without AVI-MJPG-support");
			return FALSE;
		}
#endif
		record_get_filename (plug, &gimg->time, gimg->fname,
							 gimg->frame_number, pname, m->name);
		fprintf (stderr, "Saving frames in movie '%s'...\n"
				 "    (Numbers: Queue length | W: Save | d: Duplicate | .: Drop)\n", m->name);

		if (!(m->AVI = AVI_open_output_file (m->name))) {
			iw_warning ("Unable to open '%s'", m->name);
			return FALSE;
		}
		switch (m->format) {
			case IW_IMG_FORMAT_AVI_RAW444:
				codec = IW_CODEC_444P;
				w = img->width;
				h = img->height;
				break;
			case IW_IMG_FORMAT_AVI_RAW420:
				codec = IW_CODEC_I420;
				w = (img->width+1) & (~1);
				h = (img->height+1) & (~1);
				break;
			case IW_IMG_FORMAT_AVI_MJPEG:
				codec = IW_CODEC_MJPG;
				w = (img->width+15) & (~15);
				h = (img->height+15) & (~15);
				break;
			default:
				iw_error ("Unknown format %d", m->format);
				return FALSE;
		}
		AVI_set_video (m->AVI, w, h, plug->values.avi_framerate, codec);
		AVI_set_audio (m->AVI, 0, 0, 0, 0);
		m->frame_ms = 1000/plug->values.avi_framerate;
		m->rem_time = 0;
		m->last_time = 0;
	}

	/* Check if frames must be dropped / duplicated */
	time = gimg->time.tv_sec*1000 + gimg->time.tv_usec/1000;
	if (plug->values.avi_usefrate) {
		if (m->last_time > 0) {
			cnt = time - m->last_time - m->rem_time;
			if (cnt < m->frame_ms/2) {
				m->last_time = time;
				m->rem_time = -cnt;
				fprintf (stderr, ".");
				return FALSE;
			}
			while (cnt > m->frame_ms + m->frame_ms/2) {
				recimg->dup_cnt++;
				cnt -= m->frame_ms;
			}
			m->rem_time = m->frame_ms - cnt;
		}
	} else
		m->rem_time  = 0;
	m->last_time = time;

	w = AVI_video_width (m->AVI);
	h = AVI_video_height (m->AVI);
	if (m->format == IW_IMG_FORMAT_AVI_RAW444)
		colsize = w*h;				/* YUV444P */
	else
		colsize = EVEN_PROD(w,h)/4;	/* YUV420P */

	recimg->length = w*h + colsize*2;
	recimg->data = malloc (recimg->length);
	planes[0] = recimg->data;
	planes[1] = planes[0]+w*h;
	planes[2] = planes[1]+colsize;
	iw_img_convert (img, planes, w, h, m->format);

	return TRUE;
}

/*********************************************************************
  Free the resources allocated during record_???().
*********************************************************************/
static void record_cleanup (plugDefinition *plug_d)
{
	recPlugin *plug = (recPlugin *)plug_d;

	if (plug->save_thread) {
		record_push_close (plug, TRUE);
		pthread_join (plug->save_thread, NULL);
		plug->save_thread = (pthread_t)NULL;
	}
}

static void help (recPlugin *plug)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 2003-2009 by Frank Loemker\n"
			 "Perform optimized saving of images with the ident 'image' of type\n"
			 "grabImageData or with a given ident.\n"
			 "\n"
			 "Usage of the %s plugin: [-g ident | -i ident]\n"
			 "-g        observe data of type grabImageData\n"
			 "-i        observe data of type iwImage\n",
			 plug->def.name, ICEWING_NAME, plug->def.name);
	gui_exit (1);
}

/*********************************************************************
  Initialisation.
  'para': command line parameter
*********************************************************************/
#define ARG_TEMPLATE "-G:Gr -I:Ir -H:H -HELP:H --HELP:H"
static void record_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	recPlugin *plug = (recPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;

	plug->para.grab = TRUE;
	plug->para.name = "image";

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'G':
				plug->para.grab = TRUE;
				plug->para.name = (char*)arg;
				break;
			case 'I':
				plug->para.grab = FALSE;
				plug->para.name = (char*)arg;
				break;
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	plug->images = queue_init();
	plug->movie_hash = NULL;
	plug->img_num = 0;
	plug->first_plug = NULL;
	plug->last_time = 0;
	plug->last_format = 0;

	pthread_create (&plug->save_thread, NULL,
					(void*(*)(void*))record_save, plug);

	plug_observ_data (plug_d, plug->para.name);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int record_init_options (plugDefinition *plug_d)
{
	static char *record[] = {"Off", "Exclusive", "Parallel", "Once", NULL};
	recPlugin *plug = (recPlugin *)plug_d;
	int p;

	plug->values.record = REC_OFF;
	plug->values.framedrop = 0;
	plug->values.rgb = FALSE;
	plug->values.format = IW_IMG_FORMAT_UNKNOWN;
	plug->values.avi_usefrate = TRUE;
	plug->values.avi_framerate = 12.5;
	plug->values.quality = 75;
	strcpy (plug->values.fname, "image_%d.ppm");
	plug->values.reset_counter = FALSE;

	p = plug_add_default_page (plug_d, NULL, PLUG_PAGE_NODISABLE);
	opts_radio_create (p, "Record:",
					   "Save received images (Exclusive: Do not call any further"
					   " plugins  Once: Auto-deactivate after one round of plugins)?",
					   record, &plug->values.record);
	opts_entscale_create (p, "Framedrop",
						  "Wait some ms if queue of to be saved images is too long"
						  " (>4xVal -> 200ms, >2xVal -> 80ms, >1xVal -> 40ms),"
						  " 0 -> never wait",
						  &plug->values.framedrop, 0, 200);
	opts_toggle_create (p, "Convert to RGB", "Convert image to RGB (ignored for AVI)?",
						&plug->values.rgb);
	opts_option_create (p, "Image format:",
						"File format used for image/movie saving",
						iwImgFormatTextBM, (int*)(void*)&plug->values.format);
	opts_toggle_create (p, "Framerate based dup/drop-frames",
						"Use frame rate to decide if frames should be dropped or"
						" duplicated (only for AVI YUV444 / YUV420 / MJPEG)?",
						&plug->values.avi_usefrate);
	opts_float_create (p, "AVI framerate",
					   "Number of frames / second (only used for AVI saving)",
					   &plug->values.avi_framerate, 0, 50);
	opts_entscale_create (p, "Quality",
						  "Quality used for JPEG / AVI MJPEG / MPEG4 saving",
						  &plug->values.quality, 0, 100);
	opts_filesel_create (p, "File Name (e.g 'image_%d.ppm'):",
						 "%d: image counter  %t: ms part of grabbing time  "
						 "%T: sec part of grabbing time  %b: user name  %h: host name  "
						 "%p: file path  %f: file name  %e: file extension  "
						 "%F: frame number/0  %P: name of providing plugin   "
						 "printf()-style modifiers are allowed",
						 plug->values.fname, PATH_MAX);

	opts_button_create (p, "Reset FilenameCounter",
						"Reset image counter to 0 (see %d), done after recording is off.\n"
						"WARNING! Existing files will be overwritten!",
						&plug->values.reset_counter);

	return p;
}

/*********************************************************************
  Process the data that was registered for observation.
  ident : The id passed to plug_observ_data(), describes 'data'.
  data  : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL record_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	recPlugin *plug = (recPlugin *)plug_d;
	grabImageData *gimg, gimg_data;
	recImage *recimg;
	iwImgFormat format;
	recMovie *movie = NULL;
	unsigned int time;
	int ftime, x;

	if (plug->values.record == REC_OFF) {
		/* Reset file counter if currently no saving in progress */
		if (plug->values.reset_counter == TRUE
			&& plug->images->nentries == 0) {
			plug->values.reset_counter = FALSE;
			plug->img_num = 0;
			iw_debug (0, "Resetting image counter");
		}

		if (plug->movie_hash)
			record_push_close (plug, FALSE);
		return TRUE;
	}

	if (plug->values.record == REC_ONCE) {
		if (plug->first_plug == data->plug) {
			plug->first_plug = NULL;
			opts_value_set (plug_name(plug_d, ".Record:"), GINT_TO_POINTER(REC_OFF));
			return TRUE;
		} else if (!plug->first_plug)
			plug->first_plug = data->plug;
	}

	if (plug->para.grab) {
		gimg = (grabImageData*)data->data;
	} else {
		gimg = &gimg_data;
		gimg->img = *(iwImage*)data->data;
		gimg->fname = NULL;
		gimg->frame_number = 0;
		gettimeofday (&gimg->time, NULL);
	}

	/* Queue length based frame drop */
	x = plug->values.framedrop;
	if (x > 0) {
		if (plug->images->nentries > x*4)
			iw_usleep (200*1000);
		else if (plug->images->nentries > x*2)
			iw_usleep (80*1000);
		else if (plug->images->nentries > x)
			iw_usleep (40*1000);
	}

	time = gimg->time.tv_sec*1000 + gimg->time.tv_usec/1000;
	if (plug->last_time > 0)
		ftime = time - plug->last_time;
	else
		ftime = -1;
	plug->last_time = time;

	if (plug->movie_hash)
		movie = g_hash_table_lookup (plug->movie_hash, data->plug->name);
	format = iw_img_save_get_format (plug->values.format, plug->values.fname);

	recimg = calloc (1, sizeof(recImage));
	recimg->format = format;

	if (movie ||
		(format >= IW_IMG_FORMAT_MOVIE && format <= IW_IMG_FORMAT_MOVIE_MAX)) {

		if (!movie) {
			if (!plug->movie_hash)
				plug->movie_hash = g_hash_table_new (g_str_hash, g_str_equal);
			movie = calloc (1, sizeof(recMovie));
			movie->plug = strdup (data->plug->name);
			movie->format = format;
			g_hash_table_insert (plug->movie_hash, movie->plug, movie);
		}

		recimg->movie = movie;
		if (!record_conv_avi (plug, recimg, data->plug->name, gimg)) {
			free (recimg);
			return plug->values.record != REC_EXCLUSIVE;
		}
	} else {
		char header[40];
		iwImage *i = &gimg->img;
		guchar *pos;

		recimg->time = gimg->time;
		if (gimg->fname && *gimg->fname) {
			gui_strlcpy (recimg->fname, gimg->fname, PATH_MAX);
		} else
			recimg->fname[0] = '\0';
		recimg->frame_number = gimg->frame_number;
		recimg->pname = data->plug->name;

		if (format == IW_IMG_FORMAT_PNM && i->type == IW_8U && !i->rowstride) {
			sprintf (header, "P%d\n# frame time %d\n%d %d\n255\n",
					 (i->planes==1 && i->ctab <= IW_COLFORMAT_MAX) ? 5:6,
					 ftime, i->width, i->height);
			x = strlen(header);

			recimg->format = REC_IMG_FORMAT_DATA;
			recimg->length = i->planes*i->width*i->height + x;
			pos = recimg->data = malloc (recimg->length);

			memcpy (pos, header, x);
			pos += x;
			if (i->planes == 1) {
				memcpy (pos, i->data[0], i->width*i->height);
			} else {
				guchar *y = i->data[0], *u = i->data[1], *v = i->data[2];
				if (plug->values.rgb) {
					for (x=i->width*i->height; x>0; x--) {
						prev_inline_yuvToRgbVis (*y++, *u++, *v++,
												 pos, pos+1, pos+2);
						pos += 3;
					}
				} else {
					for (x=i->width*i->height; x>0; x--) {
						*pos++ = *y++;
						*pos++ = *u++;
						*pos++ = *v++;
					}
				}
			}
		} else {
			record_copy_img (recimg, plug->values.rgb, i);
		}
	}
	plug->last_format = recimg->format;
	queue_push (plug->images, recimg);

	return plug->values.record != REC_EXCLUSIVE;
}

static plugDefinition plug_record = {
	"Record%d",
	PLUG_ABI_VERSION,
	record_init,
	record_init_options,
	record_cleanup,
	record_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_record_get_info (int instCount, BOOL *append)
{
	recPlugin *plug = calloc (1, sizeof(recPlugin));

	plug->def = plug_record;
	plug->def.name = g_strdup_printf (plug_record.name, instCount);

	*append = FALSE;
	return (plugDefinition*)plug;
}
