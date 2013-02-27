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

#ifndef iw_sclas_image_H
#define iw_sclas_image_H

#include "gui/Gimage.h"
#include "main/region.h"
#include "tools/tools.h"

#define SCLAS_COMB_NONE		0		/* Combine-Modi */
#define SCLAS_COMB_ADD		1
#define SCLAS_COMB_MUL		2

#define SCLAS_FEAT_ONE_YUV	0		/* Feature vector: 1 Pixel YUV 7 bit */
#define SCLAS_FEAT_ONE_UV	1		/* Feature vector: 1 Pixel UV 8 bit */
#define SCLAS_FEAT_TWO_UV	2		/* Feature vector: 2 Pixel UV 6 bit */

typedef struct {
	uchar *lookup;				/* Lookup table */
	char *name;					/* Filename for lookup table */
	char *datadir;
	BOOL confidence;			/* Lookup table with data for confidence mapping? */
	BOOL twoclass;				/* Lookup table for 2 class problem? */
	int feat_mode;				/* FEAT_..., is set according to size of lookup table */
} sclasLookup;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Calculate difference image of s1 and s2, threshold result with
  thresh, and write median smoothed result (mask size: size*2+1) to d.
  Return approximation for the max value in d:
		thresh >  0 -> 255
		thresh <= 0 -> MAX{abs(127-d[i][j])} (before median is used)
*********************************************************************/
uchar iw_sclas_difference (uchar *s1, uchar *s2, uchar *d, int width, int height,
						   int size, int thresh);

/*********************************************************************
  Color classification of img by using a lookup table. Write median
  smoothed result (mask size: size*2+1) to d.
  pix_cnt=0,1,2 -> calculate average over 1,4,5 pixels
  Lookup table is loaded from look->name if it is NULL.
*********************************************************************/
BOOL iw_sclas_classify (iwImage *img, uchar *d, int size, int pix_cnt, sclasLookup *look);

/*********************************************************************
  Combine difference image and color classification (size
  width x height) according to mode comb_mode. Threshold result with
  thresh, and write it to d.
*********************************************************************/
void iw_sclas_combine (uchar *diff, uchar max_diff, uchar *classif, uchar *d,
					   int width, int height, int thresh, int comb_mode);

/*********************************************************************
  Judge regions based on avgConf and based on the diff regions regDiff
  (are attached to neighboring regions).
  image: region labeled image of size xsize x ysize.

  Regions with a judgement < bew_min -> pixelanzahl = 0
  max_reg_anz>0: Only the max_reg_anz regions with the best judgement
                 will have pixelanzahl > 0.
*********************************************************************/
void iw_sclas_judge_diff (iwRegion *regions, int nregions,
						  iwRegCOMinfo *regDiff, int nregDiff,
						  int xsize, int ysize, gint32 *image,
						  float bew_min, int max_reg_anz);

/*********************************************************************
  Judge the regions 'regions' based on avgConf.

  If judgement is < bew_min -> pixelanzahl = 0
  max_reg_anz>0: Only the max_reg_anz regions with the best judgement
                 will have pixelanzahl > 0.
*********************************************************************/
void iw_sclas_judge (iwRegion *regions, int nregions,
					 float bew_min, int max_reg_anz);

#ifdef __cplusplus
}
#endif

#endif /* iw_sclas_image_H */
