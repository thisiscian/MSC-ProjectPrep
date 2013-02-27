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


/*********************************************************************
  The code for drawing ellipses and circles is based on code from
  OpenCV Version 0.9.5, which is released under the following
  copyright:

                         Intel License Agreement
                 For Open Source Computer Vision Library

  Copyright (C) 2000, Intel Corporation, all rights reserved.
  Third party copyrights are property of their respective owners.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

    * Redistribution's of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistribution's in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * The name of Intel Corporation may not be used to endorse or promote products
      derived from this software without specific prior written permission.

  This software is provided by the copyright holders and contributors "as is" and
  any express or implied warranties, including, but not limited to, the implied
  warranties of merchantability and fitness for a particular purpose are disclaimed.
  In no event shall the Intel Corporation or contributors be liable for any direct,
  indirect, incidental, special, exemplary, or consequential damages
  (including, but not limited to, procurement of substitute goods or services;
  loss of use, data, or profits; or business interruption) however caused
  and on any theory of liability, whether in contract, strict liability,
  or tort (including negligence or otherwise) arising in any way out of
  the use of this software, even if advised of the possibility of such damage.
*********************************************************************/

#include "config.h"
#include <gtk/gtk.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "Grender.h"
#include "Gpolygon.h"
#include "Gpreview_i.h"
#include "Goptions.h"
#include "Ggui_i.h"
#include "GrenderImg.h"
#include "GrenderText.h"
#include "Gtools.h"

#define DISP_NONE		0
#define DISP_COM		1

#define COL_COPY(s,d) \
	(d).ctab = (s)->ctab; \
	(d).r = (s)->r; (d).g = (s)->g; (d).b = (s)->b;

typedef struct {
	int display;
	gboolean show_points;
	gboolean white;
} prevOptsRegion;

static prevBuffer *b_pBuf;
static gint b_red, b_green, b_blue, b_dispMode;
static float b_zoom = 1;

/*********************************************************************
  Set color and dest buffer for the primitive drawing functions
  prev_drawPoint() to prev_drawString(). If a color component is set
  to -1, the corresponding channel is not changed.
  prev_drawInitFull(): Return zoom factor.
*********************************************************************/
void prev_drawInit (prevBuffer *buf, gint r, gint g, gint b)
{
	b_pBuf = buf;
	b_red = r; b_green = g; b_blue = b;
}
float prev_drawInitFull (prevBuffer *buf, gint r, gint g, gint b, gint disp_mode)
{
	b_pBuf = buf;
	b_red = r; b_green = g; b_blue = b;
	b_dispMode = disp_mode;
	b_zoom = prev_get_zoom (b_pBuf);
	return b_zoom;
}

void prev_colConvert (iwColtab ctab, gint r, gint g, gint b,
					  gint *rd, gint *gd, gint *bd)
{
	if (ctab == IW_YUV) {
		guchar R,G,B;
		prev_yuvToRgbVis (r, g, b, &R, &G, &B);
		*rd = R; *gd = G; *bd = B;
	} else if (ctab == IW_RGB) {
		*rd = r;
		*gd = g;
		*bd = b;
	} else if (ctab == IW_BGR) {
		*rd = b;
		*gd = g;
		*bd = r;
	} else if (ctab == IW_BW) {
		*rd = *gd = *bd = r;
	} else if (ctab > IW_COLFORMAT_MAX && r>=0 && r < IW_CTAB_SIZE) {
		*rd = ctab[r][0];
		*gd = ctab[r][1];
		*bd = ctab[r][2];
	} else if (ctab == IW_HSV) {
		guchar R,G,B;
		prev_hsvToRgb (r, g, b, &R, &G, &B);
		*rd = R; *gd = G; *bd = B;
	} else {
		*rd = *gd = *bd = 255;
	}
}

/*********************************************************************
  Calls prev_colConvert() and returns a pointer to a static string
  '#rrggbb'.
*********************************************************************/
char *prev_colString (iwColtab ctab, gint r, gint g, gint b)
{
	static char col[8];
	gint rd, gd, bd;

	prev_colConvert (ctab, r, g, b, &rd, &gd, &bd);
	if (rd < 0 || rd > 255) rd = 255;
	if (gd < 0 || gd > 255) gd = 255;
	if (bd < 0 || bd > 255) bd = 255;
	sprintf (col, "#%02x%02x%02x", rd, gd, bd);
	return col;
}

/*********************************************************************
  Set color and buffer for following drawing primitives.
  Calls prev_drawInitFull() and returns zoom factor.
*********************************************************************/
float prev_colInit (prevBuffer *buf, int disp_mode, iwColtab ctab,
					gint r, gint g, gint b, gint colindex)
{
	gint rd, gd, bd;

	if (ctab > IW_COLFORMAT_MAX)
		r = colindex;
	prev_colConvert (ctab, r, g, b, &rd, &gd, &bd);
	return prev_drawInitFull (buf, rd, gd, bd, disp_mode);
}

/*********************************************************************
  Draw a point at pos (x,y).
	_nc: No clipping.
	_nm: No masking (-1 for a color component is not allowed).
*********************************************************************/
void prev_drawPoint (int x, int y)
{
	if (x>=0 && y>=0 && x<b_pBuf->width && y<b_pBuf->height) {
		if (b_dispMode & RENDER_XOR) {
			if (b_pBuf->gray) {
				*(b_pBuf->buffer + y*b_pBuf->width + x) ^= b_red;
			} else {
				guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
				if (b_red>=0) *buf ^= b_red;
				if (b_green>=0) *(buf+1) ^= b_green;
				if (b_blue>=0) *(buf+2) ^= b_blue;
			}
		} else {
			if (b_pBuf->gray) {
				*(b_pBuf->buffer + y*b_pBuf->width + x) = b_red;
			} else {
				guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
				if (b_red>=0) *buf = b_red;
				if (b_green>=0) *(buf+1) = b_green;
				if (b_blue>=0) *(buf+2) = b_blue;
			}
		}
	}
}
void prev_drawPoint_gray_nc (int x, int y)
{
	*(b_pBuf->buffer + y*b_pBuf->width + x) = b_red;
}
void prev_drawPoint_gray_nc_xor (int x, int y)
{
	*(b_pBuf->buffer + y*b_pBuf->width + x) ^= b_red;
}
void prev_drawPoint_color_nc (int x, int y)
{
	guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
	if (b_red>=0) *buf = b_red;
	if (b_green>=0) *(buf+1) = b_green;
	if (b_blue>=0) *(buf+2) = b_blue;
}
void prev_drawPoint_color_nc_xor (int x, int y)
{
	guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
	if (b_red>=0) *buf ^= b_red;
	if (b_green>=0) *(buf+1) ^= b_green;
	if (b_blue>=0) *(buf+2) ^= b_blue;
}
void prev_drawPoint_color_nc_nm (int x, int y)
{
	guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
	*buf = b_red;
	*(buf+1) = b_green;
	*(buf+2) = b_blue;
}

static void prev_drawHLine_nc (int y, int x, int x2)
{
	uchar *buf;
	x2 = x2-x+1;
	if (b_dispMode & RENDER_XOR) {
		if (b_pBuf->gray) {
			buf = b_pBuf->buffer + y*b_pBuf->width + x;
			while (x2-- > 0)
				*buf++ ^= b_red;
		} else {
			buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
			while (x2-- > 0) {
				if (b_red>=0) *buf ^= b_red;
				if (b_green>=0) *(buf+1) ^= b_green;
				if (b_blue>=0) *(buf+2) ^= b_blue;
				buf += 3;
			}
		}
	} else {
		if (b_pBuf->gray) {
			buf = b_pBuf->buffer + y*b_pBuf->width + x;
			while (x2-- > 0)
				*buf++ = b_red;
		} else {
			buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
			while (x2-- > 0) {
				if (b_red>=0) *buf = b_red;
				if (b_green>=0) *(buf+1) = b_green;
				if (b_blue>=0) *(buf+2) = b_blue;
				buf += 3;
			}
		}
	}
}

/*********************************************************************
  Draw a filled rectangle at pos (x,y).
	_nc: No clipping.
*********************************************************************/
void prev_drawFRect (int x, int y, int width, int height)
{
	guchar *buf;
	int step, i, j;

	if (x < 0) {
		width += x;
		x = 0;
	}
	if (y < 0) {
		height += y;
		y = 0;
	}
	if (x+width > b_pBuf->width)
		width = b_pBuf->width - x;
	if (y+height > b_pBuf->height)
		height = b_pBuf->height - y;

	if (b_pBuf->gray) {
		buf =  b_pBuf->buffer + y*b_pBuf->width + x;
		step = b_pBuf->width - width;
	} else {
		buf =  b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
		step = (b_pBuf->width - width) * 3;
	}

	if (width > 0 && height > 0) {
		if (b_dispMode & RENDER_XOR) {
			for (i=height; i>0; i--) {
				for (j=width; j>0; j--) {
					if (b_red>=0) *buf ^= b_red;
					buf++;
					if (! b_pBuf->gray) {
						if (b_green>=0) *buf ^= b_green;
						if (b_blue>=0) *(buf+1) ^= b_blue;
						buf += 2;
					}
				}
				buf += step;
			}
		} else {
			for (i=height; i>0; i--) {
				for (j=width; j>0; j--) {
					if (b_red>=0) *buf = b_red;
					buf++;
					if (! b_pBuf->gray) {
						if (b_green>=0) *buf = b_green;
						if (b_blue>=0) *(buf+1) = b_blue;
						buf += 2;
					}
				}
				buf += step;
			}
		}
	}
}
void prev_drawFRect_gray_nc (int x, int y, int width, int height)
{
	guchar *buf =  b_pBuf->buffer + y*b_pBuf->width + x;
	int step = b_pBuf->width - width;
	int i, j;

	for (i=height; i>0; i--) {
		for (j=width; j>0; j--)
			*buf++ = b_red;
		buf += step;
	}
}
void prev_drawFRect_color_nc (int x, int y, int width, int height)
{
	guchar *buf =  b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
	int step = (b_pBuf->width - width) * 3;
	int i, j;

	for (i=height; i>0; i--) {
		for (j=width; j>0; j--) {
			if (b_red>=0) *buf = b_red;
			if (b_green>=0) *(buf+1) = b_green;
			if (b_blue>=0) *(buf+2) = b_blue;
			buf += 3;
		}
		buf += step;
	}
}

static const float icvSinTable[] = {
	0.0000000f, 0.0174524f, 0.0348995f, 0.0523360f, 0.0697565f, 0.0871557f,
	0.1045285f, 0.1218693f, 0.1391731f, 0.1564345f, 0.1736482f, 0.1908090f,
	0.2079117f, 0.2249511f, 0.2419219f, 0.2588190f, 0.2756374f, 0.2923717f,
	0.3090170f, 0.3255682f, 0.3420201f, 0.3583679f, 0.3746066f, 0.3907311f,
	0.4067366f, 0.4226183f, 0.4383711f, 0.4539905f, 0.4694716f, 0.4848096f,
	0.5000000f, 0.5150381f, 0.5299193f, 0.5446390f, 0.5591929f, 0.5735764f,
	0.5877853f, 0.6018150f, 0.6156615f, 0.6293204f, 0.6427876f, 0.6560590f,
	0.6691306f, 0.6819984f, 0.6946584f, 0.7071068f, 0.7193398f, 0.7313537f,
	0.7431448f, 0.7547096f, 0.7660444f, 0.7771460f, 0.7880108f, 0.7986355f,
	0.8090170f, 0.8191520f, 0.8290376f, 0.8386706f, 0.8480481f, 0.8571673f,
	0.8660254f, 0.8746197f, 0.8829476f, 0.8910065f, 0.8987940f, 0.9063078f,
	0.9135455f, 0.9205049f, 0.9271839f, 0.9335804f, 0.9396926f, 0.9455186f,
	0.9510565f, 0.9563048f, 0.9612617f, 0.9659258f, 0.9702957f, 0.9743701f,
	0.9781476f, 0.9816272f, 0.9848078f, 0.9876883f, 0.9902681f, 0.9925462f,
	0.9945219f, 0.9961947f, 0.9975641f, 0.9986295f, 0.9993908f, 0.9998477f,
	1.0000000f, 0.9998477f, 0.9993908f, 0.9986295f, 0.9975641f, 0.9961947f,
	0.9945219f, 0.9925462f, 0.9902681f, 0.9876883f, 0.9848078f, 0.9816272f,
	0.9781476f, 0.9743701f, 0.9702957f, 0.9659258f, 0.9612617f, 0.9563048f,
	0.9510565f, 0.9455186f, 0.9396926f, 0.9335804f, 0.9271839f, 0.9205049f,
	0.9135455f, 0.9063078f, 0.8987940f, 0.8910065f, 0.8829476f, 0.8746197f,
	0.8660254f, 0.8571673f, 0.8480481f, 0.8386706f, 0.8290376f, 0.8191520f,
	0.8090170f, 0.7986355f, 0.7880108f, 0.7771460f, 0.7660444f, 0.7547096f,
	0.7431448f, 0.7313537f, 0.7193398f, 0.7071068f, 0.6946584f, 0.6819984f,
	0.6691306f, 0.6560590f, 0.6427876f, 0.6293204f, 0.6156615f, 0.6018150f,
	0.5877853f, 0.5735764f, 0.5591929f, 0.5446390f, 0.5299193f, 0.5150381f,
	0.5000000f, 0.4848096f, 0.4694716f, 0.4539905f, 0.4383711f, 0.4226183f,
	0.4067366f, 0.3907311f, 0.3746066f, 0.3583679f, 0.3420201f, 0.3255682f,
	0.3090170f, 0.2923717f, 0.2756374f, 0.2588190f, 0.2419219f, 0.2249511f,
	0.2079117f, 0.1908090f, 0.1736482f, 0.1564345f, 0.1391731f, 0.1218693f,
	0.1045285f, 0.0871557f, 0.0697565f, 0.0523360f, 0.0348995f, 0.0174524f,
	0.0000000f, -0.0174524f, -0.0348995f, -0.0523360f, -0.0697565f, -0.0871557f,
	-0.1045285f, -0.1218693f, -0.1391731f, -0.1564345f, -0.1736482f, -0.1908090f,
	-0.2079117f, -0.2249511f, -0.2419219f, -0.2588190f, -0.2756374f, -0.2923717f,
	-0.3090170f, -0.3255682f, -0.3420201f, -0.3583679f, -0.3746066f, -0.3907311f,
	-0.4067366f, -0.4226183f, -0.4383711f, -0.4539905f, -0.4694716f, -0.4848096f,
	-0.5000000f, -0.5150381f, -0.5299193f, -0.5446390f, -0.5591929f, -0.5735764f,
	-0.5877853f, -0.6018150f, -0.6156615f, -0.6293204f, -0.6427876f, -0.6560590f,
	-0.6691306f, -0.6819984f, -0.6946584f, -0.7071068f, -0.7193398f, -0.7313537f,
	-0.7431448f, -0.7547096f, -0.7660444f, -0.7771460f, -0.7880108f, -0.7986355f,
	-0.8090170f, -0.8191520f, -0.8290376f, -0.8386706f, -0.8480481f, -0.8571673f,
	-0.8660254f, -0.8746197f, -0.8829476f, -0.8910065f, -0.8987940f, -0.9063078f,
	-0.9135455f, -0.9205049f, -0.9271839f, -0.9335804f, -0.9396926f, -0.9455186f,
	-0.9510565f, -0.9563048f, -0.9612617f, -0.9659258f, -0.9702957f, -0.9743701f,
	-0.9781476f, -0.9816272f, -0.9848078f, -0.9876883f, -0.9902681f, -0.9925462f,
	-0.9945219f, -0.9961947f, -0.9975641f, -0.9986295f, -0.9993908f, -0.9998477f,
	-1.0000000f, -0.9998477f, -0.9993908f, -0.9986295f, -0.9975641f, -0.9961947f,
	-0.9945219f, -0.9925462f, -0.9902681f, -0.9876883f, -0.9848078f, -0.9816272f,
	-0.9781476f, -0.9743701f, -0.9702957f, -0.9659258f, -0.9612617f, -0.9563048f,
	-0.9510565f, -0.9455186f, -0.9396926f, -0.9335804f, -0.9271839f, -0.9205049f,
	-0.9135455f, -0.9063078f, -0.8987940f, -0.8910065f, -0.8829476f, -0.8746197f,
	-0.8660254f, -0.8571673f, -0.8480481f, -0.8386706f, -0.8290376f, -0.8191520f,
	-0.8090170f, -0.7986355f, -0.7880108f, -0.7771460f, -0.7660444f, -0.7547096f,
	-0.7431448f, -0.7313537f, -0.7193398f, -0.7071068f, -0.6946584f, -0.6819984f,
	-0.6691306f, -0.6560590f, -0.6427876f, -0.6293204f, -0.6156615f, -0.6018150f,
	-0.5877853f, -0.5735764f, -0.5591929f, -0.5446390f, -0.5299193f, -0.5150381f,
	-0.5000000f, -0.4848096f, -0.4694716f, -0.4539905f, -0.4383711f, -0.4226183f,
	-0.4067366f, -0.3907311f, -0.3746066f, -0.3583679f, -0.3420201f, -0.3255682f,
	-0.3090170f, -0.2923717f, -0.2756374f, -0.2588190f, -0.2419219f, -0.2249511f,
	-0.2079117f, -0.1908090f, -0.1736482f, -0.1564345f, -0.1391731f, -0.1218693f,
	-0.1045285f, -0.0871557f, -0.0697565f, -0.0523360f, -0.0348995f, -0.0174524f,
	-0.0000000f, 0.0174524f, 0.0348995f, 0.0523360f, 0.0697565f, 0.0871557f,
	0.1045285f, 0.1218693f, 0.1391731f, 0.1564345f, 0.1736482f, 0.1908090f,
	0.2079117f, 0.2249511f, 0.2419219f, 0.2588190f, 0.2756374f, 0.2923717f,
	0.3090170f, 0.3255682f, 0.3420201f, 0.3583679f, 0.3746066f, 0.3907311f,
	0.4067366f, 0.4226183f, 0.4383711f, 0.4539905f, 0.4694716f, 0.4848096f,
	0.5000000f, 0.5150381f, 0.5299193f, 0.5446390f, 0.5591929f, 0.5735764f,
	0.5877853f, 0.6018150f, 0.6156615f, 0.6293204f, 0.6427876f, 0.6560590f,
	0.6691306f, 0.6819984f, 0.6946584f, 0.7071068f, 0.7193398f, 0.7313537f,
	0.7431448f, 0.7547096f, 0.7660444f, 0.7771460f, 0.7880108f, 0.7986355f,
	0.8090170f, 0.8191520f, 0.8290376f, 0.8386706f, 0.8480481f, 0.8571673f,
	0.8660254f, 0.8746197f, 0.8829476f, 0.8910065f, 0.8987940f, 0.9063078f,
	0.9135455f, 0.9205049f, 0.9271839f, 0.9335804f, 0.9396926f, 0.9455186f,
	0.9510565f, 0.9563048f, 0.9612617f, 0.9659258f, 0.9702957f, 0.9743701f,
	0.9781476f, 0.9816272f, 0.9848078f, 0.9876883f, 0.9902681f, 0.9925462f,
	0.9945219f, 0.9961947f, 0.9975641f, 0.9986295f, 0.9993908f, 0.9998477f,
	1.0000000f
};

static void icvCosSin (int angle, float *cosval, float *sinval)
{
	if (angle < 0) angle += 360;
	*sinval = icvSinTable[angle];
	*cosval = icvSinTable[450 - angle];
}

/*********************************************************************
  Draw a line (with the Bresenham algorithm).
*********************************************************************/
void prev_drawLine (int x, int y, int X, int Y)
{
	int dx, dy, d, step_E, step_XE, b;
	void (*putPoint) (int x, int y);

	if ((b_dispMode & RENDER_THICK) && b_pBuf->gc.thickness*b_zoom > 1) {
		static int dtable[][2] = {
			{14, 10}, {7, 20}, {3, 30}, {1, 45}, {0, 90}};
		prevDataPoint *pts, *point;
		float radius = b_pBuf->gc.thickness * b_zoom / 2;
		int i, delta, arc_start;
		float r, arc_al, arc_be, da, db;

		if (X < x || (X == x && y < Y)) {
			i = X; X = x; x = i;
			i = Y; Y = y; y = i;
		}

		for (i=0; dtable[i][0] > radius; i++) /* empty */;
		delta = dtable[i][1];
		pts = malloc (sizeof(prevDataPoint)*2*(180/delta+1));

		if (x == X)
			arc_start = 180;
		else
			arc_start = 180*(G_PI_2 + atan((double)(y-Y)/(X-x))) / G_PI;

		/* Get coordinates of two half circles connected by two lines */
		icvCosSin (arc_start, &arc_al, &arc_be);
		icvCosSin (delta, &da, &db);
		point = pts;
		for (i = arc_start; i < arc_start + 180; i += delta) {
			point->x = gui_lrint (x + radius * arc_al);
			point->y = gui_lrint (y - radius * arc_be);

			r = arc_al * da - arc_be * db;
			arc_be = arc_al * db + arc_be * da;
			arc_al = r;
			point++;
		}
		point->x = gui_lrint (x + radius * arc_al);
		point->y = gui_lrint (y - radius * arc_be);
		point++;
		for (; i <= arc_start + 360; i += delta) {
			point->x = gui_lrint (X + radius * arc_al);
			point->y = gui_lrint (Y - radius * arc_be);

			r = arc_al * da - arc_be * db;
			arc_be = arc_al * db + arc_be * da;
			arc_al = r;
			point++;
		}

		prev_drawConvexPoly (2*(180/delta+1), pts);
		free (pts);
		return;
	}

	if (X < x) {
		b=x; x=X; X=b; b=y; y=Y; Y=b;
	}
	if (x>=0 && y>=0 && X<b_pBuf->width && Y<b_pBuf->height && y<b_pBuf->height && Y>=0) {
		if (b_dispMode & RENDER_XOR) {
			if (b_pBuf->gray)
				putPoint = prev_drawPoint_gray_nc_xor;
			else
				putPoint = prev_drawPoint_color_nc_xor;
		} else {
			if (b_pBuf->gray)
				putPoint = prev_drawPoint_gray_nc;
			else if (b_red>=0 && b_green>=0 && b_blue>=0)
				putPoint = prev_drawPoint_color_nc_nm;
			else
				putPoint = prev_drawPoint_color_nc;
		}
	} else if ((x<0 && X<0) || (x>=b_pBuf->width && X>=b_pBuf->width) ||
			   (y<0 && Y<0) || (y>=b_pBuf->height && Y>=b_pBuf->height)) {
		/* Line is completely outside, nothing to draw */
		return;
	} else
		putPoint = prev_drawPoint;

	dx = X-x;
	dy = Y-y;

	if (dy>=0 && dx>=dy) {
		step_E = dy<<1;
		d      = step_E-dx;
		step_XE= d-dx;
		for (; x<=X; x++) {
			putPoint (x, y);
			if (d<=0) {
				d += step_E;
			} else {
				d += step_XE;
				y++;
			}
		}
	} else if (dy<0 && dx>=-dy) {
		step_E = -dy<<1;
		d      = step_E-dx;
		step_XE= d-dx;
		for (; x<=X; x++) {
			putPoint (x, y);
			if (d<=0) {
				d += step_E;
			} else {
				d += step_XE;
				y--;
			}
		}
	} else if (dy>=0 && dy>dx) {
		step_E = dx<<1;
		d      = step_E-dy;
		step_XE= d-dy;
		for (; y<=Y; y++) {
			putPoint (x, y);
			if (d<=0) {
				d += step_E;
			} else {
				d += step_XE;
				x++;
			}
		}
	} else if (dy<0 && -dy>dx) {
		step_E = dx<<1;
		d      = step_E+dy;
		step_XE= d+dy;
		for (; y>=Y; y--) {
			putPoint (x, y);
			if (d<=0) {
				d += step_E;
			} else {
				d += step_XE;
				x++;
			}
		}
	}
}

/*********************************************************************
  Draw a polygon.
*********************************************************************/
void prev_drawPoly (int npts, const prevDataPoint *pts)
{
	int i, x, y;

	x = pts->x;
	y = pts->y;
	pts++;
	for (i=1; i<npts; i++) {
		prev_drawLine (x, y, pts->x, pts->y);
		x = pts->x;
		y = pts->y;
		pts++;
	}
}

/*********************************************************************
  Draw a filled polygon.
*********************************************************************/
static void cb_prev_drawFPoly (int x, int y, int cnt, void *data)
{
	guchar *buf;

	if (b_dispMode & RENDER_XOR) {
		if (b_pBuf->gray) {
			buf = b_pBuf->buffer + y*b_pBuf->width + x;
			while (cnt--)
				*buf++ ^= b_red;
		} else {
			buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
			while (cnt--) {
				if (b_red>=0) *buf ^= b_red;
				if (b_green>=0) *(buf+1) ^= b_green;
				if (b_blue>=0) *(buf+2) ^= b_blue;
				buf+=3;
			}
		}
	} else {
		if (b_pBuf->gray) {
			buf = b_pBuf->buffer + y*b_pBuf->width + x;
			while (cnt--)
				*buf++ = b_red;
		} else {
			buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
			while (cnt--) {
				if (b_red>=0) *buf = b_red;
				if (b_green>=0) *(buf+1) = b_green;
				if (b_blue>=0) *(buf+2) = b_blue;
				buf+=3;
			}
		}
	}
}
static void cb_prev_drawFPoly_color_nm (int x, int y, int cnt, void *data)
{
	guchar *buf = b_pBuf->buffer + y*b_pBuf->width*3 + x*3;
	while (cnt--) {
		*buf = b_red;
		*(buf+1)= b_green;
		*(buf+2)= b_blue;
		buf+=3;
	}
}
void prev_drawFPoly (int npts, const prevDataPoint *pts)
{
	if (b_dispMode & RENDER_XOR || b_pBuf->gray || b_red < 0 || b_green < 0 || b_blue < 0)
		prev_drawFPoly_raw (b_pBuf->width, b_pBuf->height, npts, pts, FALSE,
							cb_prev_drawFPoly, NULL);
	else
		prev_drawFPoly_raw (b_pBuf->width, b_pBuf->height, npts, pts, FALSE,
							cb_prev_drawFPoly_color_nm, NULL);
}
void prev_drawConvexPoly (int npts, const prevDataPoint *pts)
{
	if (b_dispMode & RENDER_XOR || b_pBuf->gray || b_red < 0 || b_green < 0 || b_blue < 0)
		prev_drawConvexPoly_raw (b_pBuf->width, b_pBuf->height, npts, pts, FALSE,
								 cb_prev_drawFPoly, NULL);
	else
		prev_drawConvexPoly_raw (b_pBuf->width, b_pBuf->height, npts, pts, FALSE,
								 cb_prev_drawFPoly_color_nm, NULL);
}

static int icvEllipse2Poly (float cx, float cy, float width, float height,
							int angle, int arc_start, int arc_end,
							prevDataPoint * pts, int delta)
{
	float ang_alpha, ang_beta;
	float arc_al, arc_be;
	float da, db;
	prevDataPoint *pts_origin = pts;
	int i;

	while (angle < 0)
		angle += 360;
	while (angle > 360)
		angle -= 360;

	if (arc_start > arc_end) {
		i = arc_start;
		arc_start = arc_end;
		arc_end = i;
	}
	while (arc_start < 0) {
		arc_start += 360;
		arc_end += 360;
	}
	while (arc_end > 360) {
		arc_end -= 360;
		arc_start -= 360;
	}
	if (arc_end - arc_start > 360) {
		arc_start = 0;
		arc_end = 360;
	}

	icvCosSin (angle, &ang_alpha, &ang_beta);
	icvCosSin (arc_start, &arc_al, &arc_be);
	icvCosSin (delta, &da, &db);

	for (i = arc_start; i < arc_end + delta; i += delta, pts++) {
		float x, y;

		if (i > arc_end)
			icvCosSin (arc_end, &arc_al, &arc_be);

		x = width * arc_al;
		y = height * arc_be;
		pts->x = gui_lrint (cx + x * ang_alpha - y * ang_beta);
		pts->y = gui_lrint (cy - x * ang_beta - y * ang_alpha);

		x = arc_al * da - arc_be * db;
		arc_be = arc_al * db + arc_be * da;
		arc_al = x;
	}

	return pts - pts_origin;
}

void prev_drawEllipse (int x, int y, int width, int height,
					   int angle, int arc_start, int arc_end, gboolean filled)
{
	prevDataPoint pts[360/2 + 3];
	int npts;

	npts = icvEllipse2Poly (x, y, width, height,
							angle, arc_start, arc_end, pts, 2);
	if (filled) {
		if (arc_end - arc_start >= 360) {
			prev_drawConvexPoly (npts, pts);
		} else {
			pts[npts].x = x;
			pts[npts++].y = y;
			prev_drawFPoly (npts, pts);
		}
	} else {
		prev_drawPoly (npts, pts);
	}
}

/*********************************************************************
  Draw a circle (with the direct Bresenham algorithm).
*********************************************************************/
void prev_drawCircle (int x, int y, int radius)
{
	int err = 0, dx = radius, dy = 0, plus = 1, minus = (radius << 1) - 1;
	int inside = x >= radius && x < b_pBuf->width - radius &&
		y >= radius && y < b_pBuf->height - radius;
	void (*putPoint) (int x, int y);

	if ((b_dispMode & RENDER_THICK) && b_pBuf->gc.thickness*b_zoom > 1) {
		prev_drawEllipse (x, y, radius, radius, 0, 0, 360, FALSE);
		return;
	}

	if (b_dispMode & RENDER_XOR) {
		if (b_pBuf->gray)
			putPoint = prev_drawPoint_gray_nc_xor;
		else
			putPoint = prev_drawPoint_color_nc_xor;
	} else {
		if (b_pBuf->gray)
			putPoint = prev_drawPoint_gray_nc;
		else if (b_red>=0 && b_green>=0 && b_blue>=0)
			putPoint = prev_drawPoint_color_nc_nm;
		else
			putPoint = prev_drawPoint_color_nc;
	}

	while (dx >= dy) {
		int mask;
		int y11 = y - dy, y12 = y + dy, y21 = y - dx, y22 = y + dx;
		int x11 = x - dx, x12 = x + dx, x21 = x - dy, x22 = x + dy;

		if (inside) {
			putPoint (x11, y11);
			putPoint (x11, y12);
			putPoint (x12, y11);
			putPoint (x12, y12);

			putPoint (x21, y21);
			putPoint (x21, y22);
			putPoint (x22, y21);
			putPoint (x22, y22);
		} else if (x11 < b_pBuf->width && x12 >= 0 && y21 < b_pBuf->height && y22 >= 0) {
			if (y11 < b_pBuf->height && y11 >= 0) {
				if (x11 >= 0)
					putPoint (x11, y11);
				if (x12 < b_pBuf->width)
					putPoint (x12, y11);
			}

			if (y12 < b_pBuf->height && y12 >= 0) {
				if (x11 >= 0)
					putPoint (x11, y12);
				if (x12 < b_pBuf->width)
					putPoint (x12, y12);
			}

			if (x21 < b_pBuf->width && x22 >= 0) {
				if (y21 < b_pBuf->height && y21 >= 0) {
					if (x21 >= 0)
						putPoint (x21, y21);
					if (x22 < b_pBuf->width)
						putPoint (x22, y21);
				}

				if (y22 < b_pBuf->height && y22 >= 0) {
					if (x21 >= 0)
						putPoint (x21, y22);
					if (x22 < b_pBuf->width)
						putPoint (x22, y22);
				}
			}
		}
		dy++;
		err += plus;
		plus += 2;

		mask = (err <= 0) - 1;

		err -= minus & mask;
		dx += mask;
		minus -= mask & 2;
	}
}

/*********************************************************************
  Draw a filled circle (with the direct Bresenham algorithm).
*********************************************************************/
void prev_drawFCircle (int x, int y, int radius)
{
	int err = 0, dx = radius, dy = 0, plus = 1, minus = (radius << 1) - 1;
	int inside = x >= radius && x < b_pBuf->width - radius &&
		y >= radius && y < b_pBuf->height - radius;

	while (dx >= dy) {
		int mask;
		int y11 = y - dy, y12 = y + dy, y21 = y - dx, y22 = y + dx;
		int x11 = x - dx, x12 = x + dx, x21 = x - dy, x22 = x + dy;

		if (inside) {
			prev_drawHLine_nc (y11, x11, x12);
			prev_drawHLine_nc (y12, x11, x12);
			prev_drawHLine_nc (y21, x21, x22);
			prev_drawHLine_nc (y22, x21, x22);
		} else if (x11 < b_pBuf->width && x12 >= 0 && y21 < b_pBuf->height && y22 >= 0) {
			x11 = MAX(x11, 0);
			x12 = MIN(x12, b_pBuf->width - 1);

			if (y11 < b_pBuf->height && y11 >= 0)
				prev_drawHLine_nc (y11, x11, x12);
			if (y12 < b_pBuf->height && y12 >= 0)
				prev_drawHLine_nc (y12, x11, x12);

			if (x21 < b_pBuf->width && x22 >= 0) {
				x21 = MAX(x21, 0);
				x22 = MIN(x22, b_pBuf->width - 1);

				if (y21 < b_pBuf->height && y21 >= 0)
					prev_drawHLine_nc (y21, x21, x22);
				if (y22 < b_pBuf->height && y22 >= 0)
					prev_drawHLine_nc (y22, x21, x22);
			}
		}
		dy++;
		err += plus;
		plus += 2;

		mask = (err <= 0) - 1;

		err -= minus & mask;
		dx += mask;
		minus -= mask & 2;
	}
}

/*********************************************************************
  Draw a text at pos (x,y) with font given with prev_drawInitFull() or
  via the menu appended with prev_opts_append().
  Differnt in-string formating options and '\n','\t' are supported:
  Format: <tag1=value tag2="value" tag3> with
  tag    value                                               result
  <                                                    '<' - string
  anchor tl|tr|br|bl|c     (x,y) at topleft,..., center pos of text
  fg     [rgb:|yuv:|bgr:|bw:|index:]r g b           forground color
  bg     [rgb:|yuv:|bgr:|bw:|index:]r g b          background color
  font   small|med|big                                  font to use
  adjust left|center|right                          text adjustment
  rotate 0|90|180|270   rotate text beginning with the current line
  shadow 1|0                                          shadow on/off
  zoom   1|0                                       zoom text on/off
  /                                 return to values before last <>
  ATTENTION: Low-Level function without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawString (int x, int y, const char *str)
{
	textOpts **opts = (textOpts **)prev_opts_get(b_pBuf, PREV_TEXT);
	textContext *tc = prev_text_context_push(NULL);
	textData data;

	if (b_dispMode & FONT_MASK)
		tc->font = &iw_fonts[(b_dispMode & FONT_MASK) - 1];
	else if (opts) {
		tc->font = &iw_fonts[(((*opts)->font+1) & FONT_MASK) - 1];
	} else
		tc->font = &iw_fonts[1];
	tc->r = b_red;
	tc->g = b_green;
	tc->b = b_blue;
	tc->bg_r = -1;
	tc->bg_g = -1;
	tc->bg_b = -1;
	tc->shadow = opts && (*opts)->shadow;

	data.b = b_pBuf;
	data.dispMode = b_dispMode;
	data.zoom = b_zoom;
	data.save = NULL;
	prev_drawString_do (&data, x, y, str, tc);
}

/*********************************************************************
  PRIVATE: Fill gc struct with default values.
*********************************************************************/
void prev_gc_init (prevGC *gc)
{
	memset (gc, 0, sizeof(prevGC));
}

/*********************************************************************
  Set color which is used for clearing the buffer (RENDER_CLEAR).
*********************************************************************/
void prev_set_bg_color (prevBuffer *buf, uchar r, uchar g, uchar b)
{
	buf->gc.bg_r = r;
	buf->gc.bg_g = g;
	buf->gc.bg_b = b;
}
/*********************************************************************
  Set thickness for line oriented drawings.
*********************************************************************/
void prev_set_thickness (prevBuffer *buf, int thickness)
{
	buf->gc.thickness = thickness;
}

/*********************************************************************
  Fill b with the bg color.
  ATTENTION: Low-Level function without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawBackground (prevBuffer *b)
{
	if (b->gray || (b->gc.bg_r == b->gc.bg_g &&  b->gc.bg_r == b->gc.bg_b)) {
		memset (b->buffer, b->gc.bg_r, b->width*b->height*(b->gray?1:3));
	} else {
		int count = b->width*b->height*(b->gray?1:3);
		guchar *pos = b->buffer;
		guchar cr = b->gc.bg_r;
		guchar cg = b->gc.bg_g;
		guchar cb = b->gc.bg_b;
		while (count) {
			*pos++ = cr;
			*pos++ = cg;
			*pos++ = cb;
			count -= 3;
		}
	}
}

/*********************************************************************
  Some random things which must be done before drawing:
    - lock the buffer
    - if RENDER_CLEAR is set:
        - clear the buffer
        - free all old render data
        - store the render size (width,height) in b->gc (if >= 0)
  Returns TRUE if anything should be done (-> if the window is open).
*********************************************************************/
gboolean prev_prepare_draw (prevBuffer *b, int disp_mode, int width, int height)
{
	if (!b->window) return FALSE;

	prev_buffer_lock();

	if (disp_mode & RENDER_CLEAR) {
		prev_drawBackground (b);
		prev_rdata_free (b, FALSE);

		prev_set_render_size (b, width, height);
	}

	return TRUE;
}

static float vector_thickness (prevBuffer *b, float zoom, int disp_mode)
{
	if (disp_mode & RENDER_THICK) {
		zoom = b->gc.thickness*zoom;
		if (zoom < 0.3)
			return 0.3;
		else
			return zoom;
	} else
		return 0.3;
}

/*********************************************************************
  Display line in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void* prev_render_line_copy (const void *data)
{
	prevDataLine *t = g_malloc (sizeof(prevDataLine));
	*t = *(prevDataLine*)data;
	return t;
}
static void prev_render_line (prevBuffer *b, const prevDataLine *line,
							  int disp_mode)
{
	prev_colInit (b, disp_mode, line->ctab, line->r, line->g, line->b, line->r);
	prev_drawLine ((line->x1-b->x)*b_zoom + b_zoom/2, (line->y1-b->y)*b_zoom + b_zoom/2,
				   (line->x2-b->x)*b_zoom + b_zoom/2, (line->y2-b->y)*b_zoom + b_zoom/2);
}
static void* prev_render_line_f_copy (const void *data)
{
	prevDataLineF *t = g_malloc (sizeof(prevDataLineF));
	*t = *(prevDataLineF*)data;
	return t;
}
static void prev_render_line_f (prevBuffer *b, const prevDataLineF *line,
								int disp_mode)
{
	prev_colInit (b, disp_mode, line->ctab, line->r, line->g, line->b, line->r);
	prev_drawLine ((line->x1-b->x)*b_zoom, (line->y1-b->y)*b_zoom,
				   (line->x2-b->x)*b_zoom, (line->y2-b->y)*b_zoom);
}
static iwImgStatus prev_render_line_f_vector (prevBuffer *b, prevVectorSave *save,
											  const prevDataLineF *line, int disp_mode)
{
	float zoom = save->zoom;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	fprintf (save->file,
			 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
			 "x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" />\n",
			 prev_colString(line->ctab, line->r, line->g, line->b),
			 vector_thickness(b, zoom, disp_mode),
			 line->x1*zoom, line->y1*zoom, line->x2*zoom, line->y2*zoom);

	return IW_IMG_STATUS_OK;
}
static iwImgStatus prev_render_line_vector (prevBuffer *b, prevVectorSave *save,
											const prevDataLine *line, int disp_mode)
{
	prevDataLineF l;
	COL_COPY(line, l);
	l.x1 = line->x1 + 0.5;
	l.y1 = line->y1 + 0.5;
	l.x2 = line->x2 + 0.5;
	l.y2 = line->y2 + 0.5;
	return prev_render_line_f_vector (b, save, &l, disp_mode);
}

/*********************************************************************
  Display the (filled) rectangle rect in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void* prev_render_rect_copy (const void *data)
{
	prevDataRect *t = g_malloc (sizeof(prevDataRect));
	*t = *(prevDataRect*)data;
	return t;
}
static void prev_render_rect (prevBuffer *b, const prevDataRect *rect,
							  int disp_mode)
{
	prev_colInit (b, disp_mode, rect->ctab, rect->r, rect->g, rect->b, rect->r);
	prev_drawLine ((rect->x1-b->x)*b_zoom + b_zoom/2, (rect->y1-b->y)*b_zoom + b_zoom/2,
				   (rect->x2-b->x)*b_zoom + b_zoom/2, (rect->y1-b->y)*b_zoom + b_zoom/2);
	prev_drawLine ((rect->x2-b->x)*b_zoom + b_zoom/2, (rect->y1-b->y)*b_zoom + b_zoom/2,
				   (rect->x2-b->x)*b_zoom + b_zoom/2, (rect->y2-b->y)*b_zoom + b_zoom/2);
	prev_drawLine ((rect->x2-b->x)*b_zoom + b_zoom/2, (rect->y2-b->y)*b_zoom + b_zoom/2,
				   (rect->x1-b->x)*b_zoom + b_zoom/2, (rect->y2-b->y)*b_zoom + b_zoom/2);
	prev_drawLine ((rect->x1-b->x)*b_zoom + b_zoom/2, (rect->y2-b->y)*b_zoom + b_zoom/2,
				   (rect->x1-b->x)*b_zoom + b_zoom/2, (rect->y1-b->y)*b_zoom + b_zoom/2);
}
static void prev_render_frect (prevBuffer *b, const prevDataRect *frect,
							   int disp_mode)
{
	int x1, x2, y1, y2;
	if (frect->x1 > frect->x2) {
		x1 = frect->x2;
		x2 = frect->x1;
	} else {
		x1 = frect->x1;
		x2 = frect->x2;
	}
	if (frect->y1 > frect->y2) {
		y1 = frect->y2;
		y2 = frect->y1;
	} else {
		y1 = frect->y1;
		y2 = frect->y2;
	}
	prev_colInit (b, disp_mode, frect->ctab, frect->r, frect->g, frect->b, frect->r);
	prev_drawFRect ((x1-b->x)*b_zoom, (y1-b->y)*b_zoom,
					(x2-x1+1)*b_zoom, (y2-y1+1)*b_zoom);
}

static void* prev_render_rect_f_copy (const void *data)
{
	prevDataRectF *t = g_malloc (sizeof(prevDataRectF));
	*t = *(prevDataRectF*)data;
	return t;
}
static void prev_render_rect_f (prevBuffer *b, const prevDataRectF *rect,
								int disp_mode)
{
	prev_colInit (b, disp_mode, rect->ctab, rect->r, rect->g, rect->b, rect->r);
	prev_drawLine ((rect->x1-b->x)*b_zoom, (rect->y1-b->y)*b_zoom,
				   (rect->x2-b->x)*b_zoom, (rect->y1-b->y)*b_zoom);
	prev_drawLine ((rect->x2-b->x)*b_zoom, (rect->y1-b->y)*b_zoom,
				   (rect->x2-b->x)*b_zoom, (rect->y2-b->y)*b_zoom);
	prev_drawLine ((rect->x2-b->x)*b_zoom, (rect->y2-b->y)*b_zoom,
				   (rect->x1-b->x)*b_zoom, (rect->y2-b->y)*b_zoom);
	prev_drawLine ((rect->x1-b->x)*b_zoom, (rect->y2-b->y)*b_zoom,
				   (rect->x1-b->x)*b_zoom, (rect->y1-b->y)*b_zoom);
}
static void prev_render_frect_f (prevBuffer *b, const prevDataRectF *frect,
								 int disp_mode)
{
	float x1, x2, y1, y2;
	if (frect->x1 > frect->x2) {
		x1 = frect->x2;
		x2 = frect->x1;
	} else {
		x1 = frect->x1;
		x2 = frect->x2;
	}
	if (frect->y1 > frect->y2) {
		y1 = frect->y2;
		y2 = frect->y1;
	} else {
		y1 = frect->y1;
		y2 = frect->y2;
	}
	prev_colInit (b, disp_mode, frect->ctab, frect->r, frect->g, frect->b, frect->r);
	prev_drawFRect ((x1-b->x)*b_zoom, (y1-b->y)*b_zoom,
					(x2-x1)*b_zoom+1, (y2-y1)*b_zoom+1);
}
static iwImgStatus prev_rect_vector (prevBuffer *b, prevVectorSave *save,
									 const prevDataRectF *rect, int disp_mode, BOOL fill)
{
	float zoom = save->zoom;
	float x, y, w, h;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (rect->x1 < rect->x2) {
		x = rect->x1*zoom;
		w = rect->x2*zoom - x;
	} else {
		x = rect->x2*zoom;
		w = rect->x1*zoom - x;
	}
	if (rect->y1 < rect->y2) {
		y = rect->y1*zoom;
		h = rect->y2*zoom - y;
	} else {
		y = rect->y2*zoom;
		h = rect->y1*zoom - y;
	}
	if (fill)
		fprintf (save->file,
				 "  <rect style=\"fill:%s\" "
				 "x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
				 prev_colString(rect->ctab, rect->r, rect->g, rect->b),
				 x, y, w, h);
	else
		fprintf (save->file,
				 "  <rect style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
				 "x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
				 prev_colString(rect->ctab, rect->r, rect->g, rect->b),
				 vector_thickness(b, zoom, disp_mode),
				 x, y, w, h);

	return IW_IMG_STATUS_OK;
}
static iwImgStatus prev_render_rect_vector (prevBuffer *b, prevVectorSave *save,
											const prevDataRect *rect, int disp_mode)
{
	prevDataRectF r;
	COL_COPY(rect,r);
	r.x1 = rect->x1 + 0.5;
	r.y1 = rect->y1 + 0.5;
	r.x2 = rect->x2 + 0.5;
	r.y2 = rect->y2 + 0.5;
	return prev_rect_vector (b, save, &r, disp_mode, FALSE);
}
static iwImgStatus prev_render_rect_f_vector (prevBuffer *b, prevVectorSave *save,
											  const prevDataRectF *rect, int disp_mode)
{
	return prev_rect_vector (b, save, rect, disp_mode, FALSE);
}
static iwImgStatus prev_render_frect_vector (prevBuffer *b, prevVectorSave *save,
											 const prevDataRect *rect, int disp_mode)
{
	prevDataRectF r;
	COL_COPY(rect, r);
	r.x1 = rect->x1 + 0.5;
	r.y1 = rect->y1 + 0.5;
	r.x2 = rect->x2 + 0.5;
	r.y2 = rect->y2 + 0.5;
	return prev_rect_vector (b, save, &r, disp_mode, TRUE);
}
static iwImgStatus prev_render_frect_f_vector (prevBuffer *b, prevVectorSave *save,
											   const prevDataRectF *rect, int disp_mode)
{
	return prev_rect_vector (b, save, rect, disp_mode, TRUE);
}

/*********************************************************************
  Display polygon in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static prevDataPoint* poly_zoom_mov (prevBuffer *b, float zoom, int nsrc, prevDataPoint *src)
{
	prevDataPoint *pts = g_malloc (sizeof(prevDataPoint)*nsrc);
	int i;

	for (i=0; i<nsrc; i++) {
		pts[i].x = (src[i].x-b->x)*zoom + zoom/2;
		pts[i].y = (src[i].y-b->y)*zoom + zoom/2;
	}
	return pts;
}

static void prev_render_poly_free (void *data)
{
	prevDataPoly *poly = (prevDataPoly*)data;

	g_free (poly->pts);
	g_free (poly);
}
static void* prev_render_poly_copy (const void *data)
{
	prevDataPoly *poly = (prevDataPoly*)data;

	prevDataPoly *t = g_malloc (sizeof(prevDataPoly));
	*t = *poly;
	t->pts = g_malloc (sizeof(prevDataPoint) * poly->npts);
	memcpy (t->pts, poly->pts, sizeof(prevDataPoint) * poly->npts);

	return t;
}
static void prev_render_poly (prevBuffer *b, const prevDataPoly *poly,
							  int disp_mode)
{
	prevDataPoint *pts;

	prev_colInit (b, disp_mode, poly->ctab, poly->r, poly->g, poly->b, poly->r);
	pts = poly_zoom_mov (b, b_zoom, poly->npts, poly->pts);
	prev_drawPoly (poly->npts, pts);
	g_free (pts);
}
static void prev_render_fpoly (prevBuffer *b, const prevDataPoly *poly,
							   int disp_mode)
{
	prevDataPoint *pts;

	prev_colInit (b, disp_mode, poly->ctab, poly->r, poly->g, poly->b, poly->r);
	pts = poly_zoom_mov (b, b_zoom, poly->npts, poly->pts);
	prev_drawFPoly (poly->npts, pts);
	g_free (pts);
}

static prevDataPoint* poly_f_zoom_mov (prevBuffer *b, float zoom, int nsrc, prevDataPointF *src)
{
	prevDataPoint *pts = g_malloc (sizeof(prevDataPoint)*nsrc);
	int i;

	for (i=0; i<nsrc; i++) {
		pts[i].x = (src[i].x-b->x)*zoom;
		pts[i].y = (src[i].y-b->y)*zoom;
	}
	return pts;
}
static void prev_render_poly_f_free (void *data)
{
	prevDataPolyF *poly = (prevDataPolyF*)data;

	g_free (poly->pts);
	g_free (poly);
}
static void* prev_render_poly_f_copy (const void *data)
{
	prevDataPolyF *poly = (prevDataPolyF*)data;

	prevDataPolyF *t = g_malloc (sizeof(prevDataPolyF));
	*t = *poly;
	t->pts = g_malloc (sizeof(prevDataPointF) * poly->npts);
	memcpy (t->pts, poly->pts, sizeof(prevDataPointF) * poly->npts);

	return t;
}
static void prev_render_poly_f (prevBuffer *b, const prevDataPolyF *poly,
								int disp_mode)
{
	prevDataPoint *pts;

	prev_colInit (b, disp_mode, poly->ctab, poly->r, poly->g, poly->b, poly->r);
	pts = poly_f_zoom_mov (b, b_zoom, poly->npts, poly->pts);
	prev_drawPoly (poly->npts, pts);
	g_free (pts);
}
static void prev_render_fpoly_f (prevBuffer *b, const prevDataPolyF *poly,
								 int disp_mode)
{
	prevDataPoint *pts;

	prev_colInit (b, disp_mode, poly->ctab, poly->r, poly->g, poly->b, poly->r);
	pts = poly_f_zoom_mov (b, b_zoom, poly->npts, poly->pts);
	prev_drawFPoly (poly->npts, pts);
	g_free (pts);
}
static iwImgStatus prev_poly_vector (prevBuffer *b, prevVectorSave *save,
									 const prevDataPolyF *poly, int disp_mode, BOOL fill)
{
	float zoom = save->zoom;
	int i;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (fill)
		fprintf (save->file,
				 "  <polygon style=\"fill:%s;fill-rule:evenodd\" points=\"",
				 prev_colString(poly->ctab, poly->r, poly->g, poly->b));
	else
		fprintf (save->file,
				 "  <polyline style=\"fill:none;stroke:%s;stroke-width:%.2f\" points=\"",
				 prev_colString(poly->ctab, poly->r, poly->g, poly->b),
				 vector_thickness(b, zoom, disp_mode));

	for (i=0; i<poly->npts; i++)
		fprintf (save->file, " %f,%f",
				 poly->pts[i].x*zoom, poly->pts[i].y*zoom);
	fprintf (save->file, "\" />\n");
	return IW_IMG_STATUS_OK;
}
static iwImgStatus prev_render_poly_vector (prevBuffer *b, prevVectorSave *save,
											const prevDataPoly *poly, int disp_mode)
{
	prevDataPointF *pts;
	prevDataPolyF p;
	int i;
	iwImgStatus status;

	COL_COPY(poly,p);
	pts = g_malloc (sizeof(prevDataPointF)*poly->npts);
	p.npts = poly->npts;
	p.pts = pts;
	for (i=0; i<poly->npts; i++) {
		pts[i].x = poly->pts[i].x + 0.5;
		pts[i].y = poly->pts[i].y + 0.5;
	}
	status = prev_poly_vector (b, save, &p, disp_mode, FALSE);
	g_free (pts);
	return status;
}
static iwImgStatus prev_render_poly_f_vector (prevBuffer *b, prevVectorSave *save,
											  const prevDataPolyF *poly, int disp_mode)
{
	return prev_poly_vector (b, save, poly, disp_mode, FALSE);
}
static iwImgStatus prev_render_fpoly_vector (prevBuffer *b, prevVectorSave *save,
											 const prevDataPoly *poly, int disp_mode)
{
	prevDataPointF *pts;
	prevDataPolyF p;
	int i;
	iwImgStatus status;

	COL_COPY(poly,p);
	pts = g_malloc (sizeof(prevDataPointF)*poly->npts);
	p.npts = poly->npts;
	p.pts = pts;
	for (i=0; i<poly->npts; i++) {
		pts[i].x = poly->pts[i].x + 0.5;
		pts[i].y = poly->pts[i].y + 0.5;
	}
	status =  prev_poly_vector (b, save, &p, disp_mode, TRUE);
	g_free (pts);
	return status;
}
static iwImgStatus prev_render_fpoly_f_vector (prevBuffer *b, prevVectorSave *save,
											   const prevDataPolyF *poly, int disp_mode)
{
	return prev_poly_vector (b, save, poly, disp_mode, TRUE);
}

/*********************************************************************
  Display circle in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void* prev_render_circle_copy (const void *data)
{
	prevDataCircle *t = g_malloc (sizeof(prevDataCircle));
	*t = *(prevDataCircle*)data;
	return t;
}
static void prev_render_circle (prevBuffer *b, const prevDataCircle *c,
								int disp_mode)
{
	prev_colInit (b, disp_mode, c->ctab, c->r, c->g, c->b, c->r);
	prev_drawCircle ((c->x-b->x)*b_zoom + b_zoom/2, (c->y-b->y)*b_zoom + b_zoom/2,
					 c->radius * b_zoom);
}
static void prev_render_fcircle (prevBuffer *b, const prevDataCircle *c,
								 int disp_mode)
{
	prev_colInit (b, disp_mode, c->ctab, c->r, c->g, c->b, c->r);
	prev_drawFCircle ((c->x-b->x)*b_zoom + b_zoom/2, (c->y-b->y)*b_zoom + b_zoom/2,
					  c->radius * b_zoom);
}

static void* prev_render_circle_f_copy (const void *data)
{
	prevDataCircleF *t = g_malloc (sizeof(prevDataCircleF));
	*t = *(prevDataCircleF*)data;
	return t;
}
static void prev_render_circle_f (prevBuffer *b, const prevDataCircleF *c,
								  int disp_mode)
{
	prev_colInit (b, disp_mode, c->ctab, c->r, c->g, c->b, c->r);
	prev_drawCircle ((c->x-b->x)*b_zoom, (c->y-b->y)*b_zoom,
					 c->radius * b_zoom);
}
static void prev_render_fcircle_f (prevBuffer *b, const prevDataCircleF *c,
								   int disp_mode)
{
	prev_colInit (b, disp_mode, c->ctab, c->r, c->g, c->b, c->r);
	prev_drawFCircle ((c->x-b->x)*b_zoom, (c->y-b->y)*b_zoom,
					  c->radius * b_zoom);
}
static iwImgStatus prev_circle_vector (prevBuffer *b, prevVectorSave *save,
									   const prevDataCircleF *c, int disp_mode, BOOL fill)
{
	float zoom = save->zoom;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (fill)
		fprintf (save->file,
				 "  <circle style=\"fill:%s\" cx=\"%f\" cy=\"%f\" r=\"%f\" />\n",
				 prev_colString(c->ctab, c->r, c->g, c->b),
				 c->x*zoom, c->y*zoom, c->radius*zoom);
	else
		fprintf (save->file,
				 "  <circle style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
				 "cx=\"%f\" cy=\"%f\" r=\"%f\" />\n",
				 prev_colString(c->ctab, c->r, c->g, c->b),
				 vector_thickness(b, zoom, disp_mode),
				 c->x*zoom, c->y*zoom, c->radius*zoom);
	return IW_IMG_STATUS_OK;
}
static iwImgStatus prev_render_circle_vector (prevBuffer *b, prevVectorSave *save,
											  const prevDataCircle *circle, int disp_mode)
{
	prevDataCircleF c;
	COL_COPY(circle,c);
	c.x = circle->x + 0.5;
	c.y = circle->y + 0.5;
	c.radius = circle->radius;
	return prev_circle_vector (b, save, &c, disp_mode, FALSE);
}
static iwImgStatus prev_render_circle_f_vector (prevBuffer *b, prevVectorSave *save,
												const prevDataCircleF *circle, int disp_mode)
{
	return prev_circle_vector (b, save, circle, disp_mode, FALSE);
}
static iwImgStatus prev_render_fcircle_vector (prevBuffer *b, prevVectorSave *save,
											   const prevDataCircle *circle, int disp_mode)
{
	prevDataCircleF c;
	COL_COPY(circle, c);
	c.x = circle->x + 0.5;
	c.y = circle->y + 0.5;
	c.radius = circle->radius;
	return prev_circle_vector (b, save, &c, disp_mode, TRUE);
}
static iwImgStatus prev_render_fcircle_f_vector (prevBuffer *b, prevVectorSave *save,
												 const prevDataCircleF *circle, int disp_mode)
{
	return prev_circle_vector (b, save, circle, disp_mode, TRUE);
}

/*********************************************************************
  Display ellipse in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void* prev_render_ellipse_copy (const void *data)
{
	prevDataEllipse *t = g_malloc (sizeof(prevDataEllipse));
	*t = *(prevDataEllipse*)data;
	return t;
}
static void prev_render_ellipse (prevBuffer *b, const prevDataEllipse *e,
								 int disp_mode)
{
	prev_colInit (b, disp_mode, e->ctab, e->r, e->g, e->b, e->r);
	prev_drawEllipse ((e->x-b->x)*b_zoom + b_zoom/2, (e->y-b->y)*b_zoom + b_zoom/2,
					  e->width*b_zoom, e->height*b_zoom,
					  e->angle, e->start, e->end, e->filled);
}

static void* prev_render_ellipse_f_copy (const void *data)
{
	prevDataEllipseF *t = g_malloc (sizeof(prevDataEllipseF));
	*t = *(prevDataEllipseF*)data;
	return t;
}
static void prev_render_ellipse_f (prevBuffer *b, const prevDataEllipseF *e,
								   int disp_mode)
{
	prev_colInit (b, disp_mode, e->ctab, e->r, e->g, e->b, e->r);
	prev_drawEllipse ((e->x-b->x)*b_zoom, (e->y-b->y)*b_zoom,
					  e->width*b_zoom, e->height*b_zoom,
					  e->angle, e->start, e->end, e->filled);
}
static iwImgStatus prev_render_ellipse_f_vector (prevBuffer *b, prevVectorSave *save,
												 const prevDataEllipseF *e, int disp_mode)
{
	float zoom = save->zoom;
	float cx, cy, sx, sy, ex, ey, sangle, cangle;
	int delta, start, end;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (e->filled)
		fprintf (save->file, "  <path style=\"fill:%s\"",
				 prev_colString(e->ctab, e->r, e->g, e->b));
	else
		fprintf (save->file, "  <path style=\"fill:none;stroke:%s;stroke-width:%.2f\"",
				 prev_colString(e->ctab, e->r, e->g, e->b),
				 vector_thickness(b, zoom, disp_mode));

	if (e->start < e->end) {
		start = e->start;
		end = e->end;
	} else {
		start = e->end;
		end = e->start;
	}
	/* Center point */
	cx = e->x*zoom;
	cy = e->y*zoom;
	/* Start point */
	sx = e->width*cos(start*G_PI / 180);
	sy = e->height*sin(start*G_PI / 180);
	/* End point */
	ex = e->width*cos(end*G_PI / 180);
	ey = e->height*sin(end*G_PI / 180);
	/* Rotation */
	sangle = sin(e->angle*G_PI/180);
	cangle = cos(e->angle*G_PI/180);
	delta = ABS(end-start);
	if (delta < 360) {
		fprintf (save->file, " d=\"M %f %f A %f,%f %d %d 0 %f,%f",
				 cx + sx*cangle - sy*sangle, cy - sx*sangle - sy*cangle,
				 e->width*zoom, e->height*zoom, 360-e->angle,
				 delta < 180 ? 0:1,
				 cx + ex*cangle - ey*sangle, cy - ex*sangle - ey*cangle);
	} else {
		float sx2 = e->width*cos((start+end)*G_PI / 360);
		float sy2 = e->height*sin((start+end)*G_PI / 360);
		fprintf (save->file, " d=\"M %f %f A %f,%f %d 1 0 %f,%f A %f,%f %d 1 0 %f,%f",
				 cx + sx*cangle - sy*sangle, cy - sx*sangle - sy*cangle,
				 e->width*zoom, e->height*zoom, 360-e->angle,
				 cx + sx2*cangle - sy2*sangle, cy - sx2*sangle - sy2*cangle,
				 e->width*zoom, e->height*zoom, 360-e->angle,
				 cx + ex*cangle - ey*sangle, cy - ex*sangle - ey*cangle);
	}
	if (e->filled) {
		if (delta < 360)
			fprintf (save->file, " L %f %f Z\" />\n", cx, cy);
		else
			fputs (" Z\" />\n", save->file);
	} else if (delta == 360)
		fputs (" Z\" />\n", save->file);
	else
		fputs ("\" />\n", save->file);

	return IW_IMG_STATUS_OK;
}
static iwImgStatus prev_render_ellipse_vector (prevBuffer *b, prevVectorSave *save,
											   const prevDataEllipse *ellipse, int disp_mode)
{
	prevDataEllipseF e;
	COL_COPY(ellipse,e);
	e.x = ellipse->x + 0.5;
	e.y = ellipse->y + 0.5;
	e.width = ellipse->width;
	e.height = ellipse->height;
	e.angle = ellipse->angle;
	e.start = ellipse->start;
	e.end = ellipse->end;
	e.filled = ellipse->filled;
	return prev_render_ellipse_f_vector (b, save, &e, disp_mode);
}

/*********************************************************************
  Display region in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void prev_drawPolygon (const Polygon_t *polygon, float zoom, BOOL mark)
{
	int i, x, y, nx, ny;

	x = (polygon->punkt[0]->x-b_pBuf->x)*zoom + zoom/2;
	y = (polygon->punkt[0]->y-b_pBuf->y)*zoom + zoom/2;
	if (mark) {
		prev_drawLine (x-3, y, x+3, y);
		prev_drawLine (x, y-3, x, y+3);
	}
	for (i=1; i<polygon->n_punkte; i++) {
		nx = (polygon->punkt[i]->x-b_pBuf->x)*zoom + zoom/2;
		ny = (polygon->punkt[i]->y-b_pBuf->y)*zoom + zoom/2;
		prev_drawLine (x, y, nx, ny);
		x = nx;
		y = ny;
		if (mark) {
			prev_drawLine (x-3, y, x+3, y);
			prev_drawLine (x, y-3, x, y+3);
		}
	}
}
static void prev_drawPolygon_vector (prevBuffer *b, prevVectorSave *save,
									 const Polygon_t *polygon,
									 int disp_mode, char *color, BOOL mark)
{
	float zoom = save->zoom;
	float thick = vector_thickness (b, zoom, disp_mode);
	int i, x, y;

	fprintf (save->file,
			 "  <polyline style=\"fill:none;stroke:%s;stroke-width:%.2f\" points=\"",
			 color, thick);
	for (i=0; i<polygon->n_punkte; i++)
		fprintf (save->file, " %d,%d",
				 (int)gui_lrint(polygon->punkt[i]->x*zoom + zoom/2),
				 (int)gui_lrint(polygon->punkt[i]->y*zoom + zoom/2));
	fprintf (save->file, "\" />\n");

	if (mark) {
		for (i=0; i<polygon->n_punkte; i++) {
			x = gui_lrint(polygon->punkt[i]->x*zoom + zoom/2);
			y = gui_lrint(polygon->punkt[i]->y*zoom + zoom/2);
			fprintf (save->file,
					 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
					 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n"
					 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
					 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n",
					 color, thick, x-3, y, x+3, y,
					 color, thick, x, y-3, x, y+3);
		}
	}
}
static void prev_render_region_opts (prevBuffer *b)
{
	static char *regionOpts[] = {"None","Center of Mass","Pixel Count",
								 "Length of Polygon","Eccentricity","Compactness",
								 "AvgConfVal","Rating","Rating Kalman","Rating Motion",
								 "ID", NULL};
	prevOptsRegion *opts = calloc (1, sizeof(prevOptsRegion));
	opts->display = DISP_COM;
	opts->show_points = FALSE;
	opts->white = FALSE;

	prev_opts_store (b, PREV_REGION, opts, TRUE);

	opts_radio_create ((long)b, "Region/RegDisplay",
					   "Information from a region, which should be displayed",
					   regionOpts, &opts->display);
	opts_toggle_create ((long)b, "Region/Show Points", "Mark all points of the region?",
						&opts->show_points);
	opts_toggle_create ((long)b, "Region/Draw white", "Render region in white?",
						&opts->white);

	prev_opts_append (b, PREV_TEXT, -1);
}
static void prev_render_region_free (void *data)
{
	iwRegion *reg = (iwRegion*)data;

	g_free (reg->r.polygon.punkt[0]);
	g_free (reg->r.polygon.punkt);

	if (reg->r.n_einschluss > 0) {
		g_free (reg->r.einschluss[0]);
		g_free (reg->r.einschluss);
	}
	g_free (reg);
}
static void* prev_render_region_copy (const void *data)
{
	int i, k, npunkte;
	Punkt_t *punkte, **pktbar, **pnew, **pold;
	Polygon_t *polygone = NULL;

	iwRegion *reg = (iwRegion*)data;
	iwRegion *t = g_malloc (sizeof(iwRegion));

	*t = *reg;

	npunkte = reg->r.polygon.n_punkte;
	for (i=0; i<reg->r.n_einschluss; i++)
		npunkte += reg->r.einschluss[i]->n_punkte;

	punkte = g_malloc (npunkte*sizeof(Punkt_t));
	pktbar = g_malloc (npunkte*sizeof(Punkt_t*));

	npunkte = 0;

	pnew = t->r.polygon.punkt = pktbar + npunkte;
	npunkte += reg->r.polygon.n_punkte;
	pold = reg->r.polygon.punkt;
	for (k=0; k<reg->r.polygon.n_punkte; k++) {
		pnew[k] = punkte++;
		pnew[k]->x = pold[k]->x;
		pnew[k]->y = pold[k]->y;
	}

	if (reg->r.n_einschluss >= 0) {
		polygone = g_malloc (reg->r.n_einschluss*sizeof(Polygon_t));
		t->r.einschluss = g_malloc (reg->r.n_einschluss*sizeof(Polygon_t*));

		for (i=0; i<reg->r.n_einschluss; i++) {
			t->r.einschluss[i] = polygone++;
			t->r.einschluss[i]->n_punkte = reg->r.einschluss[i]->n_punkte;
			pnew = t->r.einschluss[i]->punkt = pktbar + npunkte;
			npunkte += reg->r.einschluss[i]->n_punkte;
			pold = reg->r.einschluss[i]->punkt;
			for (k=0; k<reg->r.einschluss[i]->n_punkte; k++) {
				pnew[k] = punkte++;
				pnew[k]->x = pold[k]->x;
				pnew[k]->y = pold[k]->y;
			}
		}
	}

	return t;
}
static void prev_render_region (prevBuffer *b, const iwRegion *region,
								int disp_mode)
{
	prevOptsRegion **opts;
	char str[15];
	int x, y, info;
	iwColtab ctab;
	gboolean mark;

	if (region->r.pixelanzahl <= 0)
		return;
	opts = (prevOptsRegion**)prev_opts_get (b, PREV_REGION);

	if (opts && (*opts)->white) {
		prev_colInit (b, disp_mode, IW_RGB, 255, 255, 255, 255);
	} else {
		switch (region->r.echtfarbe.modell) {
			case FARB_MODELL_YUV: ctab = IW_YUV;
				break;
			case FARB_MODELL_RGB: ctab = IW_RGB;
				break;
			case FARB_MODELL_SW: ctab = IW_BW;
				break;
			default: ctab = IW_INDEX;
				break;
		}
		prev_colInit (b, disp_mode, ctab,
					  region->r.echtfarbe.x, region->r.echtfarbe.y,
					  region->r.echtfarbe.z, region->r.farbe);
	}
	if (b_dispMode & REGION_MASK) {
		info = (b_dispMode & REGION_MASK);
	} else if (opts) {
		info = (((*opts)->display - DISP_NONE + 1)<<FONT_SHIFT) & REGION_MASK;
	} else
		info = 0;
	mark = opts && (*opts)->show_points;

	prev_drawPolygon (&region->r.polygon, b_zoom, mark);
	for (x=0; x<region->r.n_einschluss; x++)
		prev_drawPolygon (region->r.einschluss[x], b_zoom, mark);

	x = gui_lrint ((region->r.schwerpunkt.x-b->x)*b_zoom + b_zoom/2);
	y = gui_lrint ((region->r.schwerpunkt.y-b->y)*b_zoom + b_zoom/2);
	prev_drawLine (x-5, y, x+5, y);
	prev_drawLine (x, y-5, x, y+5);

	if (info == REGION_COM) {
		sprintf (str, "(%ld,%ld)", gui_lrint(region->r.schwerpunkt.x),
				 gui_lrint(region->r.schwerpunkt.y));
		prev_drawString (x, y, str);
	} else if (info == REGION_PCOUNT) {
		sprintf (str, "(%d)", region->r.pixelanzahl);
		prev_drawString (x, y, str);
	} else if (info == REGION_LENGTH) {
		sprintf (str, "(%d)", region->r.umfang);
		prev_drawString (x, y, str);
	} else if (info == REGION_ECCENTRICITY) {
		sprintf (str, "(%.2f)", region->r.exzentrizitaet);
		prev_drawString (x, y, str);
	} else if (info == REGION_COMPACTNESS) {
		sprintf (str, "(%.2f)", region->r.compactness);
		prev_drawString (x, y, str);
	} else if (info == REGION_AVGCONF) {
		sprintf (str, "(%.0f)", region->avgConf);
		prev_drawString (x, y, str);
	} else if (info == REGION_RATING) {
		sprintf (str, "(%.2f)", region->judgement);
		prev_drawString (x, y, str);
	} else if (info == REGION_RAT_KALMAN) {
		sprintf (str, "(%.2f)", region->judge_kalman);
		prev_drawString (x, y, str);
	} else if (info == REGION_RAT_MOTION) {
		sprintf (str, "(%.2f)", region->judge_motion);
		prev_drawString (x, y, str);
	} else if (info == REGION_ID) {
		sprintf (str, "(%d)", region->id);
		prev_drawString (x, y, str);
	}
}
static iwImgStatus prev_render_region_vector (prevBuffer *b, prevVectorSave *save,
											  const iwRegion *region, int disp_mode)
{
	prevDataText text;
	prevOptsRegion **opts;
	float zoom = save->zoom, thick;
	char str[15];
	char *color;
	int x, y, info;
	gboolean mark;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (region->r.pixelanzahl <= 0)
		return IW_IMG_STATUS_OK;

	opts = (prevOptsRegion**)prev_opts_get (b, PREV_REGION);
	if (opts && (*opts)->white) {
		text.ctab = IW_RGB;
		text.r = text.g = text.b = 255;
	} else {
		text.r = region->r.echtfarbe.x;
		text.g = region->r.echtfarbe.y;
		text.b = region->r.echtfarbe.z;
		switch (region->r.echtfarbe.modell) {
			case FARB_MODELL_YUV:
				text.ctab = IW_YUV;
				break;
			case FARB_MODELL_RGB:
				text.ctab = IW_RGB;
				break;
			case FARB_MODELL_SW:
				text.ctab = IW_BW;
				break;
			default:
				text.ctab = IW_INDEX;
				text.r = region->r.farbe;
				break;
		}
	}
	color = prev_colString (text.ctab, text.r, text.g, text.b);
	if (disp_mode & REGION_MASK) {
		info = (disp_mode & REGION_MASK);
	} else if (opts) {
		info = (((*opts)->display - DISP_NONE + 1)<<FONT_SHIFT) & REGION_MASK;
	} else
		info = 0;
	mark = opts && (*opts)->show_points;

	prev_drawPolygon_vector (b, save, &region->r.polygon,
							 disp_mode, color, mark);
	for (x=0; x<region->r.n_einschluss; x++)
		prev_drawPolygon_vector (b, save, region->r.einschluss[x],
								 disp_mode, color, mark);

	thick = vector_thickness (b, zoom, disp_mode);
	x = gui_lrint (region->r.schwerpunkt.x*zoom + zoom/2);
	y = gui_lrint (region->r.schwerpunkt.y*zoom + zoom/2);
	fprintf (save->file,
			 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
			 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n"
			 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
			 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n",
			 color, thick, x-5, y, x+5, y,
			 color, thick, x, y-5, x, y+5);

	text.x = x;
	text.y = y;
	text.text = str;
	if (info == REGION_COM) {
		sprintf (str, "(%ld,%ld)", gui_lrint(region->r.schwerpunkt.x),
				 gui_lrint(region->r.schwerpunkt.y));
	} else if (info == REGION_PCOUNT) {
		sprintf (str, "(%d)", region->r.pixelanzahl);
	} else if (info == REGION_LENGTH) {
		sprintf (str, "(%d)", region->r.umfang);
	} else if (info == REGION_ECCENTRICITY) {
		sprintf (str, "(%.2f)", region->r.exzentrizitaet);
	} else if (info == REGION_COMPACTNESS) {
		sprintf (str, "(%.2f)", region->r.compactness);
	} else if (info == REGION_AVGCONF) {
		sprintf (str, "(%.0f)", region->avgConf);
	} else if (info == REGION_RATING) {
		sprintf (str, "(%.2f)", region->judgement);
	} else if (info == REGION_RAT_KALMAN) {
		sprintf (str, "(%.2f)", region->judge_kalman);
	} else if (info == REGION_RAT_MOTION) {
		sprintf (str, "(%.2f)", region->judge_motion);
	} else if (info == REGION_ID) {
		sprintf (str, "(%d)", region->id);
	}
	prev_render_text_vector (b, save, &text, disp_mode);

	return IW_IMG_STATUS_OK;
}

/*********************************************************************
  Display region in buffer b.
  Mode of display disp_mode defined by the flags in header file.
*********************************************************************/
static void prev_render_COMinfo_opts (prevBuffer *b)
{
	static char *comOpts[] = {"None","Center of Mass","Pixel Count",NULL};
	int *com = (int*)prev_opts_store (b, PREV_COMINFO,
									  GINT_TO_POINTER(DISP_COM), FALSE);
	opts_radio_create ((long)b, "COMDisplay",
					   "Information from a COMinfos, which should be displayed",
					   comOpts, com);
	prev_opts_append (b, PREV_TEXT, -1);
}
static void prev_render_COMinfo_free (void *data)
{
	g_free (data);
}
static void* prev_render_COMinfo_copy (const void *data)
{
	iwRegCOMinfo *t = g_malloc (sizeof(iwRegCOMinfo));
	*t = *(iwRegCOMinfo*)data;
	return t;
}
static void prev_render_COMinfo (prevBuffer *b, const iwRegCOMinfo *region,
								 int disp_mode)
{
	char str[15];
	int x, y, info, *infoPtr;

	if (region->pixelcount <= 0)
		return;

	prev_colInit (b, disp_mode, IW_INDEX, 0, 0, 0, region->color);

	x = (region->com_x-b->x)*b_zoom + b_zoom/2;
	y = (region->com_y-b->y)*b_zoom + b_zoom/2;

	prev_drawLine (x-5, y, x+5, y);
	prev_drawLine (x, y-5, x, y+5);

	if (b_dispMode & REGION_MASK) {
		info = (b_dispMode & REGION_MASK);
	} else if ((infoPtr = (int*)prev_opts_get (b, PREV_COMINFO))) {
		info = ((*infoPtr - DISP_NONE + 1)<<FONT_SHIFT) & REGION_MASK;
	} else
		info = 0;

	if (info == REGION_COM) {
		sprintf (str, "(%d,%d)", region->com_x, region->com_y);
		prev_drawString (x, y, str);
	} else if (info == REGION_PCOUNT) {
		sprintf (str, "(%d)", region->pixelcount);
		prev_drawString (x, y, str);
	}
}
static iwImgStatus prev_render_COMinfo_vector (prevBuffer *b, prevVectorSave *save,
											   const iwRegCOMinfo *region, int disp_mode)
{
	float zoom = save->zoom, thick;
	char str[15];
	char *color;
	int x, y, info, *infoPtr;
	prevDataText text;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (region->pixelcount <= 0)
		return IW_IMG_STATUS_OK;

	color = prev_colString (IW_INDEX, region->color, 0, 0);

	thick = vector_thickness (b, zoom, disp_mode);
	x = gui_lrint (region->com_x*zoom + zoom/2);
	y = gui_lrint (region->com_y*zoom + zoom/2);
	fprintf (save->file,
			 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
			 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n"
			 "  <line style=\"fill:none;stroke:%s;stroke-width:%.2f\" "
			 "x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />\n",
			 color, thick, x-5, y, x+5, y,
			 color, thick, x, y-5, x, y+5);

	if (disp_mode & REGION_MASK) {
		info = (disp_mode & REGION_MASK);
	} else if ((infoPtr = (int*)prev_opts_get (b, PREV_COMINFO))) {
		info = ((*infoPtr - DISP_NONE + 1)<<FONT_SHIFT) & REGION_MASK;
	} else
		info = 0;

	text.x = x;
	text.y = y;
	text.ctab = IW_INDEX;
	text.r = region->color;
	text.text = str;
	if (info == REGION_COM) {
		sprintf (str, "(%d,%d)", region->com_x, region->com_y);
	} else if (info == REGION_PCOUNT) {
		sprintf (str, "(%d)", region->pixelcount);
	}
	prev_render_text_vector (b, save, &text, disp_mode);

	return IW_IMG_STATUS_OK;
}

/*********************************************************************
  Draw the data (
        - one element of type type
        - a list, length: cnt, size of one element: size, type:type
        - a set of different elements with cnt elements)
  in buffer b, such that an area of size width x height fits into the
  buffer. If width/height < 0 or RENDER_CLEAR is not set, the last
  values for this buffer b are used. Mode of display disp_mode
  defined by flags in header file. If no flags are set and option
  widgets are appended to the window, these widgets are used.
*********************************************************************/
void prev_render_data (prevBuffer *b, prevType type, const void *data,
					   int disp_mode, int width, int height)
{
	renderDbRenderFunc r_fkt;

	if (prev_prepare_draw (b, disp_mode, width, height)) {
		if ((r_fkt = prev_renderdb_get_render (type))) {
			prev_rdata_append (b, type, data, disp_mode);
			r_fkt (b, data, disp_mode);
		}
		prev_buffer_unlock();
	}
}
void prev_render_list (prevBuffer *b, prevType type, const void *data,
					   int size, int cnt,
					   int disp_mode, int width, int height)
{
	int n;
	renderDbRenderFunc r_fkt;

	if (prev_prepare_draw (b, disp_mode, width, height)) {
		if ((r_fkt = prev_renderdb_get_render (type))) {
			for (n=0; n<cnt; n++) {
				prev_rdata_append (b, type, data, disp_mode);
				r_fkt (b, data, disp_mode);
				data = (char*)data+size;
			}
		}
		prev_buffer_unlock();
	}
}
void prev_render_set (prevBuffer *b, const prevData *data, int cnt,
					  int disp_mode, int width, int height)
{
	int n;
	renderDbRenderFunc r_fkt;

	if (prev_prepare_draw (b, disp_mode, width, height)) {
		for (n=0; n<cnt; n++, data++) {
			prev_rdata_append (b, data->type, data->data, disp_mode);
			r_fkt = prev_renderdb_get_render (data->type);
			if (r_fkt) r_fkt (b, data->data, disp_mode);
		}
		prev_buffer_unlock();
	}
}

/*********************************************************************
  Wrapper around prev_render_list().
*********************************************************************/
void prev_render_texts (prevBuffer *b, const prevDataText *texts, int cnt,
						int disp_mode, int width, int height)
{ prev_render_list (b, PREV_TEXT, texts, sizeof(prevDataText),
					cnt, disp_mode, width, height); }

void prev_render_lines (prevBuffer *b, const prevDataLine *lines, int cnt,
						int disp_mode, int width, int height)
{ prev_render_list (b, PREV_LINE, lines, sizeof(prevDataLine),
					cnt, disp_mode, width, height); }

void prev_render_rects (prevBuffer *b, const prevDataRect *rects, int cnt,
						int disp_mode, int width, int height)
{ prev_render_list (b, PREV_RECT, rects, sizeof(prevDataRect),
					cnt, disp_mode, width, height); }

void prev_render_frects (prevBuffer *b, const prevDataRect *frects, int cnt,
						 int disp_mode, int width, int height)
{ prev_render_list (b, PREV_FRECT, frects, sizeof(prevDataRect),
					cnt, disp_mode, width, height); }

void prev_render_regions (prevBuffer *b, const iwRegion *regions, int cnt,
						  int disp_mode, int width, int height)
{ prev_render_list (b, PREV_REGION, regions, sizeof(iwRegion),
					cnt, disp_mode, width, height); }

void prev_render_polys (prevBuffer *b, const prevDataPoly *polys, int cnt,
						int disp_mode, int width, int height)
{ prev_render_list (b, PREV_POLYGON, polys, sizeof(prevDataPoly),
					cnt, disp_mode, width, height); }

void prev_render_fpolys (prevBuffer *b, const prevDataPoly *polys, int cnt,
						 int disp_mode, int width, int height)
{ prev_render_list (b, PREV_FPOLYGON, polys, sizeof(prevDataPoly),
					cnt, disp_mode, width, height); }

void prev_render_circles (prevBuffer *b, const prevDataCircle *circ, int cnt,
						  int disp_mode, int width, int height)
{ prev_render_list (b, PREV_CIRCLE, circ, sizeof(prevDataCircle),
					cnt, disp_mode, width, height); }

void prev_render_fcircles (prevBuffer *b, const prevDataCircle *circ, int cnt,
						   int disp_mode, int width, int height)
{ prev_render_list (b, PREV_FCIRCLE, circ, sizeof(prevDataCircle),
					cnt, disp_mode, width, height); }

void prev_render_ellipses (prevBuffer *b, const prevDataEllipse *ellipse, int cnt,
						   int disp_mode, int width, int height)
{ prev_render_list (b, PREV_ELLIPSE, ellipse, sizeof(prevDataEllipse),
					cnt, disp_mode, width, height); }

void prev_render_COMinfos (prevBuffer *b, const iwRegCOMinfo *regions, int cnt,
						   int disp_mode, int width, int height)
{ prev_render_list (b, PREV_COMINFO, regions, sizeof(iwRegCOMinfo),
					cnt, disp_mode, width, height); }

void prev_render_imgs (prevBuffer *b, const prevDataImage *imgs, int cnt,
					   int disp_mode, int width, int height)
{
	if (b->window && (b->save == SAVE_ORIG || b->save == SAVE_ORIGSEQ)) {
		prev_chk_save (b, imgs[0].i);
		b->save_done = TRUE;
	}

	prev_render_list (b, PREV_IMAGE, imgs, sizeof(prevDataImage),
					  cnt, disp_mode, width, height);
}

/*********************************************************************
  Render planes of size width x height and color mode/table ctab
  (IW_YUV, IW_RGB, ..., or an own table) in preview window b.
*********************************************************************/
void prev_render (prevBuffer *b, guchar **planes, int width, int height, iwColtab ctab)
{
	prevDataImage img;
	iwImage i;

	iw_img_init (&i);
	i.planes = (ctab > IW_COLFORMAT_MAX || b->gray || ctab == IW_BW) ? 1:3;
	i.type = IW_8U;
	i.width = width;
	i.height = height;
	i.ctab = ctab;
	i.data = planes;

	img.i = &i;
	img.x = 0;
	img.y = 0;

	prev_render_imgs (b, &img, 1, RENDER_CLEAR, width, height);
}

/*********************************************************************
  Render the img src (size: w x h) in buffer b by using the function
  dest = src * col_step % 256.
  If col_step > 255 it is interpreted as a color table for rendering
  the dest image (and a col_Step of 1 is used).
*********************************************************************/
void prev_render_int (prevBuffer *b, const gint32 *src, int w, int h, long col_step)
{
	if (b->window) {
		guchar *d, *buffer = gui_get_buffer (w*h);
		iwColtab ctab = IW_INDEX;
		int x;

		if (col_step < 1) {
			col_step = 1;
		} else if (col_step > 255) {
			ctab = (iwColtab) col_step;
			col_step = 1;
		}

		d = buffer;
		for (x = w*h; x>0; x--) {
			if (*src>0) {
				*d++ = *src++ * col_step % 256;
			} else {
				*d++ = 0;
				src++;
			}
		}
		prev_render (b, &buffer, w, h, ctab);
		gui_release_buffer (buffer);
	}
}

/*********************************************************************
  Render str in b at position (x,y). Printf style arguments and the
  formating options of prev_drawString() are supported.
*********************************************************************/
void prev_render_text (prevBuffer *b, int disp_mode, int width, int height,
					   int x, int y, const char *format, ...)
{
	va_list args;
	prevDataText t;

	t.ctab = IW_INDEX;
	t.r = weiss;
	t.x = x;
	t.y = y;

	va_start (args, format);
	t.text = g_strdup_vprintf (format, args);
	va_end (args);

	prev_render_texts (b, &t, 1, disp_mode, width, height);
	g_free (t.text);
}

/*********************************************************************
  PRIVATE: Called from prev_init() to initialise the render functions.
*********************************************************************/
void prev_render_init (void)
{
	prev_renderdb_register (PREV_TEXT, prev_render_text_opts,
							prev_render_text_free,
							prev_render_text_copy,
							(renderDbRenderFunc)prev_render_text_do);
	prev_renderdb_register_vector (PREV_TEXT,
								   (renderDbVectorFunc)prev_render_text_vector);
	prev_renderdb_register (PREV_LINE, NULL,
							g_free,
							prev_render_line_copy,
							(renderDbRenderFunc)prev_render_line);
	prev_renderdb_register_vector (PREV_LINE,
								   (renderDbVectorFunc)prev_render_line_vector);
	prev_renderdb_register (PREV_RECT, NULL,
							g_free,
							prev_render_rect_copy,
							(renderDbRenderFunc)prev_render_rect);
	prev_renderdb_register_vector (PREV_RECT,
								   (renderDbVectorFunc)prev_render_rect_vector);
	prev_renderdb_register (PREV_FRECT, NULL,
							g_free,
							prev_render_rect_copy,
							(renderDbRenderFunc)prev_render_frect);
	prev_renderdb_register_vector (PREV_FRECT,
								   (renderDbVectorFunc)prev_render_frect_vector);
	prev_renderdb_register (PREV_REGION, prev_render_region_opts,
							prev_render_region_free,
							prev_render_region_copy,
							(renderDbRenderFunc)prev_render_region);
	prev_renderdb_register_vector (PREV_REGION,
								   (renderDbVectorFunc)prev_render_region_vector);
	prev_renderdb_register (PREV_POLYGON, NULL,
							prev_render_poly_free,
							prev_render_poly_copy,
							(renderDbRenderFunc)prev_render_poly);
	prev_renderdb_register_vector (PREV_POLYGON,
								   (renderDbVectorFunc)prev_render_poly_vector);
	prev_renderdb_register (PREV_FPOLYGON, NULL,
							prev_render_poly_free,
							prev_render_poly_copy,
							(renderDbRenderFunc)prev_render_fpoly);
	prev_renderdb_register_vector (PREV_FPOLYGON,
								   (renderDbVectorFunc)prev_render_fpoly_vector);
	prev_renderdb_register (PREV_CIRCLE, NULL,
							g_free,
							prev_render_circle_copy,
							(renderDbRenderFunc)prev_render_circle);
	prev_renderdb_register_vector (PREV_CIRCLE,
								   (renderDbVectorFunc)prev_render_circle_vector);
	prev_renderdb_register (PREV_FCIRCLE, NULL,
							g_free,
							prev_render_circle_copy,
							(renderDbRenderFunc)prev_render_fcircle);
	prev_renderdb_register_vector (PREV_FCIRCLE,
								   (renderDbVectorFunc)prev_render_fcircle_vector);
	prev_renderdb_register (PREV_ELLIPSE, NULL,
							g_free,
							prev_render_ellipse_copy,
							(renderDbRenderFunc)prev_render_ellipse);
	prev_renderdb_register_vector (PREV_ELLIPSE,
								   (renderDbVectorFunc)prev_render_ellipse_vector);
	prev_renderdb_register (PREV_COMINFO,prev_render_COMinfo_opts,
							prev_render_COMinfo_free,
							prev_render_COMinfo_copy,
							(renderDbRenderFunc)prev_render_COMinfo);
	prev_renderdb_register_vector (PREV_COMINFO,
								   (renderDbVectorFunc)prev_render_COMinfo_vector);
	prev_renderdb_register (PREV_IMAGE, prev_render_image_opts,
							prev_render_image_free,
							prev_render_image_copy,
							(renderDbRenderFunc)prev_render_image);
	prev_renderdb_register_vector (PREV_IMAGE,
								   (renderDbVectorFunc)prev_render_image_vector);
	prev_renderdb_register_info (PREV_IMAGE,
								 (renderDbInfoFunc)prev_render_image_info);

	prev_renderdb_register (PREV_LINE_F, NULL,
							g_free,
							prev_render_line_f_copy,
							(renderDbRenderFunc)prev_render_line_f);
	prev_renderdb_register_vector (PREV_LINE_F,
								   (renderDbVectorFunc)prev_render_line_f_vector);
	prev_renderdb_register (PREV_RECT_F, NULL,
							g_free,
							prev_render_rect_f_copy,
							(renderDbRenderFunc)prev_render_rect_f);
	prev_renderdb_register_vector (PREV_RECT_F,
								   (renderDbVectorFunc)prev_render_rect_f_vector);
	prev_renderdb_register (PREV_FRECT_F, NULL,
							g_free,
							prev_render_rect_f_copy,
							(renderDbRenderFunc)prev_render_frect_f);
	prev_renderdb_register_vector (PREV_FRECT_F,
								   (renderDbVectorFunc)prev_render_frect_f_vector);
	prev_renderdb_register (PREV_POLYGON_F, NULL,
							prev_render_poly_f_free,
							prev_render_poly_f_copy,
							(renderDbRenderFunc)prev_render_poly_f);
	prev_renderdb_register_vector (PREV_POLYGON_F,
								   (renderDbVectorFunc)prev_render_poly_f_vector);
	prev_renderdb_register (PREV_FPOLYGON_F, NULL,
							prev_render_poly_f_free,
							prev_render_poly_f_copy,
							(renderDbRenderFunc)prev_render_fpoly_f);
	prev_renderdb_register_vector (PREV_FPOLYGON_F,
								   (renderDbVectorFunc)prev_render_fpoly_f_vector);
	prev_renderdb_register (PREV_CIRCLE_F, NULL,
							g_free,
							prev_render_circle_f_copy,
							(renderDbRenderFunc)prev_render_circle_f);
	prev_renderdb_register_vector (PREV_CIRCLE_F,
								   (renderDbVectorFunc)prev_render_circle_f_vector);
	prev_renderdb_register (PREV_FCIRCLE_F, NULL,
							g_free,
							prev_render_circle_f_copy,
							(renderDbRenderFunc)prev_render_fcircle_f);
	prev_renderdb_register_vector (PREV_FCIRCLE_F,
								   (renderDbVectorFunc)prev_render_fcircle_f_vector);
	prev_renderdb_register (PREV_ELLIPSE_F, NULL,
							g_free,
							prev_render_ellipse_f_copy,
							(renderDbRenderFunc)prev_render_ellipse_f);
	prev_renderdb_register_vector (PREV_ELLIPSE_F,
								   (renderDbVectorFunc)prev_render_ellipse_f_vector);
}
