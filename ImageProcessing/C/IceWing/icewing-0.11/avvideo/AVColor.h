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

#ifndef iw_AVColor_H
#define iw_AVColor_H

#include "tools/tools.h"
#include "gui/Gimage.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Convert the data from src to YUV and save it in dst->data.
*********************************************************************/
/* V4L2_PIX_FMT_YUYV / V4L2_PIX_FMT_UYVY: Packed Y(0) U(0,1) Y(1) V(0,1) Y(2) ... */
void av_color_yuyv (iwImage *dst, guchar *src, int y);
/* Packed 16 bit Y(0) U(0,1) Y(1) V(0,1) Y(2) ... */
	void av_color_yuyv_16 (iwImage *dst, guchar *src, int y);
/* Packed U(0,1,2,3) Y(0) Y(1) V(0,1,2,3) Y(2) Y(3) U ...*/
void av_color_uyyvyy (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_YUV420 / V4L2_PIX_FMT_YVU420: Planar
      Y(0) Y(1) ... U(0,1,w,w+1) U(2,3,w+2,w+3) ...
      V(0,1,w,w+1) V(2,3,w+2,w+3) ... */
void av_color_yuv420 (iwImage *dst, guchar *src, int u);
/* V4L2_PIX_FMT_YVU422P: Planar
      Y(0) Y(1) ... U(0,1) U(2,3) ... V(0,1) V(2,3) ... */
void av_color_yuv422P (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_YVU41P: Packed
      U(0,1,2,3) Y(0) V(0,1,2,3) Y(1) U(4,5,6,7) Y(2) U(4,5,6,7) Y(3)
      Y(4) Y(5) Y(6) Y(7) ... */
void av_color_yuv41P (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_YVU411P: Planar
      Y(0) Y(1) ... U(0,1,2,3) U(4,5,6,7) ...
      V(0,1,2,3) V(4,5,6,7) ... */
void av_color_yuv411P (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_YUV410 / V4L2_PIX_FMT_YVU410: Planar
      Y(0) Y(1) ...
      U(0,1,2,3,4, w,w+1,w+2,w+3, 2*w,2*w+1,2*w+2,2*w+3, 3*w,3*w+1,3*w+2,3*w+3) ...
      V(0,1,2,3,4, w,w+1,w+2,w+3, 2*w,2*w+1,2*w+2,2*w+3, 3*w,3*w+1,3*w+2,3*w+3) ... */
void av_color_yuv410 (iwImage *dst, guchar *src, int u);

/*********************************************************************
  Convert the data from src to RGB and save it in dst->data.
*********************************************************************/
/* V4L2_PIX_FMT_RGB15: Packed BGR */
void av_color_bgr15 (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_RGB15X: Packed RGB */
void av_color_bgr15x (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_RGB16: Packed BGR */
void av_color_bgr16 (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_RGB16X: Packed RGB */
void av_color_bgr16x (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_RGB24 / V4L2_PIX_FMT_BGR24: Packed RGB */
void av_color_rgb24 (iwImage *dst, guchar *src, int r, int g, int b);
/*  Packed 10 bit RGB */
void av_color_rgb24_10 (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_RGB32: Packed ARGB */
void av_color_rgb32 (iwImage *dst, guchar *src);
/* V4L2_PIX_FMT_BGR32: Packed BGRA */
void av_color_bgr32 (iwImage *dst, guchar *src);
/*  Packed 16 bit RGB */
void av_color_rgb24_16 (iwImage *dst, guchar *src, BOOL swap, int r, int g, int b);
/* Packed 16 bit RGB, mask the result */
void av_color_rgb24_16_mask (iwImage *dst, guchar *src, guint16 mask, int r, int g, int b);
/* 12 bit per pixel packed, i.e. 3 bytes to store 2 12 bit pixel */
void av_color_mono12 (guchar *dst, guchar *src, int cnt);
/* Planar RGB */
void av_color_rgb24P (iwImage *dst, guchar *src);
/* Planar 16 bit RGB */
void av_color_rgb24P_16 (iwImage *dst, guchar *src);

#ifdef __cplusplus
}
#endif

#endif /* iw_AVColor_H */
