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

#ifndef iw_image_H
#define iw_image_H

#include "gui/Gimage.h"
#include "gui/Grender.h"
#include "tools/tools.h"

typedef void (*imgLineFunc) (iwImage *img, uchar *y, uchar *u, uchar *v,
							 int cnt, void *data);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return a pointer to an internal intermediate buffer. If the buffer
  is smaller than size bytes, the buffer is reallocated.
  Calls to iw_img_get_buffer() can be nested. iw_img_release_buffer()
  must be called if the buffer is not needed any more.
*********************************************************************/
void* iw_img_get_buffer (int size);

/*********************************************************************
  Release the buffer requested with iw_img_get_buffer().
*********************************************************************/
void iw_img_release_buffer (void);

/*********************************************************************
  Output color of image s at position (x,y) to stderr. If s has 3
  planes result of prev_yuvToRgbVis() is given additionally.
*********************************************************************/
void iw_img_col_debug (const char *str, const iwImage *s, int x, int y);

/*********************************************************************
  Downsample source by factor (downw,downh) and put it to dest.
  dest->data is freed and newly allocated if the size or the type
  of source has changed. Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_downsample (const iwImage *source, iwImage *dest,
						int downw, int downh);

/*********************************************************************
  Resize image srcPR to size of image dstPR and save it in dstPR.
  interpolate==TRUE: Use linear interpolation.
*********************************************************************/
void iw_img_resize (const iwImage *srcPR, iwImage *destPR, BOOL interpolate);

/*********************************************************************
  Resize image src (size: src_w x src_h with 'planes' planes)
  to size dst_w x dst_h and save it in dst.
  interpolate==TRUE: Use linear interpolation.
*********************************************************************/
void iw_img_resize_raw (const uchar** src, uchar** dst,
						int src_w, int src_h, int dst_w, int dst_h,
						int planes, BOOL interpolate);

/*********************************************************************
  Set border of size border in image src to 0.
*********************************************************************/
void iw_img_border (uchar *src, int width, int height, int border);

/*********************************************************************
  Copy border of width border from src to dest.
*********************************************************************/
void iw_img_copy_border (const uchar *src, uchar *dest,
						 int width, int height, int border);

/*********************************************************************
  Put median smoothed image (mask size: size*2+1) from s to d.
*********************************************************************/
void iw_img_median (const uchar *s, uchar *d,
					int width, int height, int size);

/*********************************************************************
  Put median smoothed image (mask size: size*2+1) from s to d.
  s is only tested on >0 (-> BLACK/WHITE).
*********************************************************************/
void iw_img_medianBW (const uchar *s, uchar *d,
					  int width, int height, int size);

/*********************************************************************
  Put smoothed image (mask size: size*2+1, center of mask gets color
  which occurs most frequently) from s to d.
*********************************************************************/
void iw_img_max (const uchar *s, uchar *d,
				 int width, int height, int size);

/*********************************************************************
  Histogram equalize src and put it to dest.
*********************************************************************/
void iw_img_histeq (const uchar *src, uchar *dest, int width, int height);

/*********************************************************************
  Contrast autostretch src and put it to dest.
  Ignore the first min_limit % and the last max_limit % pixels.
*********************************************************************/
void iw_img_cstretch (const uchar *src, uchar *dest, int width, int height,
					  int min_limit, int max_limit);

/*********************************************************************
  Apply a sobel filter in x and y direction to src and save |dx|+|dy|
  to dest. If thresh>0 threshold dest.
  A border of one pixel is copied unchanged.
*********************************************************************/
void iw_img_sobel (const uchar *src, uchar *dest,
				   int width, int height, int thresh);

/*********************************************************************
  Perform RGB to YUV conversion with intervall reduction.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_rgbToYuvVis (iwImage *img);

/*********************************************************************
  Perform YUV to RGB conversion with intervall expansion.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_yuvToRgbVis (iwImage *img);

/*********************************************************************
  Convert an interleaved image 'src' to a planed one and vice versa.
  Save the result in the newly allocated image 'dst'. If dst is NULL,
  additionally allocate a new image structure. Returns dst.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
iwImage *iw_img_convert_layout (const iwImage *src, iwImage *dst);

/*********************************************************************
  count: number of points , ptsIn: the points
  invers==FALSE: Fill the polygon defined by the points ptsIn.
  invers==TRUE: Fill everything except the polygon.
  fkt!=NULL: fkt() is called (instead of changing the image directly.
  fkt==NULL: The polygon (or the inverse) is filled,
                data!=NULL: data defines the color to use.
*********************************************************************/
BOOL iw_img_fillPoly (iwImage *img, int count, const prevDataPoint *ptsIn,
					  BOOL invers, imgLineFunc fkt, void *data);
BOOL iw_img_fillPoly_raw (uchar **s, int width, int height, int planes,
						  const Polygon_t *p, BOOL invers,
						  imgLineFunc fkt, void *data);

#ifdef __cplusplus
}
#endif

#endif /* iw_image_H */
