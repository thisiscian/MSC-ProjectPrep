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

#ifndef iw_Gsession_H
#define iw_Gsession_H

#include <gtk/gtk.h>

#include "Gpreview.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return name of the default session file. Returned value is a pointer
  to a static variable.
*********************************************************************/
char *iw_session_get_name (void);

/*********************************************************************
  Set name of the session file.
*********************************************************************/
void iw_session_set_name (const char *name);

/*********************************************************************
  Delete the session file and thus clear the session.
*********************************************************************/
void iw_session_clear (void);

/*********************************************************************
  Save session information for all open windows to file
  `iw_session_get_name()`.
*********************************************************************/
void iw_session_save (void);

/*********************************************************************
  Load new (not already loaded) session information from file
  `iw_session_get_name()`.
*********************************************************************/
gboolean iw_session_load (void);

/*********************************************************************
  PRIVATE: Apply the session entry for 'title', free the entry
  afterwards, and add window to the list of session managed windows.
  set_pos: Set position of window ?
  set_size: Set size of window ?
  Return: Session entry for 'title' found and applied?
*********************************************************************/
gboolean iw_session_set_prev (GtkWidget *window, prevBuffer *buf,
							  const char *title, gboolean set_pos, gboolean set_size);

/*********************************************************************
  Apply the session entry for 'title', free the entry afterwards, and
  add window to the list of session managed windows.
  set_pos: Set position of window ?
  set_size: Set size of window ?
  Return: Session entry for 'title' found and applied?
*********************************************************************/
gboolean iw_session_set (GtkWidget *window, const char *title,
						 gboolean set_pos, gboolean set_size);

/*********************************************************************
  Return if a session entry with 'title' was loaded.
*********************************************************************/
gboolean iw_session_find (const char *title);

/*********************************************************************
  Remove window with 'title' from the list of session managed windows,
  e.g. if window was closed.
*********************************************************************/
void iw_session_remove (const char *title);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gsession_H */
