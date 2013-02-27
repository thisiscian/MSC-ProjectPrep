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

#ifndef iw_AVImg_H
#define iw_AVImg_H

#include "AVCapture.h"

/* Deprecated: Definition for std colour pictures */
#define YUV_COLOR_IMAGE_INHALT	0

/* Image sequence data structure */
typedef struct {
	unsigned int nbImg;
	iwImage *imgs;
	struct timeval *captureTime;
} ImgSeqData;

#ifdef __cplusplus
extern "C" {
#endif

void AVClose (VideoDevData *dat);

VideoDevData *AVOpen (char *options, VideoStandard signalIn,
					  VideoMode vMode, unsigned int subSample);

/*********************************************************************
  Parse arg and set drv to the appropriate driver.
  Return 1 on sucess and 0 on error.
*********************************************************************/
int AVParseDriver (char *arg, VideoStandard *drv);

/*********************************************************************
  Free memory, which was allocated by AVInitImgSeqData().
*********************************************************************/
void AVReleaseImgSeqData (ImgSeqData *frames);

/*********************************************************************
  Allocate and initialize memory for nbImg images and return a
  pointer to the structure. whichPlanes is currently ignored.
*********************************************************************/
ImgSeqData *AVInitImgSeqData (VideoDevData *handle,
							  unsigned int nbImg,
							  unsigned int whichPlanes);

/*********************************************************************
  Grab frames->nbImg images and save them in frames.
*********************************************************************/
int AVGetLastImg (VideoDevData *handle, ImgSeqData *frames);


#ifdef __cplusplus
}
#endif

#endif /* iw_AVImg_H */
