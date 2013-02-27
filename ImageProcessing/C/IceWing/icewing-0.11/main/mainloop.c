/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2012
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef WITH_GPB
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "about.xbm"
#endif

#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Gdialog.h"
#include "gui/Goptions.h"
#include "gui/Gpreferences.h"
#include "gui/Gtools.h"
#include "output_i.h"
#include "plugin_gui.h"
#include "plugin.h"
#include "plugin_i.h"
#include "grab.h"
#include "about.xpm"
#include "mainloop.h"

#define MANUAL_FILE		DATADIR"/icewing.pdf"

static prevBuffer *b_input;
static int time_cnt = 0;

/*********************************************************************
  The iceWing mainloop creates one preview window, where e.g. the grab
  plugin displays the first image. Return a pointer to this window,
  so that other plugins can display additional data in this window.
*********************************************************************/
prevBuffer *grab_get_inputBuf (void)
{
	return b_input;
}

/*********************************************************************
  Show results of the time_() calls and call any functions set with
  plug_function_register("mainCallback"). E.g. the grab plugin allows
  to stop/slow down the processing. Should be called periodically.
*********************************************************************/
void iw_mainloop_cyclic (void)
{
	plugDataFunc *func;
#ifdef IW_TIME_MESSURE
	static int call_cnt = 0;
	static int time_clock = -1;

	if (time_cnt > 0) {
		if (time_clock < 0) {
			static char name[15];
			sprintf (name, "Cycle%d", time_cnt);
			time_clock = iw_time_add (name);
			iw_time_start (time_clock);
		}
		call_cnt++;

		if (call_cnt % time_cnt == 0) {
			iw_time_stop (time_clock, FALSE);

			iw_time_show();
			iw_time_init_all();
			iw_time_start (time_clock);
		}
	}
#endif
	gui_check_exit (TRUE);

	func = NULL;
	while ((func = plug_function_get ("mainCallback", func)))
		func->func (func->plug);

	gui_check_exit (FALSE);
}

/*********************************************************************
  Like fork, but the new process is immediately orphaned (won't leave
  a zombie when it exits). The parent cannot wait() for the new
  process as it is unrelated.
  Returns -1 (error), 0 (child process), or 1 (parent process, not
  any meaningful pid as fork does).
*********************************************************************/
static int fork2 (void)
{
	pid_t pid;
	int status;

	if (!(pid = fork())) {
		switch (fork()) {			/* Child process */
			case -1:				/* fork() failed */
				_exit (errno);
			case 0:					/* Child process2 */
				return 0;
			default:				/* Main process of first fork() */
				_exit (0);
		}
	}

	if (pid < 0 || waitpid (pid, &status, 0) < 0)
		return -1;

	if (WIFEXITED(status))
		if (WEXITSTATUS(status) == 0)
			return 1;
		else
			errno = WEXITSTATUS(status);
	else
		errno = EINTR;	/* Well, sort of :-) */

	return -1;
}

/*********************************************************************
  Show any error messages from the manual showing child process.
*********************************************************************/
static void cb_manual_open (gpointer data, gint source, GdkInputCondition condition)
{
	char line[1024];
	int cnt = -1;

	if (condition == GDK_INPUT_READ)
		cnt = read (source, line, 1024);

	if (cnt > 0) {
		gui_message_show__nl ("Error", "%s", line);
	} else {
		int *tag = data;

		if (cnt < 0)
			gui_message_show__nl ("Error",
								  "Unable to show '"MANUAL_FILE
								  "'? Read error from child process!");
		gdk_input_remove (*tag);
		close (source);
		free (tag);
	}
}

/*********************************************************************
  Show the iceWing manual by spawning a PDF viewer.
*********************************************************************/
static void manual_open (void)
{
#define ERR_PARSE	"Unable to parse the preferences PDF viewer arguments for showing\n"\
					"'"MANUAL_FILE"'!"
#define ERR_EXEC1	"Unable to start the preferences PDF viewer for showing\n'"MANUAL_FILE"'!"
#define ERR_EXEC	"Unable to start any PDF viewer for showing\n'"MANUAL_FILE"'!"
	static char *viewer[] = {
		"-", "-", "-",			/* Appropriate desktop viewer */
		"acroread", "-",		/* Fallbacks */
		"evince", "-",
		"okular", "-",
		"kpdf", "-",
		"xpdf", "-",
		"gv", "-",
		NULL
	};
	struct stat stbuf;
	int pfd[2];

	if (stat(MANUAL_FILE, &stbuf) != 0) {
		gui_message_show__nl ("Error",
							  "Unable to read '"MANUAL_FILE"'!\n"
							  "Try installing "ICEWING_NAME_D
							  ", the file will be created then.");
		return;
	}

	if (pipe (pfd)) {
		gui_message_show__nl ("Error",
							  "Unable to show '"MANUAL_FILE"', pipe() failed!");
	} else {
		switch (fork2()) {
			case -1:						/* fork() failed */
				gui_message_show__nl ("Error",
									  "Unable to show '"MANUAL_FILE"', fork() failed!");
				break;
			case 0: {						/* fork() succeeded, child process code */
				int i;
				char *pviewer;

				close (pfd[0]);

				pviewer = iw_prefs_get()->pdfviewer;
				if (pviewer && *pviewer) {
					/* Start the viewer specified in the preferences */
					char *sargs[] = {"'"MANUAL_FILE"'"};
					char args[2*PATH_MAX], **argv = NULL;
					int argc;

					if (gui_sprintf (args, 2*PATH_MAX, pviewer, NULL, NULL,
									 "f", sargs) > 0) {
						iw_string_split (args, &argc, &argv);
						execvp (argv[0], argv);
						write (pfd[1], ERR_EXEC1, sizeof(ERR_EXEC1));
					} else
						write (pfd[1], ERR_PARSE, sizeof(ERR_PARSE));

					if (argv)
						free (argv);
				} else {
					/* Try to detect the users desktop */
					if (viewer[0][0] == '-') {
						if (getenv("KDE_FULL_SESSION")) {
							viewer[0] = "kfmclient";
							viewer[1] = "exec";
						} else if (getenv("GNOME_DESKTOP_SESSION_ID")) {
							viewer[0] = "gnome-open";
						} else {
							viewer[0] = "xdg-open";
						}
					}

					/* Start a viewer */
					for (i=0; viewer[i]; i++) {
						char *argv[4];
						int j = 0;
						while (viewer[i][0] != '-')
							argv[j++] = viewer[i++];
						if (j > 0) {
							argv[j++] = MANUAL_FILE;
							argv[j] = NULL;
							execvp (argv[0], argv);
						}
					}
					write (pfd[1], ERR_EXEC, sizeof(ERR_EXEC));
				}

				_exit (0);
				break;
			}
			default: {						 /* fork() succeeded, main process code */
				int *tag = malloc (sizeof(int));
				close (pfd[1]);
				*tag = gdk_input_add (pfd[0], GDK_INPUT_READ|GDK_INPUT_EXCEPTION,
									  cb_manual_open, tag);
			}
		}
	}
}

#ifdef WITH_GPB

static GtkWidget *drawing_area;
static GdkPixbuf *pixbuf, *background;
static int back_width, back_height;
static guint timer = 0;

static gint cb_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	guchar *pixels = gdk_pixbuf_get_pixels (pixbuf)
		+ rowstride * event->area.y + event->area.x * 3;

	gdk_draw_rgb_image_dithalign (widget->window,
								  widget->style->black_gc,
								  event->area.x, event->area.y,
								  event->area.width, event->area.height,
								  GDK_RGB_DITHER_NORMAL,
								  pixels, rowstride,
								  event->area.x, event->area.y);
  return TRUE;
}

#define HALO_RAD		8
#define HALO_CNT		10
static struct {
	int x, y;
	int rad;
} halo[HALO_CNT];

static gint cb_advance_frame (gpointer data)
{
#define BITS_TEST(w, data, x, y) \
		((data)[(((w) + 7) >> 3) * (y) + ((x) >> 3)] & (1 << ((x) & 7)))
#define ADD(pos,add)	\
		if (*(pos) < 255-(add)) \
			*(pos) += (add); \
		else \
			*(pos) = 255;

	int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	guchar *pos, *buf = gdk_pixbuf_get_pixels (pixbuf);
	int x, y, rad, h, add;

	gdk_pixbuf_copy_area (background, 0, 0, back_width, back_height,
						  pixbuf, 0, 0);

	for (h = 0; h < HALO_CNT; h++) {
		if (halo[h].rad > 8*HALO_RAD) {
			halo[h].x = HALO_RAD + rand() % (back_width-2*HALO_RAD);
			halo[h].y = HALO_RAD + rand() % (back_height-2*HALO_RAD);
			if (BITS_TEST (back_width, about_bits, halo[h].x, halo[h].y))
				halo[h].rad = - (rand() % 60);
		}
		halo[h].rad++;
		if (halo[h].rad > 0 && halo[h].rad < 8*HALO_RAD) {
			rad = halo[h].rad % (2*HALO_RAD);
			if (rad > HALO_RAD) rad = 2*HALO_RAD - rad;

			for (y = -rad; y <= rad; y++) {
				for (x = -rad; x <= rad; x++) {
					add = sqrt(x*x+y*y);
					if (add <= rad) {
						add = 40*rad - 40*add;
						pos = buf + rowstride*(y+halo[h].y) + (x+halo[h].x)*3;
						ADD(pos, add);
						ADD(pos+1, add);
						ADD(pos+2, add);
					}
				}
			}
		}
	}
	gtk_widget_queue_draw (drawing_area);

	return TRUE;
#undef ADD
#undef BITS_TEST
}

#endif

static gint cb_about_close (GtkWidget *widget)
{
#ifdef WITH_GPB
	if (timer) {
		gtk_timeout_remove (timer);
		timer = 0;
	}
#endif
	gui_dialog_hide_window (widget);
	return TRUE;
}

/*********************************************************************
  Open the about window.
*********************************************************************/
static void about_open (void)
{
	static GtkWidget *window = NULL;

	if (!window) {
		GtkWidget *vbox, *hbox, *widget, *frame;
		GtkAccelGroup *accel;
		char string[200];
#ifndef WITH_GPB
		GdkPixmap *pix;
		GdkBitmap *mask;
		GdkColormap *colormap;
#endif

		accel = gtk_accel_group_new();
		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_position (GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
		gtk_signal_connect_object (GTK_OBJECT(window), "delete_event",
								   GTK_SIGNAL_FUNC(cb_about_close),
								   GTK_OBJECT (window));
		gtk_window_set_title (GTK_WINDOW(window),
							  "About "ICEWING_NAME_D);

		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (window), vbox);

		hbox = gtk_hbox_new (FALSE, 5);
		gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

		/* Logo */
		widget = gtk_alignment_new (0.5, 0.5, 0, 0);
		gtk_box_pack_start (GTK_BOX(hbox), widget, FALSE, FALSE, 0);
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
		gtk_container_add (GTK_CONTAINER(widget), frame);

#ifdef WITH_GPB
		background = gdk_pixbuf_new_from_xpm_data (about_xpm);
		back_width = gdk_pixbuf_get_width (background);
		back_height = gdk_pixbuf_get_height (background);
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, back_width, back_height);

		drawing_area = gtk_drawing_area_new();
		gtk_widget_set_usize (drawing_area, back_width, back_height);
		gtk_signal_connect (GTK_OBJECT(drawing_area), "expose_event",
							(GtkSignalFunc)cb_expose_event, NULL);
		gtk_container_add (GTK_CONTAINER(frame), drawing_area);
#else
		gtk_widget_realize (frame);
		colormap = gtk_widget_get_colormap (frame);
		pix = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
													 NULL, (char**)about_xpm);
		widget = gtk_pixmap_new (pix, mask);
		gdk_pixmap_unref (pix);
		gdk_bitmap_unref (mask);
		gtk_container_add (GTK_CONTAINER(frame), widget);
#endif

		/* Text */
		sprintf (string, "%s %s\n"
				 "an Integrated Communication Environment\n"
				 "Which Is Not Gesten\n\n"
				 "(c) 1999-2012 by Frank Loemker\n"
				 "floemker@techfak.uni-bielefeld.de\n\n"
				 "Released under the GNU General Public License",
				 ICEWING_NAME, ICEWING_VERSION);
		widget = gtk_label_new (string);
		gtk_label_set_justify (GTK_LABEL(widget), GTK_JUSTIFY_CENTER);
		gtk_misc_set_padding (GTK_MISC(widget), 5, 5);
		gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, FALSE, 0);

		/* Button bar */
		widget = gtk_hseparator_new();
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

		hbox = gtk_hbox_new (TRUE, 5);
		gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

		  widget = gtk_button_new_with_label("Close");
		  gtk_signal_connect_object (GTK_OBJECT(widget), "clicked",
									 GTK_SIGNAL_FUNC(cb_about_close),
									 GTK_OBJECT(window));
		  gtk_widget_add_accelerator (widget, "clicked", accel, GDK_Escape, 0, 0);
		  gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, TRUE, 0);
		  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
		  gtk_widget_grab_default (widget);

		gtk_widget_show_all (vbox);
		gui_apply_icon (window);
		gtk_window_add_accel_group (GTK_WINDOW(window), accel);
	}
#ifdef WITH_GPB
	if (!timer) {
		int h;
		for (h = 0; h < HALO_CNT; h++)
			halo[h].rad = 9999;
		gdk_pixbuf_copy_area (background, 0, 0, back_width, back_height,
							  pixbuf, 0, 0);
		timer = gtk_timeout_add (100, (GtkFunction) cb_advance_frame, NULL);
	}
#endif
	gtk_widget_show (window);
	gdk_window_raise (window->window);
}

static void cb_help_menu (GtkWidget *widget, int button, void *data)
{
	switch (button) {
		case 1:
			manual_open();
			break;
		case 2:
			about_open();
			break;
	}
}
static void cb_plugin_menu (GtkWidget *widget, int button, void *data)
{
	plug_gui_open();
}

static void init_options (grabParameter *para)
{
	b_input = prev_new_window ("Input data",-1,-1,FALSE,FALSE);
	prev_opts_append (b_input, PREV_IMAGE, -1);

	plug_init_options_all();

	opts_toggle_create (-1, "MainMenu._Edit/_Enable DACS-Output",
						"Output data via DACS as specified on the command line?",
						iw_output_get_output());
	opts_buttoncb_create (-1, "MainMenu._View/_Plugin Info<<control>i>",
						  "Open a window with information about all plugins",
						  cb_plugin_menu, NULL);
	opts_buttoncb_create (-1, "MainMenu._Help/"ICEWING_NAME_D" _Manual<F1><stock gtk-help>|"
						  "Help/_About "ICEWING_NAME_D"<stock gtk-about>",
						  "Open the "ICEWING_NAME_D" PDF manual|"
						  "Open the "ICEWING_NAME_D" about window",
						  cb_help_menu, NULL);
}

/*********************************************************************
  PRIVATE: Main loop for calling all plugins.
*********************************************************************/
void iw_mainloop_start (grabParameter *para)
{
	plugDefinition plug_start = {"MainLoop", PLUG_ABI_VERSION, NULL, NULL, NULL, NULL};
	iw_time_add_static (time_clock, "Cycle");

	time_cnt = para->time_cnt;
	iw_output_init (para->output);
	init_options (para);

	iw_showtid (1, ICEWING_NAME_D" mainloop");
	while (1) {
		iw_mainloop_cyclic();

		iw_time_start (time_clock);

		plug_data_set (&plug_start, "start", NULL, NULL);
		plug_main_iteration();
		prev_draw_buffer (b_input);

		iw_time_stop (time_clock, FALSE);
	}
}
