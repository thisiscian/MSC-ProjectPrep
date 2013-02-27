/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Copyright (C) 1999-2009
 * Applied Computer Science, Faculty of Technology, Bielefeld University, Germany
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "AVCapture.h"
#include "AVImg.h"

void AVClose (VideoDevData *dat)
{
	if (dat) {
		AVcap_close (dat);
		free (dat);
	}
}

VideoDevData *AVOpen (char *devOptions, VideoStandard vidSignalIn,
					  VideoMode vMode, unsigned int subSample)
{
	VideoDevData *retData;

	retData = (VideoDevData*)calloc (1, sizeof(VideoDevData));
	if (!retData) {
		fprintf (stderr, "AVOpen: Cannot allocate memory!\n");
		return NULL;
	}

	retData->field = vMode;

	if (!AVcap_open (retData, devOptions, subSample, vidSignalIn)) {
		fprintf (stderr, "AVOpen: Capture init failed!\n");
		free (retData);
		return NULL;
	}
	return retData;
}

/*********************************************************************
  Return a string describing the grabbing command line "-sg".
*********************************************************************/
char *AVDriverHelp (void)
{
	static char help[300];
	int i, pos;

	strcpy (help, "use driver [");
	strcat (help, AVdriver[0].name);
	for (i=1; AVdriver[i].id; i++) {
		strcat (help, "|");
		strcat (help, AVdriver[i].name);
	}

	strcat (help, "] (or [");
	pos = strlen(help);
	help[pos++] = AVdriver[0].id;
	for (i=1; AVdriver[i].id; i++) {
		help[pos++] = '|';
		help[pos++] = AVdriver[i].id;
	}
	help[pos] = '\0';

	strcat (help, "])\n"
			"          with driver specific options ('help' for help output) for grabbing,\n"
			"          default: PAL_S_VIDEO, no options\n");
	return help;
}

/*********************************************************************
  Parse arg and set drv to the appropriate driver.
  Return 1 on sucess and 0 on error.
*********************************************************************/
int AVParseDriver (char *arg, VideoStandard *drv)
{
	char a_up[21];
	int len, i;

	for (len=0; len<20 && arg[len]; len++)
		a_up[len] = toupper((int)arg[len]);
	a_up[len] = '\0';

	for (i=0; AVdriver[i].id; i++) {
		if (!strncmp (a_up, AVdriver[i].name, len)) {
			*drv = AVdriver[i].drv;
			return 1;
		}
	}
	for (i=0; AVdriver[i].id; i++) {
		if (strchr(a_up, AVdriver[i].id)) {
			*drv = AVdriver[i].drv;
			return 1;
		}
	}
	return 0;
}

/*********************************************************************
  Free memory, which was allocated by AVInitImgSeqData().
*********************************************************************/
void AVReleaseImgSeqData (ImgSeqData *frames)
{
	int i;

	if (!frames) return;

	if (frames->imgs) {
		for (i=0; i<frames->nbImg; i++)
			iw_img_free (&frames->imgs[i], IW_IMG_FREE_DATA);
		free (frames->imgs);
	}
	if (frames->captureTime)
		free (frames->captureTime);
	free (frames);
}

static ImgSeqData *errMem (ImgSeqData *frames)
{
	fprintf (stderr, "AVInitImgSeqData: Cannot allocate memory!\n");
	AVReleaseImgSeqData (frames);
	return NULL;
}

/*********************************************************************
  Allocate and initialize memory for nbImg images and return a
  pointer to the structure. whichPlanes is currently ignored.
*********************************************************************/
ImgSeqData *AVInitImgSeqData (VideoDevData *vidDat,
							  unsigned int nbImg, unsigned int whichPlanes)
{
	int i;
	ImgSeqData *frames = NULL;

	frames = (ImgSeqData*)calloc (1, sizeof(ImgSeqData));
	if (!frames)
		return errMem (frames);

	frames->captureTime = calloc (nbImg, sizeof(struct timeval));
	if (!(frames->captureTime))
		return errMem (frames);

	frames->nbImg = nbImg;
	frames->imgs = calloc (nbImg, sizeof(iwImage));
	if (!(frames->imgs))
		return errMem (frames);
	for (i=0; i<nbImg; i++) {
		frames->imgs[i].planes = vidDat->planes;
		frames->imgs[i].type = vidDat->type;
		frames->imgs[i].width = vidDat->width;
		frames->imgs[i].height = vidDat->height;
		if (!iw_img_allocate (&frames->imgs[i]))
			return errMem (frames);
	}

	return frames;
}

/*********************************************************************
  Grab frames->nbImg images and save them in frames.
*********************************************************************/
int AVGetLastImg (VideoDevData *vidDat, ImgSeqData *frames)
{
	int i;

	for (i=0; i<frames->nbImg; i++) {
		if (!AVcap_capture (vidDat, &frames->imgs[i], &frames->captureTime[i])) {
			fprintf (stderr, "AVGetLastImg: Grabbing frame failed!\n");
			return -1;
		}
	}
	return 0;
}
