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

#ifdef WITH_ICUBE

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#ifndef bool
#  define bool unsigned char
#endif
#include <NETUSBCAM_API.h>

#include "AVColor.h"
#include "AVOptions.h"
#include "drv_icube.h"

static struct {
	int id;
	char *name;
} properties[] = {
	{1, "Brightness"},
	{2, "Contrast"},
	{3, "Gamma"},
	{4, "FlippedV"},
	{5, "FlippedH"},
	{6, "WhiteBalance"},
	{7, "ExposureTime"},
	{8, "ExposureTarget"},
	{9, "Red"},
	{10, "Green"},
	{11, "Blue"},
	{12, "Blacklevel"},
	{13, "Gain"},
	{14, "Color"},
	{15, "Pixelclock"},
	{16, "StrobeLenght"},
	{17, "StrobeDelay"},
	{18, "TriggerDelay"},
	{19, "Saturation"},
	{20, "ColorMachine"},
	{21, "TriggerInvert"},
	{22, "MeasureFieldAE"},
	{26, "Shutter"},
	{43, "DefectCorrection"},
	{-1, NULL}
};

static char *modelist[] = {
	"320x240",
	"640x480",
	"752x480",
	"800x600",
	"1024x768",
	"1280x1024",
	"1600x1200",
	"2048x1536",
	"2592x1944",
	NULL
};
#define MODELIST_CNT	(sizeof(modelist)/sizeof(modelist[0]))

#define ARG_LEN			301
#define MAX_REQUESTS	4
#define Idebug(x)		if (data->debug) fprintf x

#define ERROR(res, x) if (res != 0) {								\
		AVcap_warning x;											\
		return FALSE;												\
	}
#define WARNING(res, x) if (res != 0) {								\
		AVcap_warning x;											\
	}

typedef struct icubeBuffer {
	int frameCnt;			/* Number of grabbed images when this image was grabbed */
	struct timeval time;	/* Time data was grabbed */
	void *data;				/* Image data */
} icubeBuffer;

typedef struct icubeData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */

	int device;				/* Number of currently open device */
	int colMode;
	int debug;

	int lastSkipped;
	pthread_cond_t bufferChanged;
	pthread_mutex_t bufferMutex;
	int frameCnt;			/* Number of grabbed images */
	int bufferCnt;			/* Number of buffers ready for processing */
	int bufferSkipped;		/* Number of skipped buffers */
	icubeBuffer buffer[MAX_REQUESTS];
} icubeData;

/*********************************************************************
  Get all available properties and their values from device data->device.
*********************************************************************/
static void properties_get (icubeData *data)
{
	AVcapData *capData = (AVcapData*)data;
	PARAM_PROPERTY prop;
	unsigned long value;
	char name[30];
	int autoVal;
	int i;

	for (i=0; properties[i].id >= 0; i++) {
		if (NETUSBCAM_GetCamParameterRange (data->device, properties[i].id, &prop) != 0
			|| !prop.bEnabled)
			continue;

		if (prop.bAuto && NETUSBCAM_GetParamAuto(data->device, properties[i].id, &autoVal) == 0) {
			sprintf (name, "Auto%s", properties[i].name);
			AVCap_prop_add_bool (capData, name,
								 autoVal, prop.nDef, properties[i].id);
		}
		if (prop.bOnePush) {
			sprintf (name, "OnePush%s", properties[i].name);
			AVCap_prop_add_command (capData, name, properties[i].id);
		}
		if (NETUSBCAM_GetCamParameter (data->device, properties[i].id, &value) != 0)
			value = prop.nMin;
		AVCap_prop_add_int(capData, properties[i].name, value,
						   prop.nMin, prop.nMax, prop.nDef, properties[i].id);
	}
}

/*********************************************************************
  Get values of all available properties of device data->device.
*********************************************************************/
static void properties_get_values (icubeData *data)
{
	AVcapData *capData = (AVcapData*)data;
	int i;

	/* Get values of all available properties */
	for (i=1; i < capData->prop_cnt; i++) {
		iwGrabProperty *prop = &capData->prop[i];
		int res = 0;
		if (prop->type <= IW_GRAB_COMMAND)
			continue;
		switch (prop->type) {
			case IW_GRAB_BOOL:
				res = NETUSBCAM_GetParamAuto (data->device, prop->priv,
											  &prop->data.b.value);
				break;
			case IW_GRAB_INT: {
				unsigned long val;
				if ((res = NETUSBCAM_GetCamParameter (data->device, prop->priv, &val)) == 0)
					prop->data.i.value = val;
				break;
			}
			default:
				Idebug ((stderr, "icube: Unsupported property %d(%s), type %d",
						 i, prop->name, prop->type));
				break;
		}
		WARNING (res, ("icube: Failed to get property %d(%s) error %d",
					   i, prop->name, res));
	}
}

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property from data->device. Return IW_GRAB_STATUS_OK on success.
*********************************************************************/
static iwGrabStatus properties_set (icubeData *data, char *devOptions)
{
	iwGrabStatus status = IW_GRAB_STATUS_OK;
	AVcapData *capData = (AVcapData*)data;
	char propArg[100], propVal[200];
	char *optPos;

	optPos = devOptions;
	while ((optPos = AVcap_optstr_lookup_prefix(optPos, "prop", propArg, sizeof(propArg),
												propVal, sizeof(propVal)))) {
		int err = 0;
		iwGrabProperty *prop;
		int propIdx = AVCap_prop_lookup (capData, propArg);
		if (propIdx < 0) {
			AVcap_warning ("icube: No property '%s' available", propArg);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}
		prop = &capData->prop[propIdx];
		if (!AVCap_prop_parse (prop, prop, propVal)) {
			AVcap_warning ("icube: Failed to parse value for property %d(%s) from '%s'",
						   propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}

		switch (prop->type) {
			case IW_GRAB_COMMAND:
				err = NETUSBCAM_SetParamOnePush (data->device, prop->priv);
				break;
			case IW_GRAB_BOOL:
				err = NETUSBCAM_SetParamAuto (data->device, prop->priv, prop->data.b.value);
				break;
			case IW_GRAB_INT:
				err = NETUSBCAM_SetCamParameter (data->device, prop->priv, prop->data.i.value);
				break;
			default: break;
		}

		if (err) {
			status = IW_GRAB_STATUS_ERR;
			AVcap_error ("icube: Failed to set property %d(%s) to '%s' (error %d)",
						 propIdx, prop->name, propVal, err);
		} else {
			status = IW_GRAB_STATUS_OK;
			Idebug ((stderr, "icube: Set property %d(%s) to value '%s'\n",
					 propIdx, prop->name, propVal));
		}
	}
	return status;
}

static int img_callback (void *buffer, unsigned int bufferSize, void *capData)
{
	icubeData *data = capData;
	int i;

	data->frameCnt++;
	for (i=0; i<MAX_REQUESTS; i++) {
		if (!data->buffer[i].frameCnt) {
			gettimeofday (&data->buffer[i].time, NULL);
			if (!data->buffer[i].data)
				data->buffer[i].data = malloc (bufferSize);
			memcpy (data->buffer[i].data, buffer, bufferSize);

			pthread_mutex_lock (&data->bufferMutex);
			data->buffer[i].frameCnt = data->frameCnt;
			data->bufferCnt++;
			pthread_cond_broadcast (&data->bufferChanged);
			pthread_mutex_unlock (&data->bufferMutex);

			return 0;
		}
	}
	pthread_mutex_lock (&data->bufferMutex);
	data->bufferSkipped++;
	pthread_mutex_unlock (&data->bufferMutex);

	return 0;
}

int AVicube_open (VideoDevData *dev, char *devOptions,
				  int subSample, VideoStandard vidSignal)
{
	int res;
	char buffer[ARG_LEN], buffer2[256];
	int deviceCount = 0;
	int device = 0, i;
	icubeData *data;

	/* Grabbing method supported ? */
	if (vidSignal != AV_ICUBE) return FALSE;

	data = calloc (1, sizeof(icubeData));
	dev->capData = (AVcapData*)data;

	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_RGGB;
	data->device = -1;
	data->colMode = CALLBACK_RGB;
	pthread_mutex_init (&data->bufferMutex, NULL);
	pthread_cond_init (&data->bufferChanged, NULL);

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "NET iCube driver V0.0 by Frank Loemker\n"
					 "usage: device=val:mode=rgbXXX|monoXXX|rawXXX:roi=geometry:propX=val:propY=val:\n"
					 "       bayer=method:pattern=RGGB|BGGR|GRBG|GBRG:stereo=raw|deinter:\n"
					 "       debug\n"
					 "device   access a camera by it's device id [0...], default: %d\n"
					 "mode     grabbing mode, XXX=desired width, default: RGB\n"
					 "roi      grabbing region in the format [width]x[height]+[x]+[y]\n"
					 "propX    set property X ([0...] or it's name) to val. If \"=val\" is not given\n"
					 "         use the properties default value.\n"
					 AVCAP_IP_HELP
					 "debug    output debugging/settings information, default: off\n\n",
					 device);
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		if (!AVcap_parse_ip ("icube", devOptions, &data->ip))
			return FALSE;
		AVcap_optstr_get (devOptions, "device", "%d", &device);
	}

	deviceCount = NETUSBCAM_Init();
	if (device < 0 || deviceCount <= 0 || device >= deviceCount) {
		AVcap_warning ("icube: Device %d not available, only device(s) 0..%d available",
					   device, deviceCount-1);
		return FALSE;
	}

	if (data->debug) {
		res = NETUSBCAM_GetApiVersion (buffer, sizeof(buffer));
		if (res == 0) fprintf (stderr, "icube: API Version %s\n", buffer);

		fprintf (stderr, "icube: Available devices:\n");
		for (i = 0; i < deviceCount; i++) {
			fprintf (stderr, "\t%d:", i);
			res = NETUSBCAM_GetName (i, buffer, sizeof(buffer));
			if (res == 0) fprintf (stderr, " %s", buffer);

			res = NETUSBCAM_GetSerialNum (i, buffer, sizeof(buffer));
			if (res == 0) fprintf (stderr, " Serial %s", buffer);

			res = NETUSBCAM_GetFWVersion (i, buffer, sizeof(buffer));
			if (res == 0) fprintf (stderr, " Firmeware %s", buffer);
			fprintf (stderr, "\n");
		}
	}
	Idebug ((stderr, "icube: %d devices available, using device %d\n",
			 deviceCount, device));

	/* Initialise the camera. */
	res = NETUSBCAM_Open (device);
	ERROR (res, ("icube: NETUSBCAM_Open() failed (error: %d)", res));
	data->device = device;

	if (data->debug) {
		*buffer = *buffer2 = '\0';
		res = NETUSBCAM_GetFWVersion (data->device, buffer, sizeof(buffer));
		res = NETUSBCAM_GetSerialNum (data->device, buffer2, sizeof(buffer2));
		if (*buffer || *buffer2)
			fprintf (stderr, "icube: Firmeware %s, Serial number %s\n", buffer, buffer2);
	}

	memset (buffer, '\0', ARG_LEN);
	if (AVcap_optstr_get (devOptions, "mode", "%300c", buffer) > 0) {
		char *pos = buffer;
		int len;

		if (!strncasecmp(pos, "rgb", 3)) {
			data->colMode = CALLBACK_RGB;
			pos += 3;
		} else if (!strncasecmp(pos, "mono", 4)) {
			data->colMode = CALLBACK_Y_8;
			pos += 4;
		} else if (!strncasecmp(pos, "raw", 3)) {
			data->colMode = CALLBACK_RAW;
			pos += 3;
		}
		len = strlen(pos);
		for (i=0; modelist[i]; i++) {
			if (   (modelist[i][len]=='x' || modelist[i][len]=='\0')
				&& !strncmp (pos, modelist[i], len)) {
				res = NETUSBCAM_SetMode (data->device, i);
				ERROR (res, ("icube: NETUSBCAM_SetMode() failed (error: %d)", res));
				break;
			}
		}
		if (!modelist[i]) {
			AVcap_warning ("icube: Unknown mode \"%s\"", buffer);
			return FALSE;
		}
	}

	if (data->debug) {
		unsigned int modes[20], mode;
		unsigned int cnt = sizeof(modes)/sizeof(modes[0]);

		if (NETUSBCAM_GetMode(data->device, &mode) != 0)
			mode = -1;
		if (NETUSBCAM_GetModeList(data->device, &cnt, modes) == 0 && cnt > 0) {
			fprintf (stderr, "icube: Available modes:\n");
			for (i=0; i<cnt; i++) {
				if (modes[i] < MODELIST_CNT)
					fprintf (stderr, "\t%c%s\n",
							 modes[i] == mode ? '*':' ', modelist[modes[i]]);
			}
		}
	}

	memset (buffer, '\0', ARG_LEN);
	if (AVcap_optstr_get (devOptions, "roi", "%300c", buffer) > 0) {
		int x, y, width, height;
		int maxWidth, maxHeight;
		int cnt;
		char *pos = buffer;

		res = NETUSBCAM_GetSize (data->device, &maxWidth, &maxHeight);
		ERROR (res, ("icube: NETUSBCAM_GetSize() failed (error: %d)", res));

		/* Parse [width]x[height]+[x]+[y] */
		if (*pos != '+' && sscanf (pos, "%d%n", &width, &cnt) >= 1) {
			pos += cnt;
		} else
			width = -1;
		if (*pos == 'x' && sscanf (++pos, "%d%n", &height, &cnt) >= 1) {
			pos += cnt;
		} else
			height = -1;
		if (*pos == '+' && *(++pos) != '+' && sscanf (pos, "%d%n", &x, &cnt) >= 1) {
			pos += cnt;
		} else
			x = 0;
		if (*pos == '+' && *(++pos) != '+' && sscanf (pos, "%d%n", &y, &cnt) >= 1) {
			pos += cnt;
		} else
			y = 0;
		if (width < 0) width = maxWidth-x;
		if (height < 0) height = maxHeight-y;

		res = NETUSBCAM_SetResolution (data->device, width, height, x, y);
		ERROR (res, ("icube: NETUSBCAM_SetResolution() failed (error: %d)", res));
	}

	properties_get (data);
	properties_set (data, devOptions);
	properties_get_values (data);
	if (data->debug)
		AVCap_prop_print ("icube", dev->capData);

	/* Get the destination image properties and allocate image memory */
	{
		int w, h, bpp;

		res = NETUSBCAM_GetSize (data->device, &w, &h);
		ERROR (res, ("icube: NETUSBCAM_GetSize() failed (error: %d)", res));

		if (data->ip.bayer != IW_IMG_BAYER_NONE)
			data->colMode = CALLBACK_RAW;

		switch (data->colMode) {
			case CALLBACK_RAW:
			case CALLBACK_Y_8:
				bpp = 8;
				break;
			case CALLBACK_RGB:
				bpp = 24;
				break;
		}
		if (data->ip.stereo != AV_STEREO_NONE && bpp != 16) {
			AVcap_warning ("icube: Stereo decoding only supported for 16BPP modes\n"
						   "          -> deactivating stereo decoding");
			data->ip.stereo = AV_STEREO_NONE;
		}
		if (data->ip.stereo == AV_STEREO_NONE && data->ip.bayer != IW_IMG_BAYER_NONE
			&& bpp > 16) {
			AVcap_warning ("icube: Bayer decoding only supported for 8/16 Bit mono modes\n"
						   "          -> deactivating bayer decoding");
			data->ip.bayer = IW_IMG_BAYER_NONE;
		}

		dev->f_width = dev->width = w;
		dev->f_height = dev->height = h;
		if (data->colMode == CALLBACK_Y_8)
			dev->planes = 1;
		else
			dev->planes = 3;
		dev->type = IW_8U;

		if (!AVcap_init_ip ("icube", dev, &data->ip, FALSE))
			return FALSE;

		Idebug ((stderr, "icube: Using images %dx%d planes: %d\n",
				 dev->width, dev->height, dev->planes));
	}

	res = NETUSBCAM_SetCallback (data->device, data->colMode, img_callback, data);
	ERROR (res, ("icube: NETUSBCAM_SetCallback() failed (error: %d)", res));

	res = NETUSBCAM_Start (data->device);
	ERROR (res, ("icube: NETUSBCAM_Start() failed (error: %d)", res));

	if (data->debug) {
		int x, y, width, height;
		if (NETUSBCAM_GetResolution(data->device, &width, &height, &x, &y) == 0)
			fprintf (stderr, "icube: Using ROI %dx%d+%d+%d\n", width, height, x, y);
	}

	return TRUE;
}

void AVicube_close (void *capData)
{
	icubeData *data = capData;
	int i;

	if (data->device >= 0) {
		int res;
		res = NETUSBCAM_Stop (data->device);
		WARNING (res, ("icube: NETUSBCAM_Stop() failed (error: %d)", res));
		res = NETUSBCAM_Close (data->device);
		WARNING (res, ("icube: NETUSBCAM_Close() failed (error: %d)", res));

		data->device = -1;
	}
	for (i=0; i<MAX_REQUESTS; i++) {
		if (data->buffer[i].data) {
			free (data->buffer[i].data);
			data->buffer[i].data = NULL;
		}
	}
	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}

	free (data);
}

iwGrabStatus AVicube_set_property (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_OK;
	icubeData *data = (icubeData*)dev->capData;

	if (data->device < 0) return IW_GRAB_STATUS_NOTOPEN;

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
			ret = properties_set (data, props);
			break;
		}
		default:
			ret = IW_GRAB_STATUS_UNSUPPORTED;
			break;
	}
	return ret;
}

int AVicube_capture (void *capData, iwImage *img, struct timeval *time)
{
	icubeData *data = capData;
	int colMode = data->colMode;
	int i, buffer = -1;
	int res;
	guchar *s;

	pthread_mutex_lock (&data->bufferMutex);
	while (!data->bufferCnt) {
		struct timeval now;
		struct timespec timeout;

		gettimeofday (&now, NULL);
		timeout.tv_sec  = now.tv_sec+2;
		timeout.tv_nsec = now.tv_usec*1000;
		if ((res = pthread_cond_timedwait (&data->bufferChanged,
										   &data->bufferMutex, &timeout)) != 0) {
			if (res == ETIMEDOUT)
				iw_warning ("icube: Timeout waiting for an image");
			else
				iw_warning ("icube: Error %d waiting for an image", res);
			return FALSE;
		}
	}
	for (i=0; i<MAX_REQUESTS; i++)
		if ( data->buffer[i].frameCnt>0 &&
			 (buffer < 0 || data->buffer[i].frameCnt<data->buffer[buffer].frameCnt) )
			buffer = i;

	data->bufferCnt--;
	if (data->lastSkipped != data->bufferSkipped) {
		Idebug ((stderr, "icube: Skipped %d images\n", data->bufferSkipped-data->lastSkipped));
		data->bufferSkipped = data->lastSkipped;
	}
	pthread_mutex_unlock (&data->bufferMutex);

	if (time) *time = data->buffer[buffer].time;

	s = (guchar*)data->buffer[buffer].data;
	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U, img->width*img->height/2);
		s = data->ip.buffer;
		colMode = CALLBACK_Y_8;
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		colMode = CALLBACK_Y_8;
	}

	switch (colMode) {
		case CALLBACK_RAW:
		case CALLBACK_Y_8:
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				memcpy (img->data[0], s, img->width*img->height);
				img->ctab = IW_GRAY;
			} else {
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
				img->ctab = IW_RGB;
			}
			break;
		case CALLBACK_RGB:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		default:
			AVcap_warning ("icube: pixelFormat %d not implemented for conversion", colMode);
			break;
	}
	data->buffer[buffer].frameCnt = 0;

	return TRUE;
}

#endif /* WITH_ICUBE */
