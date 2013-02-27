/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Jannik Fritsch and Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Applied Computer Science, Faculty of Technology, Bielefeld University, Germany
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

#ifndef iw_skin_H
#define iw_skin_H

#include "tools/tools.h"

#define SKIN_SCALE 1024

typedef struct {
	double r;
	double g;
} t_Pixel;

typedef struct {
	BOOL excludeWhite;
	int useLocus;
} skinOptions;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 Convert YUV-Pixel to rg-Pixel
*********************************************************************/
void iw_skin_yuv2rg (uchar y, uchar u, uchar v, t_Pixel *rg);

/*********************************************************************
  Return true if rg-value is white pixel and in skinlocus.
*********************************************************************/
BOOL iw_skin_pixelIsSkin (skinOptions *opts, t_Pixel *rg);

/*********************************************************************
  Add GUI elements for skin locus management to page.
*********************************************************************/
skinOptions* iw_skin_init (int page);

#ifdef __cplusplus
}
#endif

#endif /* iw_skin_H */
