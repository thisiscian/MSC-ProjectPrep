/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/* PRIVATE HEADER */

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

#ifndef iw_Gimage_i_H
#define iw_Gimage_i_H

#include "Gimage.h"

#define IW_CODEC_444P	IW_FOURCC('4','4','4','P')
#define IW_CODEC_I420	IW_FOURCC('I','4','2','0')
#define IW_CODEC_MJPG	IW_FOURCC('M','J','P','G')
#define IW_CODEC_FFV1	IW_FOURCC('F','F','V','1')
#define IW_CODEC_FMP4	IW_FOURCC('F','M','P','4')

struct _iwImgFileData {
	iwMovie **movie;
	float framerate;
	int quality;
};

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Convert img to YUV444P (IW_IMG_FORMAT_AVI_FFV1), YVU444P
  (IW_IMG_FORMAT_AVI_RAW444), or YUV420P and save in dst
  (which is of size width x height).
*********************************************************************/
void iw_img_convert (const iwImage *img, guchar **dst, int width, int height,
					 iwImgFormat format);

/*********************************************************************
  If '*data->movie' is not open, open it as 'fname' with parameters
  from data. Write img as a new frame to the movie '*data->movie'.
*********************************************************************/
iwImgStatus iw_movie_write (const iwImgFileData *data, iwImgFormat format,
							const iwImage *img, const char *fname);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gimage_i_H */
