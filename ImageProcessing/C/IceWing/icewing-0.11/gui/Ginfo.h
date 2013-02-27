/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/* PRIVATE HEADER */

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

#ifndef iw_Ginfo_H
#define iw_Ginfo_H

#include "Gpreview.h"

typedef struct _infoWinData infoWinData;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return the coordinates of the currently selected rectangle from the
  ginfo measurement function.
*********************************************************************/
void ginfo_get_coords (int *x1, int *y1, int *x2, int *y2);

/*********************************************************************
  Draw ginfo specific markers into b->drawing->window.
*********************************************************************/
void ginfo_render (prevBuffer *buf);

/*********************************************************************
  Update information displayed in the info window.
  return value >0 : Button press was handled by this function.
               >1 : Window must be redrawn.
*********************************************************************/
int ginfo_update (int x, int y, int button, prevEvent signal, prevBuffer *buf);

/*********************************************************************
  Open / show a window which displays information about the cursor
  position and the color at that position.
*********************************************************************/
void ginfo_open (void);

/*********************************************************************
  If info window was open in the last session, open it again.
*********************************************************************/
void ginfo_init (void);

#ifdef __cplusplus
}
#endif

#endif /* iw_Ginfo_H */
