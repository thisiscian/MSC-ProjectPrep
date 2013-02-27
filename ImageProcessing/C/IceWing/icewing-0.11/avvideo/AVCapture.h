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

#ifndef iw_AVCapture_H
#define iw_AVCapture_H

#include <stdlib.h>
#include <linux/types.h>
#include <sys/time.h>
#include <stdarg.h>

#include "tools/tools.h"
#include "tools/img_video.h"
#include "gui/Gimage.h"
#include "main/grab_prop.h"

#include <stdint.h>
#define IWPTR_TO_INT64(x) ((gint64)(intptr_t)(x))
#define IWINT64_TO_PTR(x) ((void*)(intptr_t)(x))

#define AV_HAS_DRIVER

#define AV_DEFAULT_WIDTH	768
#define AV_DEFAULT_HEIGHT	576

typedef enum {
	PAL_COMPOSITE,
	PAL_S_VIDEO,
	AV_V4L2,
	AV_FIREWIRE,
	AV_UNICAP,
	AV_MVIMPACT,
	AV_UEYE,
	AV_ARAVIS,
	AV_ICUBE
} VideoStandard;

typedef enum {
	FIELD_BOTH,
	FIELD_EVEN_ONLY,
	FIELD_ODD_ONLY
} VideoMode;

typedef enum {
	AV_STEREO_NONE,
	AV_STEREO_RAW,
	AV_STEREO_DEINTER
} AVcapStereo;

/* Image processing data for the different drivers */
typedef struct AVcapIP {
	iwImgBayer bayer;		/* Bayer decomposition mode */
	iwImgBayerPattern pattern;/* Bayer pattern */
	AVcapStereo stereo;		/* Do stereo decomposition? */
	guchar *buffer;			/* Intermediate buffer for decomposing stereo/bayer images */
} AVcapIP;

#define AVCAP_IP_HELP_D(default) \
"bayer    expect gray image with bayer pattern, use specified method to\n" \
"         decompose it, default: down;\n" \
IW_IMG_BAYER_HELP \
"pattern  bayer pattern to use, default: "default"\n" \
"stereo   expect stereo images, use them unchanged as gray images (raw)\n" \
"         or decompose them by deinterlacing (deinter)\n"
#define AVCAP_IP_HELP AVCAP_IP_HELP_D("RGGB")

typedef struct AVcapData {
	int driver;
	iwGrabProperty *prop;
	int prop_len;
	int prop_cnt;
} AVcapData;

/* Device data structure */
typedef struct {
	int planes;
	iwType type;
	int width, height;
	VideoMode field;
	int f_width, f_height;		/* Width/Height without downsampling */
	AVcapData *capData;
} VideoDevData;

typedef struct {
	char id;
	char *name;
	VideoStandard drv;

	int (*open) (VideoDevData *dev, char *devOptions,
				 int subSample, VideoStandard vidSignal);
	void (*close) (void *capData);
	int (*capture) (void *capData, iwImage *img, struct timeval *time);
	iwGrabStatus (*setProp) (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit);
} AVcapDriver;
extern AVcapDriver AVdriver[];

/*********************************************************************
  If necessary, byte swap and bayer decode the data from src (of
  length srccnt), and save them in dst->data[0].
*********************************************************************/
void AVcap_mono16_ip (AVcapIP *ip, iwImage *dst, guchar *src, int srccnt, BOOL swap);

/*********************************************************************
  Parse "stereo", "bayer", and "pattern" options from devOptions.
  driver: Name for error messages
  Returns FALSE if a parsing error occured.
*********************************************************************/
BOOL AVcap_parse_ip (char *driver, char *devOptions, AVcapIP *ip);

/*********************************************************************
  Allocate ip->buffer if needed and adapt dev->{f_}[width|height]
  according to ip.
  needBuf: If IW_16U and bayerDecoding allocate ip->buffer unconditionally
  driver: Name for error messages
  Returns FALSE on error.
*********************************************************************/
BOOL AVcap_init_ip (char *driver, VideoDevData *dev, AVcapIP *ip, BOOL needBuf);

/*********************************************************************
  Add a property of the specified type to cap->prop.
*********************************************************************/
iwGrabProperty *AVCap_prop_add_category (AVcapData *cap, const char *name, gint64 priv);
iwGrabProperty *AVCap_prop_add_command (AVcapData *cap, const char *name, gint64 priv);
iwGrabProperty *AVCap_prop_add_bool (AVcapData *cap, const char *name, BOOL val, BOOL def, gint64 priv);
iwGrabProperty *AVCap_prop_add_int (AVcapData *cap, const char *name,
									gint64 val, gint64 min, gint64 max, gint64 def, gint64 priv);
iwGrabProperty *AVCap_prop_add_double (AVcapData *cap, const char *name,
									   double val, double min, double max, double def, gint64 priv);
iwGrabProperty *AVCap_prop_add_enum (AVcapData *cap, const char *name, int val, int def, gint64 priv);
iwGrabProperty *AVCap_prop_append_enum (AVcapData *cap, const char *enumName, BOOL val, gint64 priv);
iwGrabProperty *AVCap_prop_add_string (AVcapData *cap, const char *name,
									   const char *val, const char *def, gint64 priv);

/*********************************************************************
  If name is a number of an existing property return that number,
  otherwise return the index of the property with the matching name,
  or -1 if no property matches.
*********************************************************************/
int AVCap_prop_lookup (AVcapData *cap, const char *name);

/*********************************************************************
  Parse 'val' according the type from 'prop' and store the result in
  'dest'. 'dest may be identical to 'prop'. Return TRUE if val could
  be parsed and is valid according to 'prop'.
*********************************************************************/
BOOL AVCap_prop_parse (iwGrabProperty *prop, iwGrabProperty *dest, const char *val);

/*********************************************************************
  Print all properties from data->prop to stderr.
  driver: Prefix string for messages
*********************************************************************/
void AVCap_prop_print (char *driver, AVcapData *data);

int AVcap_open (VideoDevData *dev, char *devOptions,
				int subSample, VideoStandard vidSignal);
void AVcap_close (VideoDevData *dev);

/*********************************************************************
  Apply all properties from args, which is a list of iwGrabControl
  and its arguments. args must be finished with IW_GRAB_LAST.
  reinit==TRUE: Image format related settings from VideoDevData
                have changed so that memory for the grabbed images
                must be newly allocated.
*********************************************************************/
iwGrabStatus AVcap_set_properties (VideoDevData *dev, va_list *args, BOOL *reinit);

int AVcap_capture (VideoDevData *dev, iwImage *img, struct timeval *time);

#endif /* iw_AVCapture_H */
