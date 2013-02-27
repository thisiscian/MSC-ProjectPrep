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
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "gui/Ggui.h"
#include "gui/Ggui_i.h"
#include "gui/Gtools.h"
#include "gui/Gdialog.h"
#include "gui/Gsession.h"
#include "gui/Goptions.h"
#include "plugin_i.h"
#include "plugin_gui_plug.h"
#include "plugin_gui.h"

#define PGUI_TITLE		"Plugin Info"

#define PGUI_DATA		0
#define PGUI_OBSERVER	1
#define PGUI_FUNC		2
#define PGUI_CTREE_LAST	PGUI_FUNC

#define UPDATE_IMMEDIATELY		0
#define UPDATE_DELAYED			1

typedef struct pguiLog {
	gboolean log_disp;
	GtkWidget *text;

	char file[PATH_MAX];
	FILE *fp;
	GtkWidget *entry;
	GtkWidget *browse;
	GtkWidget *filesel;
} pguiLog;

static GtkWidget *ctrees[PGUI_CTREE_LAST+1] = {NULL, NULL, NULL};
static GtkWidget *window = NULL;

static pguiLog pgui_log;

/*********************************************************************
  Callback of the different plug_xxx() functions.
*********************************************************************/
static void log_add (pguiLog *log, char *str, ...)
{
	va_list args;
	char *text;

	if (!log->log_disp && !log->fp) return;

	gui_threads_enter();
	va_start (args, str);
	text = g_strdup_vprintf (str, args);
	va_end (args);

	if (log->log_disp) {
		gtk_text_freeze (GTK_TEXT(log->text));
		gtk_text_set_point (GTK_TEXT(log->text),
							gtk_text_get_length (GTK_TEXT(log->text)));
		gtk_text_insert (GTK_TEXT(log->text), NULL, NULL, NULL, text, strlen(text));
		gtk_text_thaw (GTK_TEXT(log->text));
		gtk_adjustment_set_value (GTK_TEXT(log->text)->vadj,
								  GTK_TEXT(log->text)->vadj->upper
								  - GTK_TEXT(log->text)->vadj->page_increment);
	}
	if (log->fp) {
		if (fputs (text, log->fp) == EOF) {
			gui_message_show__nl ("Error",
								  "Unable to write to the log file '%s'!", log->file);
			fclose (log->fp);
			log->fp = NULL;
			if (!log->log_disp)
				plug_log_register (NULL, NULL);
		}
	}
	gui_threads_leave();

	g_free (text);
}

static void cb_log_clear (GtkWidget *widget, pguiLog *log)
{
	gtk_text_freeze (GTK_TEXT(log->text));
	gtk_editable_delete_text (GTK_EDITABLE(log->text), 0, -1);
	gtk_text_thaw (GTK_TEXT(log->text));
}

static void cb_log_display_toggled (GtkWidget *widget, pguiLog *log)
{
	log->log_disp = GTK_TOGGLE_BUTTON(widget)->active;
	if (log->log_disp || log->fp)
		plug_log_register ((plugLogFunc)log_add, log);
	else
		plug_log_register (NULL, NULL);
}

static void cb_log_entry (GtkWidget *widget, pguiLog *log)
{
	const char *entry;

	if ((entry = gtk_entry_get_text (GTK_ENTRY(widget)))) {
		/* Copy from back to front ->
		   value is always terminated by '\0' */
		gui_strcpy_back (log->file, entry, PATH_MAX);
	}
}

static void cb_log_filesel_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	pguiLog *log = gtk_object_get_user_data(GTK_OBJECT(fs));

	gtk_entry_set_text (GTK_ENTRY(log->entry),
						gtk_file_selection_get_filename (fs));
	gtk_editable_set_position (GTK_EDITABLE(log->entry), -1);
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void cb_log_filesel (GtkWidget *widget, pguiLog *log)
{
	gui_file_selection_open (&log->filesel, "Select a file",
							 gtk_entry_get_text (GTK_ENTRY(log->entry)),
							 GTK_SIGNAL_FUNC(cb_log_filesel_ok));
	gtk_object_set_user_data (GTK_OBJECT(log->filesel), log);
}

static void cb_log_file_toggled (GtkWidget *widget, pguiLog *log)
{
	gboolean file = GTK_TOGGLE_BUTTON(widget)->active;

	gtk_widget_set_sensitive (log->entry, !file);
	gtk_widget_set_sensitive (log->browse, !file);
	if (log->fp) {
		fclose (log->fp);
		log->fp = NULL;
	}
	if (file) {
		if (log->file[0]) {
			FILE *fp = fopen (log->file, "w");
			if (!fp)
				gui_message_show__nl ("Error",
									  "Unable to open '%s' for log recording!", log->file);
			log->fp = fp;
		} else {
			gui_message_show__nl ("Error",
								  "No file name given for recording the log!");
			log->fp = NULL;
		}
	}

	if (log->log_disp || log->fp)
		plug_log_register ((plugLogFunc)log_add, log);
	else
		plug_log_register (NULL, NULL);
}

/*********************************************************************
  Callback for pgui_log.file. Called if the string is loaded.
*********************************************************************/
static gboolean log_entry_set (void *value, void *new_value, void *data)
{
	pguiLog *log = data;

	gtk_entry_set_text (GTK_ENTRY(log->entry), new_value);
	gtk_editable_set_position (GTK_EDITABLE(log->entry), -1);

	return FALSE;
}

/*********************************************************************
  Create the "Logging" notebook page.
*********************************************************************/
static void log_create (GtkWidget *notebook, GtkTooltips *tooltips)
{
	GtkWidget *widget, *vbox, *hbox, *scroll, *table, *align;

	pgui_log.log_disp = FALSE;
	pgui_log.file[0] = '\0';
	pgui_log.fp = NULL;
	pgui_log.filesel = NULL;

	widget = gtk_label_new ("Logging");
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, widget);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
									GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	pgui_log.text = gtk_text_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER(scroll), pgui_log.text);

	table = gtk_table_new (2, 2, FALSE);
	gtk_box_pack_start (GTK_BOX(vbox), table, FALSE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE(table), 2);

	widget = gtk_check_button_new_with_label ("Display");
	gtk_signal_connect (GTK_OBJECT(widget), "toggled",
						GTK_SIGNAL_FUNC(cb_log_display_toggled), &pgui_log);
	gtk_tooltips_set_tip (tooltips, widget,
						  "Log plugin calls in the display above", NULL);
	gtk_table_attach (GTK_TABLE(table), widget, 0, 1, 0, 1,
					  GTK_FILL, 0, 0, 0);

	  align = gtk_alignment_new (0, 0, 0, 1);
	  gtk_table_attach (GTK_TABLE(table), align, 1, 2, 0, 1,
						GTK_FILL, GTK_FILL, 0, 0);

	  widget = gtk_button_new_with_label ("   Clear   ");
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(cb_log_clear), &pgui_log);
	  gtk_tooltips_set_tip (tooltips, widget,
							"Clear the display above", NULL);
	  gtk_container_add (GTK_CONTAINER(align), widget);

	widget = gtk_check_button_new_with_label ("File");
	gtk_signal_connect (GTK_OBJECT(widget), "toggled",
						GTK_SIGNAL_FUNC(cb_log_file_toggled), &pgui_log);
	gtk_tooltips_set_tip (tooltips, widget,
						  "Log plugin calls in the file specified on the right", NULL);
	gtk_table_attach (GTK_TABLE(table), widget, 0, 1, 1, 2,
					  GTK_FILL, 0, 0, 0);

	  hbox = gtk_hbox_new (FALSE, 2);
	  gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 1, 2,
						GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	  pgui_log.entry = gtk_entry_new_with_max_length (PATH_MAX);
	  gtk_signal_connect (GTK_OBJECT(pgui_log.entry), "changed",
						  GTK_SIGNAL_FUNC(cb_log_entry), &pgui_log);
	  gtk_box_pack_start (GTK_BOX(hbox), pgui_log.entry, TRUE, TRUE, 0);

	  pgui_log.browse = gtk_button_new_with_label (" ... ");
	  gtk_signal_connect (GTK_OBJECT(pgui_log.browse), "clicked",
						  GTK_SIGNAL_FUNC(cb_log_filesel), &pgui_log);
	  gtk_box_pack_start (GTK_BOX(hbox), pgui_log.browse, FALSE, FALSE, 0);

	opts_varstring_add (PGUI_TITLE".file", log_entry_set, &pgui_log,
						pgui_log.file, PATH_MAX);
}

/*********************************************************************
  Display one row in the data display.
*********************************************************************/
static void cb_update_data (plugInfoData *info, void *ctree)
{
	static GtkCTreeNode *parent;
	GtkCTreeNode *node;
	const char *text[4] = {NULL, NULL, NULL, NULL};
	char ref_count[10];

	text[0] = info->name;
	if (!info->ident) {
		sprintf (ref_count, "%d", info->ref_count);
		text[2] = ref_count;
	}
	if (info->ident) {
		node = gtk_ctree_insert_node (GTK_CTREE(ctree), NULL, NULL,
									  (char**)text, 0,
									  NULL, NULL, NULL, NULL,
									  FALSE, TRUE);
		parent = node;
	} else {
		node = gtk_ctree_insert_node (GTK_CTREE(ctree), parent, NULL,
									  (char**)text, 0,
									  NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
	}
	gui_tree_show_bool (ctree, node, 1, !info->ident && info->active);
	gui_tree_show_bool (ctree, node, 3, !info->ident && info->floating);
}

/*********************************************************************
  Display one row in the observer display.
*********************************************************************/
static void cb_update_observer (plugInfoObserver *info, void *ctree)
{
	static GtkCTreeNode *parent;
	GtkCTreeNode *node;
	const char *text[3] = {NULL, NULL, NULL};
	char data_count[10];

	text[0] = info->name;
	if (info->ident) {
		sprintf (data_count, "%d", info->data_count);
		text[2] = data_count;
	}
	if (info->ident) {
		node = gtk_ctree_insert_node (GTK_CTREE(ctree), NULL, NULL,
									  (char**)text, 0,
									  NULL, NULL, NULL, NULL,
									  FALSE, TRUE);
		parent = node;
	} else {
		node = gtk_ctree_insert_node (GTK_CTREE(ctree), parent, NULL,
									  (char**)text, 0,
									  NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
	}
	gui_tree_show_bool (ctree, node, 1, info->ident && info->active);
}

/*********************************************************************
  Display one row in the function display.
*********************************************************************/
static void cb_update_func (plugInfoFunc *info, void *ctree)
{
	static GtkCTreeNode *parent;
	const char *text[1] = {NULL};

	text[0] = info->name;
	if (info->ident) {
		parent = gtk_ctree_insert_node (GTK_CTREE(ctree), NULL, NULL,
										(char**)text, 0,
										NULL, NULL, NULL, NULL,
										FALSE, TRUE);
	} else {
		gtk_ctree_insert_node (GTK_CTREE(ctree), parent, NULL,
							   (char**)text, 0,
							   NULL, NULL, NULL, NULL,
							   TRUE, TRUE);
	}
}

/*********************************************************************
  Try to set the width of the name column to some reasonable value.
*********************************************************************/
static void pgui_set_width (GtkCList *clist, int maxwidth)
{
	int width;

	width = gtk_clist_optimal_column_width (clist, 0);
	if (width > maxwidth) width = maxwidth;

	gtk_clist_set_column_width (clist, 0, width);
}

/*********************************************************************
  Show current plugin infos in the gui.
*********************************************************************/
void plug_update (void)
{
	GtkCList *clist;
	GtkRequisition req;
	int maxwidth;

	gtk_widget_size_request (window, &req);
	maxwidth = req.width/1.5;

	plug_update_plugs();

	clist = GTK_CLIST (ctrees[PGUI_DATA]);
	gtk_clist_freeze (clist);
	gtk_clist_clear (clist);
	plug_info_data (cb_update_data, clist);
	pgui_set_width (clist, maxwidth);
	gtk_clist_thaw (clist);

	clist = GTK_CLIST (ctrees[PGUI_OBSERVER]);
	gtk_clist_freeze (clist);
	gtk_clist_clear (clist);
	plug_info_observer (cb_update_observer, clist);
	pgui_set_width (clist, maxwidth);
	gtk_clist_thaw (clist);

	clist = GTK_CLIST (ctrees[PGUI_FUNC]);
	gtk_clist_freeze (clist);
	gtk_clist_clear (clist);
	plug_info_func (cb_update_func, clist);
	pgui_set_width (clist, maxwidth);
	gtk_clist_thaw (clist);
}

static gint cb_pgui_delete (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gui_dialog_hide_window (widget);
	iw_session_remove (PGUI_TITLE);
	return TRUE;
}

static void cb_pgui_close (GtkWidget *widget, GtkWidget *window)
{
	cb_pgui_delete (window, NULL, NULL);
}

static void cb_update_cleaning (void *id)
{
	gdk_threads_enter();
	plug_update();
	gdk_threads_leave();
	plug_main_unregister (id);
}

static void cb_plug_update (GtkWidget *widget, void *mode)
{
	if (GPOINTER_TO_INT(mode) == UPDATE_DELAYED)
		plug_main_register_after (cb_update_cleaning, NULL);
	else
		plug_update();
}

/*********************************************************************
  Add a new page 'titel' with a ctree with columns 'header' (NULL
  terminated) to the notebook and return the ctree. If paned!= NULL
  add the ctree to the paned and the paned to the notebook.
*********************************************************************/
static GtkWidget *plug_page_add (GtkWidget *notebook, char *titel, char **header)
{
	int i;

	GtkWidget *scrolledwindow, *widget, *ctree;

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
									GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	widget = gtk_label_new (titel);
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), scrolledwindow, widget);

	for (i=0; header[i]; i++) /* empty */;
	ctree = gtk_ctree_new_with_titles (i, 0, header);
	gtk_ctree_set_indent (GTK_CTREE (ctree), 15);
	gtk_clist_column_titles_passive (GTK_CLIST(ctree));
	gtk_clist_set_selection_mode (GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), ctree);

	return ctree;
}

/*********************************************************************
  Open the plugin info window.
*********************************************************************/
void plug_gui_open (void)
{
	static char *data[] = {"Data", "New", "RefCount", "Floating", NULL};
	static char *observer[] = {"Observer", "New", "DataCnt", NULL};
	static char *funcs[] = {"Functions", NULL};

	if (!window) {
		GtkWidget *vbox_main, *hbox, *widget, *notebook;
		GtkTooltips *tooltips;
		GtkAccelGroup *accel;

		tooltips = gtk_tooltips_new();
		accel = gtk_accel_group_new();

		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size (GTK_WINDOW(window), 300, 250);
		gtk_signal_connect (GTK_OBJECT(window), "delete_event",
							GTK_SIGNAL_FUNC(cb_pgui_delete), NULL);
		gtk_window_set_title (GTK_WINDOW(window), PGUI_TITLE);

		vbox_main = gtk_vbox_new (FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox_main), 0);
		gtk_container_add (GTK_CONTAINER(window), vbox_main);

		notebook = gtk_notebook_new ();
		gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);
		gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
		gtk_box_pack_start (GTK_BOX (vbox_main), notebook, TRUE, TRUE, 0);

		/* Info pages */
		plug_open_plugs (notebook, tooltips, accel);

		ctrees[PGUI_DATA] = plug_page_add (notebook, "Data", data);
		ctrees[PGUI_OBSERVER] = plug_page_add (notebook, "Observer", observer);
		ctrees[PGUI_FUNC] = plug_page_add (notebook, "Functions", funcs);

		log_create (notebook, tooltips);

		/* Button bar */
		widget = gtk_hseparator_new();
		gtk_box_pack_start (GTK_BOX (vbox_main), widget, FALSE, FALSE, 0);

		hbox = gtk_hbox_new (TRUE, 5);
		gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
		gtk_box_pack_start (GTK_BOX (vbox_main), hbox, FALSE, TRUE, 0);

		  widget = gtk_button_new_with_label("RefreshEnd");
		  gtk_tooltips_set_tip (tooltips, widget,
								"Refresh the display immediately before"
								" deleting any floating data elements",
								NULL);
		  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							  GTK_SIGNAL_FUNC(cb_plug_update),
							  GINT_TO_POINTER(UPDATE_DELAYED));
		  gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		  widget = gtk_button_new_with_label("Refresh");
		  gtk_tooltips_set_tip (tooltips, widget,
								"Refresh the display immediately", NULL);
		  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							  GTK_SIGNAL_FUNC(cb_plug_update),
							  GINT_TO_POINTER(UPDATE_IMMEDIATELY));
		  gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		  widget = gtk_button_new_with_label("Close");
		  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							  GTK_SIGNAL_FUNC(cb_pgui_close),
							  GTK_OBJECT(window));
		  gtk_widget_add_accelerator (widget, "clicked", accel, GDK_Escape, 0, 0);
		  gtk_box_pack_start (GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		gtk_window_add_accel_group (GTK_WINDOW(window), accel);
		gtk_widget_show_all (vbox_main);
		gui_apply_icon (window);
		if (!iw_session_set (window, PGUI_TITLE, TRUE, TRUE))
			gtk_window_position (GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
	} else
		iw_session_set (window, PGUI_TITLE, FALSE, FALSE);

	plug_update();
	gtk_widget_show (window);
	gdk_window_raise (window->window);
}

/*********************************************************************
  If plugin window was open in the last session, open it again.
*********************************************************************/
void plug_gui_init (void)
{
	if (iw_session_find (PGUI_TITLE))
		plug_gui_open();
}
