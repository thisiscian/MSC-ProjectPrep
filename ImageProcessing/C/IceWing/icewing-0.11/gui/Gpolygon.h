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

#ifndef iw_Gpolygon_H
#define iw_Gpolygon_H

#include "Grender.h"

typedef void (*prevPolyFunc) (int x, int y, int cnt, void *data);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Fill the polygon defined by the points pts (or everything except
  the polygon if invers==TRUE).
  width, height: clipping range for the polygon
  npts         : number of points
  fkt          : is called with data as argument
*********************************************************************/
gboolean prev_drawFPoly_raw (int width, int height, int npts, const prevDataPoint *pts,
							 gboolean invers, prevPolyFunc fkt, void *data);
gboolean prev_drawConvexPoly_raw (int width, int height, int npts, const prevDataPoint *pts,
								  gboolean invers, prevPolyFunc fkt, void *data);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gpolygon_H */
