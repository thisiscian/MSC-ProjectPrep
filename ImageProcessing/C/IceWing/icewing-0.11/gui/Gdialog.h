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

#ifndef iw_Gdialog_H
#define iw_Gdialog_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Hide window and remember its position. So after showing it again the
  position is restored.
*********************************************************************/
void gui_dialog_hide_window (GtkWidget *window);

/*********************************************************************
  Display a message box.
*********************************************************************/
void gui_message_show (const char *title, const char *message, ...) G_GNUC_PRINTF(2, 3);
/*********************************************************************
  PRIVATE: Display a message box. Never lock the gdk mutex.
*********************************************************************/
void gui_message_show__nl (const char *title, const char *message, ...) G_GNUC_PRINTF(2, 3);

/*********************************************************************
  If !*window open a new file selection window, otherwise map the
  before opened window.
*********************************************************************/
void gui_file_selection_open (GtkWidget **window, const char *title,
							  const char *file, GtkSignalFunc func_okbut);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gdialog_H */
