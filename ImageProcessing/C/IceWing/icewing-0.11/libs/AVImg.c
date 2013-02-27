/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

#include <stdio.h>
#include "AVImg.h"

#define ERROR	" AVlib support disabled in this iceWing installation !\n"

char *AVDriverHelp (void)
{
	static char *help = "grabber support disabled in this iceWing installation\n";
	return help;
}

void AVClose (VideoDevData *dat)
{
	fprintf (stderr, "! AVClose:"ERROR);
}

VideoDevData *AVOpen (char *options, VideoStandard signalIn,
					  VideoMode vMode, unsigned int subSample)
{
	fprintf (stderr, "! AVInit:"ERROR);
	return NULL;
}

int AVParseDriver (char *arg, VideoStandard *drv)
{
	fprintf (stderr, "! AVParseDriver:"ERROR);
	return 0;
}

void AVReleaseImgSeqData(ImgSeqData *frames)
{
	fprintf (stderr, "! AVReleaseImgSeqData:"ERROR);
}

ImgSeqData *AVInitImgSeqData (VideoDevData *handle,
							  unsigned int nbImg,
							  unsigned int whichPlanes)
{
	fprintf (stderr, "! AVInitImgSeqData:"ERROR);
	return NULL;
}

int AVGetLastImg (VideoDevData *vidDat, ImgSeqData *frames)
{
	fprintf (stderr, "! AVGetLastImg:"ERROR);
	return 1;
}

iwGrabStatus AVcap_set_properties (VideoDevData *dev, va_list *args, BOOL *reinit)
{
	fprintf (stderr, "! AVcap_set_properties:"ERROR);
	return IW_GRAB_STATUS_UNSUPPORTED;
}
