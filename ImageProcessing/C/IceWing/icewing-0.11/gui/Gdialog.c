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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <gdk/gdkkeysyms.h>

#include "Ggui_i.h"
#include "Gdialog.h"

/*********************************************************************
  Hide window and remember its position. So after showing it again the
  position is restored.
*********************************************************************/
void gui_dialog_hide_window (GtkWidget *window)
{
	gint x, y;

	gdk_window_get_root_origin (window->window, &x, &y);
	gtk_widget_hide (window);
	gtk_widget_set_uposition (window, x, y);
}

static void message_show (const char *title, const char *message, va_list args)
{
	GtkWidget *window, *hbox, *vbox, *label, *button;
	char str[1024];
	GtkAccelGroup *accel;

	vsprintf (str, message, args);

	accel = gtk_accel_group_new();
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
						GTK_SIGNAL_FUNC (gtk_widget_destroy), window);

	gtk_window_position (GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
	gtk_window_set_title (GTK_WINDOW (window), title);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(window),vbox);

	label = gtk_label_new (str);
	gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
	gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX(vbox),label,TRUE,TRUE,5);
	gtk_misc_set_padding (GTK_MISC(label),10,0);

	label = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX(vbox),label,FALSE,FALSE,5);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start (GTK_BOX(vbox),hbox,FALSE,FALSE,0);

	button = gtk_button_new_with_label ("     OK     ");
	gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
							   GTK_SIGNAL_FUNC(gtk_widget_destroy),
							   GTK_OBJECT (window));
	gtk_widget_add_accelerator (button, "clicked", accel, GDK_Escape, 0, 0);
	gtk_container_border_width (GTK_CONTAINER(button), 3);
	gtk_box_pack_start (GTK_BOX(hbox),button,FALSE,FALSE,0);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (button);

	gui_apply_icon (window);
	gtk_window_add_accel_group (GTK_WINDOW(window), accel);
	gtk_widget_show_all (window);
}

/*********************************************************************
  Display a message box.
*********************************************************************/
void gui_message_show (const char *title, const char *message, ...)
{
	va_list args;

	va_start (args, message);
	gui_threads_enter ();
	message_show (title, message, args);
	gui_threads_leave ();
	va_end (args);
}

/*********************************************************************
  PRIVATE: Display a message box. Never lock the gdk mutex.
*********************************************************************/
void gui_message_show__nl (const char *title, const char *message, ...)
{
	va_list args;

	va_start (args, message);
	message_show (title, message, args);
	va_end (args);
}

/*********************************************************************
  If !*window open a new file selection window, otherwise map the
  before opened window.
*********************************************************************/
void gui_file_selection_open (GtkWidget **window, const char *title,
							  const char *file, GtkSignalFunc func_okbut)
{
	if (!*window) {
		*window = gtk_file_selection_new (title);
		gtk_window_set_position (GTK_WINDOW (*window), GTK_WIN_POS_MOUSE);

		gtk_signal_connect (GTK_OBJECT (*window), "destroy",
							GTK_SIGNAL_FUNC(gtk_widget_destroyed),
							window);
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (*window)->ok_button),
							"clicked", GTK_SIGNAL_FUNC(func_okbut), *window);
		gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (*window)->cancel_button),
								   "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
								   GTK_OBJECT (*window));
		if (file && *file)
			gtk_file_selection_set_filename (GTK_FILE_SELECTION(*window),
											 file);
		gtk_widget_show_all (*window);
	} else if ((*window)->window) {
		gtk_widget_map (*window);
		gdk_window_raise ((*window)->window);
	}
}
