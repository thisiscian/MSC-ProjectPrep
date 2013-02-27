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

#ifndef iw_region_H
#define iw_region_H

#include <gtk/gtk.h>
#include "tools/tools.h"
#include "gui/Gimage.h"
#include "main/sfb_iw.h"

typedef struct _regCalcData iwRegCalcData;

typedef enum {
	IW_REG_INCLUSION	= 1 << 0,	/* Calculate inclusion */
	IW_REG_NO_ZERO		= 1 << 1	/* Ignore regions with a label of 0 */
} iwRegMode;

typedef enum {
	IW_REG_THIN_OFF	= 1 << 0,	/* Don't perform any thinning */
	IW_REG_THIN_ABS	= 1 << 1,	/* Only leave every nth point */
	IW_REG_THIN_DIST= 1 << 2	/* Only leave points which are farther away than the threshold */
} iwRegThinning;

typedef struct {
	int pixelcount;			/* Number of pixels */
	int color;				/* Region color */
	int com_x, com_y;		/* COM of the region */
	int summe_x, summe_y;	/* Coordinate sum for the COM calculation */
} iwRegCOMinfo;

typedef struct {
	Region_t r;
	int id;					/* On output this is put in the HypothesenKopf_t */
	int alter;				/* Number of 40ms-slides the region is tracked */
	int labindex;			/* Index from regionlab for region r */
	float judgement;		/* Judgement from the classifier */
	float judge_kalman;		/* Judgement from the kalman filter (time and distance) */
	float judge_motion;		/* Judgement from motion information */
	float avgConf;
	void *data;
} iwRegion;

#include "plugin_comm.h"

typedef void* (*iwRegDataFunc) (plugDefinition *plug, iwRegion *region, int mode);

#define IW_REG_DATA_SET		0
#define IW_REG_DATA_CONVERT	1
#define IW_REG_DATA_IDENT	"regionData"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Do a region labeling of the image color (size: xsize x ysize) and
  write the result to region.
  Return: Number of regions.
*********************************************************************/
int iw_reg_label (int xsize, int ysize, const uchar *color, gint32 *region);

/*********************************************************************
  Do a region labeling of the image color (size: xsize x ysize) and
  write the result to region. Calculate pixel count, color, and COM
  of the regions.
  pixel count < minPixelCount -> Pixel count of the region = 0
  Return value is a pointer to a static variable!
*********************************************************************/
iwRegCOMinfo *iw_reg_label_with_calc (int xsize, int ysize, const uchar *color,
									  gint32 *region, int *nregions, int minPixelCount);

/*********************************************************************
  Maintain the struct holding settings for the region calculation.
  The different settings are:
  color   : Classified image (supported: 1 plane, IW_8U, IW_16U, and
            IW_32S), used to get a color index
  orig_img: Original color image, used to get the average region color
  confimg : Confidence mapped image from the color classifier.
			!= NULL: regionen->avgConf = average confidence value of
                                         the region
  minPixelCount: Minimal size of calculated regions
  iwRegMode|iwRegThinning: See the flags above.
  maxdist : Distance value for the modes IW_REG_THIN_ABS and
            IW_REG_THIN_DIST.
*********************************************************************/
iwRegCalcData *iw_reg_data_create (void);
void iw_reg_data_free			(iwRegCalcData *data);
void iw_reg_data_set_images		(iwRegCalcData *data,
								 iwImage *color, uchar **orig_img, uchar *confimg);
void iw_reg_data_set_minregion	(iwRegCalcData *data, int minPixelCount);
void iw_reg_data_set_mode		(iwRegCalcData *data, iwRegMode mode);
void iw_reg_data_set_thinning	(iwRegCalcData *data, iwRegThinning mode, float maxdist);

/*********************************************************************
  Calculate regions from the image "image".
  xlen, ylen: Size of image
  image     : Region labeld image
  num_reg   : in  : Number of labeld regions
              out : Number of calculated regions
  data      : Additional settings for the region calculation.
*********************************************************************/
iwRegion *iw_reg_calc_data (int xlen, int ylen, gint32 *image, int *num_reg,
							iwRegCalcData *data);
iwRegion *iw_reg_calc (int xlen, int ylen, uchar *color,
					   gint32 *image, uchar **orig_img, uchar *confimg,
					   int *num_reg, int doEinschluss, int minPixelCount);
iwRegion *iw_reg_calc_img (int xlen, int ylen, iwImage *color,
						   gint32 *image, uchar **orig_img, uchar *confimg,
						   int *num_reg, iwRegMode mode, int minPixelCount);

/*********************************************************************
  Stretch region reg by scale pixels in all directions and
  restrict the region to a size of width x height.
*********************************************************************/
void iw_reg_stretch (int width, int height, iwRegion *reg, int scale);

/*********************************************************************
  Return bounding box of polygon p.
*********************************************************************/
void iw_reg_boundingbox (const Polygon_t *p, int *x1, int *y1, int *x2, int *y2);

/*********************************************************************
  Upsample regions by factor (upw,uph).
*********************************************************************/
void iw_reg_upsample (int nregions, iwRegion *regions, float upw, float uph);

/*********************************************************************
  Free regions allocated with iw_reg_copy(). Attention: This works
  only correctly for regions allocated with iw_reg_copy().
*********************************************************************/
void iw_reg_free (int nregions, iwRegion *regions);

/*********************************************************************
  Return a copy of regions. Attention: Single regions can't be freed
  as all points and polygons are malloced en bloc.
*********************************************************************/
iwRegion *iw_reg_copy (int nregions, const iwRegion *regions);

#ifdef __cplusplus
}
#endif

#endif /* iw_region_H */
