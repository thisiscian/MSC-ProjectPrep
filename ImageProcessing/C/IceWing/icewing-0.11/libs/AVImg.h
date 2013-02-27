/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

#ifndef iw_AVImg_H
#define iw_AVImg_H

#include <stdarg.h>

#include "tools/tools.h"
#include "gui/Gimage.h"
#include "main/grab_prop.h"

#define AV_HAS_DRIVER

#define YUV_COLOR_IMAGE_INHALT	0
#define PAL_WIDTH_DEFAULT		768
#define PAL_HEIGHT_DEFAULT		288

typedef enum {
	PAL_COMPOSITE,
	PAL_S_VIDEO
} VideoStandard;

typedef enum {
	FIELD_BOTH,
	FIELD_EVEN_ONLY,
	FIELD_ODD_ONLY
} VideoMode;

typedef struct {
	int planes;
	iwType type;
	int width, height;
	int fieldFlag;
	int f_width, f_height;
	void *conData;
} VideoDevData;

typedef struct {
	unsigned int nbImg;
	iwImage *imgs;
	struct timeval *captureTime;
} ImgSeqData;

#ifdef __cplusplus
extern "C" {
#endif

char *AVDriverHelp (void);

void AVClose (VideoDevData *dat);

VideoDevData *AVOpen (char *options, VideoStandard signalIn,
					  VideoMode vMode, unsigned int subSample);

int AVParseDriver (char *arg, VideoStandard *drv);

void AVReleaseImgSeqData(ImgSeqData *frames);

ImgSeqData *AVInitImgSeqData (VideoDevData *handle,
							  unsigned int nbImg,
							  unsigned int whichPlanes);

int AVGetLastImg (VideoDevData *vidDat, ImgSeqData *frames);

iwGrabStatus AVcap_set_properties (VideoDevData *dev, va_list *args, BOOL *reinit);

#ifdef __cplusplus
}
#endif

#endif /* iw_AVImg_H */
