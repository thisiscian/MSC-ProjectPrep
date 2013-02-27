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

#ifndef iw_Gcolor_H
#define iw_Gcolor_H

#include <gtk/gtk.h>
#include "Gimage.h"

#ifndef gui_lrint

#define	__USE_ISOC9X 1
#define __USE_ISOC99 1
#include <math.h>

#if defined(__GNUC__) && defined(__i386__)
static inline long __gui_lrint_code (double v)
{
	long ret;
	__asm__ __volatile__ ("fistpl %0" : "=m" (ret) : "t" (v) : "st") ;
	return ret;
}
#elif defined(__GLIBC__) && defined(__GNUC__) && __GNUC__ >= 3
#define __gui_lrint_code(v)	lrint(v)
#else
static inline long __gui_lrint_code (double v)
{
	if (v > 0)
		return (long)(v+0.5);
	else
		return (long)(v-0.5);
}
#endif

#  define gui_lrint(v)	__gui_lrint_code(v)

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Perform RGB to HSV conversion. The intervall is allways 0..255.
*********************************************************************/
void prev_rgbToHsv (guchar rc, guchar gc, guchar bc,
					guchar *h, guchar *s, guchar *v);

/*********************************************************************
  Perform HSV to RGB conversion. The intervall is  allways 0..255.
*********************************************************************/
void prev_hsvToRgb (guchar h, guchar s, guchar v,
					guchar *rc, guchar *gc, guchar *bc);

/*********************************************************************
  Perform YUV to RGB conversion with intervall expansion.
*********************************************************************/
void prev_yuvToRgbVis (guchar yc, guchar uc, guchar vc,
					   guchar *rc, guchar *gc, guchar *bc);

/*********************************************************************
  Perform RGB to YUV conversion with intervall reduction.
*********************************************************************/
void prev_rgbToYuvVis (guchar rc, guchar gc, guchar bc,
					   guchar *yc, guchar *uc, guchar *vc);

/* PRIVATE */
#define PREVCOL_SHIFT	10
#define PREVCOL_HALF	(1 << (PREVCOL_SHIFT - 1))
#define PREVCOL_FIX(x)	((int) ((x) * (1<<PREVCOL_SHIFT) + 0.5))

#define PREVCOL_MAX_NEG_CROP 1024
extern guchar prev_col_cropTbl[256 + 2 * PREVCOL_MAX_NEG_CROP];
/* END PRIVATE */

/*********************************************************************
  Perform YUV to RGB conversion with intervall expansion.
*********************************************************************/
static inline void prev_inline_yuvToRgbVis (guchar yc, guchar uc, guchar vc,
											  guchar *rc, guchar *gc, guchar *bc)
{
	guchar *ct = prev_col_cropTbl + PREVCOL_MAX_NEG_CROP;
	int U = uc - 128;
	int V = vc - 128;
	int r_add =   PREVCOL_FIX(1.40252*255.0/224.0) * V;
	int g_add = - PREVCOL_FIX(0.34434*255.0/224.0) * U
				- PREVCOL_FIX(0.71440*255.0/224.0) * V;
	int b_add =   PREVCOL_FIX(1.77304*255.0/224.0) * U;
	int y = PREVCOL_FIX(255.0/219.0) * (yc - 16) + PREVCOL_HALF;

	*rc = ct[(y + r_add) >> PREVCOL_SHIFT];
	*gc = ct[(y + g_add) >> PREVCOL_SHIFT];
	*bc = ct[(y + b_add) >> PREVCOL_SHIFT];
}

/*********************************************************************
  Perform RGB to YUV conversion with intervall reduction.
*********************************************************************/
static inline void prev_inline_rgbToYuvVis (guchar rc, guchar gc, guchar bc,
											  guchar *yc, guchar *uc, guchar *vc)

{
	int y_shift = PREVCOL_FIX(0.299*219.0/255.0) * rc +
				  PREVCOL_FIX(0.587*219.0/255.0) * gc +
				  PREVCOL_FIX(0.114*219.0/255.0) * bc + PREVCOL_HALF;
	int y = y_shift >> PREVCOL_SHIFT;

	*uc = ((PREVCOL_FIX(0.564*224.0/255.0) * bc -
			PREVCOL_FIX(0.564*224.0/219.0) * y + PREVCOL_HALF) >> PREVCOL_SHIFT) + 128;
	*vc = ((PREVCOL_FIX(0.713*224.0/255.0) * rc -
			PREVCOL_FIX(0.713*224.0/219.0) * y + PREVCOL_HALF) >> PREVCOL_SHIFT) + 128;
	*yc = (y_shift + (16 << PREVCOL_SHIFT)) >> PREVCOL_SHIFT;
}

/*********************************************************************
  Perform RGB to YUV conversion without intervall change.
*********************************************************************/
static inline void  prev_inline_rgbToYuvCal (guchar rc, guchar gc, guchar bc,
											   guchar *yc, guchar *uc, guchar *vc)
{
	guchar *ct = prev_col_cropTbl + PREVCOL_MAX_NEG_CROP;
	int y = (PREVCOL_FIX(0.299) * rc +
			 PREVCOL_FIX(0.587) * gc +
			 PREVCOL_FIX(0.114) * bc + PREVCOL_HALF) >> PREVCOL_SHIFT;
	*yc = y;
	*uc = ct[((PREVCOL_FIX(0.564*224.0/219.0) * (bc-y) +
			   PREVCOL_HALF) >> PREVCOL_SHIFT) + 128];
	*vc = ct[((PREVCOL_FIX(0.713*224.0/219.0) * (rc-y) +
			   PREVCOL_HALF) >> PREVCOL_SHIFT) + 128];
}

/*********************************************************************
  PRIVATE: Initialize the color module.
*********************************************************************/
void prev_col_init (void);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gcolor_H */
