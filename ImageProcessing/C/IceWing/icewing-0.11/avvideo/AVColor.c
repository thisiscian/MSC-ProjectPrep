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

#include <string.h>
#include "AVColor.h"

/*********************************************************************
  V4L2_PIX_FMT_YUYV / V4L2_PIX_FMT_UYVY: Packed Y(0) U(0,1) Y(1) V(0,1) Y(2) ...
*********************************************************************/
void av_color_yuyv (iwImage *dst, guchar *src, int y)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	for (i=0; i<cnt; i+=2) {
		pos = i*2;

		data[0][i] = src[pos+y];
		data[1][i] = src[pos+1-y];
		data[2][i] = src[pos+3-y];

		data[0][i+1] = src[pos+2+y];
		data[1][i+1] = src[pos+1-y];
		data[2][i+1] = src[pos+3-y];
	}
}

/*********************************************************************
  Packed 16 bit Y(0) U(0,1) Y(1) V(0,1) Y(2) ...
*********************************************************************/
void av_color_yuyv_16 (iwImage *dst, guchar *src, int y)
{
	guint16 **data = (guint16 **)dst->data;
	guint16 *s = (guint16 *)src;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	for (i=0; i<cnt; i+=2) {
		pos = i*2;

		data[0][i] = s[pos+y];
		data[1][i] = s[pos+1-y];
		data[2][i] = s[pos+3-y];

		data[0][i+1] = s[pos+2+y];
		data[1][i+1] = s[pos+1-y];
		data[2][i+1] = s[pos+3-y];
	}
}

/*********************************************************************
  Packed U(0,1,2,3) Y(0) Y(1) V(0,1,2,3) Y(2) Y(3) U ...
*********************************************************************/
void av_color_uyyvyy (iwImage *dst, guchar *src)
{
	guchar **data = dst->data, value;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i+=4) {
		data[0][i] = src[pos+1];
		data[0][i+1] = src[pos+2];
		data[0][i+2] = src[pos+4];
		data[0][i+3] = src[pos+5];

		value = src[pos];
		data[1][i] = value;
		data[1][i+1] = value;
		data[1][i+2] = value;
		data[1][i+3] = value;

		value = src[pos+3];
		data[2][i] = value;
		data[2][i+1] = value;
		data[2][i+2] = value;
		data[2][i+3] = value;

		pos += 6;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_YUV420 / V4L2_PIX_FMT_YVU420: Planar
      Y(0) Y(1) ... U(0,1,w,w+1) U(2,3,w+2,w+3) ...
      V(0,1,w,w+1) V(2,3,w+2,w+3) ...
*********************************************************************/
void av_color_yuv420 (iwImage *dst, guchar *src, int u)
{
	guchar *plane, value;
	int i, w, cnt, pos;

	w = dst->width;
	cnt = w*dst->height;

	memcpy (dst->data[0], src, cnt * sizeof(guchar));
	src += cnt;

	cnt = (w/2)*(dst->height/2);
	plane = dst->data[u];
	for (i=0; i<cnt; i++) {
		value = src[i];
		pos = (i/(w/2))*w + 2*i;
		plane[pos] = value;
		plane[pos+1] = value;
		plane[pos+w] = value;
		plane[pos+w+1] = value;
	}
	src += cnt;

	plane = dst->data[3-u];
	for (i=0; i<cnt; i++) {
		value = src[i];
		pos = (i/(w/2))*w + 2*i;
		plane[pos] = value;
		plane[pos+1] = value;
		plane[pos+w] = value;
		plane[pos+w+1] = value;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_YVU422P: Planar
      Y(0) Y(1) ... U(0,1) U(2,3) ... V(0,1) V(2,3) ...
*********************************************************************/
void av_color_yuv422P (iwImage *dst, guchar *src)
{
	guchar **data = dst->data, value;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	memcpy (data[0], src, cnt * sizeof(guchar));
	src += cnt;

	cnt = (dst->width/2)*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		value = src[i];
		data[1][pos++] = value;
		data[1][pos++] = value;
	}
	src += cnt;

	pos = 0;
	for (i=0; i<cnt; i++) {
		value = src[i];
		data[2][pos++] = value;
		data[2][pos++] = value;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_YVU41P: Packed
      U(0,1,2,3) Y(0) V(0,1,2,3) Y(1) U(4,5,6,7) Y(2) U(4,5,6,7) Y(3)
      Y(4) Y(5) Y(6) Y(7) ...
*********************************************************************/
void av_color_yuv41P (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i+=8) {
		data[0][i] = src[pos+1];
		data[0][i+1] = src[pos+3];
		data[0][i+2] = src[pos+5];
		data[0][i+3] = src[pos+7];
		data[0][i+4] = src[pos+8];
		data[0][i+5] = src[pos+9];
		data[0][i+6] = src[pos+10];
		data[0][i+7] = src[pos+11];

		data[1][i] = src[pos];
		data[1][i+1] = src[pos];
		data[1][i+2] = src[pos];
		data[1][i+3] = src[pos];
		data[1][i+4] = src[pos+4];
		data[1][i+5] = src[pos+4];
		data[1][i+6] = src[pos+4];
		data[1][i+7] = src[pos+4];

		data[2][i] = src[pos+2];
		data[2][i+1] = src[pos+2];
		data[2][i+2] = src[pos+2];
		data[2][i+3] = src[pos+2];
		data[2][i+4] = src[pos+6];
		data[2][i+5] = src[pos+6];
		data[2][i+6] = src[pos+6];
		data[2][i+7] = src[pos+6];

		pos += 12;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_YVU411P: Planar
      Y(0) Y(1) ... U(0,1,2,3) U(4,5,6,7) ...
      V(0,1,2,3) V(4,5,6,7) ...
*********************************************************************/
void av_color_yuv411P (iwImage *dst, guchar *src)
{
	guchar **data = dst->data, value;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	memcpy (data[0], src, cnt * sizeof(guchar));
	src += cnt;

	cnt = (dst->width/4)*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		value = src[i];
		data[1][pos++] = value;
		data[1][pos++] = value;
		data[1][pos++] = value;
		data[1][pos++] = value;
	}
	src += cnt;

	pos = 0;
	for (i=0; i<cnt; i++) {
		value = src[i];
		data[2][pos++] = value;
		data[2][pos++] = value;
		data[2][pos++] = value;
		data[2][pos++] = value;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_YUV410 / V4L2_PIX_FMT_YVU410: Planar
      Y(0) Y(1) ...
      U(0,1,2,3,4, w,w+1,w+2,w+3, 2*w,2*w+1,2*w+2,2*w+3, 3*w,3*w+1,3*w+2,3*w+3) ...
      V(0,1,2,3,4, w,w+1,w+2,w+3, 2*w,2*w+1,2*w+2,2*w+3, 3*w,3*w+1,3*w+2,3*w+3) ...
*********************************************************************/
void av_color_yuv410 (iwImage *dst, guchar *src, int u)
{
	guchar *plane, value;
	int i, w, cnt, pos;

	w = dst->width;
	cnt = w*dst->height;

	memcpy (dst->data[0], src, cnt * sizeof(guchar));
	src += cnt;

	cnt = (w/4)*(dst->height/4);
	plane = dst->data[u];
	for (i=0; i<cnt; i++) {
		value = src[i];
		pos = 3*(i/(w/4))*w + 4*i;
		plane[pos] = value;
		plane[pos+1] = value;
		plane[pos+2] = value;
		plane[pos+3] = value;
		plane[pos+w] = value;
		plane[pos+w+1] = value;
		plane[pos+w+2] = value;
		plane[pos+w+3] = value;
		plane[pos+2*w] = value;
		plane[pos+2*w+1] = value;
		plane[pos+2*w+2] = value;
		plane[pos+2*w+3] = value;
		plane[pos+3*w] = value;
		plane[pos+3*w+1] = value;
		plane[pos+3*w+2] = value;
		plane[pos+3*w+3] = value;
	}
	src += cnt;

	plane = dst->data[3-u];
	for (i=0; i<cnt; i++) {
		value = src[i];
		pos = 3*(i/(w/4))*w + 4*i;
		plane[pos] = value;
		plane[pos+1] = value;
		plane[pos+2] = value;
		plane[pos+3] = value;
		plane[pos+w] = value;
		plane[pos+w+1] = value;
		plane[pos+w+2] = value;
		plane[pos+w+3] = value;
		plane[pos+2*w] = value;
		plane[pos+2*w+1] = value;
		plane[pos+2*w+2] = value;
		plane[pos+2*w+3] = value;
		plane[pos+3*w] = value;
		plane[pos+3*w+1] = value;
		plane[pos+3*w+2] = value;
		plane[pos+3*w+3] = value;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB15: Packed BGR
*********************************************************************/
void av_color_bgr15 (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[0][i] = (src[pos+1] & 0x7C) << 1;
		data[1][i] = ((src[pos] >> 5) + ((src[pos+1] & 0x3) << 3)) << 3;
		data[2][i] = src[pos] << 3;
		pos += 2;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB15X: Packed RGB
*********************************************************************/
void av_color_bgr15x (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[0][i] = (src[pos] & 0x7C) << 1;
		data[1][i] = ((src[pos+1] >> 5) + ((src[pos] & 0x3) << 3)) << 3;
		data[2][i] = (src[pos+1] & 0x1F) << 3;
		pos += 2;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB16: Packed BGR
*********************************************************************/
void av_color_bgr16 (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[0][i] = src[pos+1] & 0xF8;
		data[1][i] = ((src[pos] >> 5) + ((src[pos+1] & 0x7) << 3)) << 2;
		data[2][i] = src[pos] << 3;
		pos += 2;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB16X: Packed RGB
*********************************************************************/
void av_color_bgr16x (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[0][i] = src[pos] & 0xF8;
		data[1][i] = ((src[pos+1] >> 5) + ((src[pos] & 0x7) << 3)) << 2;
		data[2][i] = (src[pos+1] & 0x1F) << 3;
		pos += 2;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB24 / V4L2_PIX_FMT_BGR24: Packed RGB
*********************************************************************/
void av_color_rgb24 (iwImage *dst, guchar *src, int r, int g, int b)
{
	guchar *rPlane = dst->data[r];
	guchar *gPlane = dst->data[g];
	guchar *bPlane = dst->data[b];
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		rPlane[i] = src[pos++];
		gPlane[i] = src[pos++];
		bPlane[i] = src[pos++];
	}
}

/*********************************************************************
  Packed 10 bit RGB
*********************************************************************/
void av_color_rgb24_10 (iwImage *dst, guchar *src)
{
	guint16 **data = (guint16 **)dst->data;
	guint32 *s = (guint32 *)src;
	int i, cnt;

	cnt = dst->width*dst->height;
	for (i=0; i<cnt; i++) {
		guint32 v = s[i];
		data[0][i] = v       & 0x3FF;
		data[1][i] = (v>>10) & 0x3FF;
		data[2][i] = (v>>20) & 0x3FF;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_RGB32: Packed ARGB
*********************************************************************/
void av_color_rgb32 (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[0][i] = src[pos+1];
		data[1][i] = src[pos+2];
		data[2][i] = src[pos+3];
		pos += 4;
	}
}

/*********************************************************************
  V4L2_PIX_FMT_BGR32: Packed BGRA
*********************************************************************/
void av_color_bgr32 (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		data[2][i] = src[pos];
		data[1][i] = src[pos+1];
		data[0][i] = src[pos+2];
		pos += 4;
	}
}

/*********************************************************************
  Packed 16 bit RGB
*********************************************************************/
void av_color_rgb24_16 (iwImage *dst, guchar *src, BOOL swap, int r, int g, int b)
{
	guint16 *rPlane = (guint16 *)dst->data[r];
	guint16 *gPlane = (guint16 *)dst->data[g];
	guint16 *bPlane = (guint16 *)dst->data[b];
	guint16 *s = (guint16 *)src;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	if (swap) {
		for (i=0; i<cnt; i++) {
			rPlane[i] = GUINT16_SWAP_LE_BE(s[pos]);
			pos++;
			gPlane[i] = GUINT16_SWAP_LE_BE(s[pos]);
			pos++;
			bPlane[i] = GUINT16_SWAP_LE_BE(s[pos]);
			pos++;
		}
	} else {
		for (i=0; i<cnt; i++) {
			rPlane[i] = s[pos++];
			gPlane[i] = s[pos++];
			bPlane[i] = s[pos++];
		}
	}
}

/*********************************************************************
  Packed 16 bit RGB, mask the result
*********************************************************************/
void av_color_rgb24_16_mask (iwImage *dst, guchar *src, guint16 mask, int r, int g, int b)
{
	guint16 *rPlane = (guint16 *)dst->data[r];
	guint16 *gPlane = (guint16 *)dst->data[g];
	guint16 *bPlane = (guint16 *)dst->data[b];
	guint16 *s = (guint16 *)src;
	int i, cnt, pos;

	cnt = dst->width*dst->height;
	pos = 0;
	for (i=0; i<cnt; i++) {
		rPlane[i] = s[pos++] & mask;
		gPlane[i] = s[pos++] & mask;
		bPlane[i] = s[pos++] & mask;
	}
}

/*********************************************************************
  Planar RGB
*********************************************************************/
void av_color_rgb24P (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int cnt = dst->width*dst->height*sizeof(guint16);

	memcpy (data[0], src,       cnt);
	memcpy (data[1], src+cnt,   cnt);
	memcpy (data[2], src+cnt*2, cnt);
}

/*********************************************************************
  Planar 16 bit RGB
*********************************************************************/
void av_color_rgb24P_16 (iwImage *dst, guchar *src)
{
	guchar **data = dst->data;
	int cnt = dst->width*dst->height*sizeof(guchar);

	memcpy (data[0], src,       cnt);
	memcpy (data[1], src+cnt,   cnt);
	memcpy (data[2], src+cnt*2, cnt);
}

/*********************************************************************
  12 bit per pixel packed, i.e. 3 bytes to store 2 12 bit pixel.
*********************************************************************/
void av_color_mono12 (guchar *dst, guchar *src, int cnt)
{
	int i;
	for (i=0; i<cnt; i+=2) {
		((guint16*)dst)[i] = (src[0] << 4) + (src[1] >> 4);
		((guint16*)dst)[i+1] = (src[1] & 0xF) + (src[2] << 4);
		src += 3;
	}
}
