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

#include "videodev2.h"
#ifdef WITH_LIBV4L
#include <libv4lconvert.h>
#endif

#ifdef WITH_JPEG
#include "gui/avi/jpegutils.h"
#endif
#include "AVOptions.h"
#include "AVColor.h"
#include "tools/tools.h"
#include "drv_v4l2.h"

#ifndef V4L2_CTRL_CLASS_CAMERA
#define V4L2_CTRL_CLASS_CAMERA		0x009a0000
#define V4L2_CID_CAMERA_CLASS_BASE	(V4L2_CTRL_CLASS_CAMERA | 0x900)
#endif

#define DEFAULT_VIDEO_DEVICE_L	"/dev/video"
#define DEFAULT_VIDEO_DEVICE	"/dev/video0"
#define Vdebug(x)				if (data->debug) fprintf x
#define EVEN_PROD(w,h)			((((w)+1)&(~1)) * (((h)+1)&(~1)))

typedef struct v4l2Image {
	struct v4l2_buffer vidbuf;
	char *data;
} v4l2Image;

typedef struct v4l2Data {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */
	int vid_fd;				/* Video device */
	int image_cnt;			/* Number of v4l2Image images */
	v4l2Image *image;		/* Buffers for images */
	char *options;			/* Device options for setting them after the first capture */
	int width, height;		/* Size of grabbed images */
	struct v4l2_format fmt;	/* Grabbing stream data format */
	BOOL select;			/* Use select() to check for available images? */
	struct v4lconvert_data *convData;
	BOOL convYUV;			/* Is YUV or RGB the destination format for v4lconvert()? */
	guchar *convBuffer;		/* Intermediate buffer for v4lconvert() */
	BOOL debug;
} v4l2Data;

#ifdef COMMENT
static void grabSetFps (int fd, int fps)
{
	struct v4l2_streamparm params;

	/* printf ("called v4l2_set_fps with fps=%d\n", fps); */
	params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl (fd, VIDIOC_G_PARM, &params);
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe); */
	params.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
	params.parm.capture.timeperframe.numerator = 10000000;
	params.parm.capture.timeperframe.denominator = fps;
	if (fps == 30) {
		params.parm.capture.timeperframe.numerator = 10010010;
		params.parm.capture.timeperframe.denominator = 30;
	}
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe); */
	ioctl (fd, VIDIOC_S_PARM, &params);

	params.parm.capture.timeperframe.numerator = 0;
	params.parm.capture.timeperframe.denominator = 0;
	ioctl (fd, VIDIOC_G_PARM, &params);
	/* printf ("time per frame is: %ld\n", params.parm.capture.timeperframe); */
}
#endif /* COMMENT */

/*********************************************************************
  Get all available properties and their values from device data->vid_fd.
*********************************************************************/
static void properties_get (v4l2Data *data)
{
	AVcapData *capData = (AVcapData*)data;
	v4l2_std_id stdId;
	struct v4l2_standard std;
	struct v4l2_queryctrl qctrl;
	struct v4l2_control ctrl;
	int id, base;

	/* Get supported video standards */
	AVCap_prop_add_enum (capData, "Video standard", 0, 0, 0);
	if (ioctl (data->vid_fd, VIDIOC_G_STD, &stdId) < 0)
		stdId = 0;
	memset (&std, 0, sizeof (std));
	for (id = 0; id < 999; id++) {
		std.index = id;
		if (ioctl (data->vid_fd, VIDIOC_ENUMSTD, &std) < 0)
			break;
		AVCap_prop_append_enum (capData, (char*)std.name, std.id == stdId, std.id);
	}

	/* Collect available controls */
	base = V4L2_CID_BASE;
	for (id = 0; id < 999; id++) {
		qctrl.id = base + id;
		if (ioctl (data->vid_fd, VIDIOC_QUERYCTRL, &qctrl) < 0) {
			/* Scan at least 64 controls */
			if (id > 64) {
				if (base == V4L2_CID_BASE)
					base = V4L2_CID_PRIVATE_BASE;
				else if (base == V4L2_CID_PRIVATE_BASE)
					base = V4L2_CID_CAMERA_CLASS_BASE;
				else
					break;
				id = -1;
				continue;
			}
		} else if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl.id = qctrl.id;
			if (ioctl (data->vid_fd, VIDIOC_G_CTRL, &ctrl) < 0)
				ctrl.value = qctrl.minimum-1;
			if (qctrl.type == V4L2_CTRL_TYPE_BOOLEAN) {
				AVCap_prop_add_bool (capData, (char*)qctrl.name,
									 ctrl.value, qctrl.default_value, ctrl.id);
			} else if (qctrl.type == V4L2_CTRL_TYPE_BUTTON) {
				AVCap_prop_add_command (capData, (char*)qctrl.name, ctrl.id);
			} else if (qctrl.type == V4L2_CTRL_TYPE_MENU) {
				struct v4l2_querymenu menu;

				AVCap_prop_add_enum (capData, (char*)qctrl.name,
									 ctrl.value, qctrl.default_value, ctrl.id);
				memset (&menu, 0, sizeof(menu));
				menu.id = qctrl.id;
				for (menu.index = qctrl.minimum; menu.index <= qctrl.maximum; menu.index++) {
					if (ioctl (data->vid_fd, VIDIOC_QUERYMENU, &menu) >= 0)
						AVCap_prop_append_enum (capData, (char*)menu.name,
												menu.index == ctrl.value, menu.index);
				}
			} else {
				AVCap_prop_add_double (capData, (char*)qctrl.name,
									   ((float)ctrl.value - qctrl.minimum)/(qctrl.maximum - qctrl.minimum),
									   0, 1,
									   ((float)qctrl.default_value - qctrl.minimum)/(qctrl.maximum - qctrl.minimum),
									   ctrl.id);
			}
		}
	}
}

/*********************************************************************
  Get values of all available properties of device data->handle.
*********************************************************************/
static void properties_get_values (v4l2Data *data)
{
	AVcapData *capData = (AVcapData*)data;
	iwGrabProperty *prop = capData->prop;
	v4l2_std_id stdId;
	int i;

	/* Get supported video standards */
	if (ioctl (data->vid_fd, VIDIOC_G_STD, &stdId) < 0)
		stdId = 0;
	for (i = 0; prop->data.e.enums[i].name; i++) {
		if (prop->data.e.enums[i].priv == stdId)  {
			prop->data.e.value = i;
			break;
		}
	}

	/* Get values of all available controls */
	for (i=1; i < capData->prop_cnt; i++) {
		struct v4l2_control ctrl;
		struct v4l2_queryctrl qctrl;
		prop = &capData->prop[i];
		if (prop->type <= IW_GRAB_COMMAND)
			continue;
		qctrl.id = prop->priv;
		ctrl.id = prop->priv;
		if (   ioctl (data->vid_fd, VIDIOC_QUERYCTRL, &qctrl) >= 0
			&& ioctl (data->vid_fd, VIDIOC_G_CTRL, &ctrl) >= 0) {

			if (prop->type == IW_GRAB_BOOL) {
				prop->data.b.value = ctrl.value;
			} else if (prop->type == IW_GRAB_ENUM) {
				prop->data.e.value = ctrl.value;
			} else {
				prop->data.d.value = ((float)ctrl.value - qctrl.minimum)/(qctrl.maximum - qctrl.minimum);
			}
		} else
			AVcap_error ("v4l2: Failed to get property %d(%s)", i, prop->name);
	}
}

typedef enum {
	SET_OPEN,
	SET_CAPTURE,
	SET_SETPROP
} PropSetMode;

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property from data->vid_fd. Return IW_GRAB_STATUS_OK on success.
*********************************************************************/
static iwGrabStatus properties_set (v4l2Data *data, char *devOptions, PropSetMode mode)
{
	iwGrabStatus status = IW_GRAB_STATUS_OK;
	AVcapData *capData = (AVcapData*)data;
	char propArg[100], propVal[200];
	char *optPos;

	optPos = devOptions;
	while ((optPos = AVcap_optstr_lookup_prefix(optPos, "prop", propArg, sizeof(propArg),
												propVal, sizeof(propVal)))) {
		BOOL err = FALSE;
		iwGrabProperty *prop;
		int propIdx = AVCap_prop_lookup (capData, propArg);
		if (propIdx < 0) {
			AVcap_warning ("v4l2: No property '%s' available", propArg);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		} else if (propIdx == 0 && mode == SET_CAPTURE)
			continue;

		prop = &capData->prop[propIdx];
		if (!AVCap_prop_parse (prop, prop, propVal)) {
			AVcap_warning ("v4l2: Failed to parse value for property %d(%s) from '%s'",
						   propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}

		if (propIdx == 0) {
			/* Set video standard */
			v4l2_std_id id = prop->data.e.enums[prop->data.e.value].priv;
			if (ioctl (data->vid_fd, VIDIOC_S_STD, &id) < 0)
				err = TRUE;
		} else {
			/* Set controls */
			struct v4l2_control ctrl;
			struct v4l2_queryctrl qctrl;

			if (mode == SET_OPEN && !data->options)
				data->options = strdup (devOptions);

			qctrl.id = prop->priv;
			if (ioctl (data->vid_fd, VIDIOC_QUERYCTRL, &qctrl) >= 0) {
				if (prop->type == IW_GRAB_BOOL)
					ctrl.value = prop->data.b.value;
				else if (prop->type == IW_GRAB_ENUM)
					ctrl.value = prop->data.e.enums[prop->data.e.value].priv;
				else if (prop->type == IW_GRAB_DOUBLE)
					ctrl.value = prop->data.d.value * (qctrl.maximum - qctrl.minimum) + qctrl.minimum;
				ctrl.id = prop->priv;
				if (ioctl (data->vid_fd, VIDIOC_S_CTRL, &ctrl) < 0)
					err = TRUE;
			} else
				err = TRUE;
		}
		if (err) {
			status = IW_GRAB_STATUS_ERR;
			AVcap_error ("v4l2: Failed to set property %d(%s) to '%s'",
						 propIdx, prop->name, propVal);
			if (mode == SET_OPEN && propIdx == 0)
				return status;
		} else {
			status = IW_GRAB_STATUS_OK;
			Vdebug ((stderr, "v4l2: Set property %d(%s) to value '%s'\n",
					 propIdx, prop->name, propVal));
		}
	}

	if (mode == SET_CAPTURE && data->options) {
		free (data->options);
		data->options = NULL;
	}
	if (mode == SET_OPEN)
		return IW_GRAB_STATUS_OK;
	return status;
}

int AVv4l2_open (VideoDevData *dev, char *devOptions,
				 int subSample, VideoStandard vidSignal)
{
	/* Default if bayer decoding and no stereo decoding */
#define FORMATIDX_MONO		17 /* V4L2_PIX_FMT_GREY */
	static struct {
		int pixel;
		int bpp;
	} formats[] = {
		{V4L2_PIX_FMT_YUYV,    16},
		{V4L2_PIX_FMT_UYVY,    16},
		{V4L2_PIX_FMT_YUV422P, 16},
		{V4L2_PIX_FMT_YUV420,  12},
		{V4L2_PIX_FMT_YVU420,  12},
		{V4L2_PIX_FMT_YUV411P, 12},
		{V4L2_PIX_FMT_Y41P,    12},
		{V4L2_PIX_FMT_YUV410,   9},
		{V4L2_PIX_FMT_YVU410,   9},
		{V4L2_PIX_FMT_RGB24,   24},
		{V4L2_PIX_FMT_BGR24,   24},
		{V4L2_PIX_FMT_RGB32,   32},
		{V4L2_PIX_FMT_BGR32,   32},
		{V4L2_PIX_FMT_RGB565,  16},
		{V4L2_PIX_FMT_RGB565X, 16},
		{V4L2_PIX_FMT_RGB555,  16},
		{V4L2_PIX_FMT_RGB555X, 16},
#ifdef WITH_JPEG
		{V4L2_PIX_FMT_MJPEG,   -1},
#endif
		{V4L2_PIX_FMT_GREY,     8},
		{V4L2_PIX_FMT_HI240,    8},
		{0, 0}
	};
	struct v4l2_requestbuffers req;
	struct v4l2_control ctrl;
	struct v4l2_input inp;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_pix_format *pixfmt;
	struct v4l2_capability cap;
	struct stat statbuf;
	char device[256];
	int err;
	int maxwidth = AV_DEFAULT_WIDTH;
	int maxheight = AV_DEFAULT_HEIGHT;
	char grayImage = '\0';
	int format_num = -1, format_pix = -1;
	int bpp;
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
	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_RGGB;
	data->image_cnt = 4;
	data->select = TRUE;

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "v4l2 driver V0.3 by Frank Loemker, Jannik Fritsch\n"
					 "usage: device=name:input=num:size=<width>x<height>:format=num:propX=val:\n"
					 "       propY=val:buffer=cnt:bayer=method:pattern=RGGB|BGGR|GRBG|GBRG:\n"
					 "       stereo=raw|deinter:gray=w|h:noselect:debug\n"
					 "device   device name, default: %s\n"
					 "input    video input [0...], default: %s\n"
					 "size     max. size of grabbed image with no downsampling, default: %dx%d\n"
					 "format   format number [0...], default: YUV format with most BPP\n"
					 "propX    set property X ([0...] or it's name) to val. If \"=val\" is not given\n"
					 "         use the properties default value.\n"
					 "buffer   number of buffers used by the device driver, default: %d\n"
					 AVCAP_IP_HELP
					 "gray     interpret image as a gray image and increase width/height\n"
					 "         accordingly to the image size in bytes\n"
					 "noselect do not use select() to access the grabber, default: off\n"
					 "debug    output debugging/settings information, default: off\n\n",
					 device,
					 vidSignal == PAL_COMPOSITE ? "first composite input" :
					 (vidSignal == PAL_S_VIDEO ? "first s-video input" : "0"),
					 maxwidth, maxheight,
					 data->image_cnt);
			return FALSE;
		}
		AVcap_optstr_get (devOptions, "device", "%s", device);
		AVcap_optstr_get (devOptions, "size", "%dx%d",
						  &maxwidth, &maxheight);
		AVcap_optstr_get (devOptions, "format", "%d", &format_num);
		AVcap_optstr_get (devOptions, "buffer", "%d", &data->image_cnt);
		if (!AVcap_parse_ip ("v4l2", devOptions, &data->ip))
			return FALSE;
		if (AVcap_optstr_get (devOptions, "gray", "%c", &grayImage) >= 0) {
			grayImage |= 'a'-'A';
			if (grayImage != 'w' && grayImage != 'h') {
				AVcap_warning ("v4l2: Expected w|h for gray option");
				return FALSE;
			}
		}
		if (AVcap_optstr_lookup (devOptions, "noselect"))
			data->select = FALSE;
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		/* If video input is specified no guessing about the input */
		if (AVcap_optstr_get (devOptions, "input", "%d", &input_num) == 1)
			vidSignal = AV_V4L2;
	}

	/* Open the video device */
	data->vid_fd = open (device, O_RDWR);
	if (data->vid_fd == -1) {
		AVcap_error ("v4l2: Could not open video device [%s]", device);
		return FALSE;
	}

	if (data->debug && ioctl (data->vid_fd, VIDIOC_QUERYCAP, &cap) >= 0)
		fprintf (stderr, "v4l2: Using card '%s' with driver '%s', bus '%s'\n",
				 cap.card, cap.driver, cap.bus_info);

	/* Set given properties and show available ones */
	properties_get (data);
	if (properties_set (data, devOptions, SET_OPEN) != IW_GRAB_STATUS_OK)
		return FALSE;
	properties_get_values (data);
	if (data->debug)
		AVCap_prop_print ("v4l2", dev->capData);

	/* Check available pixel formats */
	Vdebug ((stderr, "v4l2: Available formats:\n"));
	best = format_best = 9999;
	memset (&fmtdesc, 0, sizeof(fmtdesc));
	for (i=0; ; i++) {
		fmtdesc.index = i;
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(data->vid_fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
			if (i == 0) {
				AVcap_warning ("v4l2: No formats available");
				return FALSE;
			}
			break;
		}

		Vdebug ((stderr, "\t%2d: %s\n", i, fmtdesc.description));
		if (i == format_num || i == 0)
			format_pix = fmtdesc.pixelformat;
		if (format_num < 0) {
			if (data->ip.bayer != IW_IMG_BAYER_NONE && data->ip.stereo == AV_STEREO_NONE)
				j = FORMATIDX_MONO;
			else
				j = 0;
			for (; j<best && formats[j].pixel; j++)
				if (formats[j].pixel == fmtdesc.pixelformat) {
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
	data->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (data->vid_fd, VIDIOC_G_FMT, &data->fmt) < 0) {
		AVcap_error ("v4l2: G_FMT returned error");
		return FALSE;
	}

	pixfmt = &data->fmt.fmt.pix;
	pixfmt->width = maxwidth;
	pixfmt->height = maxheight;
	pixfmt->pixelformat = format_pix;
	pixfmt->field = V4L2_FIELD_ANY;
	/* Get possible full width/height */
	if (ioctl (data->vid_fd, VIDIOC_TRY_FMT, &data->fmt) < 0) {
		if (ioctl (data->vid_fd, VIDIOC_S_FMT, &data->fmt) < 0) {
			/* Ignore error */
		}
	}
	dev->f_width = pixfmt->width;
	dev->f_height = pixfmt->height;

	if (subSample > 0) {
		pixfmt->width /= subSample;
		pixfmt->height /= subSample;
	}
	/* HACK: Prevent grabbing errors by rounding to values divisible by 16 */
	pixfmt->width -= pixfmt->width % 16;
	pixfmt->height -= pixfmt->height % 16;
	if (dev->field == FIELD_EVEN_ONLY)
		pixfmt->field = V4L2_FIELD_TOP;
	else if (dev->field == FIELD_ODD_ONLY)
		pixfmt->field = V4L2_FIELD_BOTTOM;
	else
		pixfmt->field = V4L2_FIELD_ANY;
	if (ioctl (data->vid_fd, VIDIOC_S_FMT, &data->fmt) < 0) {
		AVcap_error ("v4l2: S_FMT returned error");
		return FALSE;
	}

	/* Get the real capture format */
	if (ioctl (data->vid_fd, VIDIOC_G_FMT, &data->fmt) < 0) {
		AVcap_error ("v4l2: regetting G_FMT returned error");
		return FALSE;
	}
	i = pixfmt->pixelformat;
	Vdebug ((stderr, "v4l2: Using format %d: %c%c%c%c  Size: %dx%d\n",
			 format_num, (char)(i), (char)(i>>8), (char)(i>>16), (char)(i>>24),
			 pixfmt->width, pixfmt->height));

	/* Set the AGC OFF */
	ctrl.id = V4L2_CID_PRIVATE_BASE + 4; /* V4L2_CID_PRIVATE_AGC_CRUSH */
	ctrl.value = 0;
	if (ioctl (data->vid_fd, VIDIOC_S_CTRL, &ctrl) < 0)
		AVcap_warning ("v4l2: Unable to switch of AGC");

	/* Check available video inputs */
	if (data->debug) {
		fprintf (stderr, "v4l2: Available video inputs:\n");
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
				AVcap_error ("v4l2: Error getting %s video input",
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
		Vdebug ((stderr, "v4l2: Using input %d: %s\n", inp.index, inp.name));
	} else
		Vdebug ((stderr, "v4l2: Using input %d\n", input_num));
	if (ioctl (data->vid_fd, VIDIOC_S_INPUT, &input_num) < 0) {
		AVcap_error ("v4l2: Error setting video input");
		return FALSE;
	}

	/* grabSetFps (data->vid_fd, 30); */

	/* Request mmap-able capture buffers */
	memset (&req, 0, sizeof(req));
	req.count = data->image_cnt;
	req.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	err = ioctl (data->vid_fd, VIDIOC_REQBUFS, &req);
	if (err < 0 || req.count < 1) {
		AVcap_error ("v4l2: REQBUFS returned error, buffer count=%d", req.count);
		return FALSE;
	}
	data->image_cnt = req.count;

	data->image = calloc (1, sizeof(v4l2Image)*data->image_cnt);
	for (i=0; i<data->image_cnt; i++) {
		data->image[i].vidbuf.index = i;
		data->image[i].vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		data->image[i].vidbuf.memory = V4L2_MEMORY_MMAP;
		err = ioctl (data->vid_fd, VIDIOC_QUERYBUF, &data->image[i].vidbuf);
		if (err < 0) {
			AVcap_error ("v4l2: QUERYBUF returned error");
			return FALSE;
		}

		data->image[i].data = (char*)mmap (0, data->image[i].vidbuf.length,
										   PROT_READ|PROT_WRITE,
										   MAP_SHARED, data->vid_fd,
										   data->image[i].vidbuf.m.offset);
		if (data->image[i].data == (void*)-1) {
			AVcap_error ("v4l2: mmap() returned error");
			return FALSE;
		}
	}

	for (i=0; i<data->image_cnt; i++) {
		if (ioctl (data->vid_fd, VIDIOC_QBUF, &data->image[i].vidbuf) < 0) {
			AVcap_error ("v4l2: QBUF returned error");
			return FALSE;
		}
	}

	/* Turn on streaming capture */
	if (ioctl (data->vid_fd, VIDIOC_STREAMON, &data->image[0].vidbuf.type) < 0) {
		AVcap_error ("v4l2: STREAMON returned error");
		return FALSE;
	}

	bpp = 8;
	for (i=0; formats[i].pixel; i++) {
		if (formats[i].pixel == pixfmt->pixelformat) {
			bpp = formats[i].bpp;
			break;
		}
	}
	if (grayImage) {
		pixfmt->pixelformat = V4L2_PIX_FMT_GREY;
		if (grayImage == 'w')
			pixfmt->width = pixfmt->width*bpp/8;
		else
			pixfmt->height = pixfmt->height*bpp/8;
		bpp = 8;
	}

	if (data->ip.stereo != AV_STEREO_NONE && bpp != 16) {
		AVcap_warning ("v4l2: Stereo decoding only supported for 2-byte modes\n"
					   "      -> deactivating stereo decoding");
		data->ip.stereo = AV_STEREO_NONE;
	}
	if (data->ip.stereo == AV_STEREO_NONE
		&& data->ip.bayer != IW_IMG_BAYER_NONE && pixfmt->pixelformat != V4L2_PIX_FMT_GREY) {
		AVcap_warning ("v4l2: Bayer decoding only supported for 8/16 Bit mono modes\n"
					   "      -> deactivating bayer decoding");
		data->ip.bayer = IW_IMG_BAYER_NONE;
	}

	dev->width = data->width = pixfmt->width;
	dev->height = data->height = pixfmt->height;
	if (data->ip.bayer == IW_IMG_BAYER_NONE &&
		(pixfmt->pixelformat == V4L2_PIX_FMT_GREY ||
		 pixfmt->pixelformat == V4L2_PIX_FMT_HI240 ||
		 data->ip.stereo != AV_STEREO_NONE))
		dev->planes = 1;
	else
		dev->planes = 3;
	dev->type = IW_8U;

	if (!AVcap_init_ip ("v4l2", dev, &data->ip, FALSE))
		return FALSE;

#ifdef WITH_LIBV4L
	data->convData = v4lconvert_create (data->vid_fd);
#endif

	return TRUE;
}

void AVv4l2_close (void *capData)
{
	v4l2Data *data = capData;
	int i;

	if (data->vid_fd >= 0) {
		i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ioctl (data->vid_fd, VIDIOC_STREAMOFF, &i);

		if (data->image) {
			for (i=0; i<data->image_cnt; i++) {
				if (data->image[i].data)
					munmap (data->image[i].data, data->image[i].vidbuf.length);
			}
			free (data->image);
		}
		if (data->convBuffer) free (data->convBuffer);
#ifdef WITH_LIBV4L
		if (data->convData) v4lconvert_destroy (data->convData);
#endif
		close (data->vid_fd);
	}
	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}
	if (data->options) free (data->options);
	free (data);
}

iwGrabStatus AVv4l2_set_property (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_OK;
	v4l2Data *data = (v4l2Data*)dev->capData;

	if (!data->vid_fd) return IW_GRAB_STATUS_NOTOPEN;

	switch (ctrl) {
		case IW_GRAB_GET_PROP:
		case IW_GRAB_GET_PROPVALUES: {
			iwGrabProperty **prop = va_arg (*args, iwGrabProperty**);
			if (prop) {
				*prop = dev->capData->prop;
				if (ctrl == IW_GRAB_GET_PROPVALUES)
					properties_get_values (data);
			} else
				ret = IW_GRAB_STATUS_ARGUMENT;
			break;
		}
		case IW_GRAB_SET_PROP: {
			char *props = va_arg (*args, char*);
			ret = properties_set (data, props, SET_SETPROP);
			break;
		}
		default:
			ret = IW_GRAB_STATUS_UNSUPPORTED;
			break;
	}
	return ret;
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
		AVcap_error ("v4l2: select error");
	} else if (n == 0 && sec > 0) {
		fprintf (stderr, "select timeout\n");
	} else if (n != 0 && !FD_ISSET (data->vid_fd, &rdset)) {
		fprintf (stderr, "select() != %d, but fd not set\n", n);
		n = -1;
	}
	return n;
}

int AVv4l2_capture (void *capData, iwImage *img, struct timeval *time)
{
	v4l2Data *data = capData;
	struct v4l2_buffer tempbuf;
	int decompose, n, skipped = 0;
	unsigned char *s;

	if (data->select) {
		int tries = 0;
		/* Grab last frame */
		n = vid_select (data, 6);
		if (n == -1 || n == 0)
			return FALSE;
		do {
			tempbuf.type = data->image[0].vidbuf.type;
			tempbuf.memory = V4L2_MEMORY_MMAP;
			if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: DQBUF1 returned error");

			/* Check if more data (== more frames) is available */
			n = vid_select (data, 0);
			if (n == 0)
				break;
			else if (n > 0)
				skipped++;
			if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: QBUF1 returned error");

			tries++;
			if (tries > data->image_cnt)
				return FALSE;
		} while (TRUE);
	} else {
		/* Grab a recent frame */
		struct timeval ctime;

		gettimeofday (&ctime, NULL);
		do {
			tempbuf.type = data->image[0].vidbuf.type;
			tempbuf.memory = V4L2_MEMORY_MMAP;
			if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: DQBUF1 returned error");
			*time = tempbuf.timestamp;

			if (ABS(IW_TIME_DIFF(ctime, *time)) < 400 || skipped >= data->image_cnt)
				break;
			skipped++;

			if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: QBUF1 returned error");
		} while (TRUE);
	}

	/* If frames were skipped, grab a new current image */
	if (skipped) {
		Vdebug ((stderr, "v4l2: Skipped %d buffers...\n", skipped));
		if (data->select && skipped == data->image_cnt-1) {
			/* Put last (very old) buffer back in Queue */
			if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: QBUF2 returned error");

			n = vid_select (data, 1);
			if (n == -1 || n == 0)
				return FALSE;

			/* ... and dequeue the next */
			tempbuf.type = data->image[0].vidbuf.type;
			tempbuf.memory = V4L2_MEMORY_MMAP;
			if (ioctl (data->vid_fd, VIDIOC_DQBUF, &tempbuf) < 0)
				AVcap_error ("v4l2: DQBUF2 returned error");
		}
	}
	if (tempbuf.index >= data->image_cnt)
		return FALSE;

	*time = tempbuf.timestamp;
	/* Captured frame data */
	s = (unsigned char *)data->image[tempbuf.index].data;
	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U,
							  data->width * data->height);
		s = data->ip.buffer;
		decompose = V4L2_PIX_FMT_GREY;
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		decompose = V4L2_PIX_FMT_GREY;
	} else
		decompose = data->fmt.fmt.pix.pixelformat;

	img->ctab = IW_YUV;
	switch (decompose) {
		case V4L2_PIX_FMT_GREY:
		case V4L2_PIX_FMT_HI240:
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				img->ctab = IW_GRAY;
				memcpy (img->data[0], s, img->width * img->height);
			} else {
				img->ctab = IW_RGB;
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
			}
			break;
		case V4L2_PIX_FMT_RGB555:
			av_color_bgr15 (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB555X:
			av_color_bgr15x (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB565:
			av_color_bgr16 (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB565X:
			av_color_bgr16x (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_BGR24:
			av_color_rgb24 (img, s, 2, 1, 0);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_BGR32:
			av_color_bgr32 (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB24:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_RGB32:
			av_color_rgb32 (img, s);
			img->ctab = IW_RGB;
			break;
		case V4L2_PIX_FMT_YUYV:
			av_color_yuyv (img, s, 0);
			break;
		case V4L2_PIX_FMT_UYVY:
			av_color_yuyv (img, s, 1);
			break;
		case V4L2_PIX_FMT_YUV422P:
			av_color_yuv422P (img, s);
			break;
		case V4L2_PIX_FMT_YUV420:
			av_color_yuv420 (img, s, 1);
			break;
		case V4L2_PIX_FMT_YVU420:
			av_color_yuv420 (img, s, 2);
			break;
		case V4L2_PIX_FMT_YUV411P:
			av_color_yuv411P (img, s);
			break;
		case V4L2_PIX_FMT_Y41P:
			av_color_yuv41P (img, s);
			break;
		case V4L2_PIX_FMT_YUV410:
			av_color_yuv410 (img, s, 1);
			break;
		case V4L2_PIX_FMT_YVU410:
			av_color_yuv410 (img, s, 2);
			break;
#ifdef WITH_JPEG
		case V4L2_PIX_FMT_MJPEG: {
			int w = img->width, h = img->height;
			int cnt = EVEN_PROD(w,h);
			guchar *d = malloc(cnt*3/2);
			if (! decode_jpeg_raw (s, tempbuf.bytesused, LAV_NOT_INTERLACED,
								   0, w, h, d, d+w*h, d + w*h + cnt/4))
				av_color_yuv420 (img, d, 1);
			else
				AVcap_warning ("v4l2: Unable to decode jpeg frame");
			free (d);
			break;
		}
#endif
		default:
#ifdef WITH_LIBV4L
		{
			int dBytes = img->width*img->height*3/2;
			struct v4l2_format dest = data->fmt;

			if (!data->convBuffer) {
				int pix = data->fmt.fmt.pix.pixelformat;
				data->convYUV = (pix == V4L2_PIX_FMT_SPCA501 ||
								 pix == V4L2_PIX_FMT_SPCA505 ||
								 pix == V4L2_PIX_FMT_SPCA508 ||
								 pix == V4L2_PIX_FMT_CIT_YYVYUY ||
								 pix == V4L2_PIX_FMT_KONICA420 ||
								 pix == V4L2_PIX_FMT_SN9C20X_I420 ||
								 pix == V4L2_PIX_FMT_M420 ||
								 pix == V4L2_PIX_FMT_HM12 ||
								 pix == V4L2_PIX_FMT_CPIA1);
			}
			if (data->convYUV) {
				dest.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
			} else {
				dest.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
				dBytes *= 2;
			}
			dest.fmt.pix.bytesperline = 0;
			dest.fmt.pix.sizeimage = dBytes;
			if (!data->convBuffer)
				data->convBuffer = malloc (dBytes);

			if (v4lconvert_convert(data->convData, &data->fmt, &dest,
								   s, tempbuf.bytesused, data->convBuffer, dBytes) >= 0) {
				if (dest.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
					av_color_yuv420 (img, data->convBuffer, 1);
					img->ctab = IW_YUV;
				} else {
					av_color_rgb24 (img, data->convBuffer, 0, 1, 2);
					img->ctab = IW_RGB;
				}
			} else
				AVcap_warning ("v4l2: Unable to convert pixel format %#x\n"
							   "\t%s",
							   data->fmt.fmt.pix.pixelformat,
							   v4lconvert_get_error_message(data->convData));
		}
#else
			AVcap_warning ("v4l2: Unsupported pixel format %#x",
						   data->fmt.fmt.pix.pixelformat);
#endif
			break;
	}

	/* Initiate the next capture */
	if (ioctl (data->vid_fd, VIDIOC_QBUF, &tempbuf) < 0)
		AVcap_error ("v4l2: QBUF3 returned error");

	properties_set (data, data->options, SET_CAPTURE);

	return TRUE;
}
