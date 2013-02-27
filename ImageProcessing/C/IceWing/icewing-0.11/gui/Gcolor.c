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
#include "Gtools.h"
#include "Gimage.h"
#include "Gcolor.h"

#define IMG_COLMAX_F ((float)IW_COLMAX)

/*********************************************************************
  Perform RGB to HSV conversion. The intervall is allways 0..255.
*********************************************************************/
void prev_rgbToHsv (guchar rc, guchar gc, guchar bc,
					guchar *h, guchar *s, guchar *v)

{
	int maximum, minimum, delta, r, g, b;
	float hf;

	r = rc;
	b = bc;
	g = gc;

	maximum = (int) (r > g) ? (r > b ? r : b) : (g > b ? g : b);
	minimum = (int) (r < g) ? (r < b ? r : b) : (g < b ? g : b);

	*v = (guchar)maximum;

	if (minimum == maximum)
		*h = *s = 0;
	else {
		delta = (maximum-minimum);
		*s = gui_lrint (((float)delta / (float)maximum) * IMG_COLMAX_F);

		if (r == maximum)
			hf = (float)(g-b)/(float)(delta);
		else if (g == maximum)
			hf = 2.0 + (float)(b-r) / (float)(delta);
		else
			hf = 4.0 + (float)(r-g) / (float)(delta);
		hf = hf < 0 ? hf + 6.0 : hf;
		*h = gui_lrint (hf / 6.0 * IMG_COLMAX_F);
	}
}

/*********************************************************************
  Perform HSV to RGB conversion. The intervall is  allways 0..255.
*********************************************************************/
void prev_hsvToRgb (guchar h, guchar s, guchar v,
					guchar *rc, guchar *gc, guchar *bc)

{
	float h1, f, sf;
	int i;

	i = h*6;
	h1 = (float)i/IMG_COLMAX_F;
	i /= IW_COLMAX;
	f = h1-i;
	sf = (float)s/IMG_COLMAX_F;

	switch (i) {
		case 0:
		case 6:
			*rc = v;
			*bc = gui_lrint ((float)v * (1.0 - sf));
			*gc = gui_lrint ((float)v * (1.0 - sf*(1.0-f)));
			break;
		case 1:
			*rc = gui_lrint ((float)v * (1.0 - sf*f));
			*bc = gui_lrint ((float)v * (1.0 - sf));
			*gc = v;
			break;
		case 2:
			*rc = gui_lrint ((float)v * (1.0 - sf));
			*bc = gui_lrint ((float)v * (1.0 - sf*(1.0-f)));
			*gc = v;
			break;
		case 3:
			*rc = gui_lrint ((float)v * (1.0 - sf));
			*bc = v;
			*gc = gui_lrint ((float)v * (1.0 - sf*f));
			break;
		case 4:
			*rc = gui_lrint ((float)v * (1.0 - sf*(1.0-f)));
			*bc = v;
			*gc = gui_lrint ((float)v * (1.0 - sf));
			break;
		case 5:
			*rc = v;
			*bc = gui_lrint ((float)v * (1.0 - sf*f));
			*gc = gui_lrint ((float)v * (1.0 - sf));
			break;
	}
}

/*********************************************************************
  Perform YUV to RGB conversion with intervall expansion.
*********************************************************************/
void prev_yuvToRgbVis (guchar yc, guchar uc, guchar vc,
					   guchar *rc, guchar *gc, guchar *bc)
{
	prev_inline_yuvToRgbVis (yc, uc, vc, rc, gc, bc);
}

/*********************************************************************
  Perform RGB to YUV conversion with intervall reduction.
*********************************************************************/
void prev_rgbToYuvVis (guchar rc, guchar gc, guchar bc,
					   guchar *yc, guchar *uc, guchar *vc)

{
	prev_inline_rgbToYuvVis (rc, gc, bc, yc, uc, vc);
}

guchar prev_col_cropTbl[256 + 2 * PREVCOL_MAX_NEG_CROP];

/*********************************************************************
  PRIVATE: Initialize the color module.
*********************************************************************/
void prev_col_init (void)
{
	int i;

	for (i = 0; i < 256; i++)
		prev_col_cropTbl[i + PREVCOL_MAX_NEG_CROP] = i;
	for (i = 0; i < PREVCOL_MAX_NEG_CROP; i++) {
		prev_col_cropTbl[i] = 0;
		prev_col_cropTbl[i + PREVCOL_MAX_NEG_CROP + 256] = 255;
	}
}
