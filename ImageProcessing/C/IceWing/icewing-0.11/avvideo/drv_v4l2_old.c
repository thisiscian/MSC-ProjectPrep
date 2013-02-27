/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*=========================================================================
  This code is based on the grabbing support from CMVision-1.2.0, which is
  released under the following copyright:

	capture.cc
  -------------------------------------------------------------------------
	Example code for video capture under Video4Linux II
  -------------------------------------------------------------------------
	Copyright 1999, 2000
	Anna Helena Reali Costa, James R. Bruce
	School of Computer Science
	Carnegie Mellon University
  -------------------------------------------------------------------------
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
  -------------------------------------------------------------------------
	Revision History:
	  2000-02-05:  Ported to work with V4L2 API
	  1999-11-23:  Quick C++ port to simplify & wrap in an object (jbruce)
	  1999-05-01:  Initial version (annar)
  =========================================================================*/

#include "config.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "videodev2_old.h"

#include "AVOptions.h"
#include "AVColor.h"
#include "tools/tools.h"
#include "drv_v4l2_old.h"

#define DEFAULT_VIDEO_DEVICE_L	"/dev/video"
#define DEFAULT_VIDEO_DEVICE	"/dev/video0"
#define Vdebug(x)				if (data->debug) fprintf x

typedef struct v4l2Image {
	struct v4l2_buffer vidbuf;
	char *data;
} v4l2Image;

typedef struct v4l2Data {
	AVcapData data;
	int vid_fd;					/* Video device */
	int image_cnt;
	v4l2Image *image;			/* Buffers for images */
	int pixelformat;
	BOOL select;
	BOOL debug;
} v4l2Data;

#ifdef COMMENT
static void grabSetFps (int fd, int fps)
{
	struct v4l2_streamparm params;

	/* printf ("called v4l2_set_fps with fps=%d\n", fps); */
	params.type = V4L2_BUF_TYPE_CAPTURE;
	ioctl (fd, VIDIOC_G_PARM, &params);
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe); */
	params.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
	params.parm.capture.timeperframe = 10000000 / fps;
	if (fps == 30)
		params.parm.capture.timeperframe = 333667;
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe); */
	ioctl (fd, VIDIOC_S_PARM, &params);

	params.parm.capture.timeperframe = 0;
	ioctl (fd, VIDIOC_G_PARM, &params);
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe);   */
}
#endif /* COMMENT */

/*********************************************************************
  Print all properties and their values from device data->vid_fd.
*********************************************************************/
static void properties_print (v4l2Data *data)
{
	struct v4l2_queryctrl qctrl;
	struct v4l2_control ctrl;
	int id, num, base;

	fprintf (stderr, "v4l2_old: Available properties:\n");

	num = 0;
	base = V4L2_CID_BASE;
	for (id = 0; id < 999; id++) {
		qctrl.id = base + id;
		if (ioctl (data->vid_fd, VIDIOC_QUERYCTRL, &qctrl)) {
			if (base != V4L2_CID_PRIVATE_BASE) {
				base = V4L2_CID_PRIVATE_BASE;
				id = -1;
				continue;
			}
			break;
		}
		if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			fprintf (stderr, "\t%2d: %-12s ", num, qctrl.name);
			ctrl.id = base + id;
			if (ioctl (data->vid_fd, VIDIOC_G_CTRL, &ctrl))
				fprintf (stderr, "unknown\n");
			else
				fprintf (stderr, "[0 - 1] %.2f\n",
						 (float)(ctrl.value - qctrl.minimum)/(qctrl.maximum - qctrl.minimum));
			num++;
		}
	}
}

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property from data->vid_fd. Return if an error occurred.
*********************************************************************/
static int properties_set (v4l2Data *data, char *devOptions)
{
#define ARG_LEN		301
	struct v4l2_queryctrl qctrl;
	struct v4l2_control ctrl;
	char option[20], arg[ARG_LEN];
	double value;
	int id, num, base;

	if (!devOptions || !strstr(devOptions, "prop"))
		return TRUE;

	num = 0;
	base = V4L2_CID_BASE;
	for (id = 0; id < 999; id++) {
		qctrl.id = base + id;
		if (ioctl (data->vid_fd, VIDIOC_QUERYCTRL, &qctrl)) {
			if (base != V4L2_CID_PRIVATE_BASE) {
				base = V4L2_CID_PRIVATE_BASE;
				id = -1;
				continue;
			}
			break;
		}
		if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			sprintf (option, "prop%d", num);
			memset (arg, '\0', ARG_LEN);
			if (AVcap_optstr_get (devOptions, option, "%300c", arg) == 1) {
				if (sscanf (arg, "%lf", &value) < 1) {
					AVcap_error ("v4l2_old: Failed to set property %d, %s to '%s'",
								 num, qctrl.name, arg);
					return FALSE;
				}
				ctrl.id = base + id;
				ctrl.value = value * (qctrl.maximum - qctrl.minimum) + qctrl.minimum;
				if (ioctl (data->vid_fd, VIDIOC_S_CTRL, &ctrl)) {
					AVcap_error ("v4l2_old: Failed to set property %d, %s to '%.2f'",
								 num, qctrl.name, value);
					return FALSE;
				}
				Vdebug ((stderr, "v4l2_old: Set property %d, %s to value '%.2f'\n",
						 num, qctrl.name, value));
			}
			num++;
		}
	}
	return TRUE;
}

int AVv4l2_old_open (VideoDevData *dev, char *devOptions,
					 int subSample, VideoStandard vidSignal)
{
	static int pref_formats[] = {
		V4L2_PIX_FMT_YUYV,
		V4L2_PIX_FMT_UYVY,
		V4L2_PIX_FMT_YUV422P,
		V4L2_PIX_FMT_YUV420,
		V4L2_PIX_FMT_YVU420,
		V4L2_PIX_FMT_YUV411P,
		V4L2_PIX_FMT_Y41P,
		V4L2_PIX_FMT_YUV410,
		V4L2_PIX_FMT_YVU410,
		V4L2_PIX_FMT_RGB24,
		V4L2_PIX_FMT_BGR24,
		V4L2_PIX_FMT_RGB32,
		V4L2_PIX_FMT_BGR32,
		V4L2_PIX_FMT_RGB565,
		V4L2_PIX_FMT_RGB565X,
		V4L2_PIX_FMT_RGB555,
		V4L2_PIX_FMT_RGB555X,
		V4L2_PIX_FMT_GREY,
		V4L2_PIX_FMT_HI240,
		0};
	struct v4l2_requestbuffers req;
	struct v4l2_control ctrl;
	struct v4l2_input inp;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_format fmt;
	struct stat statbuf;
	char device[256];
	int err;
	int format_num = -1, format_pix = -1;
	int input_num = 0;
	int i, j, best, format_best;
	v4l2Data *data;

	/* Grabbing method supported? */
	if (vidSignal != PAL_COMPOSITE && vidSignal != PAL_S_VIDEO && vidSignal != AV_V4L2)
		return FALSE;

	data = calloc (1, sizeof(v4l2Data));
	dev->capData = (AVcapData*)data;

	/* Init arguments to default values */
	if (stat (DEFAULT_VIDEO_DEVICE_L, &statbuf) != -1)
		strcpy (device, DEFAULT_VIDEO_DEVICE_L);
	else
		strcpy (device, DEFAULT_VIDEO_DEVICE);
	data->image_cnt = 4;
	data->select = TRUE;

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "v4l2 (old interface) driver V0.2 by Frank Loemker, Jannik Fritsch\n"
					 "usage: device=name:input=num:format=num:prop0=val:prop1=val:buffer=cnt:debug\n"
					 "device   device name, default: %s\n"
					 "input    video input [0...], default: %s\n"
					 "format   format number [0...], default: YUV format with most BPP\n"
					 "propX    set property X [0...] to val\n"
					 "buffer   number of buffers used by the device driver, default: %d\n"
					 "noselect do not use select() to access the grabber, default: off\n"
					 "debug    output debugging/settings information, default: off\n\n",
					 device,
					 vidSignal == PAL_COMPOSITE ? "first composite input" :
					 (vidSignal == PAL_S_VIDEO ? "first s-video input" : "0"),
					 data->image_cnt);
			return FALSE;
		}
		AVcap_optstr_get (devOptions, "device", "%s", device);
		AVcap_optstr_get (devOptions, "format", "%d", &format_num);
		AVcap_optstr_get (devOptions, "buffer", "%d", &data->image_cnt);
		if (AVcap_optstr_lookup (devOptions, "noselect"))
			data->select = FALSE;
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		/* If video input is specified no guessing about the input */
		if (AVcap_optstr_get (devOptions, "input", "%d", &input_num) == 1)
			vidSignal = AV_V4L2;
	}

	/* Open the video device */
	data->vid_fd = open (device, O_RDONLY);
	if (data->vid_fd == -1) {
		AVcap_error ("v4l2_old: Could not open video device [%s]", device);
		return FALSE;
	}

	/* Set given properties and show available ones */
	if (!properties_set (data, devOptions))
		return FALSE;
	if (data->debug)
		properties_print (data);

	/* Check available pixel formats */
	Vdebug ((stderr, "v4l2_old: Available formats:\n"));
	best = format_best = 9999;
	memset (&fmtdesc, 0, sizeof(fmtdesc));
	for (i=0; ; i++) {
		fmtdesc.index = i;
		if (ioctl(data->vid_fd, VIDIOC_ENUM_PIXFMT, &fmtdesc)) {
			if (i == 0) {
				AVcap_warning ("v4l2_old: No formats available");
				return FALSE;
			}
			break;
		}

		Vdebug ((stderr, "\t%2d: %s\n", i, fmtdesc.description));
		if (i == format_num || i == 0)
			format_pix = fmtdesc.pixelformat;
		if (format_num < 0) {
			for (j=0; j<best && pref_formats[j]; j++)
				if (pref_formats[j] == fmtdesc.pixelformat) {
					best = j;
					format_best = i;
					format_pix = fmtdesc.pixelformat;
				}
		}
	}
	if (format_num < 0) {
		format_num = format_best;
		if (format_num > i)
			format_num = 0;
	}

	/* Set the capture format and the capture size */
	fmt.type = V4L2_BUF_TYPE_CAPTURE;
	if (ioctl (data->vid_fd, VIDIOC_G_FMT, &fmt)) {
		AVcap_error ("v4l2_old: G_FMT returned error");
		return FALSE;
	}

	fmt.fmt.pix.width = AV_DEFAULT_WIDTH;
	fmt.fmt.pix.height = AV_DEFAULT_HEIGHT;
	fmt.fmt.pix.pixelformat = format_pix;
	/* Get possible full width/height */
	if (ioctl (data->vid_fd, VIDIOC_S_FMT, &fmt)) {
		/* Ignore error */
	}
	dev->f_width = fmt.fmt.pix.width;
	dev->f_height = fmt.fmt.pix.height;

	if (subSample > 0) {
		fmt.fmt.pix.width /= subSample;
		fmt.fmt.pix.height /= subSample;
	}
	/* HACK: Prevent grabbing errors by rounding to values divisible by 16 */
	fmt.fmt.pix.width -= fmt.fmt.pix.width % 16;
	fmt.fmt.pix.height -= fmt.fmt.pix.height % 16;
	fmt.fmt.pix.flags = fmt.fmt.pix.flags & ~V4L2_FMT_FLAG_INTERLACED;
	if (dev->field == FIELD_EVEN_ONLY)
		fmt.fmt.pix.flags |= V4L2_FMT_FLAG_TOPFIELD;
	else if (dev->field == FIELD_ODD_ONLY)
		fmt.fmt.pix.flags |= V4L2_FMT_FLAG_ODDFIELD;
	if (ioctl (data->vid_fd, VIDIOC_S_FMT, &fmt)) {
		AVcap_error ("v4l2_old: S_FMT returned error");
		return FALSE;
	}

	/* Get the real capture format */
	if (ioctl (data->vid_fd, VIDIOC_G_FMT, &fmt) < 0) {
		AVcap_error ("v4l2_old: regetting G_FMT returned error");
		return FALSE;
	}
	i = fmt.fmt.pix.pixelformat;
	Vdebug ((stderr, "v4l2_old: Using format %d: %c%c%c%c  Size: %dx%d\n",
			 format_num, (char)(i), (char)(i>>8), (char)(i>>16), (char)(i>>24),
			 fmt.fmt.pix.width, fmt.fmt.pix.height));

	/* Set the AGC OFF */
	ctrl.id = V4L2_CID_PRIVATE_BASE + 11; /* experimentell-simulativ ermittelt :-) */
	ctrl.value = 0;
	if (ioctl (data->vid_fd, VIDIOC_S_CTRL, &ctrl))
		AVcap_warning ("v4l2_old: Unable to switch of AGC");

	/* Check available video inputs */
	if (data->debug) {
		fprintf (stderr, "v4l2_old: Available video inputs:\n");
		for (i = 0; ; i++) {
			inp.index = i;
			if (ioctl (data->vid_fd, VIDIOC_ENUMINPUT, &inp) < 0)
				break;
			fprintf (stderr, "\t%d: %s\n", i, inp.name);
		}
	}

	/* Select the input appropriate for vidSignal */
	if (vidSignal != AV_V4L2) {
		for (input_num = 0; ; input_num++) {
			inp.index = input_num;
			if (ioctl (data->vid_fd, VIDIOC_ENUMINPUT, &inp) < 0) {
				AVcap_error ("v4l2_old: Error getting %s video input",
							 vidSignal == PAL_COMPOSITE ? "composite":"s-video");
				break;
			}

			if (vidSignal == PAL_COMPOSITE) {
				if (!strncasecmp((char*)inp.name, "composite", 9))
					break;
			} else if (vidSignal == PAL_S_VIDEO) {
				if (!strncasecmp((char*)inp.name, "svideo", 6) ||
					!strncasecmp((char*)inp.name, "s-video", 7))
					break;
			}
		}
		Vdebug ((stderr, "v4l2_old: Using input %d: %s\n", inp.index, inp.name));
	} else
		Vdebug ((stderr, "v4l2_old: Using input %d\n", input_num));

	if (ioctl (data->vid_fd, VIDIOC_S_INPUT, &input_num)) {
		AVcap_error ("v4l2_old: Error setting video input");
		return FALSE;
	}

	/* grabSetFps (data->vid_fd, 30); */

	/* Request mmap-able capture buffers */
	req.count = data->image_cnt;
	req.type  = V4L2_BUF_TYPE_CAPTURE;
	err = ioctl (data->vid_fd, VIDIOC_REQBUFS, &req);
	if (err < 0 || req.count < 1) {
		AVcap_error ("v4l2_old: REQBUFS returned error, buffer count=%d", req.count);
		return FALSE;
	}
	data->image_cnt = req.count;

	data->image = calloc (1, sizeof(v4l2Image)*data->image_cnt);
	for (i=0; i<data->image_cnt; i++) {
		data->image[i].vidbuf.index = i;
		data->image[i].vidbuf.type = V4L2_BUF_TYPE_CAPTURE;
		err = ioctl (data->vid_fd, VIDIOC_QUERYBUF, &data->image[i].vidbuf);
		if (err < 0) {
			AVcap_error ("v4l2_old: QUERYBUF returned error");
			return FALSE;
		}

		data->image[i].data = (char*)mmap (0, data->image[i].vidbuf.length, PROT_READ,
										   MAP_SHARED, data->vid_fd,
										   data->image[i].vidbuf.offset);
		if (data->image[i].data == (void*)-1) {
			AVcap_error ("v4l2_old: mmap() returned error");
			return FALSE;
		}
	}

	for (i=0; i<data->image_cnt; i++) {
		if (ioctl (data->vid_fd, VIDIOC_QBUF, &data->image[i].vidbuf)) {
			AVcap_error ("v4l2_old: QBUF returned error");
			return FALSE;
		}
	}

	/* Turn on streaming capture */
	if (ioctl (data->vid_fd, VIDIOC_STREAMON, &data->image[0].vidbuf.type)) {
		AVcap_error ("v4l2_old: STREAMON returned error");
		return FALSE;
	}

	data->pixelformat = fmt.fmt.pix.pixelformat;

	dev->width = fmt.fmt.pix.width;
	dev->height = fmt.fmt.pix.height;
	if (data->pixelformat == V4L2_PIX_FMT_GREY ||
		data->pixelformat == V4L2_PIX_FMT_HI240)
		dev->planes = 1;
	else
		dev->planes = 3;
	dev->type = IW_8U;

	return TRUE;
}

void AVv4l2_old_close (void *capData)
{
	v4l2Data *data = capData;
	int i;

	if (data->vid_fd >= 0) {
		i = V4L2_BUF_TYPE_CAPTURE;
		ioctl (data->vid_fd, VIDIOC_STREAMOFF, &i);

		if (data->image) {
			for (i=0; i<data->image_cnt; i++) {
				if (data->image[i].data)
					munmap (data->image[i].data, data->image[i].vidbuf.length);
			}
			free (data->image);
		}
		close (data->vid_fd);
	}
	free (data);
}

static int vid_select (v4l2Data *data, int sec)
{
	fd_set rdset;
	struct timeval timeout;
	int n;

	FD_ZERO (&rdset);
	FD_SET (data->vid_fd, &rdset);
	timeout.tv_sec = sec;
	timeout.tv_usec = 0;
	n = select (data->vid_fd + 1, &rdset, NULL, NULL, &timeout);
	if (n == -1) {
		AVcap_error ("v4l2_old: select error");
	} else if (n == 0 && sec > 0) {
		fprintf (stderr, "select timeout\n");
	} else if (n != 0 && !FD_ISSET (data->vid_fd, &rdset)) {
		fprintf (stderr, "select() != %d, but fd not set\n", n);
		n = -1;
	}
	return n;
}

int AVv4l2_old_capture (void *capData, iwImage *img, struct timeval *time)
{
	v4l2Data *data = capData;
	struct v4l2_buffer tempbuf;
	int n, skipped = 0;
	unsigned char *current;

	if (data->select) {
		/* Grab last frame */
		n = vid_select (data, 1);
		if (n == -1 || n == 0)
			return FALSE;
		do {
			tempbuf.type = data->image[0].vidbuf.type;
			if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf))
				AVcap_error ("v4l2_old: DQBUF1 returned error");

			/* Check if more data (== more frames) is available */
			n = vid_select (data, 0);
			if (n == 0)
				break;
			else if (n > 0)
				skipped++;

			if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf))
				AVcap_error ("v4l2_old: QBUF1 returned error");
		} while (TRUE);
	} else {
		/* Grab the last frame */
		tempbuf.type = data->image[0].vidbuf.type;
		if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf))
			AVcap_error ("v4l2_old: DQBUF1 returned error");
	}

	/* If frames were skipped, grab a new current image */
	if (skipped) {
		Vdebug ((stderr, "Skipped %d buffers...\n", skipped));
		if (skipped == data->image_cnt-1) {
			/* Put last (very old) buffer back in Queue */
			if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf))
				AVcap_error ("v4l2_old: QBUF2 returned error");

			n = vid_select (data, 1);
			if (n == -1 || n == 0)
				return FALSE;

			/* ... and dequeue the next */
			tempbuf.type = data->image[0].vidbuf.type;
			if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf))
				AVcap_error ("v4l2_old: DQBUF2 returned error");
		}
	}

	/* Set current to point to captured frame data */
	current = (unsigned char *)data->image[tempbuf.index].data;
	gettimeofday (time, NULL);

	img->ctab = IW_YUV;
	switch (data->pixelformat) {
		case V4L2_PIX_FMT_GREY:
		case V4L2_PIX_FMT_HI240:
			memcpy (img->data[0], current, img->width * img->height);
			img->ctab = IW_GRAY;
			break;
		case V4L2_PIX_FMT_RGB555:
			av_color_bgr15 (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB555X:
			av_color_bgr15x (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB565:
			av_color_bgr16 (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB565X:
			av_color_bgr16x (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_BGR24:
			av_color_rgb24 (img, current, 2, 1, 0);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_BGR32:
			av_color_bgr32 (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB24:
			av_color_rgb24 (img, current, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB32:
			av_color_rgb32 (img, current);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_YUYV:
			av_color_yuyv (img, current, 0);
			break;
		case V4L2_PIX_FMT_UYVY:
			av_color_yuyv (img, current, 1);
			break;
		case V4L2_PIX_FMT_YUV422P:
			av_color_yuv422P (img, current);
			break;
		case V4L2_PIX_FMT_YUV420:
			av_color_yuv420 (img, current, 1);
			break;
		case V4L2_PIX_FMT_YVU420:
			av_color_yuv420 (img, current, 2);
			break;
		case V4L2_PIX_FMT_YUV411P:
			av_color_yuv411P (img, current);
			break;
		case V4L2_PIX_FMT_Y41P:
			av_color_yuv41P (img, current);
			break;
		case V4L2_PIX_FMT_YUV410:
			av_color_yuv410 (img, current, 1);
			break;
		case V4L2_PIX_FMT_YVU410:
			av_color_yuv410 (img, current, 2);
			break;
		default:
			AVcap_warning ("v4l2_old: unsupported pixel format %d",
						   data->pixelformat);
			break;
	}

	/* Initiate the next capture */
	if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf))
		AVcap_error ("v4l2_old: QBUF3 returned error");

	return TRUE;
}
