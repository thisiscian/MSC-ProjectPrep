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

#ifndef iw_Ggui_i_H
#define iw_Ggui_i_H

#include "Ggui.h"

extern pthread_t gui_thread;

#define GUI_IS_GUI_THREAD() (pthread_equal(gui_thread, pthread_self()))

#define gui_threads_enter()	G_STMT_START {				\
	if (!GUI_IS_GUI_THREAD()) gdk_threads_enter();		\
} G_STMT_END

#define gui_threads_leave()	G_STMT_START {				\
	if (!GUI_IS_GUI_THREAD()) gdk_threads_leave();		\
} G_STMT_END

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  If gui_exit_lock() is not active, call gui_exit().
  Otherwise schedule an exit after gui_exit_unlock() and return.
*********************************************************************/
void gui_try_exit (int status);

/*********************************************************************
  Lock/Unlock gui_try_exit().
*********************************************************************/
void gui_exit_lock (void);
void gui_exit_unlock (void);

/*********************************************************************
  Set a tooltip for widget 'widget'.
*********************************************************************/
void gui_tooltip_set (GtkWidget *widget, const gchar *tip_text);

/*********************************************************************
  Append a new page to a clist/notebook-combination.
*********************************************************************/
GtkWidget *gui_listbook_append (GtkWidget *ctree, const gchar *label);

/*********************************************************************
  Set version and name of programm. Used for files and windows.
*********************************************************************/
void gui_set_prgname (char *prgname, char *version);

/*********************************************************************
  Start main window iconified? Should be called before gui_init().
*********************************************************************/
void gui_set_iconified (gboolean iconified);

/*********************************************************************
  Use icon for all new windows as a window icon. Should be called
  before gui_init().
*********************************************************************/
void gui_set_icon (char **icon);

/*********************************************************************
  Can be used as a GtkSignalFunc() for a callback to exit the program.
*********************************************************************/
gboolean gui_delete_cb (GtkWidget *widget, GdkEvent *event, gpointer *data);

/*********************************************************************
  Initialises the gui_(), prev_(), and opts_() - functions. Opens a
  new toplevel window with a user interface for the selection of
  images and option widgets.
  prev_width, prev_height: default size for a preview window
  rcfile: additional settings files for opts_load_default()
*********************************************************************/
void gui_init (char **rcfile, int prev_width, int prev_height);

#ifdef __cplusplus
}
#endif

#endif /* iw_Ggui_i_H */
