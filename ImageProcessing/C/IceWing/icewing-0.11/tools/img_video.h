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

#ifndef iw_img_video_H
#define iw_img_video_H

#include "gui/Gimage.h"
#include "tools/tools.h"

typedef enum {
	IW_IMG_BAYER_RGGB,
	IW_IMG_BAYER_BGGR,
	IW_IMG_BAYER_GRBG,
	IW_IMG_BAYER_GBRG,
	IW_IMG_BAYER_AUTO /* Auto detect if the driver supports it or IW_IMG_BAYER_RGGB */
} iwImgBayerPattern;

typedef enum {
	IW_IMG_BAYER_NONE,
	IW_IMG_BAYER_DOWN,
	IW_IMG_BAYER_NEIGHBOR,
	IW_IMG_BAYER_BILINEAR,
	IW_IMG_BAYER_HUE,
	IW_IMG_BAYER_EDGE,
	IW_IMG_BAYER_AHD
} iwImgBayer;

#define IW_IMG_BAYER_HELP \
"              methods: down:     downsampling\n" \
"                       neighbor: nearest neighbor interpolation\n" \
"                       bilinear: bilinear interpolation\n" \
"                       hue:      smooth hue transition interpolation\n" \
"                       edge:     edge sensing interpolation\n" \
"                       ahd:      adaptive homogeneity-directed interpolation\n"


/*********************************************************************
  Decode stereo image by deinterlacing and saving the single images
  one after the other. length: Size of one decoded image.
  Supports IW_8U and IW_16U.
*********************************************************************/
void iw_img_stereo_decode (guchar *s, guchar *d, iwType type, int length);

/*********************************************************************
  Return in *bayer the enum corresponding to str.
*********************************************************************/
BOOL iw_img_bayer_parse (char *str, iwImgBayer *bayer);

/*********************************************************************
  Return in *pattern the enum corresponding to str.
*********************************************************************/
BOOL iw_img_bayer_pattern_parse (char *str, iwImgBayerPattern *pattern);

/*********************************************************************
  Bayer decomposition. Supported modes are:
      IW_IMG_BAYER_DOWN    : Image downsampling.
      IW_IMG_BAYER_NEIGHBOR: Nearest neighbor interpolation.
      IW_IMG_BAYER_BILIEAR : Bilinear interpolation.
      IW_IMG_BAYER_HUE     : Smooth hue transition interpolation.
      IW_IMG_BAYER_EDGE    : Edge sensing interpolation.
      IW_IMG_BAYER_AHD     : Adaptive homogeneity-directed interpolation.
  See http://www-ise.stanford.edu/~tingchen/main.htm
  Supports IW_8U and IW_16U.
*********************************************************************/
void iw_img_bayer_decode (guchar *s, iwImage *dimg, iwImgBayer bayer,
						  iwImgBayerPattern pattern);

#endif /* iw_img_video_H */
