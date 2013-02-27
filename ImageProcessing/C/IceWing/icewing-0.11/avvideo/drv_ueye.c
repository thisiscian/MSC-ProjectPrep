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

#ifdef WITH_UEYE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ueye.h>
/* Enhance possibility that it works with future driver versions */
#include <ueye_deprecated.h>
#define HAVE_BOOL

#include "AVColor.h"
#include "AVOptions.h"
#include "drv_ueye.h"

#define MAX_REQUESTS	4
#define Udebug(x)		if (data->debug) fprintf x

#define ERROR(res, x) if (res != IS_SUCCESS) {						\
		INT err;													\
		char *msg;													\
		AVcap_warning x;											\
		if (is_GetError(data->camera, &err, &msg) == IS_SUCCESS && err != IS_SUCCESS) \
			fprintf (stderr, "        %d: %s\n", err, msg);			\
		return FALSE;												\
	}

typedef struct ueyeData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */

	HIDS camera;
	INT colMode;
	int debug;
} ueyeData;

static int get_planecnt (int colormode)
{
	switch (colormode) {
		default:
		case IS_CM_MONO8:
		case IS_CM_BAYER_RG8:
		case IS_CM_MONO12:
		case IS_CM_MONO16:
		case IS_CM_BAYER_RG12:
		case IS_CM_BAYER_RG16:
			return 1;
		case IS_CM_BGR555_PACKED:
		case IS_CM_BGR565_PACKED:
		case IS_CM_UYVY_PACKED:
		case IS_CM_UYVY_MONO_PACKED:
		case IS_CM_UYVY_BAYER_PACKED:
		case IS_CM_CBYCRY_PACKED:
		case IS_CM_RGB8_PACKED:
		case IS_CM_BGR8_PACKED:
		case IS_CM_RGBA8_PACKED:
		case IS_CM_BGRA8_PACKED:
		case IS_CM_RGBY8_PACKED:
		case IS_CM_BGRY8_PACKED:
		case IS_CM_RGB10V2_PACKED:
		case IS_CM_BGR10V2_PACKED:
			return 3;
	}
}

static int get_bpp (int colormode)
{
	switch (colormode) {
		default:
		case IS_CM_MONO8:
		case IS_CM_BAYER_RG8:
			return 8;
		case IS_CM_MONO12:
		case IS_CM_MONO16:
		case IS_CM_BAYER_RG12:
		case IS_CM_BAYER_RG16:
		case IS_CM_BGR555_PACKED:
		case IS_CM_BGR565_PACKED:
		case IS_CM_UYVY_PACKED:
		case IS_CM_UYVY_MONO_PACKED:
		case IS_CM_UYVY_BAYER_PACKED:
		case IS_CM_CBYCRY_PACKED:
			return 16;
		case IS_CM_RGB8_PACKED:
		case IS_CM_BGR8_PACKED:
			return 24;
		case IS_CM_RGBA8_PACKED:
		case IS_CM_BGRA8_PACKED:
		case IS_CM_RGBY8_PACKED:
		case IS_CM_BGRY8_PACKED:
		case IS_CM_RGB10V2_PACKED:
		case IS_CM_BGR10V2_PACKED:
			return 32;
	}
}

int AVueye_open (VideoDevData *dev, char *devOptions,
				 int subSample, VideoStandard vidSignal)
{
#define ARG_LEN			301
	INT res;
	INT deviceCount = 0;
	int cameraId = 0, deviceId = 0, i;
	char configName[ARG_LEN];
	char trigger[ARG_LEN];
	ueyeData *data;

	/* Grabbing method supported ? */
	if (vidSignal != AV_UEYE) return FALSE;

	data = calloc (1, sizeof(ueyeData));
	dev->capData = (AVcapData*)data;

	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_RGGB;
	data->camera = 0;
	memset (configName, '\0', ARG_LEN);
	memset (trigger, '\0', ARG_LEN);

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "IDS uEye driver V0.0 by Frank Loemker\n"
					 "usage: camera=val:device=val:config=fname:trigger=hilo|lohi|software:\n"
					 "       bayer=method:pattern=RGGB|BGGR|GRBG|GBRG:stereo=raw|deinter:debug\n"
					 "camera   access a camera by it's camera id [0...], default: %d\n"
					 "device   access a camera by it's device id [1...], default: %d\n"
					 "config   load camera settings from fname\n"
					 "trigger  use the specified trigger mode for image capture\n"
					 AVCAP_IP_HELP
					 "debug    output debugging/settings information, default: off\n\n",
					 cameraId, deviceId);
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		if (!AVcap_parse_ip ("ueye", devOptions, &data->ip))
			return FALSE;
		AVcap_optstr_get (devOptions, "trigger", "%300c", trigger);
		AVcap_optstr_get (devOptions, "config", "%300c", configName);
		AVcap_optstr_get (devOptions, "device", "%d", &deviceId);
		AVcap_optstr_get (devOptions, "camera", "%d", &cameraId);
	}

	res = is_GetNumberOfCameras (&deviceCount);
	ERROR (res, ("ueye: is_GetNumberOfCameras failed (code: %d)", res));
	if (deviceCount <= 0) {
		AVcap_warning ("ueye: No cameras available");
		return FALSE;
	}

	if (data->debug) {
		UEYE_CAMERA_LIST *list = malloc (sizeof(DWORD) + deviceCount*sizeof(UEYE_CAMERA_INFO));
		list->dwCount = deviceCount;
		fprintf (stderr, "ueye: Available devices:\n");
		if (is_GetCameraList(list) == IS_SUCCESS) {
			for (i = 0; i < list->dwCount; i++)
				fprintf (stderr, "\t%d: CameraId %d Serial %s Model %s Used %d\n",
						 list->uci[i].dwDeviceID, list->uci[i].dwCameraID,
						 list->uci[i].SerNo, list->uci[i].Model, list->uci[i].dwInUse);
		}
		free (list);
	}

	/* Initialise the camera. */
	if (deviceId)
		data->camera = deviceId | IS_USE_DEVICE_ID;
	else
		data->camera = cameraId;
	Udebug ((stderr, "ueye: %d devices available, using %s %ld\n",
			 deviceCount, deviceId ? "device":"camera", data->camera & ~IS_USE_DEVICE_ID));
	res = is_InitCamera (&data->camera, NULL);
	ERROR (res, ("ueye: is_InitCamera failed (code: %d)", res));

	if (configName[0]) {
		res = is_LoadParameters (data->camera, configName);
		ERROR (res, ("is_LoadParameters failed (code: %d)\n", res));
	}

	/* Get the destination image properties and allocate image memory */
	{
		INT bpp, colMode;
		IS_SIZE_2D size;

		is_AOI (data->camera, IS_AOI_IMAGE_GET_SIZE, &size, sizeof(size));
		colMode = is_SetColorMode (data->camera, IS_GET_COLOR_MODE);
		bpp = get_bpp (colMode);
		for (i=0; i<MAX_REQUESTS; i++) {
			char *adr;
			INT id;
			res = is_AllocImageMem (data->camera, size.s32Width, size.s32Height, bpp, &adr, &id);
			ERROR (res, ("ueye: is_AllocImageMem failed (code: %d)", res));
			res = is_AddToSequence (data->camera, adr, id);
			ERROR (res, ("ueye: is_AddToSequence failed (code: %d)", res));
		}
		res = is_InitImageQueue (data->camera, 0);
		ERROR (res, ("ueye: is_InitImageQueue failed (code: %d)", res));

		if (data->ip.stereo != AV_STEREO_NONE && bpp != 16) {
			AVcap_warning ("ueye: Stereo decoding only supported for 16BPP modes\n"
						   "          -> deactivating stereo decoding");
			data->ip.stereo = AV_STEREO_NONE;
		}
		if (data->ip.stereo == AV_STEREO_NONE && data->ip.bayer != IW_IMG_BAYER_NONE
			&& (bpp > 16
				|| colMode == IS_CM_BGR555_PACKED || colMode == IS_CM_BGR565_PACKED
				|| colMode == IS_CM_UYVY_PACKED || colMode == IS_CM_CBYCRY_PACKED)) {
			AVcap_warning ("ueye: Bayer decoding only supported for 8/16 Bit mono modes\n"
						   "          -> deactivating bayer decoding");
			data->ip.bayer = IW_IMG_BAYER_NONE;
		}

		data->colMode = colMode;
		dev->f_width = dev->width = size.s32Width;
		dev->f_height = dev->height = size.s32Height;
		if (data->ip.bayer != IW_IMG_BAYER_NONE)
			dev->planes = 3;
		else if (data->ip.stereo != AV_STEREO_NONE)
			dev->planes = 1;
		else
			dev->planes = get_planecnt (colMode);

		if (data->ip.stereo != AV_STEREO_NONE) {
			dev->type = IW_8U;
		} else if (colMode == IS_CM_MONO12 || colMode == IS_CM_MONO16 ||
				   colMode == IS_CM_BAYER_RG12 || colMode == IS_CM_BAYER_RG16 ||
				   colMode == IS_CM_RGB10V2_PACKED || colMode == IS_CM_BGR10V2_PACKED) {
			dev->type = IW_16U;
		} else {
			dev->type = IW_8U;
		}

		if (!AVcap_init_ip ("ueye", dev, &data->ip, FALSE))
			return FALSE;

		Udebug ((stderr, "ueye: Using images %dx%d planes: %d camera: %d\n",
				 dev->width, dev->height, dev->planes, data->camera));
	}

	if (trigger[0]) {
		int mode = IS_SET_TRIGGER_OFF;
		if (!strncasecmp (trigger, "HILO", 4))
			mode = IS_SET_TRIGGER_HI_LO;
		else if (!strncasecmp (trigger, "LOHI", 4))
			mode = IS_SET_TRIGGER_LO_HI;
		else if (!strncasecmp (trigger, "SOFTWARE", 8))
			mode = IS_SET_TRIGGER_SOFTWARE;
		if (mode != IS_SET_TRIGGER_OFF) {
			res = is_SetExternalTrigger (data->camera, mode);
			ERROR (res, ("ueye: is_SetExternalTrigger failed (code: %d)", res));
			Udebug ((stderr, "ueye: Set trigger to %d\n", mode));
		}
	}

	res = is_CaptureVideo (data->camera, IS_WAIT);
	ERROR (res, ("ueye: is_CaptureVideo failed (code: %d)", res));

	return TRUE;
}

void AVueye_close (void *capData)
{
	ueyeData *data = capData;

	if (data->camera) {
		is_ExitCamera (data->camera);
		data->camera = 0;
	}
	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}

	free (data);
}

int AVueye_capture (void *capData, iwImage *img, struct timeval *time)
{
	ueyeData *data = capData;
	INT colMode = data->colMode;
	INT res;
	INT memID;
	char *buffer = NULL;
	UEYEIMAGEINFO info;
	guchar *s;
	int cnt = 0;

	do {
		res = is_WaitForNextImage (data->camera, 5000, &buffer, &memID);
		if (res == IS_TIMED_OUT) {
			ERROR (res, ("ueye: Timeout during is_WaitForNextImage()"));
		} else if (res != IS_SUCCESS) {
			AVcap_warning ("ueye: is_WaitForNextImage failed (code: %d)", res);
			if (cnt > MAX_REQUESTS+2)
				return FALSE;
		}
		cnt++;
	} while (res != IS_SUCCESS);

	if (is_GetImageInfo(data->camera, memID, &info, sizeof(info)) == IS_SUCCESS) {
		time->tv_sec = info.TimestampSystem.wDay*3600*24 +
			info.TimestampSystem.wHour*3600 +
			info.TimestampSystem.wMinute*60 +
			info.TimestampSystem.wSecond;
		time->tv_usec = info.TimestampSystem.wMilliseconds*1000;
	}

	s = (guchar*)buffer;
	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U, img->width*img->height/2);
		s = data->ip.buffer;
		colMode = IS_CM_MONO8;
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		colMode = IS_CM_MONO8;
	}

	switch (colMode) {
		case IS_CM_MONO8:
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				memcpy (img->data[0], s, img->width*img->height);
				img->ctab = IW_GRAY;
			} else {
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
				img->ctab = IW_RGB;
			}
			break;
		case IS_CM_MONO12:
		case IS_CM_MONO16:
			AVcap_mono16_ip (&data->ip, img, s, img->width*img->height, FALSE);
			break;
		case IS_CM_RGB8_PACKED:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case IS_CM_RGBY8_PACKED:
		case IS_CM_RGBA8_PACKED:
			av_color_rgb32 (img, s);
			img->ctab = IW_RGB;
			break;
		case IS_CM_RGB10V2_PACKED:
			av_color_rgb24_10 (img, s);
			img->ctab = IW_RGB;
			break;
		case IS_CM_BGR555_PACKED:
			av_color_bgr15 (img, s);
			img->ctab = IW_RGB;
			break;
		case IS_CM_BGR565_PACKED:
			av_color_bgr16 (img, s);
			img->ctab = IW_RGB;
			break;
		case IS_CM_BGR8_PACKED:
			av_color_rgb24 (img, s, 2, 1, 0);
			img->ctab = IW_RGB;
			break;
		case IS_CM_BGRY8_PACKED:
		case IS_CM_BGRA8_PACKED:
			av_color_bgr32 (img, s);
			img->ctab = IW_RGB;
			break;
		case IS_CM_BGR10V2_PACKED:
			img->ctab = IW_RGB;
			break;
		case IS_CM_UYVY_PACKED:
		case IS_CM_UYVY_MONO_PACKED:
		case IS_CM_UYVY_BAYER_PACKED:
			av_color_yuyv (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case IS_CM_CBYCRY_PACKED:
			av_color_yuyv (img, s, 1);
			img->ctab = IW_YUV;
			break;
		default:
			AVcap_warning ("ueye: pixelFormat %d not implemented for conversion",
						   data->colMode);
			break;
	}
	is_UnlockSeqBuf (data->camera, memID, buffer);

	return TRUE;
}

#endif /* WITH_UEYE */
