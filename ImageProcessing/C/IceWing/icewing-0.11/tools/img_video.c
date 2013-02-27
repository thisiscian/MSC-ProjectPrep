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
#include <stdlib.h>
#include <string.h>
#include "gui/Gtools.h"
#include "tools/tools.h"
#include "tools/img_video.h"

#define IVsize 255
#define IVtype guchar
#define RENAME(n)		n##_guchar
#include "img_video_template.c"
#undef RENAME
#undef IVsize
#undef IVtype

#define IVsize 65535
#define IVtype guint16
#define RENAME(n)		n##_guint16
#include "img_video_template.c"
#undef RENAME
#undef IVsize
#undef IVtype

/*********************************************************************
  Decode stereo image by deinterlacing and saving the single images
  one after the other. length: Size of one decoded image.
  Supports IW_8U and IW_16U.
*********************************************************************/
void iw_img_stereo_decode (guchar *s, guchar *d, iwType type, int length)
{
	switch (type) {
		case IW_8U:
			img_stereo_decode_guchar (s, d, length);
			break;
		case IW_16U:
			img_stereo_decode_guint16 ((guint16*)s, (guint16*)d, length);
			break;
		default:
			iw_error ("Unsupported image type %d", type);
	}
}

/*********************************************************************
  Return in *bayer the enum corresponding to str.
*********************************************************************/
BOOL iw_img_bayer_parse (char *str, iwImgBayer *bayer)
{
	if (!*str || !strcasecmp ("down", str)) {
		*bayer = IW_IMG_BAYER_DOWN;
		return TRUE;
	} else if (!strcasecmp ("neighbor", str)) {
		*bayer = IW_IMG_BAYER_NEIGHBOR;
		return TRUE;
	} else if (!strcasecmp ("bilinear", str)) {
		*bayer = IW_IMG_BAYER_BILINEAR;
		return TRUE;
	} else if (!strcasecmp ("hue", str)) {
		*bayer = IW_IMG_BAYER_HUE;
		return TRUE;
	} else if (!strcasecmp ("edge", str)) {
		*bayer = IW_IMG_BAYER_EDGE;
		return TRUE;
	} else if (!strcasecmp ("ahd", str)) {
		*bayer = IW_IMG_BAYER_AHD;
		return TRUE;
	}
	*bayer = IW_IMG_BAYER_NONE;
	return FALSE;
}

/*********************************************************************
  Return in *pattern the enum corresponding to str.
*********************************************************************/
BOOL iw_img_bayer_pattern_parse (char *str, iwImgBayerPattern *pattern)
{
	if (!strcasecmp ("RGGB", str)) {
		*pattern = IW_IMG_BAYER_RGGB;
		return TRUE;
	} else if (!strcasecmp ("BGGR", str)) {
		*pattern = IW_IMG_BAYER_BGGR;
		return TRUE;
	} else if (!strcasecmp ("GRBG", str)) {
		*pattern = IW_IMG_BAYER_GRBG;
		return TRUE;
	} else if (!strcasecmp ("GBRG", str)) {
		*pattern = IW_IMG_BAYER_GBRG;
		return TRUE;
	} else if (!strcasecmp ("AUTO", str)) {
		*pattern = IW_IMG_BAYER_AUTO;
		return TRUE;
	}
	*pattern = IW_IMG_BAYER_AUTO;
	return FALSE;
}

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
						  iwImgBayerPattern pattern)
{
	if (pattern == IW_IMG_BAYER_AUTO) pattern = IW_IMG_BAYER_RGGB;

	switch (dimg->type) {
		case IW_8U:
			switch (bayer) {
				case IW_IMG_BAYER_DOWN:
					img_bayer_down_guchar (s, dimg, pattern);
					break;
				case IW_IMG_BAYER_NEIGHBOR:
					img_bayer_neighbor_guchar (s, dimg, pattern);
					break;
				case IW_IMG_BAYER_BILINEAR:
					img_bayer_bilinear_guchar (s, dimg, pattern);
					break;
				case IW_IMG_BAYER_HUE:
					img_bayer_hue_guchar (s, dimg, pattern, FALSE);
					break;
				case IW_IMG_BAYER_EDGE:
					img_bayer_hue_guchar (s, dimg, pattern, TRUE);
					break;
				case IW_IMG_BAYER_AHD:
					img_bayer_ahd_guchar (s, dimg, pattern);
					break;
				default:
					iw_warning ("Decompose mode %d not implemented for conversion",
								bayer);
					break;
			}
			break;
		case IW_16U:
			switch (bayer) {
				case IW_IMG_BAYER_DOWN:
					img_bayer_down_guint16 ((guint16*)s, dimg, pattern);
					break;
				case IW_IMG_BAYER_NEIGHBOR:
					img_bayer_neighbor_guint16 ((guint16*)s, dimg, pattern);
					break;
				case IW_IMG_BAYER_BILINEAR:
					img_bayer_bilinear_guint16 ((guint16*)s, dimg, pattern);
					break;
				case IW_IMG_BAYER_HUE:
					img_bayer_hue_guint16 ((guint16*)s, dimg, pattern, FALSE);
					break;
				case IW_IMG_BAYER_EDGE:
					img_bayer_hue_guint16 ((guint16*)s, dimg, pattern, TRUE);
					break;
				case IW_IMG_BAYER_AHD:
					img_bayer_ahd_guint16 ((guint16*)s, dimg, pattern);
					break;
				default:
					iw_warning ("Decompose mode %d not implemented for conversion",
								bayer);
					break;
			}
			break;
		default:
			iw_error ("Unsupported image type %d", dimg->type);
	}
}
