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

#ifndef iw_Gpreview_i_H
#define iw_Gpreview_i_H

#include "Gimage.h"
#include "Gpreview.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return the render function associated with type.
*********************************************************************/
renderDbRenderFunc prev_renderdb_get_render (prevType type);

/*********************************************************************
  Call the renderdb info-funktion for all data stored with
  prev_rdata_append(). Return a newly allocated string of all strings
  returned from the funktions (separated by '\n').
*********************************************************************/
	char* prev_rdata_get_info (prevBuffer *b, int x, int y, int radius);

/*********************************************************************
  Free the data stored with prev_rdata_append(). Should be called if
  the buffer is cleared (RENDER_CLEAR specified).
  full: Free also the maintenance structures.
*********************************************************************/
void prev_rdata_free (prevBuffer *b, gboolean full);

/*********************************************************************
  Store data needed for repainting of the type 'type' with mode
  disp_mode in b. Copying of the data is done by calling
  renderDbCopyFunc().
*********************************************************************/
void prev_rdata_append (prevBuffer *b, prevType type, const void *data, int disp_mode);

/*********************************************************************
  Return the deafult width/height for new preview windows.
*********************************************************************/
int prev_get_default_width (void);
int prev_get_default_height (void);

/*********************************************************************
  Lock saveing-mutex and close all AVI-files of all buffer.
*********************************************************************/
void prev_close_avi_all (void);

/*********************************************************************
  If zoom >= 0, change zoom level of the preview. If x >= 0 or
  y >= 0 pan the preview to that position. Never lock the gdk mutex.
*********************************************************************/
void prev_pan_zoom__nl (prevBuffer *b, int x, int y, float zoom);

/*********************************************************************
  Initialise the preview window selection list. prev_width
  and prev_height are the default sizes for a preview window.
*********************************************************************/
void prev_init (GtkBox *vbox, int prev_width, int prev_height);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gpreview_i_H */
