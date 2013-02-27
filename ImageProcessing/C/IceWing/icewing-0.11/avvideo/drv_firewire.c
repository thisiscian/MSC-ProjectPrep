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

#if defined(WITH_FIRE) || defined(WITH_FIRE2)

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "AVColor.h"
#include "AVOptions.h"
#include "drv_firewire.h"

#ifdef WITH_FIRE2

#include <inttypes.h>
#include <dc1394/dc1394.h>

#define FORMAT_VGA_NONCOMPRESSED		0
#define FORMAT_SVGA_NONCOMPRESSED_1		1
#define FORMAT_SVGA_NONCOMPRESSED_2		2
#define FORMAT_SCALABLE_IMAGE_SIZE		3

#ifdef __linux__
extern dc1394error_t dc1394_capture_set_device_filename (dc1394camera_t *camera, char *filename);
#endif

static BOOL mode_check (dc1394video_modes_t *mlist, int mode)
{
	int i;
	for (i=0; i < mlist->num; i++)
		if (mlist->modes[i] == mode)
			return TRUE;
	return FALSE;
}

#else

#include <libdc1394/dc1394_control.h>

extern int _dc1394_basic_format7_setup(raw1394handle_t handle, nodeid_t node,
									   unsigned int channel, unsigned int mode, unsigned int speed,
									   unsigned int bytes_per_packet,
									   unsigned int left, unsigned int top,
									   unsigned int width, unsigned int height,
									   dc1394_cameracapture *camera);

#define dc1394color_coding_t				unsigned int
#define dc1394operation_mode_t				unsigned int
#define dc1394speed_t						unsigned int

#define DC1394_VIDEO_MODE_1600x1200_YUV422	MODE_1600x1200_YUV422
#define DC1394_VIDEO_MODE_1280x960_YUV422	MODE_1280x960_YUV422
#define DC1394_VIDEO_MODE_1024x768_YUV422	MODE_1024x768_YUV422
#define DC1394_VIDEO_MODE_800x600_YUV422	MODE_800x600_YUV422
#define DC1394_VIDEO_MODE_640x480_YUV422	MODE_640x480_YUV422
#define DC1394_VIDEO_MODE_320x240_YUV422	MODE_320x240_YUV422
#define DC1394_VIDEO_MODE_160x120_YUV444	MODE_160x120_YUV444
#define DC1394_VIDEO_MODE_640x480_YUV411	MODE_640x480_YUV411
#define DC1394_VIDEO_MODE_1600x1200_RGB8	MODE_1600x1200_RGB
#define DC1394_VIDEO_MODE_1280x960_RGB8		MODE_1280x960_RGB
#define DC1394_VIDEO_MODE_1024x768_RGB8		MODE_1024x768_RGB
#define DC1394_VIDEO_MODE_800x600_RGB8		MODE_800x600_RGB
#define DC1394_VIDEO_MODE_640x480_RGB8		MODE_640x480_RGB
#define DC1394_VIDEO_MODE_1600x1200_MONO8	MODE_1600x1200_MONO
#define DC1394_VIDEO_MODE_1280x960_MONO8	MODE_1280x960_MONO
#define DC1394_VIDEO_MODE_1024x768_MONO8	MODE_1024x768_MONO
#define DC1394_VIDEO_MODE_800x600_MONO8		MODE_800x600_MONO
#define DC1394_VIDEO_MODE_640x480_MONO8		MODE_640x480_MONO
#define DC1394_VIDEO_MODE_1600x1200_MONO16	MODE_1600x1200_MONO16
#define DC1394_VIDEO_MODE_1280x960_MONO16	MODE_1280x960_MONO16
#define DC1394_VIDEO_MODE_1024x768_MONO16	MODE_1024x768_MONO16
#define DC1394_VIDEO_MODE_800x600_MONO16	MODE_800x600_MONO16
#define DC1394_VIDEO_MODE_640x480_MONO16	MODE_640x480_MONO16

#define DC1394_VIDEO_MODE_FORMAT7_0			MODE_FORMAT7_0
#define DC1394_VIDEO_MODE_FORMAT7_1			MODE_FORMAT7_1
#define DC1394_VIDEO_MODE_FORMAT7_2			MODE_FORMAT7_2
#define DC1394_VIDEO_MODE_FORMAT7_3			MODE_FORMAT7_3
#define DC1394_VIDEO_MODE_FORMAT7_4			MODE_FORMAT7_4
#define DC1394_VIDEO_MODE_FORMAT7_5			MODE_FORMAT7_5
#define DC1394_VIDEO_MODE_FORMAT7_6			MODE_FORMAT7_6
#define DC1394_VIDEO_MODE_FORMAT7_7			MODE_FORMAT7_7

#define DC1394_COLOR_CODING_MONO8			COLOR_FORMAT7_MONO8
#define DC1394_COLOR_CODING_YUV411			COLOR_FORMAT7_YUV411
#define DC1394_COLOR_CODING_YUV422			COLOR_FORMAT7_YUV422
#define DC1394_COLOR_CODING_YUV444			COLOR_FORMAT7_YUV444
#define DC1394_COLOR_CODING_RGB8			COLOR_FORMAT7_RGB8
#define DC1394_COLOR_CODING_MONO16			COLOR_FORMAT7_MONO16
#define DC1394_COLOR_CODING_RGB16			COLOR_FORMAT7_RGB16
#define DC1394_COLOR_CODING_MONO16S			COLOR_FORMAT7_MONO16S
#define DC1394_COLOR_CODING_RGB16S			COLOR_FORMAT7_RGB16S
#define DC1394_COLOR_CODING_RAW8			COLOR_FORMAT7_RAW8
#define DC1394_COLOR_CODING_RAW16			COLOR_FORMAT7_RAW16

#define DC1394_FRAMERATE_1_875	FRAMERATE_1_875
#define DC1394_FRAMERATE_3_75	FRAMERATE_3_75
#define DC1394_FRAMERATE_7_5	FRAMERATE_7_5
#define DC1394_FRAMERATE_15		FRAMERATE_15
#define DC1394_FRAMERATE_30		FRAMERATE_30
#define DC1394_FRAMERATE_60		FRAMERATE_60

#define DC1394_OPERATION_MODE_LEGACY	OPERATION_MODE_LEGACY
#define DC1394_OPERATION_MODE_1394B		OPERATION_MODE_1394B

#define DC1394_ISO_SPEED_100	SPEED_100
#define DC1394_ISO_SPEED_200	SPEED_200
#define DC1394_ISO_SPEED_400	SPEED_400
#define DC1394_ISO_SPEED_800	SPEED_800

#define DC1394_USE_MAX_AVAIL	USE_MAX_AVAIL

#endif

#define Fdebug(x)				if (data->debug) fprintf x
#define DEFAULT_VIDEO_DEVICE_N	"/dev/video1394/0"
#define DEFAULT_VIDEO_DEVICE	"/dev/video1394"
#define NUM_BUFFERS 8
#define DROP_FRAMES 1		/* Uncomment to drop frames to prevent delays */

typedef struct fireData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */
	int modeidx;			/* Index in the modes array, see below */
	dc1394color_coding_t color;/* Color decomposition mode, derived from mode (above) */
	int bus_speed;			/* e.g. 400 Mbps */

	BOOL debug;

	BOOL free_unlisten;		/* Flags to indicate what has to be freed */
	BOOL pg_timestamp;		/* Use the image embedded timestamp from different point grey cameras? */
	time_t pg_timestamp_last_seconds;
	time_t pg_timestamp_tv_sec;

	int f7_x, f7_y, f7_width, f7_height;

#ifdef WITH_FIRE2
	dc1394_t *d;
	dc1394camera_t *camera;
	dc1394featureset_t feats;
	uint32_t f7_maxw, f7_maxh;
	uint32_t f7_stepw, f7_steph;
	uint32_t f7_stepx, f7_stepy;
#else
	int free_handle;
	int free_camera;
	raw1394handle_t handle;
	dc1394_cameracapture camera;
#endif
} fireData;

	/* Default mode */
#define MODEIDX_YUV			4 /* DC1394_VIDEO_MODE_640x480_YUV422 */
	/* Default if bayer decoding and no stereo decoding */
#define MODEIDX_MONO		17 /* DC1394_VIDEO_MODE_640x480_MONO */

#define COLOR_CODING_CNT		(DC1394_COLOR_CODING_RAW16 - DC1394_COLOR_CODING_MONO8 + 1)
	/* Bit depths for DC1394_COLOR_CODING_MONO8, ... */
static const int colorBPP[COLOR_CODING_CNT] = {
	8, 12, 16, 24, 24, 16, 48, 16, 48, 8, 16
};
	/* Bit depths of encoded data */
static const int colorDepths[COLOR_CODING_CNT] = {
	8, 24, 24, 24, 24, 16, 48, 16, 48, 8, 16
};
	/* Names for DC1394_COLOR_CODING_MONO8, ... */
static const char *colorNames[COLOR_CODING_CNT] = {
	"MONO8", "YUV411", "YUV422", "YUV444", "RGB8", "MONO16", "RGB16",
	"MONO16S", "RGB16S", "RAW8", "RAW16"
};

static struct {
	char *name;
	int width;
	int height;
	int format;
	int mode;
	char *modestr;
	dc1394color_coding_t color;
} modes[] = {
	{"yuv",    1600, 1200, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1600x1200_YUV422, "1600x1200_YUV422", DC1394_COLOR_CODING_YUV422},
	{"yuv",    1280,  960, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1280x960_YUV422,  "1280x960_YUV422",  DC1394_COLOR_CODING_YUV422},
	{"yuv",    1024,  768, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_1024x768_YUV422,  "1024x768_YUV422",  DC1394_COLOR_CODING_YUV422},
	{"yuv",     800,  600, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_800x600_YUV422,   "800x600_YUV422",   DC1394_COLOR_CODING_YUV422},
	{"yuv",     640,  480, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_640x480_YUV422,   "640x480_YUV422",   DC1394_COLOR_CODING_YUV422},
	{"yuv",     320,  240, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_320x240_YUV422,   "320x240_YUV422",   DC1394_COLOR_CODING_YUV422},
	{"yuv",     160,  120, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_160x120_YUV444,   "160x120_YUV444",   DC1394_COLOR_CODING_YUV444},
	{"yuv",     640,  480, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_640x480_YUV411,   "640x480_YUV411",   DC1394_COLOR_CODING_YUV411},
	{"rgb",    1600, 1200, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1600x1200_RGB8,   "1600x1200_RGB",    DC1394_COLOR_CODING_RGB8},
	{"rgb",    1280,  960, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1280x960_RGB8,    "1280x960_RGB",     DC1394_COLOR_CODING_RGB8},
	{"rgb",    1024,  768, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_1024x768_RGB8,    "1024x768_RGB",     DC1394_COLOR_CODING_RGB8},
	{"rgb",     800,  600, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_800x600_RGB8,     "800x600_RGB",      DC1394_COLOR_CODING_RGB8},
	{"rgb",     640,  480, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_640x480_RGB8,     "640x480_RGB",      DC1394_COLOR_CODING_RGB8},
	{"mono",   1600, 1200, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1600x1200_MONO8,  "1600x1200_MONO",   DC1394_COLOR_CODING_MONO8},
	{"mono",   1280,  960, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1280x960_MONO8,   "1280x960_MONO",    DC1394_COLOR_CODING_MONO8},
	{"mono",   1024,  768, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_1024x768_MONO8,   "1024x768_MONO",    DC1394_COLOR_CODING_MONO8},
	{"mono",    800,  600, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_800x600_MONO8,    "800x600_MONO",     DC1394_COLOR_CODING_MONO8},
	{"mono",    640,  480, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_640x480_MONO8,    "640x480_MONO",     DC1394_COLOR_CODING_MONO8},
	{"16mono", 1600, 1200, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1600x1200_MONO16, "1600x1200_MONO16", DC1394_COLOR_CODING_MONO16},
	{"16mono", 1280,  960, FORMAT_SVGA_NONCOMPRESSED_2,
	 DC1394_VIDEO_MODE_1280x960_MONO16,  "1280x960_MONO16",  DC1394_COLOR_CODING_MONO16},
	{"16mono", 1024,  768, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_1024x768_MONO16,  "1024x768_MONO16",  DC1394_COLOR_CODING_MONO16},
	{"16mono",  800,  600, FORMAT_SVGA_NONCOMPRESSED_1,
	 DC1394_VIDEO_MODE_800x600_MONO16,   "800x600_MONO16",   DC1394_COLOR_CODING_MONO16},
	{"16mono",  640,  480, FORMAT_VGA_NONCOMPRESSED,
	 DC1394_VIDEO_MODE_640x480_MONO16,   "640x480_MONO16",   DC1394_COLOR_CODING_MONO16},
	{"f70", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_0,        "Format7_0",        DC1394_COLOR_CODING_MONO8},
	{"f71", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_1,        "Format7_1",        DC1394_COLOR_CODING_MONO8},
	{"f72", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_2,        "Format7_2",        DC1394_COLOR_CODING_MONO8},
	{"f73", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_3,        "Format7_3",        DC1394_COLOR_CODING_MONO8},
	{"f74", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_4,        "Format7_4",        DC1394_COLOR_CODING_MONO8},
	{"f75", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_5,        "Format7_5",        DC1394_COLOR_CODING_MONO8},
	{"f76", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_6,        "Format7_6",        DC1394_COLOR_CODING_MONO8},
	{"f77", -1, -1, FORMAT_SCALABLE_IMAGE_SIZE,
	 DC1394_VIDEO_MODE_FORMAT7_7,        "Format7_7",        DC1394_COLOR_CODING_MONO8},
	{NULL, 99999, 99999, 0, 0, NULL, 0}
};

#ifdef WITH_FIRE2

static struct {
	int id;
	char *name;
} properties[] = {
	{DC1394_FEATURE_BRIGHTNESS,     "Brightness"},
	{DC1394_FEATURE_EXPOSURE,       "Exposure"},
	{DC1394_FEATURE_SHARPNESS,      "Sharpness"},
	{DC1394_FEATURE_WHITE_BALANCE,  "WhiteBalance"},
	{DC1394_FEATURE_HUE,            "Hue"},
	{DC1394_FEATURE_SATURATION,     "Saturation"},
	{DC1394_FEATURE_GAMMA,          "Gamma"},
	{DC1394_FEATURE_SHUTTER,        "Shutter"},
	{DC1394_FEATURE_GAIN,           "Gain"},
	{DC1394_FEATURE_IRIS,           "Iris"},
	{DC1394_FEATURE_FOCUS,          "Focus"},
	{DC1394_FEATURE_TEMPERATURE,    "Temperature"},
	{DC1394_FEATURE_TRIGGER,        "Trigger"},
	{DC1394_FEATURE_TRIGGER_DELAY,  "TriggerDelay"},
	{DC1394_FEATURE_WHITE_SHADING,  "WhiteShading"},
	{DC1394_FEATURE_FRAME_RATE,     "FrameRate"},
	{DC1394_FEATURE_ZOOM,           "Zoom"},
	{DC1394_FEATURE_PAN,            "Pan"},
	{DC1394_FEATURE_TILT,           "Tilt"},
	{DC1394_FEATURE_OPTICAL_FILTER, "OpticalFilter"},
	{DC1394_FEATURE_CAPTURE_SIZE,   "CaptureSize"},
	{DC1394_FEATURE_CAPTURE_QUALITY,"CaptureQuality"},
	{-1, NULL}
};

/* Storing id and an additional sub-identifier in iwGrabProperty */
#define P_ID(v)			((v) & 0xFFFFFFFF)
#define P_SUB(v)		((v) >> 32)
#define P_FROM_SUB(v)	(((gint64)v) << 32)

/*********************************************************************
  Get all available properties of device data->camera.
*********************************************************************/
static void properties_get (fireData *data)
{
	AVcapData *capData = (AVcapData*)data;
	int pnum, m;
	dc1394error_t err;
	dc1394feature_info_t *feat;
	char name[30];

	err = dc1394_feature_get_all (data->camera, &data->feats);
	if (err != DC1394_SUCCESS) {
		AVcap_warning ("firewire: Could not get features (%s)", dc1394_error_get_string(err));
		return;
	}
	for (pnum=0; properties[pnum].id >= 0; pnum++) {
		int id = properties[pnum].id;
		feat = &data->feats.feature[id-DC1394_FEATURE_MIN];
		if (!feat->available) continue;

		for (m=0; m<feat->modes.num; m++) {
			switch (feat->modes.modes[m]) {
				case DC1394_FEATURE_MODE_AUTO:
					sprintf (name, "Auto%s", properties[pnum].name);
					AVCap_prop_add_bool (capData, name,
										 feat->current_mode == DC1394_FEATURE_MODE_AUTO,
										 FALSE, id);
					break;
				case DC1394_FEATURE_MODE_ONE_PUSH_AUTO:
					sprintf (name, "OnePush%s", properties[pnum].name);
					AVCap_prop_add_command (capData, name, id);
					break;
				case DC1394_FEATURE_MODE_MANUAL:
					switch (id) {
						case DC1394_FEATURE_WHITE_BALANCE:
							AVCap_prop_add_int(capData, "WhiteBalanceRV", feat->RV_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id);
							AVCap_prop_add_int(capData, "WhiteBalanceBU", feat->BU_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id+P_FROM_SUB(1));
							break;
						case DC1394_FEATURE_WHITE_SHADING:
							AVCap_prop_add_int(capData, "WhiteShadingR", feat->R_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id);
							AVCap_prop_add_int(capData, "WhiteShadingG", feat->G_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id+P_FROM_SUB(1));
							AVCap_prop_add_int(capData, "WhiteShadingB", feat->B_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id+P_FROM_SUB(2));
							break;
						case DC1394_FEATURE_TEMPERATURE:
							AVCap_prop_add_int(capData, properties[pnum].name, feat->target_value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id);
							break;
						default:
							AVCap_prop_add_int(capData, properties[pnum].name, feat->value,
											   feat->min, feat->max, (feat->min+feat->max)/2, id);
							break;
					}
					break;
			}
		}
	}
}

/*********************************************************************
  Get values of all available properties of device data->camera.
*********************************************************************/
static void properties_get_values (fireData *data)
{
	AVcapData *capData = (AVcapData*)data;
	int pnum;

	for (pnum=0; pnum < capData->prop_cnt; pnum++) {
		dc1394error_t err = DC1394_SUCCESS;
		dc1394feature_mode_t mode;
		iwGrabProperty *prop = capData->prop+pnum;
		int id = P_ID(prop->priv);
		int sub = P_SUB(prop->priv);
		uint32_t val, val2, val3;

		if (prop->type <= IW_GRAB_COMMAND)
			continue;
		switch (prop->type) {
			case IW_GRAB_BOOL:
				err = dc1394_feature_get_mode(data->camera, id, &mode);
				if (err == DC1394_SUCCESS)
					prop->data.b.value = mode == DC1394_FEATURE_MODE_AUTO;
				break;
			case IW_GRAB_INT:
				switch (id) {
					case DC1394_FEATURE_WHITE_BALANCE:
						err = dc1394_feature_whitebalance_get_value(data->camera, &val2, &val);
						if (sub == 1) val = val2;
						break;
					case DC1394_FEATURE_WHITE_SHADING:
						err = dc1394_feature_whiteshading_get_value(data->camera, &val, &val2, &val3);
						if (sub == 1)
							val = val2;
						else if (sub == 2)
							val = val3;
						break;
					case DC1394_FEATURE_TEMPERATURE:
						err = dc1394_feature_temperature_get_value(data->camera, &val, &val2);
						break;
					default:
						err = dc1394_feature_get_value(data->camera, id, &val);
						break;
				}
				if (err == DC1394_SUCCESS)
					prop->data.i.value = val;
				break;
			default:
				Fdebug ((stderr, "firewire: Unsupported property %d(%s), type %d",
						 pnum, prop->name, prop->type));
				continue;
		}
		if (err != DC1394_SUCCESS)
			AVcap_error ("firewire: Failed to get property %d(%s) (%s)",
						 pnum, prop->name, dc1394_error_get_string(err));
	}
}

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property of data->camera. Return IW_GRAB_STATUS_OK on success.
*********************************************************************/
static iwGrabStatus properties_set (fireData *data, char *devOptions)
{
	iwGrabStatus status = IW_GRAB_STATUS_OK;
	AVcapData *capData = (AVcapData*)data;
	char propArg[100], propVal[200];
	char *optPos;

	optPos = devOptions;
	while ((optPos = AVcap_optstr_lookup_prefix(optPos, "prop", propArg, sizeof(propArg),
												propVal, sizeof(propVal)))) {
		dc1394error_t err = DC1394_SUCCESS;
		dc1394feature_info_t *feat;
		int id, sub;
		uint32_t val, val2, val3, newVal;
		iwGrabProperty *prop;
		int propIdx = AVCap_prop_lookup (capData, propArg);

		if (propIdx < 0) {
			AVcap_warning ("firewire: No property '%s' available", propArg);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}
		prop = &capData->prop[propIdx];
		if (!AVCap_prop_parse (prop, prop, propVal)) {
			AVcap_warning ("firewire: Failed to parse value for property %d(%s) from '%s'",
						   propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}

		id = P_ID(prop->priv);
		sub = P_SUB(prop->priv);

		feat = &data->feats.feature[id-DC1394_FEATURE_MIN];
		if (feat->on_off_capable && feat->is_on == DC1394_OFF) {
			if (dc1394_feature_set_power (data->camera, id, DC1394_ON) == DC1394_SUCCESS)
				feat->is_on = DC1394_ON;
		}
		if (feat->absolute_capable && feat->abs_control == DC1394_ON) {
			if (dc1394_feature_set_absolute_control (data->camera, id, DC1394_OFF) == DC1394_SUCCESS)
				feat->abs_control = DC1394_OFF;
		}
		switch (prop->type) {
			case IW_GRAB_COMMAND:
				err = dc1394_feature_set_mode(data->camera, id,
											  DC1394_FEATURE_MODE_ONE_PUSH_AUTO);
				break;
			case IW_GRAB_BOOL:
				err = dc1394_feature_set_mode(data->camera, id,
											  prop->data.b.value ? DC1394_FEATURE_MODE_AUTO:DC1394_FEATURE_MODE_MANUAL);
				break;
			case IW_GRAB_INT:
				newVal = prop->data.i.value;
				switch (id) {
					case DC1394_FEATURE_WHITE_BALANCE:
						err = dc1394_feature_whitebalance_get_value(data->camera, &val2, &val);
						if (sub == 0)
							val = newVal;
						else
							val2 = newVal;
						if (err == DC1394_SUCCESS)
							err = dc1394_feature_whitebalance_set_value(data->camera, val2, val);
						break;
					case DC1394_FEATURE_WHITE_SHADING:
						err = dc1394_feature_whiteshading_get_value(data->camera, &val, &val2, &val3);
						if (sub == 0)
							val = newVal;
						else if (sub == 1)
							val2 = newVal;
						else
							val3 = newVal;
						if (err == DC1394_SUCCESS)
							err = dc1394_feature_whiteshading_set_value(data->camera, val, val2, val3);
						break;
					case DC1394_FEATURE_TEMPERATURE:
						err = dc1394_feature_temperature_set_value(data->camera, newVal);
						break;
					default:
						err = dc1394_feature_set_value(data->camera, id, newVal);
						break;
				}
				break;
			default: continue;
		}

		if (err == DC1394_SUCCESS) {
			status = IW_GRAB_STATUS_OK;
			Fdebug ((stderr, "firewire: Set property %d(%s) to value '%s'\n",
					 propIdx, prop->name, propVal));
		} else {
			status = IW_GRAB_STATUS_ERR;
			AVcap_error ("firewire: Failed to set property %d(%s) to '%s' (%s)",
						 propIdx, prop->name, propVal, dc1394_error_get_string(err));
		}
	}
	return status;
}

static int f7_round (int v, int step, int min, int max)
{
	v = (v+step/2)/step * step;
	if (v < min)
		v = min;
	else if (v > max)
		v = max/step * step;
	return v;
}

static BOOL AVfire_set_roi (fireData *data, int packetSize)
{
	dc1394error_t err;
	int mode = modes[data->modeidx].mode;
	int x = f7_round (data->f7_x, data->f7_stepx, 0, data->f7_maxw - data->f7_stepw);
	int y = f7_round (data->f7_y, data->f7_stepy, 0, data->f7_maxh - data->f7_steph);
	int w = f7_round (data->f7_width, data->f7_stepw, data->f7_stepw, data->f7_maxw - x);
	int h = f7_round (data->f7_height, data->f7_steph, data->f7_steph, data->f7_maxh - y);

	Fdebug ((stderr, "firewire: set_roi (mode %d ROI %dx%d+%d+%d color %d)\n",
			 mode, w, h, x, y, data->color));
	if ((err = dc1394_format7_set_roi(data->camera, mode,
									  data->color, packetSize, x, y, w, h))
		!= DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to set roi for a format7 mode (%s)",
					 dc1394_error_get_string(err));
		return FALSE;
	}
	return TRUE;
}

#endif

static void AVfire_print_mode (int modeidx)
{
	char name[20];
	if (modes[modeidx].width >= 0)
		sprintf (name, "%s%d", modes[modeidx].name, modes[modeidx].width);
	else
		strcpy (name, modes[modeidx].name);
	fprintf (stderr, "    %-10s  %-3d  %s\n", name, modes[modeidx].mode, modes[modeidx].modestr);
}

static int AVfire_fps2enum (float fps)
{
	if (fps <= 0)
		return DC1394_FRAMERATE_15;
	else if (fps <= 1.975)
		return DC1394_FRAMERATE_1_875;
	else if (fps <= 3.85)
		return DC1394_FRAMERATE_3_75;
	else if (fps <= 7.6)
		return DC1394_FRAMERATE_7_5;
	else if (fps <= 15.1)
		return DC1394_FRAMERATE_15;
	else if (fps <= 30.1)
		return DC1394_FRAMERATE_30;
	else
		return DC1394_FRAMERATE_60;
}

static int AVfire_fps2packetsize (fireData *data, float fps, int width, int height)
{
	unsigned int unit_bytes, max_bytes, packet_size;
	long denom;
	int num_packets;

	if (fps <= 0) {
		num_packets = 4095;
	} else {
		num_packets = (40*data->bus_speed + fps)/(2*fps);
		if (num_packets < 1)
			num_packets = 1;
		else if (num_packets > 4095)
			num_packets = 4095;
	}

	unit_bytes = max_bytes = 0;
#ifdef WITH_FIRE2
	dc1394_format7_get_packet_parameters (data->camera, modes[data->modeidx].mode,
										  &unit_bytes, &max_bytes);
#else
	dc1394_query_format7_packet_para (data->handle, data->camera.node, modes[data->modeidx].mode,
									  &unit_bytes, &max_bytes);
#endif
	if (unit_bytes < 4) unit_bytes = 4;		   /* At least a quadlet */
	/* Max 4915 quadlets in S1600, minus 3 for header and data CRC */
	if (max_bytes < unit_bytes)
		max_bytes = 4*(4915*data->bus_speed/1600 - 3);

	denom = (long)unit_bytes*num_packets*8;
	packet_size = (((long)width*height*colorBPP[data->color - DC1394_COLOR_CODING_MONO8] + denom - 1)/
				   denom)*unit_bytes;
	if (packet_size > max_bytes) packet_size = max_bytes;

	Fdebug ((stderr, "firewire: Using packetSize %u for frame rate %f\n",
			 packet_size, fps));
	return packet_size;
}

static BOOL AVfire_open_common (VideoDevData *dev, fireData *data, int modeidx)
{
	int depth = colorDepths[data->color - DC1394_COLOR_CODING_MONO8];

	if (depth < 8 || depth > 48) {
		AVcap_warning ("firewire: Color depth %d not supported", depth);
		return FALSE;
	}

	if (   data->ip.stereo != AV_STEREO_NONE
		&& colorBPP[data->color - DC1394_COLOR_CODING_MONO8] != 16) {
		AVcap_warning ("firewire: Stereo decoding only supported for 16BPP modes (422YUV, mono16, raw16)\n"
					   "          -> deactivating stereo decoding");
		data->ip.stereo = AV_STEREO_NONE;
	}
	if (data->ip.stereo == AV_STEREO_NONE
		&& data->ip.bayer != IW_IMG_BAYER_NONE && depth > 16) {
		AVcap_warning ("firewire: Bayer decoding only supported for 8/16 Bit mono modes\n"
					   "          -> deactivating bayer decoding");
		data->ip.bayer = IW_IMG_BAYER_NONE;
	}

	if (data->ip.bayer == IW_IMG_BAYER_NONE &&
		(depth <= 16 || data->ip.stereo != AV_STEREO_NONE))
		dev->planes = 1;
	else
		dev->planes = 3;
	if ((depth == 16 || depth == 48) && data->ip.stereo == AV_STEREO_NONE)
		dev->type = IW_16U;
	else
		dev->type = IW_8U;

	if (!AVcap_init_ip ("firewire", dev, &data->ip, FALSE))
		return FALSE;

	Fdebug ((stderr, "firewire: Image size %dx%d planes %d\n",
			 dev->width, dev->height, dev->planes));
	return TRUE;
}

int AVfire_open (VideoDevData *dev, char *devOptions,
				 int subSample, VideoStandard vidSignal)
{
	char device[256];
	int mode, modeidx, i, p;
	int camNumArg = 1, devGiven = -1;
	int f7_packetSize = DC1394_USE_MAX_AVAIL;
	float fps = -1;
	fireData *data;
	struct stat statbuf;
	dc1394operation_mode_t opmode = DC1394_OPERATION_MODE_LEGACY;
	dc1394speed_t iso_speed = DC1394_ISO_SPEED_400;

	/* Grabbing method supported ? */
	if (vidSignal != AV_FIREWIRE) return FALSE;

	data = calloc (1, sizeof(fireData));
	dev->capData = (AVcapData*)data;

	if (stat (DEFAULT_VIDEO_DEVICE_N, &statbuf) != -1)
		strcpy (device, DEFAULT_VIDEO_DEVICE_N);
	else
		strcpy (device, DEFAULT_VIDEO_DEVICE);
	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_AUTO;
	data->bus_speed = 400;
	modeidx = MODEIDX_YUV;

	if (devOptions) {
#define STR_LEN         301
		char str[STR_LEN];

		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "\nfirewire driver V0.3 by Frank Loemker\n"
					 "usage: device=name:camera=val:fps=val:packetSize=val:propX=val:propY=val:\n"
					 "       mode=yuvXXX|rgbXXX|monoXXX|16monoXXX|f7XXX:speed=val:reset:\n"
					 "       bayer=method:pattern=RGGB|BGGR|GRBG|GBRG:stereo=raw|deinter:\n"
					 "       pgtimestamp:debug\n"
					 "device   device name, default: %s\n"
					 "camera   camera number [1...], default: %d\n"
					 "fps      frame rate, supported: 1.875, 3.75, 7.5, 15, 30, 60; default: 15\n"
					 "packetSize bytes per packet for format 7 modes;\n"
					 "         only used if no fps value is given;\n"
					 "         -1: Use curent value, -2: Use maximal value; default: -2\n"
					 "mode     full size starting mode, XXX=desired width, default: YUV640x480\n"
					 "         for f7: XXX=0|1|2|3|4|5|6|7[mono8|yuv411|yuv422|yuv444|rgb8|mono16\n"
					 "                     |rgb16][width]x[height]+[x]+[y]\n"
					 "                 defaults: first available color; max width/height; position 0,0\n"
					 "speed    iso speed, supported: 100, 200, 400, 800; default: 400. 1394b mode is\n"
					 "         used for 800, 1394a for the rest. Will cause problems if wrong bus!\n"
#ifdef WITH_FIRE2
					 "propX    set property X ([0...] or it's name) to val. If \"=val\" is not given\n"
					 "         use the properties default value.\n"
					 "reset    Reset bus the camera is attached to prior to capture. WILL AFFFECT ALL\n"
					 "         DEVICES ON THE BUS!\n"
					 "pgtimestamp Try to enable the image embedded timestamp, which is supported by\n"
					 "         different cameras from point grey research\n"
#endif
					 AVCAP_IP_HELP_D("Auto detection / RGGB")
					 "debug    output debugging/settings information, default: off\n\n",
					 device, camNumArg);
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		devGiven = AVcap_optstr_get (devOptions, "device", "%s", device);
		AVcap_optstr_get (devOptions, "camera", "%d", &camNumArg);
		AVcap_optstr_get (devOptions, "fps", "%f", &fps);
		AVcap_optstr_get (devOptions, "packetSize", "%d", &f7_packetSize);
		AVcap_optstr_get (devOptions, "speed", "%d", &data->bus_speed);
		if (AVcap_optstr_lookup (devOptions, "pgtimestamp"))
			data->pg_timestamp = TRUE;

		if (!AVcap_parse_ip ("firewire", devOptions, &data->ip))
			return FALSE;
		if (data->ip.bayer != IW_IMG_BAYER_NONE && data->ip.stereo == AV_STEREO_NONE)
			modeidx = MODEIDX_MONO;

		switch (data->bus_speed) {
			case 100:
				iso_speed = DC1394_ISO_SPEED_100;
				break;
			case 200:
				iso_speed = DC1394_ISO_SPEED_200;
				break;
			case 400:
				iso_speed = DC1394_ISO_SPEED_400;
				break;
			case 800:
				iso_speed = DC1394_ISO_SPEED_800;
				opmode = DC1394_OPERATION_MODE_1394B;
				break;
			default:
				AVcap_warning ("firewire: Speed %d is not supported", data->bus_speed);
				return FALSE;
		}

		memset (str, '\0', STR_LEN);
		AVcap_optstr_get (devOptions, "mode", "%300c", str);
		for (p=0; modes[p].name; p++) {
			int len = strlen(modes[p].name);
			if (!strncasecmp (modes[p].name, str, len)) {
				int w;
				char *spos = str+len;
				if (modes[p].format == FORMAT_SCALABLE_IMAGE_SIZE) {
					/* Parse [color][width]x[height]+[x]+[y] */
					int cnt;

					data->color = -1;
					for (i=COLOR_CODING_CNT-1; i>=0; i--) {
						int len = strlen(colorNames[i]);
						if (!strncasecmp (colorNames[i], spos, len)) {
							spos += len;
							data->color = i + DC1394_COLOR_CODING_MONO8;
							break;
						}
					}
					if (*spos != '+' && sscanf (spos, "%d%n", &data->f7_width, &cnt) >= 1) {
						spos += cnt;
					} else
						data->f7_width = DC1394_USE_MAX_AVAIL;
					if (*spos == 'x' && sscanf (++spos, "%d%n", &data->f7_height, &cnt) >= 1) {
						spos += cnt;
					} else
						data->f7_height = DC1394_USE_MAX_AVAIL;
					if (   *spos == '+' && *(++spos) != '+'
						&& sscanf (spos, "%d%n", &data->f7_x, &cnt) >= 1) {
						spos += cnt;
					} else
						data->f7_x = 0;
					if (   *spos == '+' && *(++spos) != '+'
						&& sscanf (spos, "%d%n", &data->f7_y, &cnt) >= 1) {
						spos += cnt;
					} else
						data->f7_y = 0;
					Fdebug ((stderr, "firewire: Format 7 with ROI %dx%d+%d+%d color %d\n",
							 data->f7_width, data->f7_height, data->f7_x, data->f7_y, data->color));
				} else {
					if (sscanf (spos, "%d", &w) != 1)
						w = 640;
				}
				if (modes[p].width < 0 || w >= modes[p].width) {
					modeidx = p;
					break;
				}
			}
		}
	}

	dev->f_width = modes[modeidx].width;
	dev->f_height = modes[modeidx].height;
	for (; subSample > 1 && modes[modeidx].width > modes[modeidx+1].width; subSample--)
		modeidx++;

#ifdef WITH_FIRE2
	{
	dc1394error_t err;
	dc1394camera_list_t *clist;
	dc1394video_modes_t mlist;

	data->d = dc1394_new();
	dc1394_camera_enumerate (data->d, &clist);
	if (!clist || clist->num < camNumArg) {
		AVcap_error ("firewire: Camera number %d not found, only %d camera[s] available",
					 camNumArg, clist ? clist->num:0);
		return FALSE;
	}
	Fdebug ((stderr, "firewire: Number of available cameras = %d\n", clist->num));

	data->camera = dc1394_camera_new (data->d, clist->ids[camNumArg-1].guid);
	dc1394_camera_free_list (clist);
	if (!data->camera) {
		AVcap_error ("firewire: Unable to access the camera number %d", camNumArg);
		return FALSE;
	}
	Fdebug ((stderr, "firewire: Using camera %s / %s\n"
			         "          GUID: 0x%"PRIx64"\n",
			 data->camera->vendor, data->camera->model, data->camera->guid));

	if (AVcap_optstr_lookup (devOptions, "reset"))
		dc1394_reset_bus (data->camera);

	/* Set operation mode and speed */
	err = dc1394_video_set_operation_mode (data->camera, opmode);
	if (err == DC1394_SUCCESS) {
		err = dc1394_video_set_iso_speed (data->camera, iso_speed);
		if (err != DC1394_SUCCESS) {
			AVcap_warning ("firewire: Could not enable ISO speed %d (%s). Higher speeds unavailable",
						   iso_speed, dc1394_error_get_string(err));
			return FALSE;
		}
	} else {
		AVcap_warning ("firewire: Could not enable operation mode (%s)",
					   dc1394_error_get_string(err));
		return FALSE;
	}

#ifdef __linux__
	if (devGiven > 0 && (err = dc1394_capture_set_device_filename (data->camera, device))
						!= DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to set video device %s (%s)",
					 device, dc1394_error_get_string(err));
		return FALSE;
	}
#endif

	err = dc1394_video_get_supported_modes (data->camera, &mlist);
	if (mlist.num <= 0 || err != DC1394_SUCCESS) {
		AVcap_error ("firewire: No video modes available");
		return FALSE;
	}
	/* Try to find a suitable format 0/1/2/7 mode,
	   starting with the before initialized mode */
	mode = modeidx;
	/* First try "smaller" modes of the same color space, then bigger */
	while (!mode_check(&mlist, modes[mode].mode)
		   && modes[mode].width > modes[mode+1].width)
		mode++;
	while (mode > 0 && !mode_check(&mlist, modes[mode].mode)
		   && modes[mode].width < modes[mode-1].width)
		mode--;
	if (!mode_check(&mlist, modes[mode].mode)) {
		/* Nothing appropriate found, try all modes */
		mode = 0;
		while (!mode_check(&mlist, modes[mode].mode) && modes[mode+1].name)
			mode++;
	}

	if (data->color == -1 || data->debug) {
		dc1394color_codings_t codings;
		int j, c;

		Fdebug ((stderr, "firewire: Modes supported by the camera:\n"));
		for (j=0; modes[j].name; j++) {
			for (i=0; i < mlist.num; i++) {
				if (modes[j].mode == mlist.modes[i]) {
					if (data->debug)
						AVfire_print_mode (j);
					if (   modes[j].format == FORMAT_SCALABLE_IMAGE_SIZE
						&& dc1394_format7_get_color_codings(data->camera, modes[j].mode, &codings)
						   == DC1394_SUCCESS) {
						if (data->debug) {
							fprintf (stderr, "            Color codings:");
							for (c=0; c<codings.num; c++) {
								int col = codings.codings[c] - DC1394_COLOR_CODING_MONO8;
								if (col < COLOR_CODING_CNT)
									fprintf (stderr, " %s", colorNames[col]);
							}
							fprintf (stderr, "\n");
						}
						if (mode == j && data->color == -1 && codings.num > 0)
							data->color = codings.codings[0];
					}
					break;
				}
			}
		}
	}

	if (!mode_check(&mlist, modes[mode].mode)) {
		AVcap_warning ("firewire: No suitable format0/1/2/7 mode found");
		return FALSE;
	} else {
		if (mode != modeidx)
			AVcap_warning ("firewire: Mode %d (%s) specified, but using available mode %d (%s)",
						   modes[modeidx].mode, modes[modeidx].modestr,
						   modes[mode].mode, modes[mode].modestr);
		data->modeidx = modeidx = mode;
		mode = modes[modeidx].mode;
		if (modes[modeidx].format != FORMAT_SCALABLE_IMAGE_SIZE)
			data->color = modes[modeidx].color;
	}

	Fdebug ((stderr, "firewire: set_mode (mode %d)\n", mode));
	if ((err = dc1394_video_set_mode (data->camera, mode)) != DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to set video format %d (%s)",
					 mode, dc1394_error_get_string(err));
		return FALSE;
	}

	Fdebug ((stderr, "firewire: set_framerate (fps %f)\n", fps));
	if ((err = dc1394_video_set_framerate (data->camera, AVfire_fps2enum(fps))) != DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to set frame rate %f (%s)",
					 fps, dc1394_error_get_string(err));
		return FALSE;
	}

	if (modes[modeidx].format == FORMAT_SCALABLE_IMAGE_SIZE) {
		if (dc1394_format7_get_max_image_size (data->camera, mode, &data->f7_maxw, &data->f7_maxh)
			!= DC1394_SUCCESS) {
			data->f7_maxw = 10000;
			data->f7_maxh = 10000;
		}
		if (dc1394_format7_get_unit_size (data->camera, mode, &data->f7_stepw, &data->f7_steph)
			!= DC1394_SUCCESS) {
			data->f7_stepw = 1;
			data->f7_steph = 1;
		}
		if (dc1394_format7_get_unit_position(data->camera, mode, &data->f7_stepx, &data->f7_stepy)
			!= DC1394_SUCCESS) {
			data->f7_stepx = 1;
			data->f7_stepy = 1;
		}
		Fdebug ((stderr, "firewire: mode %d, maxSize %ux%u stepSize %ux%u stepPos %ux%u\n",
				 mode, data->f7_maxw, data->f7_maxh,
				 data->f7_stepw, data->f7_steph,
				 data->f7_stepx, data->f7_stepy));

		if (!AVfire_set_roi (data, f7_packetSize))
			return FALSE;
		if (fps > 0) {
			uint32_t w, h;
			if (dc1394_format7_get_image_size (data->camera, mode, &w, &h)
				!= DC1394_SUCCESS) {
				w = data->f7_width;
				h = data->f7_height;
			}
			f7_packetSize = AVfire_fps2packetsize (data, fps, w, h);
			if (dc1394_format7_set_packet_size (data->camera, mode, f7_packetSize)) {
				AVcap_error ("firewire: Unable to set packet size to %d (%s)",
							 f7_packetSize, dc1394_error_get_string(err));
			}
		}
	}

	if (data->pg_timestamp) {
		uint32_t value;
		Fdebug ((stderr, "firewire: Checking for embedded FRAME_INFO support\n"));
		if (dc1394_get_adv_control_register(data->camera, 0x2F8, &value) != DC1394_SUCCESS) {
			AVcap_error ("firewire: Error reading advanced control register (FRAME_INFO)");
			return FALSE;
		} else if (value & 0x80000000) {
			Fdebug ((stderr, "firewire: FRAME_INFO supported. Activating reporting.\n"));
			/* Enable the frame timestamp setting */
			if (dc1394_set_adv_control_register(data->camera, 0x2F8, 0x80000000 | (1<<0))
				!= DC1394_SUCCESS) {
				AVcap_error ("firewire: Error setting advanced control register (FRAME_INFO)");
				return FALSE;
			}
		} else {
			AVcap_warning("firewire: Camera has no FRAME_INFO support, pgtimestamp not available");
			return FALSE;
		}
	}

	/* Set given properties and show available ones */
	properties_get (data);
	properties_set (data, devOptions);
	properties_get_values (data);
	if (data->debug)
		AVCap_prop_print ("firewire", dev->capData);

	Fdebug ((stderr, "firewire: capture_setup()\n"));
	if ((err = dc1394_capture_setup (data->camera, NUM_BUFFERS, DC1394_CAPTURE_FLAGS_DEFAULT))
		!= DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to set up camera (%s)",
					 dc1394_error_get_string(err));
		return FALSE;
	}
	data->free_unlisten = TRUE;

	Fdebug ((stderr, "firewire: set_transmission()\n"));
	if ((err = dc1394_video_set_transmission (data->camera, DC1394_ON)) != DC1394_SUCCESS) {
		AVcap_error ("firewire: Unable to start camera iso transmission (%s)",
					 dc1394_error_get_string(err));
		return FALSE;
	}
	dc1394_get_image_size_from_video_mode (data->camera, mode,
										   (uint32_t*)&dev->width, (uint32_t*)&dev->height);
	if (modes[modeidx].format == FORMAT_SCALABLE_IMAGE_SIZE) {
		dev->f_width = dev->width;
		dev->f_height = dev->height;
	}

	return AVfire_open_common (dev, data, modeidx);

	}
#else
	{

	nodeid_t *camera_nodes = NULL;
	quadlet_t value;
	raw1394handle_t raw_handle = raw1394_new_handle();
	int err, numPorts, camCount, camNum;

	if (raw_handle == NULL) {
		AVcap_error ("firewire: Unable to aquire a raw1394 handle");
		return FALSE;
	}

	numPorts = raw1394_get_port_info (raw_handle, NULL, 0);
	raw1394_destroy_handle (raw_handle);
	Fdebug ((stderr, "firewire: Number of ports = %d\n", numPorts));

	camNum = camNumArg;
	/* Get dc1394 handle to each port */
	for (p = 0; p < numPorts; p++) {
		data->handle = dc1394_create_handle (p);
		if (data->handle == NULL) {
			AVcap_error ("firewire: Unable to aquire a dc1394 handle");
			return FALSE;
		}
		data->free_handle = TRUE;

		/* Get the camera nodes */
		Fdebug ((stderr, "firewire: get_camera_nodes (%d)\n", p));
		camera_nodes = dc1394_get_camera_nodes (data->handle, &camCount, data->debug);

		if (camNum > camCount) {
			camNum -= camCount;
			free (camera_nodes);
		} else {
#define FORMAT_CNT		4
			static const int format_ids[] = {
				FORMAT_VGA_NONCOMPRESSED,
				FORMAT_SVGA_NONCOMPRESSED_1,
				FORMAT_SVGA_NONCOMPRESSED_2,
				FORMAT_SCALABLE_IMAGE_SIZE
			};
			static const int format_nums[] = {0, 1, 2, 7};
			quadlet_t format[FORMAT_CNT];
			int version, f;

			/* Perhaps check for one fitting camera ?
			   -> Continue and free resources instead of returning FALSE */
			data->camera.node = camera_nodes[camNum-1];
			free (camera_nodes);

			if (dc1394_set_operation_mode(data->handle, data->camera.node, opmode) < 0) {
				AVcap_warning ("firewire: Could not enable operation mode %d", opmode);
				return FALSE;
			}

			/* Check for format0/1/2/7 and its available modes */
			Fdebug ((stderr, "firewire: query_supported_formats()\n"));
			if (dc1394_query_supported_formats (data->handle, data->camera.node, &value) < 0) {
				AVcap_error ("firewire: Unable to query supported formats");
				return FALSE;
			}
			for (f=0; f<FORMAT_CNT; f++) {
				if (value & (0x1<<(31-format_nums[f]))) {
					Fdebug ((stderr, "firewire: query_supported_modes (format%d)\n",
							 format_nums[f]));
					if (dc1394_query_supported_modes (data->handle, data->camera.node,
													  format_ids[f], &format[f]) < 0) {
						AVcap_error ("firewire: Unable to query format%d modes",
									 format_nums[f]);
						format[f] = 0;
					}
				} else
					format[f] = 0;
			}

#define MCHECK(m) ((modes[m].format == format_ids[0] && \
					(format[0] & (0x1<<(31+MODE_FORMAT0_MIN-modes[m].mode)))) || \
				   (modes[m].format == format_ids[1] && \
					(format[1] & (0x1<<(31+MODE_FORMAT1_MIN-modes[m].mode)))) || \
				   (modes[m].format == format_ids[2] && \
					(format[2] & (0x1<<(31+MODE_FORMAT2_MIN-modes[m].mode)))) || \
				   (modes[m].format == format_ids[3] && \
					(format[3] & (0x1<<(31+MODE_FORMAT7_MIN-modes[m].mode)))) )

			/* Try to find a suitable format 0/1/2/7 mode,
			   starting with the before initialized mode */
			mode = modeidx;
			/* First try "smaller" modes of the same color space, then bigger */
			while (!MCHECK(mode) && modes[mode].width > modes[mode+1].width)
				mode++;
			while (mode > 0 && !MCHECK(mode) && modes[mode].width < modes[mode-1].width)
				mode--;
			if (!MCHECK(mode)) {
				/* Nothing appropriate found, try all modes */
				mode = 0;
				while (!MCHECK(mode) && modes[mode+1].name)
					mode++;
			}

			if (data->color == -1 || data->debug) {
				int c;

				Fdebug ((stderr, "firewire: Modes supported by the camera:\n"));
				for (i=0; modes[i].name; i++) {
					if (MCHECK(i)) {
						if (data->debug)
							AVfire_print_mode (i);
						if (   modes[i].format == FORMAT_SCALABLE_IMAGE_SIZE
							&& dc1394_query_format7_color_coding (data->handle, data->camera.node,
																  modes[i].mode, &value)
							   == DC1394_SUCCESS) {
							Fdebug ((stderr, "            Color codings:"));
							for (c=0; c<COLOR_CODING_CNT; c++) {
								if ((value & (0x1 << (31-c))) > 0) {
									if (data->debug)
										fprintf (stderr, " %s", colorNames[c]);
									if (i == mode && data->color == -1)
										data->color = c+DC1394_COLOR_CODING_MONO8;
								}
							}
							fprintf (stderr, "\n");
						}
					}
				}
			}

			if (!MCHECK(mode)) {
				AVcap_warning ("firewire: No suitable format0/1/2/7 mode found");
				return FALSE;
			} else {
				if (mode != modeidx)
					AVcap_warning ("firewire: Mode %d (%s) specified, but using available mode %d (%s)",
								   modes[modeidx].mode, modes[modeidx].modestr,
								   modes[mode].mode, modes[mode].modestr);
				data->modeidx = modeidx = mode;
				mode = modes[modeidx].mode;
				if (modes[modeidx].format != FORMAT_SCALABLE_IMAGE_SIZE)
					data->color = modes[modeidx].color;
			}
#undef MCHECK

			/* Setup cameras for capture */
			Fdebug ((stderr, "firewire: setup_capture (num %d mode %d)\n",
					 camNum, mode));
			data->camera.dma_device_file = strdup(device);

			/* Get IIDC version, otherwise libdc handshaking may fail */
			if (dc1394_get_sw_version (data->handle, data->camera.node, &version) == DC1394_SUCCESS)
				Fdebug ((stderr, "firewire: Camera IIDC version %d\n", version));

			if (modes[modeidx].format == FORMAT_SCALABLE_IMAGE_SIZE) {
				if (dc1394_set_format7_color_coding_id (data->handle, data->camera.node,
														mode, data->color) < 0) {
					AVcap_error ("firewire: Unable to set color coding %d", data->color);
					return FALSE;
				}
				if (fps > 0) {
					if (_dc1394_basic_format7_setup (data->handle, data->camera.node, camNum,
													 mode, iso_speed, f7_packetSize,
													 data->f7_x, data->f7_y, data->f7_width, data->f7_height,
													 &data->camera)
						== DC1394_SUCCESS) {
						f7_packetSize = AVfire_fps2packetsize (data, fps, data->camera.frame_width,
															   data->camera.frame_height);
					}
				}
				Fdebug ((stderr, "firewire: setup_format7_capture (ROI %dx%d+%d+%d color %d)\n",
						 data->f7_width, data->f7_height, data->f7_x, data->f7_y, data->color));
				err = dc1394_dma_setup_format7_capture (data->handle, data->camera.node, camNum,
														mode, iso_speed, f7_packetSize,
														data->f7_x, data->f7_y, data->f7_width, data->f7_height,
														NUM_BUFFERS, DROP_FRAMES,
														data->camera.dma_device_file, &data->camera);
				dev->f_width = data->camera.frame_width;
				dev->f_height = data->camera.frame_height;
			} else {
				err = dc1394_dma_setup_capture (data->handle, data->camera.node, camNum,
												modes[modeidx].format, mode,
												iso_speed, AVfire_fps2enum(fps),
												NUM_BUFFERS, DROP_FRAMES,
												data->camera.dma_device_file, &data->camera);
			}
			if (err != DC1394_SUCCESS) {
				AVcap_error ("firewire: Unable to setup camera for DMA grabbing");
				return FALSE;
			}
			data->free_camera = TRUE;

			Fdebug ((stderr, "firewire: start_iso_transmission()\n"));
			if (dc1394_start_iso_transmission (data->handle, data->camera.node)
				!= DC1394_SUCCESS) {
				AVcap_error ("firewire: Unable to start camera iso transmission");
				return FALSE;
			}
			data->free_unlisten = TRUE;
			Fdebug ((stderr, "firewire: fire_open() done\n"));

			dev->width = data->camera.frame_width;
			dev->height = data->camera.frame_height;
			return AVfire_open_common (dev, data, modeidx);
		}
		raw1394_destroy_handle (data->handle);
		data->free_handle = FALSE;
	}
	AVcap_warning ("firewire: Camera number %d not found, only %d camera[s] available",
				   camNumArg, camNumArg-camNum);

	}
#endif

	return FALSE;
}

void AVfire_close (void *capData)
{
	fireData *data = capData;

#ifdef WITH_FIRE2

	if (data->free_unlisten) {
		dc1394error_t ret = dc1394_video_set_transmission (data->camera, DC1394_OFF);
		if (ret != DC1394_SUCCESS)
			AVcap_warning ("firewire: Could not disable ISO transmission (%s)",
						   dc1394_error_get_string(ret));
		ret = dc1394_capture_stop (data->camera);
		if (ret != DC1394_SUCCESS)
			AVcap_warning("firewire: Could not stop capture (%s)",
						  dc1394_error_get_string(ret));
		data->free_unlisten = FALSE;
	}
	if (data->camera) {
		dc1394_camera_free (data->camera);
		data->camera = NULL;
	}
	if (data->d) {
		dc1394_free (data->d);
		data->d = NULL;
	}

#else

	if (data->free_unlisten) {
		dc1394_stop_iso_transmission (data->handle, data->camera.node);
		dc1394_dma_unlisten (data->handle, &data->camera);
		data->free_unlisten = FALSE;
	}
	if (data->free_camera) {
		dc1394_dma_release_camera (data->handle, &data->camera);
		data->free_camera = FALSE;
	}
	if (data->free_handle) {
		raw1394_destroy_handle (data->handle);
		data->free_handle = FALSE;
	}

#endif

	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}
	free (data);
}

iwGrabStatus AVfire_set_property (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_UNSUPPORTED;
#ifdef WITH_FIRE2
	fireData *data = (fireData*)dev->capData;
	dc1394error_t err;

	if (!data->camera) return IW_GRAB_STATUS_NOTOPEN;

	switch (ctrl) {
		case IW_GRAB_GET_PROP:
		case IW_GRAB_GET_PROPVALUES: {
			iwGrabProperty **prop = va_arg (*args, iwGrabProperty**);
			if (prop) {
				*prop = dev->capData->prop;
				if (ctrl == IW_GRAB_GET_PROPVALUES)
					properties_get_values (data);
				ret = IW_GRAB_STATUS_OK;
			} else
				ret = IW_GRAB_STATUS_ARGUMENT;
			break;
		}
		case IW_GRAB_SET_PROP: {
			char *props = va_arg (*args, char*);
			ret = properties_set (data, props);
			break;
		}
		case IW_GRAB_SET_ROI: {
			dc1394switch_t iso_on;
			int x = va_arg(*args, int);
			int y = va_arg(*args, int);
			int w = va_arg(*args, int);
			int h = va_arg(*args, int);
			BOOL sizeChanged = w != data->f7_width || h != data->f7_height;

			if (modes[data->modeidx].format != FORMAT_SCALABLE_IMAGE_SIZE)
				return IW_GRAB_STATUS_UNSUPPORTED;

			if (x != -1) data->f7_x = x;
			if (y != -1) data->f7_y = y;
			if (w != -1) data->f7_width = w;
			if (h != -1) data->f7_height = h;

			if (dc1394_video_get_transmission(data->camera, &iso_on) != DC1394_SUCCESS)
				iso_on = DC1394_ON; /* Disable ISO transmission anyway */
			if (iso_on)
				dc1394_video_set_transmission (data->camera, DC1394_OFF);

			if (AVfire_set_roi (data, DC1394_QUERY_FROM_CAMERA))
				ret = IW_GRAB_STATUS_OK;
			else
				ret = IW_GRAB_STATUS_ERR;

			if (iso_on) {
				if (sizeChanged) {
					if ((err = dc1394_capture_stop(data->camera)) !=  DC1394_SUCCESS) {
						AVcap_error ("firewire: Could not stop capture (%s)", dc1394_error_get_string(err));
						return FALSE;
					}
					if ((err = dc1394_capture_setup(data->camera, NUM_BUFFERS, DC1394_CAPTURE_FLAGS_DEFAULT))
						!= DC1394_SUCCESS) {
						AVcap_error ("firewire: Unable to set up camera (%s)", dc1394_error_get_string(err));
						return FALSE;
					}
				} else
					usleep (50000); /* Really necessary? Might be to avoid desynchronized ISO flow. */

				if ((err = dc1394_video_set_transmission(data->camera, DC1394_ON)) != DC1394_SUCCESS)
					AVcap_error ("firewire: Unable to restart ISO transmission (%s)",
								 dc1394_error_get_string(err));
			}

			dc1394_get_image_size_from_video_mode (data->camera, modes[data->modeidx].mode,
												   (uint32_t*)&dev->width, (uint32_t*)&dev->height);
			dev->f_width = dev->width;
			dev->f_height = dev->height;

			*reinit = TRUE;
			break;
		}
		default:
			break;
	}
#endif
	return ret;
}

int AVfire_capture (void *capData, iwImage *img, struct timeval *time)
{
	fireData *data = capData;
	int w, h;
	guchar *s;
	dc1394color_coding_t color;

#ifdef WITH_FIRE2
	dc1394video_frame_t *frame = NULL, *newFrame;
	dc1394capture_policy_t policy = DC1394_CAPTURE_POLICY_WAIT;
	dc1394error_t err;

	Fdebug ((stderr, "firewire: capture_dequeue()\n"));
	do {
		if ((err = dc1394_capture_dequeue(data->camera, policy, &newFrame)) != DC1394_SUCCESS) {
			AVcap_error ("firewire: Error capturing image (%s)", dc1394_error_get_string(err));
			if (!frame)
				return FALSE;
		}
		if (newFrame) {
			if (frame) dc1394_capture_enqueue (data->camera, frame);
			frame = newFrame;
		}
		policy = DC1394_CAPTURE_POLICY_POLL;
	} while (newFrame);
	s = (guchar *)frame->image;
	w = frame->size[0];
	h = frame->size[1];

	if (data->pg_timestamp) {
		/* Decode timestamp info from raw frame data */
		uint32_t timestamp = (s[0]<<24) | (s[1]<<16) | (s[2]<<8) | (s[3]);
		time_t seconds = (timestamp >> 25) & 0x7F;
		/* Note: seconds overflows after 127 seconds -> keep track of that */
		if (seconds < data->pg_timestamp_last_seconds)
			data->pg_timestamp_tv_sec += seconds+128 - data->pg_timestamp_last_seconds;
		else
			data->pg_timestamp_tv_sec += seconds - data->pg_timestamp_last_seconds;
		data->pg_timestamp_last_seconds = seconds;
		time->tv_sec = data->pg_timestamp_tv_sec;
		time->tv_usec = ((timestamp >> 12) & 0x1FFF)*125 + (timestamp & 0xFFF)*125/3072;
	} else {
		time->tv_sec = frame->timestamp / 1000000;
		time->tv_usec = frame->timestamp % 1000000;
	}

	/* Suppose that the pattern does not change between grabbed images */
	if (data->ip.pattern == IW_IMG_BAYER_AUTO && data->camera->iidc_version >= DC1394_IIDC_VERSION_1_31) {
		switch (frame->color_filter) {
			case DC1394_COLOR_FILTER_RGGB:
				data->ip.pattern = IW_IMG_BAYER_RGGB;
			case DC1394_COLOR_FILTER_GRBG:
				data->ip.pattern = IW_IMG_BAYER_GRBG;
			case DC1394_COLOR_FILTER_GBRG:
				data->ip.pattern = IW_IMG_BAYER_GRBG;
			case DC1394_COLOR_FILTER_BGGR:
				data->ip.pattern = IW_IMG_BAYER_BGGR;
		}
	}
#else
	Fdebug ((stderr, "firewire: single_capture()\n"));
	if (dc1394_dma_single_capture (&data->camera) != DC1394_SUCCESS) {
		AVcap_error ("firewire: Error capturing image");
		return FALSE;
	}

	*time = data->camera.filltime;
	s = (guchar *)data->camera.capture_buffer;
	w = data->camera.frame_width;
	h = data->camera.frame_height;
#endif
	Fdebug ((stderr, "firewire: capture(w %d h %d color %d) done\n",
			 w, h, data->color));

	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U, w * h);
		s = data->ip.buffer;
		color = DC1394_COLOR_CODING_MONO8;
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		color = DC1394_COLOR_CODING_MONO8;
	} else
		color = data->color;

	switch (color) {
		case DC1394_COLOR_CODING_RAW8:
		case DC1394_COLOR_CODING_MONO8: {
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				img->ctab = IW_GRAY;
				memcpy (img->data[0], s, img->width*img->height);
			} else {
				img->ctab = IW_RGB;
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
			}
			break;
		}
		case DC1394_COLOR_CODING_RAW16:
		case DC1394_COLOR_CODING_MONO16:
			AVcap_mono16_ip (&data->ip, img, s, w*h, G_BYTE_ORDER != G_BIG_ENDIAN);
			break;
		case DC1394_COLOR_CODING_RGB8:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case DC1394_COLOR_CODING_RGB16:
			av_color_rgb24_16 (img, s, FALSE, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case DC1394_COLOR_CODING_YUV411:
			av_color_uyyvyy (img, s);
			img->ctab = IW_YUV;
			break;
		case DC1394_COLOR_CODING_YUV422:
			av_color_yuyv (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case DC1394_COLOR_CODING_YUV444:
			av_color_rgb24 (img, s, 1, 0, 2);
			img->ctab = IW_YUV;
			break;
		default:
			AVcap_warning ("firewire: Mode %d not implemented for conversion",
						   modes[data->modeidx].mode);
			break;
	}

#ifdef WITH_FIRE2
	if (dc1394_capture_enqueue(data->camera, frame) != DC1394_SUCCESS) {
#else
	if (dc1394_dma_done_with_buffer (&data->camera) != DC1394_SUCCESS) {
#endif
		AVcap_error ("firewire: Error freeing buffer");
		return FALSE;
	}
	Fdebug ((stderr, "firewire: fire_capture() done\n"));

	return TRUE;
}

#endif	/* WITH_FIRE */
