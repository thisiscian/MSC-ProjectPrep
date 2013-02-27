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
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "Gpreferences.h"
#include "Ggui.h"
#include "Gdialog.h"
#include "Gsession.h"
#include "Goptions_i.h"

#define PREF_TITLE		"Preferences Window"
#define PREF_TITLE2		"Preferences Window2"
typedef struct prefOptions {
	GtkWidget *window;
	GtkWidget *ses_win;			/* File selector for session saving */
	prevPrefs prefs;
} prefOptions;

static prefOptions pref_options;

static gint cb_prefs_delete (GtkWidget *widget, GdkEvent *event, gpointer *data)
{
	gui_dialog_hide_window (widget);
	iw_session_remove (PREF_TITLE);
	return TRUE;
}

static void cb_prefs_close (GtkWidget *widget, GtkWidget *window)
{
	cb_prefs_delete (window, NULL, NULL);
}

static void cb_prefs_session_save_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	iw_session_set_name (gtk_file_selection_get_filename (fs));
	iw_session_save();
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void cb_prefs_session (GtkWidget *widget, int number, void *data)
{
	switch (number) {
		case 1:
			gui_file_selection_open (&pref_options.ses_win, "Save session",
									 iw_session_get_name(),
									 GTK_SIGNAL_FUNC(cb_prefs_session_save_ok));
			break;
		case 2:
			iw_session_save();
			break;
		case 3:
			iw_session_clear();
			break;
	}
}

/*********************************************************************
  Return pointer to global variables. Used in the preferences gui.
*********************************************************************/
prevPrefs* iw_prefs_get (void)
{
	return &pref_options.prefs;
}

/*********************************************************************
  Open the prefs window.
*********************************************************************/
void iw_prefs_open (void)
{
	iw_session_set (pref_options.window, PREF_TITLE, TRUE, TRUE);

	gtk_widget_show (pref_options.window);
	gdk_window_raise (pref_options.window->window);
}

/*********************************************************************
  Init the prefs window (so that all opts_..._widgets() are
  initialized, but do not show it.
*********************************************************************/
void iw_prefs_init (void)
{
	static char *tree[] = {"Nothing", "Images", "Categories",
						   "Images/Categories", NULL};
	GtkWidget *vbox_main, *vbox, *widget, *notebook;
	GtkAccelGroup *accel;
	int p;

	pref_options.prefs.format = IW_IMG_FORMAT_UNKNOWN;
	strcpy (pref_options.prefs.fname, "Gsnap_%03d.ppm");
	pref_options.prefs.framerate = 12.5;
	pref_options.prefs.quality = 75;
	pref_options.prefs.save_full = TRUE;
	pref_options.prefs.show_message = TRUE;
	pref_options.prefs.reset_cnt = FALSE;
	pref_options.prefs.save_atexit = FALSE;
	pref_options.prefs.save_pan = FALSE;
	pref_options.prefs.use_tree = PREF_TREE_BOTH;
	pref_options.prefs.use_scrolled = TRUE;
	*pref_options.prefs.pdfviewer = '\0';

	pref_options.ses_win = NULL;

	accel = gtk_accel_group_new();
	pref_options.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_position (GTK_WINDOW(pref_options.window), GTK_WIN_POS_MOUSE);
	gtk_signal_connect (GTK_OBJECT(pref_options.window), "delete_event",
						GTK_SIGNAL_FUNC(cb_prefs_delete), NULL);
	gtk_window_set_title (GTK_WINDOW (pref_options.window), PREF_TITLE);

	vbox_main = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(pref_options.window), vbox_main);

	notebook = gtk_notebook_new ();
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
	gtk_container_add (GTK_CONTAINER (vbox_main), notebook);

	widget =  gtk_label_new ("Image Saving");
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, widget);
	p = opts_page_add__nl (vbox, NULL, PREF_TITLE);

	opts_option_create (p, "Image format:",
						"File format used for image saving",
						iwImgFormatText, (int*)(void*)&pref_options.prefs.format);
	opts_filesel_create (p, "Image name (e.g 'Gsnap_%d.ppm'):",
						 "File name used for saving images (context menu in images)"
						 "\n"
						 "%d: image counter  %t: ms part of saving time  "
						 "%T: sec part of saving time  %b: user name  %h: host name  "
						 "%w: window title   "
						 "printf()-style modifiers are allowed",
						 pref_options.prefs.fname, PATH_MAX);
	opts_float_create (p, "AVI framerate",
					   "Framerate used for saving of movie files (the AVI formats)",
					   &pref_options.prefs.framerate, 0, 50);
	opts_entscale_create (p, "Quality",
						  "Quality used for JPEG / AVI MJPEG / MPEG4 saving",
						  &pref_options.prefs.quality, 0, 100);
	opts_toggle_create (p, "Show SaveMessage",
						"Show a dialog after saving of images?",
						&pref_options.prefs.show_message);
	opts_toggle_create (p, "Save full window",
						"Save full window (incl. black border) on 'Save/Save Seq'?",
						&pref_options.prefs.save_full);
	opts_button_create (p, "Reset FilenameCounter",
						"Reset counter used for saving images to 0 (see %d)\n"
						"WARNING! Existing files will be overwritten!",
						&pref_options.prefs.reset_cnt);

	widget =  gtk_label_new ("Other");
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, widget);
	p = opts_page_add__nl (vbox, NULL, PREF_TITLE2);

	opts_separator_create (p, "Session");

	opts_buttoncb_create (p, "Save As| Save |Clear",
						  "Save position and size of all open windows in a selectable file|"
						  "Save position and size of all open windows in the currently active session file|"
						  "Delete the currently active session file", cb_prefs_session, NULL);
	opts_toggle_create (p, "Auto-save at exit",
						"Automatically save session file at program exit?",
						&pref_options.prefs.save_atexit);
	opts_toggle_create (p, "Save pan/zoom values",
						"Save pan/zoom values of all windows in the session file?",
						&pref_options.prefs.save_pan);

	opts_separator_create (p, "User Interface");
	opts_option_create (p, "Use tree for",
						"Use a tree widget for the image window list and/or the categories list"
						" (needs restart to take effect)?",
						tree, &pref_options.prefs.use_tree);
	opts_toggle_create (p, "Scrollbars in categorie pages",
						"Use scrollbars in the categorie pages to allow a smaller main window"
						" (needs restart to take effect)?",
						&pref_options.prefs.use_scrolled);
	opts_filesel_create (p, "PDF viewer (e.g 'acroread %f'):",
						 "Program for showing the iceWing PDF manual, "
						 "if empty autoselect a viewer"
						 "\n"
						 "%f: the to be shown PDF file   "
						 "printf()-style modifiers are allowed",
						 pref_options.prefs.pdfviewer, PATH_MAX);

	widget = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox_main), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label("Close");
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						GTK_SIGNAL_FUNC(cb_prefs_close),
						GTK_OBJECT(pref_options.window));
	gtk_widget_add_accelerator (widget, "clicked", accel, GDK_Escape, 0, 0);
	gtk_box_pack_start (GTK_BOX(vbox_main), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox_main);
	gui_apply_icon (pref_options.window);
	gtk_window_add_accel_group (GTK_WINDOW(pref_options.window), accel);

	/* Should the PrefsWindow open on program start ? */
	/* if (iw_session_find (PREF_TITLE)) pref_open(); */
}
