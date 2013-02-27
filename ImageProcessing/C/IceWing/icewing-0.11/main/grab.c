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
#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "avvideo/AVOptions.h"
#include "AVImg.h"
#include "main/sfb_iw.h"
#include "gui/Gtools.h"
#include "gui/Ggui_i.h"
#include "gui/Goptions.h"
#include "gui/Grender.h"
#include "tools/tools.h"
#include "tools/fileset.h"
#include "tools/img_video.h"
#include "image.h"
#include "output.h"
#include "plugin.h"
#include "grab_prop.h"
#include "grab_avgui.h"
#include "grab.h"

#define SYNC_LEVEL	0
#define MODULE_ATTACH_RETRY	60

#define FRAME_BOTH			0
#define FRAME_EVEN			1
#define FRAME_EVEN_ASPECT	2
#define FRAME_DOWN21		3

#define GRAB_CONT_FIRST		1
#define GRAB_CONT_AUTOPREV	2
#define GRAB_CONT_PREV		3
#define GRAB_CONT_CURRENT	4
#define GRAB_CONT_NEXT		5
#define GRAB_CONT_AUTONEXT	6

#define DEVOPT_LENGTH		200

typedef enum {
	GRAB_NONE,
	GRAB_AV,
	GRAB_DACS,
	GRAB_PPM
} grabSource;

typedef enum {
	GRAB_IMG_FREE,
	GRAB_IMG_USE,
	GRAB_IMG_DETACHED
} grabImgStatus;

typedef struct grabImgEntry {
	grabImageData img;
	grabImgStatus status;
} grabImgEntry;

typedef struct grabImgRing {
	grabImgEntry **i;		/* All images */
	grabImgEntry **cur;		/* Current position in the ring buffer */
	int max;				/* Length of ring buffer */
} grabImgRing;

typedef struct grabValues {
	int interlace;			/* both, even, even + correct aspect ratio */
	int downsampling;		/* Downsample factor */
	char dev_options[DEVOPT_LENGTH]; /* AV: driver options */
	int ppm_timestep;		/* Timestep for loaded ppm images */
	BOOL time_wait_frame;
	int time_wait;			/* Length of the usleep() between grabbin of images */
	int fset_pos;			/* Current image number */
	int grab_cont;			/* If time_wait < 0 -> Grab which image ? */
} grabValues;

/* Command line arguments */
typedef struct grabLParameter {
	grabSource device;		/* Input'-device': AVlib, Dacs-Stream, or PPM-File-Set */
	BOOL avgui;				/* Use a GUI for the grabber properties? */
	int img_down;			/* The factor with which the input images were already downsampled */
	BOOL stereo;			/* Decode stereo interlaced image? */
	iwImgBayer bayer;		/* Bayer decomposition mode */
	iwImgBayerPattern pattern;/* Bayer pattern */
	int crop_x, crop_y;		/* Crop values for the input image */
	int crop_w, crop_h;
	int rotate;				/* Angle the input image should be rotated */
	VideoStandard input_sig;/* AV		: PAL_COMPOSITE | PAL_S_VIDEO | ... */
	char *source;			/* DACS, REGIONS: stream name  AV: driver options */
	BOOL ppm_once;			/* PPM		: play fileset only once? */
	int sync_level;			/* DACS     : sync level for registered stream */
	int nb_imgs_up;			/* Number of full images holded in memory */
	int nb_imgs_down;		/* Number of downsampled images holded in memory */
	int out_interval;		/* Output interval for images over DACS */
} grabLParameter;

typedef struct grabPlugin {		/* All parameter of one plugin instance */
	plugDefinition def;
	grabValues values;
	grabLParameter para;
	void *avgui_data;

	grabImageData img;
	uchar *planeptr[3];
	VideoDevData *av_video;		/* Connection to the grabber */
	ImgSeqData *av_frames;		/* Memory for the grabber */
	Bild_t *dacs_input;			/* Last image received via DACS */
	fsetList *fset;
	char *fset_name;			/* Name of 'Image Num' slider */
	int interlace;				/* Currently used interlace setting */
	int downsamp;				/* Currently used downsampling setting */
	char dev_options[DEVOPT_LENGTH]; /* AV: Currently used driver options */

	int gray_bytes;				/* Memory to convert images of type gray to color */
	iwType gray_type;
	uchar *gray_data[2];

	char *image_ident;
	char *image_rgb_ident;

	grabImgRing ring_up;
	grabImgRing ring_down;
	pthread_mutex_t ring_img_mutex;

	prevBuffer *b_input, *b_y, *b_u, *b_v, *b_rgb;
} grabPlugin;

static grabPlugin *plug_first = NULL;
	/* "static" grab_cont value of first plugin */
static int grab_cont;

/*********************************************************************
  Called from the main loop for every iteration one time.
*********************************************************************/
static void cb_grab_main (plugDefinition *plug_d)
{
	grabPlugin *plug = (grabPlugin *)plug_d;
	long ms = plug->values.time_wait;

	if (ms > 0) {
		if (plug->values.time_wait_frame) {
			static struct timeval time = {0, 0};

			if (time.tv_sec != 0) {
#define LCNT 10
				static struct timeval last[LCNT] = {
					{0, 0},{0, 0},{0, 0},{0, 0},{0, 0},
					{0, 0},{0, 0},{0, 0},{0, 0},{0, 0}};
				static int lpos = LCNT-1, ldist = 0;
				struct timeval new;
				int i;

				gettimeofday (&new, NULL);

				/* Check for outlier in the time history */
				if (IW_TIME_DIFF(new,last[lpos]) > ms*2+30)
					ldist = 0;
				lpos = (lpos + 1) % LCNT;

				/* Get reduced correction term (to prevent oscillation) */
				if (ldist != 0) {
					i = (lpos-ldist+LCNT) % LCNT;
					i = (ldist*ms - IW_TIME_DIFF(new,last[i])) / 2;
				} else
					i = -1;
				ms = ms - IW_TIME_DIFF(new,time) + i;

				last[lpos] = new;
				if (ldist < LCNT) ldist++;
#undef LCNT
			}
			if (ms > 0)
				iw_usleep (ms*1000);
			gettimeofday (&time, NULL);
		} else
			iw_usleep (ms*1000);
	} else if (ms < 0) {
		while (!plug->values.grab_cont && plug->values.time_wait < 0)
			iw_usleep (10000);
		if (plug->para.device != GRAB_PPM)
			plug->values.grab_cont = 0;
		else if (!plug->values.grab_cont &&
				 plug->values.time_wait >= 0 &&
				 (grab_cont == GRAB_CONT_AUTOPREV || grab_cont == GRAB_CONT_AUTONEXT))
			plug->values.grab_cont = grab_cont;
	}
	if (plug->para.device == GRAB_PPM) {
		grab_cont = plug->values.grab_cont;
		if (plug->values.time_wait == -1 ||
			(grab_cont != GRAB_CONT_AUTOPREV && grab_cont != GRAB_CONT_AUTONEXT))
			plug->values.grab_cont = 0;
	} else
		grab_cont = GRAB_CONT_AUTONEXT;
}

/*********************************************************************
  Function callback for plugins: Apply a list of iwGrabControl's and
  it's arguments to the grabber driver. The argument list must be
  finished with IW_GRAB_LAST.
*********************************************************************/
static iwGrabStatus cb_grab_set_properties (plugDefinition *plug_d, ...)
{
	iwGrabStatus ret;
	BOOL reinit = FALSE;
	va_list args;
	grabPlugin *plug = (grabPlugin *)plug_d;

	if (!plug->av_video) return IW_GRAB_STATUS_NOTOPEN;
	va_start (args, plug_d);
	ret = AVcap_set_properties (plug->av_video, &args, &reinit);
	va_end (args);

	if (reinit) {
		AVReleaseImgSeqData (plug->av_frames);
		if (!(plug->av_frames = AVInitImgSeqData (plug->av_video, 1, YUV_COLOR_IMAGE_INHALT)))
			iw_error ("Unable to init ImageSequence for the AV-driver");
	}

	return ret;
}

/*********************************************************************
  Return the grabbed image with
    time!=NULL: a grab-time most similar to time
    time==NULL: number img_num (0: last grabbed image) if still in
                memory or NULL otherwise.
    img_num<=0: return a full size image, if available
*********************************************************************/
grabImageData* grab_get_image_from_plug (plugDefinition *plug_d,
										 int img_num, const struct timeval *time)
{
	grabPlugin *plug = (grabPlugin *)plug_d;
	grabImgRing *ring;
	int i;

	if (!plug) return NULL;

	ring = &plug->ring_down;
	if (img_num <= 0) {
		img_num = -img_num;
		if (plug->ring_up.cur) ring = &plug->ring_up;
	}

	pthread_mutex_lock (&plug->ring_img_mutex);
	if (time) {
		int diff, min = -1, min_diff = G_MAXINT;

		for (i=0; i<ring->max; i++) {
			diff = abs(IW_TIME_DIFF(*time, ring->i[i]->img.time));
			if (diff < min_diff) {
				min = i;
				min_diff = diff;
			}
		}
		if (min >= 0)
			return &ring->i[min]->img;
	} else if (img_num == 0) {
		return &ring->cur[0]->img;
	} else {
		for (i=0; i<ring->max; i++)
			if (ring->i[i]->img.img_number == img_num)
				return &ring->i[i]->img;
	}
	pthread_mutex_unlock (&plug->ring_img_mutex);

	return NULL;
}
grabImageData* grab_get_image (int img_num, const struct timeval *time)
{
	return grab_get_image_from_plug (&plug_first->def, img_num, time);
}

/*********************************************************************
  Indicate that the img obtained with grab_get_image() is not needed
  any longer.
*********************************************************************/
void grab_release_image_from_plug (plugDefinition *plug_d, const grabImageData *img)
{
	grabPlugin *plug = (grabPlugin *)plug_d;

	if (!plug) return;
	if (img)
		pthread_mutex_unlock (&plug->ring_img_mutex);
}
void grab_release_image (const grabImageData *img)
{
	grab_release_image_from_plug (&plug_first->def, img);
}

#ifndef AV_HAS_DRIVER
VideoDevData *AVOpen (char *options, VideoStandard signalIn,
					  VideoMode vMode, unsigned int subSample)
{
#define NO_DISPLAY			0
	int camera = 0;
	float fps = 25;

	if (options) {
		if (AVcap_optstr_lookup (options, "help")) {
			fprintf (stderr, "\nAVVideo driver V0.1\n"
					 "Usage: camera=val:fps=val\n"
					 "camera   camera 0/1 to use, default: %d\n"
					 "fps      frame rate for image grabbing, default: %.0f\n\n",
					 camera, fps);
			return NULL;
		}
		AVcap_optstr_get (options, "camera", "%d", &camera);
		if (camera < 0 || camera > 1) {
			AVcap_warning ("AVVideo: Only 0 and 1 as cameras supported, not %d", camera);
			return NULL;
		}
		AVcap_optstr_get (options, "fps", "%f", &fps);
		if (fps <= 0) {
			AVcap_warning ("AVVideo: Frame rate %.2f must be greater than zero", fps);
			return NULL;
		}
	}

	return AVInit (camera, signalIn, vMode,
				   (int)(0.5+1000/fps), subSample, NO_DISPLAY);
}

iwGrabStatus AVcap_set_properties (VideoDevData *vidDat, va_list args)
{
	return IW_GRAB_UNSUPPORTED;
}

int AVParseDriver (char *arg, VideoStandard *drv)
{
	char a_up[21];
	int i;

	for (i=0; i<20 && arg[i]; i++)
		a_up[i] = toupper((int)arg[i]);
	a_up[i] = '\0';

	if (!strncmp (a_up, "PAL_COMPOSITE", i)) {
		*drv = PAL_COMPOSITE;
		return 1;
	} else if (!strncmp (a_up, "PAL_S_VIDEO", i)) {
		*drv = PAL_S_VIDEO;
		return 1;
	} else if (strchr (a_up, 'C')) {
		*drv = PAL_COMPOSITE;
		return 1;
	} else if (strchr (a_up, 'S')) {
		*drv = PAL_S_VIDEO;
		return 1;
	}
	return 0;
}

char *AVDriverHelp (void)
{
	static char *help = "use driver [PAL_COMPOSITE|PAL_S_VIDEO] (or [C|S])\n"
		"          with driver specific options ('help' for help output) for grabbing,\n"
		"          default: PAL_S_VIDEO, no options\n";
	return help;
}
#endif

static void grab_av_init (grabPlugin *plug, BOOL quit)
{
	VideoMode vidMode = FIELD_EVEN_ONLY;
	int downsamp = 1;

	if (plug->av_frames) {
		AVReleaseImgSeqData (plug->av_frames);
		plug->av_frames = NULL;
	}
	if (plug->av_video)
		AVClose (plug->av_video);

	if (plug->interlace == FRAME_BOTH || plug->interlace == FRAME_DOWN21)
		vidMode = FIELD_BOTH;
	if (!plug->para.nb_imgs_up)
		downsamp = plug->downsamp;

	if (!(plug->av_video = AVOpen (plug->dev_options, plug->para.input_sig,
								   vidMode, downsamp))) {
		if (quit)
			iw_error ("Unable to init AV-driver for input signal %d", plug->para.input_sig);
		else {
			iw_warning ("Unable to init AV-driver for input signal %d", plug->para.input_sig);
			opts_value_set (plug_name((plugDefinition*)plug, ".Wait Time"),
							GINT_TO_POINTER(-1));
		}
	} else if (!(plug->av_frames = AVInitImgSeqData (plug->av_video, 1,
													 YUV_COLOR_IMAGE_INHALT)))
		iw_error ("Unable to init ImageSequence for the AV-driver");
}

/*********************************************************************
  Free one ring entry.
*********************************************************************/
static void ring_entry_free (grabImgEntry *entry)
{
	if (entry) {
		iw_img_free (&entry->img.img, IW_IMG_FREE_DATA);
		free (entry);
	}
}

/*********************************************************************
  Allocate and init a new ring entry.
*********************************************************************/
static grabImgEntry* ring_entry_alloc (void)
{
	grabImgEntry *entry = calloc (1, sizeof(grabImgEntry));
	iw_img_init (&entry->img.img);
	return entry;
}

/*********************************************************************
  Free memory allocated with ring_img_init().
*********************************************************************/
/* Currently not needed.
static void ring_img_free (grabImgRing *ring)
{
	if (ring && ring->i) {
		int i;
		for (i=0; i<ring->max; i++)
			ring_entry_free (ring->i[i]);
		free (ring->i);
		ring->i = NULL;
	}
	ring->cur = NULL;
}
*/

/*********************************************************************
  Allocate memory for 'max' images in 'ring'.
*********************************************************************/
static void ring_img_init (grabImgRing *ring, int max)
{
	ring->max = max;
	if (max > 0) {
		int i;
		ring->i = malloc (sizeof(grabImgEntry*) * max);
		ring->cur = ring->i;
		for (i=0; i<max; i++)
			ring->i[i] = ring_entry_alloc();
	} else
		ring->i = ring->cur = NULL;
}

/*********************************************************************
  Read new image from AVLib, DACS, or FileSet and return it (with
  pointers to a function internal image) in img.
*********************************************************************/
static BOOL img_grab (grabPlugin *plug, grabImageData *img)
{
	iw_img_init (&img->img);
	img->downw = img->downh = plug->para.img_down;

	if (plug->para.device == GRAB_AV) {
		if (!plug->av_video) return FALSE;

		gui_exit_lock();
		if (AVGetLastImg (plug->av_video, plug->av_frames)) {
			iw_warning ("Can't grab image from input signal %d", plug->para.input_sig);
			gui_exit_unlock();
			return FALSE;
		}
		gui_exit_unlock();

#ifdef AV_HAS_DRIVER
		img->img = plug->av_frames->imgs[0];
		img->downw *= (float)plug->av_video->f_width / img->img.width;
		img->downh *= (float)plug->av_video->f_height / img->img.height;
		img->time.tv_sec = plug->av_frames->captureTime[0].tv_sec;
		img->time.tv_usec = plug->av_frames->captureTime[0].tv_usec;
#else
		img->img.width = plug->av_frames->imgWidth;
		img->img.height = plug->av_frames->imgHeight;
		img->img.data = plug->planeptr;
		img->img.data[0] = plug->av_frames->imgData[0];
		img->img.data[1] = plug->av_frames->imgData[1];
		img->img.data[2] = plug->av_frames->imgData[2];
		img->img.planes = plug->av_frames->nbPlanesPerFrame;
		img->img.ctab = IW_YUV;
		img->downw *= (float)PAL_WIDTH_DEFAULT / img->img.width;
		img->downh *= (float)PAL_HEIGHT_DEFAULT*2 / img->img.height;

		img->time.tv_sec = plug->av_frames->captureTime[0]->tv_sec;
		img->time.tv_usec = plug->av_frames->captureTime[0]->tv_usec;
		/* Check for problem in AVlib/MMElib with timestamps */
		{
			struct timeval t;
			gettimeofday (&t, NULL);
			if (abs(IW_TIME_DIFF(img->time,t)) > 1000)
				iw_warning ("Wrong time stamp:\n"
							"    gettimeofday(): %d %d\n"
							"    AVLib         : %d %d",
							t.tv_sec, t.tv_usec, img->time.tv_sec, img->time.tv_usec);
		}
#endif
	} else if (plug->para.device == GRAB_DACS) {
		Bild_t *input = plug->dacs_input;		/* Input image for DACS-Stream */
		DACSstatus_t status;

		if (input) {
			ndr_free ((void **)&input, (NDRfunction_t*)ndr_Bild);
			input = NULL;
		}
		status = dacs_recv (iw_dacs_entry, plug->para.source,
							(NDRfunction_t*)ndr_Bild, (void **)&input);
		plug->dacs_input = input;
		if (status != D_OK) {
			if (status == D_NO_DATA) {
				iw_debug (3,"re-attaching to signal stream '%s'", plug->para.source);
				if (dacs_order_stream_with_retry (iw_dacs_entry, plug->para.source, NULL,
												  plug->para.sync_level, MODULE_ATTACH_RETRY)
					!= D_OK)
					iw_error ("Can't re-attach to signal stream '%s'", plug->para.source);
			} else if (status != D_SYNC) {
				iw_warning ("Received image with status %d from '%s', ignoring...",
							status, plug->para.source);
			}
			return FALSE;
		}

		if ((input->inhalt & R_BILD) &&
			(input->inhalt & G_BILD) &&
			(input->inhalt & B_BILD)) {
			img->img.ctab = IW_RGB;
		} else if ((input->inhalt & Y_BILD) &&
				   (input->inhalt & U_BILD) &&
				   (input->inhalt & YUV_V_BILD)) {
			img->img.ctab = IW_YUV;
		} else if (input->inhalt & SW_BILD) {
			img->img.ctab = IW_GRAY;
		} else {
			iw_warning ("Received image with unknown content %d, ignoring...",
						input->inhalt);
			return FALSE;
		}

		img->img.width = input->delta.x;
		img->img.height = input->delta.y;
		img->img.planes = input->anzahl;
		img->img.data = plug->planeptr;
		img->img.data[0] = input->bild[0][0];
		if (img->img.planes > 1)
			img->img.data[1] = input->bild[1][0];
		else
			img->img.data[1] = NULL;
		if (img->img.planes > 2)
			img->img.data[2] = input->bild[2][0];
		else
			img->img.data[2] = NULL;
		img->time.tv_sec = input->kopf.sec;
		img->time.tv_usec = input->kopf.usec;
	} else {	/* if (plug->para.device == GRAB_PPM) */
		fsetImage f_img;
		iwImgStatus status;
		int fset_length, i, idelta, icount;
		int direction;

		f_img.fname = img->fname;

		/* Set new fileset position */
		i = plug->values.fset_pos;
		idelta = 1;
		direction = 0;
		if (grab_cont == GRAB_CONT_PREV || grab_cont == GRAB_CONT_AUTOPREV) {
			i--;
			idelta = -1;
			direction = IW_MOVIE_PREV_FRAME;
		} else if (grab_cont == GRAB_CONT_NEXT || grab_cont == GRAB_CONT_AUTONEXT) {
			i++;
			direction = IW_MOVIE_NEXT_FRAME;
		} else if (grab_cont == GRAB_CONT_FIRST)
			i = 0;
		fset_length = iw_fset_get_length (plug->fset);
		icount = 0;
		do {
			i = iw_fset_set_pos (plug->fset, i, FALSE);
			if (i < fset_length)
				opts_value_set (plug->fset_name, GINT_TO_POINTER(i));

			/* Try to read an image */
			status = iw_fset_get (plug->fset, &f_img, direction);
			if (status == IW_IMG_STATUS_EOS) {
				i = iw_fset_set_pos (plug->fset, 0, TRUE);
				opts_value_set (plug->fset_name, GINT_TO_POINTER(i));
				if (plug->para.ppm_once) {
					iw_debug (3, "file set played, exiting...");
					gui_exit (0);
				}
				status = iw_fset_get (plug->fset, &f_img, direction);
			}
			/* If iw_fset_get() skipped images update the slider */
			if (i != f_img.fset_pos && f_img.fset_pos < fset_length)
				opts_value_set (plug->fset_name, GINT_TO_POINTER(f_img.fset_pos));

			if (status != IW_IMG_STATUS_OK) {
				iw_warning ("Can't read image %d from file set, skipping...", i);
				i += idelta;
				icount++;
				if (icount > fset_length)
					iw_error ("Unable to read any image from file set");
			}
		} while (status != IW_IMG_STATUS_OK);
		img->frame_number = f_img.frame_num;

		img->img = *f_img.img;
		if (img->img.ctab == IW_RGB)
			img->img.ctab = f_img.rgb ? IW_RGB : IW_YUV;

		if (plug->values.ppm_timestep < 0) {
			gettimeofday (&img->time, NULL);
		} else if (plug->values.ppm_timestep > 0 || f_img.time.tv_sec == 0) {
			if (plug->values.ppm_timestep > 0)
				img->time.tv_usec += 1000*plug->values.ppm_timestep;
			else
				img->time.tv_usec += f_img.time.tv_usec;
			if (img->time.tv_usec >= 1000*1000) {
				img->time.tv_sec++;
				img->time.tv_usec -= 1000*1000;
			}
		} else {
			img->time = f_img.time;
		}
	}

	return TRUE;
}

/*********************************************************************
  Process image src by stereo-deinterlacing, bayer-decoding, cropping,
  downsampling, and rotating it (in that order) and save the result
  to dst.
*********************************************************************/
static void img_process (grabPlugin *plug, iwImage *orig, iwImage *dst,
						 int downw, int downh)
{
	int c_x = plug->para.crop_x, c_y = plug->para.crop_y,
		c_w = plug->para.crop_w, c_h = plug->para.crop_h;
	int rotate = plug->para.rotate;
	iwImgBayer bayer = plug->para.bayer;
	iwImage *src = orig, s_img, b_img;

	b_img.data = s_img.data = NULL;

	if (plug->para.stereo) {
		if (src->planes == 1 && src->type <= IW_16U) {
			s_img = *src;
			s_img.data = iw_img_get_buffer (sizeof(uchar*)+src->width*src->height);
			s_img.data[0] = (uchar*)s_img.data + sizeof(uchar*);
			iw_img_stereo_decode (src->data[0], s_img.data[0], src->type,
								  src->width*src->height/2);
			src = &s_img;
		} else
			iw_warning ("Stereo deinterlacing only supported\n"
						"with 8/16 bit images with one plane, skipping...");
	}

	if (bayer != IW_IMG_BAYER_NONE) {
		if (src->planes == 1 && src->type <= IW_16U && src->width%2 == 0) {
			b_img = *src;
			b_img.planes = 3;
			b_img.ctab = IW_RGB;
			if (bayer == IW_IMG_BAYER_DOWN) {
				b_img.width /= 2;
				b_img.height /= 2;
			}
			if (!iw_img_allocate (&b_img))
				iw_error ("Unable to allocate memory for bayer converted image");
			iw_img_bayer_decode (src->data[0], &b_img, bayer, plug->para.pattern);
			src = &b_img;
		} else
			iw_warning ("Bayer decoding only supported\n"
						"\twith 8/16 bit images with one plane and even width, skipping...");
	}

	dst->ctab = src->ctab;

	if (c_x < 0) c_x = 0;
	if (c_y < 0) c_y = 0;
	if (c_w <= 0) c_w = src->width + c_w;
	if (c_x+c_w > src->width) c_w = src->width - c_x;
	if (c_h <= 0) c_h = src->height + c_h;
	if (c_y+c_h > src->height) c_h = src->height - c_y;

	if (rotate == 0 && c_x == 0 && c_y == 0 && c_w == src->width && c_h == src->height) {
		iw_img_downsample (src, dst, downw, downh);
	} else {
		int d_width, d_height, line, start, xs, ys;
		int bytes = IW_TYPE_SIZE(src), planes = src->planes;
		uchar *s, *d;

		if (rotate == 90 || rotate == 270) {
			d_width = c_h / downh;
			d_height = c_w / downw;
		} else {
			d_width = c_w / downw;
			d_height = c_h / downh;
		}

		if (dst->width != d_width || dst->height != d_height ||
			dst->planes != planes || dst->type != src->type) {
			iw_img_free (dst, IW_IMG_FREE_PLANE|IW_IMG_FREE_PLANEPTR);
			dst->width = d_width;
			dst->height = d_height;
			dst->planes = planes;
			dst->type = src->type;
			if (!iw_img_allocate (dst))
				iw_error ("Unable to allocate memory for converted image");
		}

		start = line = 0;
		if (rotate == 0) {
			line = (downh-1)*src->width + src->width-c_w + (c_w%downw);
		} else if (rotate == 90) {
			start = (d_width-1)*downh*src->width;
			line = d_width*downh*src->width+downw;
			downw = -downh*src->width;
		} else if (rotate == 180) {
			start = (d_height-1)*downh*src->width+(d_width-1)*downw;
			line = -((downh-1)*src->width + src->width-c_w + (c_w%downw));
			downw = -downw;
		} else if (rotate == 270) {
			start = (d_height-1)*downw;
			line = -(d_width*downh*src->width+downw);
			downw = downh*src->width;
		}
		start = (start + c_y*src->width + c_x) * bytes;
		line *= bytes;
		downw *= bytes;

		planes--;
		for (; planes>=0; planes--) {
			d = dst->data[planes];
			s = src->data[planes] + start;
			/* Special case 8U to speed it up */
			if (src->type == IW_8U) {
				for (ys=0; ys<d_height; ys++) {
					for (xs=0; xs<d_width; xs++) {
						*d++ = *s;
						s += downw;
					}
					s += line;
				}
			} else {
				for (ys=0; ys<d_height; ys++) {
					for (xs=0; xs<d_width; xs++) {
						memcpy (d, s, bytes);
						d += bytes;
						s += downw;
					}
					s += line;
				}
			}
		}
	}
	if (b_img.data)
		iw_img_free (&b_img, IW_IMG_FREE_DATA);
	if (s_img.data)
		iw_img_release_buffer();
}

/*********************************************************************
  Free the resources allocated during grab_???().
*********************************************************************/
static void grab_cleanup (plugDefinition *plug_d)
{
	grabPlugin *plug = (grabPlugin *)plug_d;
	if (plug->av_video) {
		AVClose (plug->av_video);
		plug->av_video = NULL;
	}
}

static void help (grabPlugin *plug, char* err, ...)
{
	va_list args;

	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Read images from the AVlib (-> a grabber), a DACS stream of type Bild_t,\n"
			 "or a file set and store them under the id '"IW_GRAB_IDENT"' with type\n"
			 "'grabImageData'. If an observer for '"IW_GRAB_RGB_IDENT"' exists, store\n"
			 "the images in RGB color space under this id with type 'grabImageData'.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-sg [inputDrv] [drvOptions] | -sd stream [synclev] | -sp <fileset>... |\n"
			 "      -sp1 <fileset>...] [-prop] [-c cnt] [-f [cnt]] [-r factor] [-stereo]\n"
			 "     [-bayer [method] [pattern]] [-crop x y w h] [-rot {0|90|180|270}]\n"
			 "     [-nyuv name] [-nrgb name] [-oi [interval]] [-h]\n"
			 "-sg       %s"
			 "-sd       use DACS stream with a specified sync level (default: %d) for input\n"
			 "-sp       use fileset for input, e.g. -sp image%%03d.ppm r image%%d.gif y image.jpg\n"
			 "          fileset = fileset | 'y' | 'r' | 'e' | 'E' | 'f' | 'F' | 'file'\n"
			 "          'y'|'r': following images are YUV|RGB-images, default: RGB\n"
			 "          'e'|'E': open files/check only extension to verify if files\n"
			 "                   are movie files, default: open files\n"
			 "          'f'|'F': FPS correct/single frames from movies, default: FPS\n"
			 "          'file' may contain %%d, %%t, and %%T\n"
			 "-sp1      use fileset for input and play it only once\n"
			 "-prop     do not create a GUI for changing grabber properties\n"
			 "-c        number of images queued in memory at the same time, default: 2\n"
			 "-f        grab full images, optional argument gives number of images\n"
			 "          to queue in memory at the same time, default: 1\n"
			 "-r        upsample factor needed to get to full sized images, default: 1\n"
			 "-stereo   expect interlaced gray stereo image and decompose it\n"
			 "-bayer    expect gray image with bayer pattern, use specified method and\n"
			 "          specified bayer pattern to decompose it, default: down RGGB;\n"
			 IW_IMG_BAYER_HELP
			 "              pattern: RGGB | BGGR | GRBG | GBRG\n"
			 "-crop     crop a rectangle from the input image\n"
			 "-rot      rotate the input image\n"
			 "-nyuv     specify the name of the id under which the images shall be stored,\n"
			 "          default: '"IW_GRAB_IDENT"'\n"
			 "-nrgb     specify the name of the id under which the RGB images shall be stored,\n"
			 "          default: '"IW_GRAB_RGB_IDENT"'\n"
			 "-oi       output every 'interval' image (default: 1) on stream <%s>_images\n"
			 "          and provide a function <%s>_setCrop(\"x1 y1 x2 y2\")\n"
			 "          to crop the streamed images\n",
			 plug->def.name, ICEWING_NAME, plug->def.name,
			 AVDriverHelp(), SYNC_LEVEL, IW_DACSNAME, IW_DACSNAME);
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
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
#define ARG_TEMPLATE "-SG:1 -SD:2r -SP:3r -SP1:4r -PROP:p -NYUV:nr -NRGB:Nr -C:ci -F:fio -R:ri -STEREO:s -BAYER:b -CROP:C -ROT:Oi -OI:7io -H:H -HELP:H --HELP:H"
static void grab_init (plugDefinition *plug_d, grabParameter *gpara, int argc, char **argv)
{
	grabPlugin *plug = (grabPlugin *)plug_d;
	grabLParameter *para;
	void *arg;
	char ch;
	int nr = 0, i;

	iw_img_init (&plug->img.img);
	plug->img.img_number = 0;
	plug->img.fname = malloc (PATH_MAX*sizeof(char));
	*plug->img.fname= '\0';
	plug->img.frame_number = 0;
	plug->img.downw = 1.0;
	plug->img.downh = 1.0;

	plug->interlace = FRAME_BOTH;
	plug->downsamp = -1;
	plug->dev_options[0] = '\0';
	plug->av_video = NULL;
	plug->av_frames = NULL;
	plug->dacs_input = NULL;
	plug->fset = NULL;
	plug->fset_name = NULL;
	plug->image_ident = IW_GRAB_IDENT;
	plug->image_rgb_ident = IW_GRAB_RGB_IDENT;

	plug->gray_bytes = -1;
	plug->gray_type = IW_8U;
	plug->gray_data[0] = NULL;
	plug->gray_data[1] = NULL;

	para = &plug->para;
	para->device = GRAB_NONE;
	para->avgui = TRUE;
	para->img_down = 1;
	para->stereo = FALSE;
	para->bayer = IW_IMG_BAYER_NONE;
	para->pattern = IW_IMG_BAYER_RGGB;
	para->crop_x = 0;
	para->crop_y = 0;
	para->crop_w = 0;
	para->crop_h = 0;
	para->rotate = 0;
	para->input_sig = PAL_S_VIDEO;
	para->source = NULL;
	para->ppm_once = FALSE;
	para->sync_level = SYNC_LEVEL;
	para->nb_imgs_up = 0;
	para->nb_imgs_down = 2;
	para->out_interval = -1;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'p':				/* -prop */
				para->avgui = FALSE;
				break;
			case 'c':				/* -c */
				para->nb_imgs_down = (int)(long)arg;
				if (para->nb_imgs_down <= 0)
					help (plug, "Number of images %d must be greater than zero!",
						  para->nb_imgs_down);
				break;
			case 'n':				/* -n */
				plug->image_ident = arg;
				nr++;
				break;
			case 'N':				/* -nrgb */
				plug->image_rgb_ident = arg;
				nr++;
				break;
			case 'f':				/* -f */
				para->nb_imgs_up = 1;
				if (arg != IW_ARG_NO_NUMBER) {
					para->nb_imgs_up = (int)(long)arg;
					if (para->nb_imgs_up <= 0)
						help (plug, "Number of images %d must be greater than zero!",
							  para->nb_imgs_up);
				}
				break;
			case 'r':				/* -r */
				para->img_down = (int)(long)arg;
				break;
			case 's':				/* -stereo */
				para->stereo = TRUE;
				break;
			case 'b':				/* -bayer */
				if (nr<argc && argv[nr][0] != '-') {
					if (!iw_img_bayer_parse (argv[nr], &para->bayer))
						help (plug, "Unknown bayer mode '%s' given!", argv[nr]);
					nr++;
					if (nr<argc && argv[nr][0] != '-') {
						if (!iw_img_bayer_pattern_parse (argv[nr], &para->pattern))
							help (plug, "Unknown bayer pattern '%s' given!", argv[nr]);
						nr++;
					}
				} else
					para->bayer = IW_IMG_BAYER_DOWN;
				break;
			case 'C':				/* -crop */
				if (nr+4 > argc)
					help (plug, "Argument '-crop' needs 4 integers!");
				for (i=0; i<4; i++) {
					char *end = NULL;
					strtol (argv[nr+i], &end, 10);
					if (!end || *end)
						help (plug, "Crop argument '%s' must be an integer!",
							  argv[nr+i]);
				}
				para->crop_x = atoi (argv[nr]); nr++;
				para->crop_y = atoi (argv[nr]); nr++;
				para->crop_w = atoi (argv[nr]); nr++;
				para->crop_h = atoi (argv[nr]); nr++;
				break;
			case 'O':				/* -rot */
				i = (int)(long)arg;
				if (i == 0 || i == 90 || i == 180 || i == 270)
					para->rotate = i;
				else
					help (plug, "Rotation angle (%d) must be in {0, 90, 180, 270}!", i);
				break;
			case '7':				/* -oi */
				if (arg != IW_ARG_NO_NUMBER)
					para->out_interval = (int)(long)arg;
				else
					para->out_interval = 1;
				gpara->output |= IW_OUTPUT_STREAM;
				break;
			case '1': {						/* -sg */
				int args = 0;

				if (para->device != GRAB_NONE)
					help (plug, "Only one of -sg, -sd, and -sp must be specified!");

				para->device = GRAB_AV;
				while (nr < argc && argv[nr][0] != '-' && args < 2) {
					if (args == 0) {
						if (!AVParseDriver (argv[nr], &para->input_sig))
							help (plug, "Unknown grabbing specification '%s'!",
								  argv[nr]);
						args++;
					} else {
						para->source = argv[nr];
						args++;
					}
					nr++;
				}
				break;
			}
			case '2':						/* -sd */
				if (para->device != GRAB_NONE)
					help (plug, "Only one of -sg, -sd, and -sp must be specified!");

				para->device = GRAB_DACS;
				para->source = (char*)arg;
				if (nr < argc) {
					char *end = NULL;
					int int_arg = strtol (argv[nr], &end, 10);
					if (end && !*end) {
						para->sync_level = int_arg;
						nr++;
					}
				}
				break;
			case '3':						/* -sp */
			case '4': {						/* -sp1 */
				int cnt = 0;

				if (para->device != GRAB_NONE && para->device != GRAB_PPM)
					help (plug, "Only one of -sg, -sd, and -sp must be specified!");

				if (ch == '4') para->ppm_once = TRUE;
				nr--;	/* Compensate for the already parsed required argument */
				do {
					cnt = iw_fset_add (&plug->fset, argv[nr]);
					nr++;
				} while (nr<argc && argv[nr][0]!='-');
				if (!cnt)
					help (plug, "Unable to find images specified with option %s!",
						  ch=='4' ? "-sp1":"-sp");
				para->device = GRAB_PPM;
				break;
			}


			case 'H':
			case '\0':
				help (plug, NULL);
			default:
				help (plug, "Unknown character %c!", ch);
		}
	}

	pthread_mutex_init (&plug->ring_img_mutex, NULL);
	ring_img_init (&plug->ring_up, plug->para.nb_imgs_up);
	ring_img_init (&plug->ring_down, plug->para.nb_imgs_down);

	if (plug->para.device == GRAB_NONE)
		iw_error ("No input source (Grabber, FileSet, or DACS) for reading images specified");

	if (plug->para.device == GRAB_DACS) {
		iw_output_register();

		if (dacs_order_stream_with_retry (iw_dacs_entry, plug->para.source,
										  NULL, plug->para.sync_level,
										  MODULE_ATTACH_RETRY) != D_OK)
			iw_error ("Can't attach to signal stream '%s'", plug->para.source);
	}

	if (plug->para.device == GRAB_AV) {
		if (para->source) {
			gui_strlcpy (plug->values.dev_options, para->source, DEVOPT_LENGTH);
			AVcap_optstr_cleanup (plug->values.dev_options);
			if (AVcap_optstr_lookup (plug->values.dev_options, "help")) {
				strcpy (plug->dev_options, plug->values.dev_options);
				grab_av_init (plug, TRUE);
			}
		}
		plug_function_register (plug_d, IW_GRAB_IDENT_PROPERTIES, (plugFunc)cb_grab_set_properties);
	} else
		plug->values.dev_options[0] = '\0';

	plug_observ_data (plug_d, "start");
	if (plug == plug_first)
		plug_function_register (plug_d, "mainCallback", (plugFunc)cb_grab_main);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int grab_init_options (plugDefinition *plug_d)
{
	static char *interlace[] = {"Both (Down 1:1)", "Even (Down 1:2)",
								"Even+Aspect (Down 2:2)", "Down 2:1/Virtual 2:2",
								NULL};
	grabPlugin *plug = (grabPlugin *)plug_d;
	int p;

	plug->values.interlace = FRAME_BOTH;
	plug->values.downsampling = 1;
	plug->values.ppm_timestep = -1;
	plug->values.time_wait_frame = FALSE;
	plug->values.time_wait = 0;
	plug->values.fset_pos = 0;
	plug->values.grab_cont = GRAB_CONT_AUTONEXT;

	if (plug == plug_first) {
		plug->b_input = grab_get_inputBuf();
	} else {
		plug->b_input = prev_new_window (plug_name(plug_d, ".Input data"),
										 -1, -1, FALSE, FALSE);
		prev_opts_append (plug->b_input, PREV_IMAGE, -1);
	}
	plug->b_y = prev_new_window (plug_name(plug_d, ".Y channel"), -1, -1, TRUE, FALSE);
	plug->b_u = prev_new_window (plug_name(plug_d, ".U channel"), -1, -1, TRUE, FALSE);
	plug->b_v = prev_new_window (plug_name(plug_d, ".V channel"), -1, -1, TRUE, FALSE);
	plug->b_rgb = prev_new_window (plug_name(plug_d, ".RGB image"), -1, -1, FALSE, FALSE);

	prev_opts_append (plug->b_y, PREV_IMAGE, -1);
	prev_opts_append (plug->b_u, PREV_IMAGE, -1);
	prev_opts_append (plug->b_v, PREV_IMAGE, -1);
	prev_opts_append (plug->b_rgb, PREV_IMAGE, -1);

	plug->fset_name = g_strconcat (plug->def.name, ".Image Num", NULL);
	opts_save_remove (plug->fset_name);

	p = opts_page_append (plug->def.name);

	opts_option_create (p, "Interlace:",
						"Grab full image / Grab one of the interlaced fields / Adapt for interlaced grabbing",
						interlace, &plug->values.interlace);
	opts_entscale_create (p, "Downsampling",
						  "Scale ratio for input image. If grabbing, the hardware may scale down.",
						  &plug->values.downsampling, 1, 10);
	if (plug->para.device == GRAB_AV) {
		char *name = g_strconcat (plug->def.name, ".Grabbing driver options:", NULL);
		opts_save_remove (name);
		free (name);

		opts_stringenter_create (p, "Grabbing driver options:",
								 "Options for the grabbing driver, e.g. \"camera=2:bayer=hue\"",
								 plug->values.dev_options, DEVOPT_LENGTH);
	}
	opts_entscale_create (p, "File TimeStep",
						  "Increment of timestamp (in ms) of loaded images  -1: gettimeofday()"
						  "  0: %T%t-timestamp / AVI-framerate is used",
						  &plug->values.ppm_timestep, -1, 500);
	if (plug == plug_first) {
		opts_toggle_create (p, "Frame correct Wait Time",
							"Should the 'Wait Time' be interpreted for a complete frame"
							" or as a local sleeping time?",
							&plug->values.time_wait_frame);

		opts_entscale_create (p, "Wait Time",
							  "Time in ms to wait between image grabs  "
							  "-1: wait until the button is pressed",
							  &plug->values.time_wait, -1, 2000);
		if (plug->para.device == GRAB_PPM) {
			opts_entscale_create (p, "Image Num", "Number of image to read",
								  &plug->values.fset_pos,
								  0, iw_fset_get_length(plug->fset)-1);
			opts_button_create (p, "\\|<|<<|<|O|>|>>",
								"Read first image|"
								"Read previous image continuously|"
								"Read previous image|"
								"Read current image|"
								"Read next image|"
								"Read next image continuously|",
								&plug->values.grab_cont);
		} else {
			opts_button_create (p, "Grab Image",
								"Grab next image if 'Wait Time == -1'",
								&plug->values.grab_cont);
		}
		if (plug->values.time_wait < 0) plug->values.grab_cont = 0;
		grab_cont = plug->values.grab_cont;
	} else if (plug->para.device == GRAB_PPM) {
		opts_entscale_create (p, "Image Num", "Number of image to read",
							  &plug->values.fset_pos,
							  0, iw_fset_get_length(plug->fset)-1);
		plug->values.grab_cont = grab_cont;
	}

	return p;
}

/*********************************************************************
  Free img including all substructures.
*********************************************************************/
void grab_image_free (grabImageData *img)
{
	if (img) {
		iw_img_free (&img->img, IW_IMG_FREE_DATA);
		if (img->fname)
			free (img->fname);
		free (img);
	}
}

/*********************************************************************
  Allocate a new image and init everything. -> Do NOT allocate any
  image data.
*********************************************************************/
grabImageData* grab_image_new (void)
{
	grabImageData *img = calloc (1, sizeof(grabImageData));
	img->downw = img->downh = 1;
	return img;
}

/*********************************************************************
  Return a deep copy of img.
*********************************************************************/
grabImageData* grab_image_duplicate (const grabImageData *img)
{
	grabImageData *ret = calloc (1, sizeof(grabImageData));

	iw_img_copy (&img->img, &ret->img);
	ret->time = img->time;
	ret->img_number = img->img_number;
	if (img->fname)
		ret->fname = strdup (img->fname);
	ret->frame_number = img->frame_number;
	ret->downw = img->downw;
	ret->downh = img->downh;

	return ret;
}

static void grab_render (prevBuffer *b, guchar **planes, int width, int height,
						 iwColtab ctab, iwType type)
{
	prevDataImage img;
	iwImage i;

	iw_img_init (&i);
	i.planes = (ctab > IW_COLFORMAT_MAX || b->gray || ctab == IW_GRAY) ? 1:3;
	i.type = type;
	i.width = width;
	i.height = height;
	i.ctab = ctab;
	i.data = planes;

	img.i = &i;
	img.x = 0;
	img.y = 0;

	prev_render_imgs (b, &img, 1, RENDER_CLEAR, width, height);
}

/*********************************************************************
  Callback of plug_data_set() for IW_GRAB_IDENT, release the stored image.
*********************************************************************/
static void grab_image_destroy (void *data)
{
	grabImgEntry *entry = data;
	if (entry) {
		if (entry->status == GRAB_IMG_DETACHED)
			ring_entry_free (entry);
		else
			entry->status = GRAB_IMG_FREE;
	}
}

/*********************************************************************
  Callback of plug_data_set() for IW_GRAB_RGB_IDENT,
  release the stored image.
*********************************************************************/
static void grab_image_destroy_rgb (void *data)
{
	grab_image_free (data);
}

/*********************************************************************
  Process the grabbed image/other data:
    AV -> crop -> down -> rotate -> FullQ,DACS -> down -> DownQ
    PPM                     \                          \
    DACS                     -> RGB -> down -> RGBDown  -> RGB
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL grab_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	grabPlugin *plug = (grabPlugin *)plug_d;
	int downw, downh;
	grabImageData *img, *rgb = NULL;
	BOOL rgb_observed = plug_data_is_observed(plug->image_rgb_ident);
	BOOL optsChanged;

	iw_time_add_static (time_grab, "Grab");

	/* Start with the initialized image number */
	if (plug->img.img_number <= 0) {
		if (grab_cont == GRAB_CONT_NEXT || grab_cont == GRAB_CONT_AUTONEXT)
			plug->values.fset_pos--;
	}
	plug->img.img_number++;

	/* Grabbing driver options changed? */
	if ((optsChanged = strcmp(plug->dev_options, plug->values.dev_options))) {
		char opts[DEVOPT_LENGTH];
		gui_strcpy_back (opts, plug->values.dev_options, 0);
		if (AVcap_optstr_cleanup(opts))
			opts_value_set (plug_name((plugDefinition*)plug, ".Grabbing driver options:"), opts);
		if ((optsChanged = strcmp(plug->dev_options, opts)))
			strcpy (plug->dev_options, opts);
	}

	/* Any image parameter changed? */
	if (plug->downsamp != plug->values.downsampling
		|| plug->interlace != plug->values.interlace
		|| optsChanged) {

		BOOL first = plug->downsamp < 0;
		plug->interlace = plug->values.interlace;
		plug->downsamp = plug->values.downsampling;

		if (plug->para.device == GRAB_AV)
			grab_av_init (plug, FALSE);
	}

	if (plug->av_video && plug->para.avgui)
		grab_process_avgui (plug_d, &plug->avgui_data);

	iw_time_start (time_grab);
	if (!img_grab (plug, &plug->img)) {
		iw_time_stop (time_grab, FALSE);
		return TRUE;
	}

	pthread_mutex_lock (&plug->ring_img_mutex);

	/* Advance to the next image in the two ring buffers */
	if (plug->ring_down.max > 1) {
		plug->ring_down.cur++;
		if (plug->ring_down.cur > plug->ring_down.i+plug->ring_down.max-1)
			plug->ring_down.cur = plug->ring_down.i;
	}
	if (plug->ring_up.max > 1) {
		plug->ring_up.cur++;
		if (plug->ring_up.cur > plug->ring_up.i+plug->ring_up.max-1)
			plug->ring_up.cur = plug->ring_up.i;
	}

	plug->ring_down.cur[0]->img.fname = NULL;
	/* If current image still in use (plug_data_set()-refCount not zero),
	   detach it and allocate a new one */
	if (plug->ring_down.cur[0]->status == GRAB_IMG_USE) {
		plug->ring_down.cur[0]->status = GRAB_IMG_DETACHED;
		plug->ring_down.cur[0] = ring_entry_alloc();
	}
	plug->ring_down.cur[0]->status = GRAB_IMG_USE;

	iw_debug (4, "Getting image...");

	/* Downsampling to get from grabbed image (not original image,
	   AV lib may have already downsampled the image) to final/full image */
	downw = downh = 1;
	if (plug->para.device == GRAB_AV) {
		if (plug->interlace == FRAME_EVEN_ASPECT || plug->interlace == FRAME_DOWN21)
			downw = 2;
	} else {
		if (!plug->ring_up.cur)
			downw = downh = plug->downsamp;
		if (plug->interlace == FRAME_DOWN21 || plug->interlace == FRAME_EVEN_ASPECT)
			downw *= 2;
		if (plug->interlace == FRAME_EVEN || plug->interlace == FRAME_EVEN_ASPECT)
			downh *= 2;
	}
	if (plug->ring_up.cur)
		img = &plug->ring_up.cur[0]->img;
	else
		img = &plug->ring_down.cur[0]->img;

	img->time = plug->img.time;
	img->img_number = plug->img.img_number;
	img->frame_number = plug->img.frame_number;
	img->downw = plug->img.downw * downw;
	img->downh = plug->img.downh * downh;
	/* Stereo/Bayer-decode, crop, resize, rotate image */
	img_process (plug, &plug->img.img, &img->img, downw, downh);

	if (img->img.ctab == IW_RGB) {
		if (rgb_observed)
			rgb = grab_image_duplicate (img);
		iw_img_rgbToYuvVis (&img->img);
	}

	plug->img.img = img->img;

	iw_time_stop (time_grab, FALSE);

#ifdef IW_DEBUG
	if (plug->img.fname[0]) {
		if (img->frame_number > 0)
			iw_debug (3, "Image %d (%s:%d) received, size: %dx%d",
					  img->img_number, plug->img.fname,img->frame_number,
					  img->img.width, img->img.height);
		else
			iw_debug (3, "Image %d (%s) received, size: %dx%d",
					  img->img_number, plug->img.fname, img->img.width, img->img.height);
	} else
		iw_debug (3, "Image %d received, size: %dx%d",
				  img->img_number, img->img.width, img->img.height);
#endif
	if (plug->para.out_interval >= 0 &&
		(img->img_number % plug->para.out_interval) == 0) {
		iw_output_image (img, NULL);
	}
	if (plug->ring_up.cur) {
		grabImageData *img = &plug->ring_down.cur[0]->img;

		img->time = plug->img.time;
		img->img_number = plug->img.img_number;
		img->frame_number = plug->img.frame_number;
		img->downw = plug->ring_up.cur[0]->img.downw * plug->downsamp;
		img->downh = plug->ring_up.cur[0]->img.downh * plug->downsamp;

		iw_img_downsample (&plug->img.img, &img->img, plug->downsamp, plug->downsamp);
		img->img.ctab = plug->img.img.ctab;
		if (rgb) {
			iwImage full = rgb->img;
			iw_img_init (&rgb->img);
			iw_img_downsample (&full, &rgb->img, plug->downsamp, plug->downsamp);
			/* To be optimized, reuse the image */
			iw_img_free (&full, IW_IMG_FREE_DATA);
		}
	}

	pthread_mutex_unlock (&plug->ring_img_mutex);

	{
		iwImage *img = &plug->ring_down.cur[0]->img.img;
		int w = img->width, h = img->height;
		uchar **planes = img->data;

		/* Always provide at least 3 planes to prevent SegV's in plugins */
		if (img->planes < 3) {
			int bytes = w * h * IW_TYPE_SIZE(img);

			if (bytes != plug->gray_bytes || img->type != plug->gray_type) {
				plug->gray_bytes = bytes;
				plug->gray_type = img->type;
				plug->gray_data[0] = realloc (plug->gray_data[0], bytes);
				plug->gray_data[1] = realloc (plug->gray_data[1], bytes);
				if (plug->gray_type == IW_8U) {
					memset (plug->gray_data[0], 128, bytes);
					memset (plug->gray_data[1], 128, bytes);
				} else if (plug->gray_type == IW_16U) {
					int x;
					bytes /= 2;
					for (x=0; x<bytes; x++) {
						((guint16*)plug->gray_data[0])[x] = 32768;
						((guint16*)plug->gray_data[1])[x] = 32768;
					}
				} else {
					memset (plug->gray_data[0], 0, bytes);
					memset (plug->gray_data[1], 0, bytes);
				}
			}
			planes[2] = plug->gray_data[1];
			if (img->planes < 2)
				planes[1] = plug->gray_data[0];
		}

		grab_render (plug->b_input, planes, w, h,
					 img->planes == 1 ? IW_GRAY:IW_YUV, img->type);
		/* Prevent flicker by not redrawing the main input window */
		if (plug != plug_first)
			prev_draw_buffer (plug->b_input);

		grab_render (plug->b_y, &planes[0], w, h, IW_YUV, img->type);
		prev_draw_buffer (plug->b_y);

		grab_render (plug->b_u, &planes[1], w, h, IW_YUV, img->type);
		prev_draw_buffer (plug->b_u);

		grab_render (plug->b_v, &planes[2], w, h, IW_YUV, img->type);
		prev_draw_buffer (plug->b_v);

		if (*plug->img.fname)
			plug->ring_down.cur[0]->img.fname = plug->img.fname;
		else
			plug->ring_down.cur[0]->img.fname = NULL;

		if (rgb_observed) {
			if (rgb) {
				if (plug->ring_down.cur[0]->img.fname)
					rgb->fname = strdup (plug->ring_down.cur[0]->img.fname);
				rgb->downw = plug->ring_down.cur[0]->img.downw;
				rgb->downh = plug->ring_down.cur[0]->img.downh;
			} else {
				rgb = grab_image_duplicate (&plug->ring_down.cur[0]->img);
				iw_img_yuvToRgbVis (&rgb->img);
			}
			plug_data_set (plug_d, plug->image_rgb_ident , rgb, grab_image_destroy_rgb);
		}
		if (plug->b_rgb->window) {
			prevDataImage img;
			img.i = &rgb->img;
			img.x = img.y = 0;
			if (!img.i) {
				img.i = iw_img_duplicate (&plug->ring_down.cur[0]->img.img);
				iw_img_yuvToRgbVis (img.i);
			}
			prev_render_imgs (plug->b_rgb, &img, 1, RENDER_CLEAR, w, h);
			prev_draw_buffer (plug->b_rgb);
			if (!rgb)
				iw_img_free (img.i, IW_IMG_FREE_ALL);
		}

		plug_data_set (plug_d, plug->image_ident, plug->ring_down.cur[0], grab_image_destroy);
	}

	return TRUE;
}

static plugDefinition plug_grab = {
	"GrabImage%d",
	PLUG_ABI_VERSION,
	grab_init,
	grab_init_options,
	grab_cleanup,
	grab_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_grab_get_info (int instCount, BOOL *append)
{
	grabPlugin *plug = calloc (1, sizeof(grabPlugin));

	plug->def = plug_grab;
	plug->def.name = g_strdup_printf (plug_grab.name, instCount);

	if (instCount == 1) plug_first = plug;

	*append = TRUE;
	return (plugDefinition*)plug;
}
