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

#ifdef WITH_MVIMPACT

#include <stdio.h>
#include <string.h>
#include <mvDeviceManager/Include/mvDeviceManager.h>

#include "AVColor.h"
#include "AVOptions.h"
#include "drv_mvimpact.h"

#define MAX_REQUESTS	4
#define Mdebug(x)		if (data->debug) fprintf x

typedef struct mvimpData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */

	HDMR dmr;
	HDEV device;
	HDRV driver;
	ImageBuffer *buffer;

	int debug;
} mvimpData;

static BOOL image_capture (mvimpData *data, struct timeval *time, int *requestNum)
{
	RequestResult reqRes;
	TDMR_ERROR result;

	*requestNum = -1;
	if ((result = DMR_ImageRequestWaitFor (data->driver, 500, 0, requestNum))
		!= DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_ImageRequestWaitFor error (code %d)", result);
		return FALSE;
	}
	if (time) gettimeofday (time, NULL);

	/* Check if the request contains a valid image */
	result = DMR_GetImageRequestResultEx (data->driver, *requestNum,
										  &reqRes, sizeof(reqRes), 0, 0);
	if (result == DMR_NO_ERROR && reqRes.result == rrOK) {

		if ((result = DMR_GetImageRequestBuffer (data->driver, *requestNum, &data->buffer))
			== DMR_NO_ERROR) {
			return TRUE;
		} else {
			AVcap_warning ("mvimpact: DMR_GetImageRequestBuffer error (code %d)", result);
		}

	} else {
		AVcap_warning ("mvimpact: DMR_GetImageRequestResult error (code: %d, request result: %d)",
					   result, reqRes.result);
	}
	return FALSE;
}

int AVmvimp_open (VideoDevData *dev, char *devOptions,
				  int subSample, VideoStandard vidSignal)
{
#define ARG_LEN			301
	unsigned int deviceCount = 0;
	int deviceNum = 0, requestNum, i;
	char configname[ARG_LEN];
	mvimpData *data;
	TDMR_ERROR result;
	BOOL ok;

	/* Grabbing method supported ? */
	if (vidSignal != AV_MVIMPACT) return FALSE;

	data = calloc (1, sizeof(mvimpData));
	dev->capData = (AVcapData*)data;

	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_RGGB;

	data->dmr = INVALID_ID;
	data->driver = INVALID_ID;
	data->device = INVALID_ID;
	data->buffer = NULL;
	memset (configname, '\0', ARG_LEN);

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "MV impact acquire driver V0.0 by Frank Loemker\n"
					 "usage: device=val:config=fname:bayer=method:pattern=RGGB|BGGR|GRBG|GBRG:\n"
					 "       stereo=raw|deinter:debug\n"
					 "device   device number [0...], default: %d\n"
					 "config   load camera settings from fname\n"
					 AVCAP_IP_HELP
					 "debug    output debugging/settings information, default: off\n\n",
					 deviceNum);
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		if (!AVcap_parse_ip ("mvimpact", devOptions, &data->ip))
			return FALSE;
		AVcap_optstr_get (devOptions, "device", "%d", &deviceNum);
		AVcap_optstr_get (devOptions, "config", "%300c", configname);
	}

	/* Initialise the library. */
	if ((result = DMR_Init(&data->dmr)) != DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_Init failed (code: %d)", result);
		return FALSE;
	}
	if ((result = DMR_GetDeviceCount(&deviceCount)) != DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_GetDeviceCount failed (code: %d: %s)",
					   result, DMR_ErrorCodeToString (result));
		return FALSE;
	}
	if (deviceNum < 0 || deviceCount <= 0 || deviceNum >= deviceCount) {
		AVcap_warning ("mvimpact: Device %d not available, only device(s) 0..%d available",
					   deviceNum, deviceCount-1);
		return FALSE;
	}
	Mdebug ((stderr, "mvimpact: %d devices available\n", deviceCount));

	/* Get access to the selected device */
	if ((result = DMR_GetDevice(&data->device, dmdsmSerial, "*", deviceNum, '*'))
		!= DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_GetDevice(%d) failed (code: %d: %s)",
					   deviceNum, result, DMR_ErrorCodeToString(result));
		return FALSE;
	}
	if (data->debug) {
		TDMR_DeviceInfo info;
		fprintf (stderr, "mvimpact: Available devices:\n");
		for (i=0; i<deviceCount; i++) {
			if ((result = DMR_GetDeviceInfo(i, &info, sizeof(info)))
				== DMR_NO_ERROR) {
				fprintf (stderr, "\t%d: %s (%s, family: %s)\n",
						 i, info.serial, info.product, info.family);
			} else
				fprintf (stderr, "\t%d\n", i);
		}
		fprintf (stderr, "mvimpact: Using device %d\n", deviceNum);
	}

	/* Initialise the selected device */
	if ((result = DMR_OpenDevice (data->device, &data->driver)) != DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_OpenDevice failed (code: %d)\n", result);
		return FALSE;
	}

	if (configname[0] &&
		(result = DMR_LoadSetting (data->driver, configname, sfFile, sUser)) != DMR_NO_ERROR) {
		AVcap_warning ("mvimpact: DMR_LoadSetting failed (code: %d)\n", result);
		return FALSE;
	}

	/* Prefill the default capture queue */
	for (i=0; i<MAX_REQUESTS; i++)
		DMR_ImageRequestSingle (data->driver, 0, 0);

	/* Grab one image to get the destination image properties */
	i = 0;
	do {
		ok = image_capture (data, NULL, &requestNum);
	} while (!ok && ++i < 10);
	if (ok) {
		ImageBuffer *ib = data->buffer;
		TImageBufferPixelFormat pf = ib->pixelFormat;

		if (data->ip.stereo != AV_STEREO_NONE &&
			pf !=ibpfYUV422_UYVYPacked && pf != ibpfYUV422Packed && pf != ibpfYUV422Planar && pf != ibpfMono16) {
			AVcap_warning ("mvimpact: Stereo decoding only supported for 16BPP modes (422YUV, mono16)\n"
						   "          -> deactivating stereo decoding");
			data->ip.stereo = AV_STEREO_NONE;
		}
		if (data->ip.stereo == AV_STEREO_NONE && data->ip.bayer != IW_IMG_BAYER_NONE
			&& (ib->iChannelCount > 1 || pf == ibpfMono32)) {
			AVcap_warning ("mvimpact: Bayer decoding only supported for 8/16 Bit mono modes\n"
						   "          -> deactivating bayer decoding");
			data->ip.bayer = IW_IMG_BAYER_NONE;
		}

		dev->f_width = dev->width = ib->iWidth;
		dev->f_height = dev->height = ib->iHeight;
		if (data->ip.bayer != IW_IMG_BAYER_NONE)
			dev->planes = 3;
		else if (data->ip.stereo != AV_STEREO_NONE)
			dev->planes = 1;
		else
			dev->planes = ib->iChannelCount;

		if (data->ip.stereo != AV_STEREO_NONE) {
			dev->type = IW_8U;
		} else if (pf == ibpfMono10 || pf == ibpfMono12 || pf == ibpfMono12Packed_V2 ||
			pf == ibpfMono14 || pf == ibpfMono16 ||
			pf == ibpfBGR101010Packed_V2 || pf == ibpfRGB101010Packed || pf == ibpfRGB121212Packed ||
			pf == ibpfRGB141414Packed || pf == ibpfRGB161616Packed ||
			pf == ibpfYUV444_10Packed || pf == ibpfYUV444_UYV_10Packed) {
			dev->type = IW_16U;
		} else if (pf == ibpfMono32) {
			dev->type = IW_32S;
		} else {
			dev->type = IW_8U;
		}

		if (!AVcap_init_ip ("mvimpact", dev, &data->ip, pf==ibpfMono12Packed_V2))
			return FALSE;

		Mdebug ((stderr, "mvimpact: Using images %dx%d planes: %d\n",
				 dev->width, dev->height, dev->planes));
	}
	if (requestNum >= 0) {
		DMR_ImageRequestUnlock (data->driver, requestNum);
		DMR_ImageRequestSingle (data->driver, 0, 0);
	}

	return ok;
}

void AVmvimp_close (void *capData)
{
	TDMR_ERROR result;
	mvimpData *data = capData;

	if (data->driver != INVALID_ID) {
		if ((result = DMR_ImageRequestReset(data->driver, 0, 0)) != DMR_NO_ERROR)
			AVcap_warning ("mvimpact: DMR_ImageRequestReset: %d",	result);
		if (data->buffer) {
			if ((result = DMR_ReleaseImageRequestBufferDesc(&data->buffer)) != DMR_NO_ERROR)
				AVcap_warning ("mvimpact: DMR_ReleaseImageRequestBufferDesc: %d", result);
		}
		if ((result = DMR_CloseDevice (data->driver, data->device)) != DMR_NO_ERROR)
			AVcap_warning ("mvimpact: DMR_CloseDevice() failed (code: %d)", result);
	}
	if (data->dmr != INVALID_ID) {
		if ((result = DMR_Close()) != DMR_NO_ERROR)
			AVcap_warning ("mvimpact: DMR_Close() failed (code: %d)", result);
	}
	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}

	free (data);
}

int AVmvimp_capture (void *capData, iwImage *img, struct timeval *time)
{
	mvimpData *data = capData;
	int requestNum;
	BOOL ok = TRUE;

	if (image_capture (data, time, &requestNum)) {
		ImageBuffer *ib = data->buffer;
		guchar *s = ib->vpData;
		TImageBufferPixelFormat pformat = ib->pixelFormat;

		if (data->ip.stereo == AV_STEREO_DEINTER) {
			iw_img_stereo_decode (s, data->ip.buffer, IW_8U, ib->iWidth * ib->iHeight);
			s = data->ip.buffer;
			pformat = ibpfMono8;
		} else if (data->ip.stereo == AV_STEREO_RAW) {
			pformat = ibpfMono8;
		}

		switch (pformat) {
			case ibpfMono8:
				if (data->ip.bayer == IW_IMG_BAYER_NONE) {
					memcpy (img->data[0], s, img->width*img->height);
					img->ctab = IW_GRAY;
				} else {
					iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
					img->ctab = IW_RGB;
				}
				break;
			case ibpfMono10:
			case ibpfMono12:
			case ibpfMono14:
			case ibpfMono16:
				AVcap_mono16_ip (&data->ip, img, s, img->width * img->height, FALSE);
				break;
			case ibpfMono32:
				memcpy (img->data[0], s, img->width*img->height * sizeof(guint32));
				img->ctab = IW_GRAY;
				break;
			case ibpfMono12Packed_V2:
				if (data->ip.bayer == IW_IMG_BAYER_NONE) {
					img->ctab = IW_GRAY;
					av_color_mono12 (img->data[0], s, img->width*img->height);
				} else {
					img->ctab = IW_RGB;
					av_color_mono12 (data->ip.buffer, s, img->width*img->height);
					iw_img_bayer_decode (data->ip.buffer, img, data->ip.bayer, data->ip.pattern);
				}
				break;
			case ibpfRGB888Packed:
				av_color_rgb24 (img, s, 2, 1, 0);
				img->ctab = IW_RGB;
				break;
			case ibpfRGBx888Packed:
				av_color_bgr32 (img, s);
				img->ctab = IW_RGB;
				break;
			case ibpfRGBx888Planar:
				av_color_rgb24P (img, s);
				img->ctab = IW_RGB;
				break;
			case ibpfRGB101010Packed:
				av_color_rgb24_16_mask (img, s, 0x3FF, 2, 1, 0);
				img->ctab = IW_RGB;
				break;
			case ibpfRGB121212Packed:
				av_color_rgb24_16_mask (img, s, 0xFFF, 2, 1, 0);
				img->ctab = IW_RGB;
				break;
			case ibpfRGB141414Packed:
				av_color_rgb24_16_mask (img, s, 0x3FFF, 2, 1, 0);
				img->ctab = IW_RGB;
				break;
			case ibpfRGB161616Packed:
				av_color_rgb24_16 (img, s, FALSE, 2, 1, 0);
				img->ctab = IW_RGB;
				break;
			case ibpfBGR888Packed:
				av_color_rgb24 (img, s, 0, 1, 2);
				img->ctab = IW_RGB;
				break;
			case ibpfBGR101010Packed_V2:
				av_color_rgb24_10 (img, s);
				img->ctab = IW_RGB;
				break;
			case ibpfYUV422Packed:
				av_color_yuyv (img, s, 0);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV422_UYVYPacked:
				av_color_yuyv (img, s, 1);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV422Planar:
				av_color_yuv422P (img, s);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV422_10Packed:
				av_color_yuyv_16 (img, s, 0);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV422_UYVY_10Packed:
				av_color_yuyv_16 (img, s, 1);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV444Packed:
				av_color_rgb24 (img, s, 0, 1, 2);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV444_UYVPacked:
				av_color_rgb24 (img, s, 1, 0, 2);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV444Planar:
				av_color_rgb24P (img, s);
				img->ctab = IW_YUV;
				break;
			case ibpfYUV444_10Packed:
				av_color_rgb24_16_mask (img, s, 0x3FF, 0, 1, 2);
				img->ctab = IW_RGB;
				break;
			case ibpfYUV444_UYV_10Packed:
				av_color_rgb24_16_mask (img, s, 0x3FF, 1, 0, 2);
				img->ctab = IW_RGB;
				break;
			default:
				AVcap_warning ("mvimpact: pixelFormat %ld not implemented for conversion",
							   ib->pixelFormat);
				break;
		}
	}
	if (requestNum >= 0) {
		DMR_ImageRequestUnlock (data->driver, requestNum);
		DMR_ImageRequestSingle (data->driver, 0, 0);
	}

	return ok;
}

#endif /* WITH_MVIMPACT */
