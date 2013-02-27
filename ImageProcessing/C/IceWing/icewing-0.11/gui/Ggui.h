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

#ifndef iw_Ggui_H
#define iw_Ggui_H

#include <gtk/gtk.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Terminate the program and return 'status' to the system.
  Must be called instead of exit() if the program should exit.
*********************************************************************/
void gui_exit (int status);

/*********************************************************************
  If immediate_exit is TRUE exit immediately. Otherwise schedule an
  exit and return. The exit will be performed when gui_check_exit()
  is called.
*********************************************************************/
void gui_schedule_exit (int status);

/*********************************************************************
  Check if an exit is pending (from a click on the Quit-Button or a
  call to gui_schedule_exit()) and exit.
  immediate_exit==TRUE (default): On click on the Quit-button the
      program exits immediately, otherwise it exits only if gui_exit()
      or gui_check_exit() is called.
*********************************************************************/
void gui_check_exit (gboolean immediate_exit);

/*********************************************************************
  Install the default window icon for the window win.
*********************************************************************/
void gui_apply_icon (GtkWidget *win);

#ifdef __cplusplus
}
#endif

#endif /* iw_Ggui_H */
