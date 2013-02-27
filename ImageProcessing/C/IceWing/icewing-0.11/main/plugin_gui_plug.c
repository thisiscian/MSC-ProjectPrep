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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gui/Gtools.h"
#include "gui/Gdialog.h"
#include "plugin_i.h"
#include "plugin_gui.h"
#include "plugin_gui_plug.h"

#define PLUG_FILE		DATADIR"/plugins.help"
	/* CTree root node for the list of installed plugins */
#define	ROOT_INSTALLED	"All installed plugins"
	/* Max length of all command line arguments */
#define ARGV_MAXLEN		300

/* Indices of the inst_names array */
#define INST_FILE		0
#define INST_HELP		1

typedef struct pguiHelp {		/* inst_hash hash table entries */
	char *name;
	char *help;
	char *fname;
} pguiHelp;

typedef struct pguiPlug {
	GtkWidget *labName;
	GtkWidget *entryArgs;
	GtkWidget *labHelp;

	char *selFile;				/* Selected parent entry in the CTree */
	plugPlugin *selPlug;		/* Selected entry in the CTree */
	GHashTable *inst_hash;		/* Parsed "Plugins.help" */
	char *(*inst_names)[2];		/* Array of all installed plugins, from "Plugins.help" */

	GtkWidget *entryFilter;
	char *filter;				/* Current filter string */
	gboolean filterName;		/* Filter by name (or by help)? */

	GtkWidget *window;			/* The toplevel widget */
	GtkWidget *paned;
	GtkWidget *ctree;

	GtkWidget *scroll;			/* Scrolled window containing plugin details */
	GtkWidget *vbox;			/* Box containing plugin details */

	GtkWidget *hbox;			/* Plugin details button bar including ">" */
	GtkWidget *hbox2;			/* Plugin details button bar excluding ">" */
	GtkWidget *wshow;			/* Button ">" */
	GtkWidget *instance;		/* Button "Add instance" */
	GtkWidget *enable;			/* Button "Toggle enabled" */

	GtkWidget *m_instance;		/* Menu item "Add instance" */
	GtkWidget *m_enable;		/* Menu item "Toggle enabled" */
} pguiPlug;

static pguiPlug pgui_plug;

static void readln (char **pos)
{
	if (**pos)
		(*pos)++;
	while (**pos != '\n' && **pos != '\0') (*pos)++;
	if (**pos)
		(*pos)++;
}

/*********************************************************************
  Load and parse help file PLUG_FILE if not already loaded.
*********************************************************************/
static void plug_load_help (pguiPlug *plug)
{
	FILE *fp = NULL;
	struct stat stbuf;
	char *content = NULL;
	char *pos, *fstart, *nstart, *hstart;
	int i_cnt = 0, i_len = 0;

	if (plug->inst_hash) return;

	/* Load help file */
	if (stat(PLUG_FILE, &stbuf) != 0 || stbuf.st_size <= 0)
		return;
	if (!(fp = fopen(PLUG_FILE, "r")))
		return;
	content = g_malloc (stbuf.st_size+1);
	content[stbuf.st_size] = '\0';
	if (fread (content, 1, stbuf.st_size, fp) != stbuf.st_size) {
		g_free (content);
		fclose (fp);
		return;
	}
	fclose (fp);

	/* Parse help file */
	plug->inst_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (pos = content; pos && *pos;) {
		pos = strstr (pos, "\n------");
		if (!pos || !*pos) break;
		readln (&pos);

		fstart = pos;
		pos = strstr (pos, " | ");
		if (!pos || !*pos) break;
		*pos = '\0';
		pos += 3;

		nstart = pos;
		while (*pos && *pos != '\n') pos++;
		if (!pos || !*pos) break;
		*pos++ = '\0';
		readln (&pos);
		while (*pos == '\n') pos++;

		hstart = pos;
		pos = strstr (pos, "\n-------");

		if (fstart && nstart && hstart) {
			pguiHelp *data;

			if (pos && *pos) {
				*pos++ = '\0';
				*pos = '\n';
			}

			data = g_malloc (sizeof(pguiHelp));
			data->help = gui_get_utf8 (hstart);
			data->name = gui_get_utf8 (nstart);
			data->fname = gui_get_utf8 (fstart);

			if (!data->help || !data->name || !data->fname) {
				if (data->help)
					g_free (data->help);
				if (data->name)
					g_free (data->name);
				if (data->fname)
					g_free (data->fname);
				g_free (data);
			} else {
				g_hash_table_insert (plug->inst_hash, data->fname, data);
				if (i_cnt >= i_len-1) {
					i_len += 20;
					plug->inst_names = g_realloc (plug->inst_names,
												  sizeof(plug->inst_names[0]) * i_len);
				}
				plug->inst_names[i_cnt][INST_FILE] = data->fname;
				plug->inst_names[i_cnt++][INST_HELP] = data->help;
			}
		}
	}
	if (i_cnt)
		plug->inst_names[i_cnt][INST_FILE] = NULL;
	g_free (content);
}

/*********************************************************************
  Return the help string which belongs to the plugin file.
*********************************************************************/
static pguiHelp *plug_get_help (pguiPlug *plug, const char *file)
{

#define HELP_NO_FILE \
	"Unable to read help file\n" \
	"\""PLUG_FILE"\" !\n" \
	"Try using \"icewing-docgen\" to generate it."
#define HELP_NO_HELP \
	"No help for this plugin available. To update the help file\n" \
	" \""PLUG_FILE"\"\n" \
	"use \"icewing-docgen\"."

	static pguiHelp helpBuf = {"N/A", HELP_NO_FILE, "N/A"};
	pguiHelp *help = &helpBuf;

	/* Get the help string */
	if (plug->inst_hash) {
		char f[PATH_MAX];
		const char *fpos;

		*f = '\0';
		if ((fpos = strrchr (file, '/')))
			fpos++;
		else
			fpos = file;
		if (strncmp(fpos, "lib", 3))
			strcpy (f, "lib");
		strcat (f, fpos);
		if (strncmp(&fpos[strlen(fpos)-3], ".so", 3))
			strcat (f, ".so");
		help = g_hash_table_lookup (plug->inst_hash, f);
		if (!help)
			help = g_hash_table_lookup (plug->inst_hash, fpos);
		if (!help) {
			helpBuf.help = HELP_NO_HELP;
			help = &helpBuf;
		}
	}
	return help;
}

/*********************************************************************
  Return if the plugin entry name/help should be shown based on the
  current filter settings.
*********************************************************************/
static gboolean filter_show (pguiPlug *plug, const char *name, const char *help)
{
	char *up;
	gboolean show = FALSE;

	if (name) {
		up = g_strdup (name);
		g_strup (up);
		show = strstr (up, plug->filter) != NULL;
		g_free (up);
	}
	if (!show && !plug->filterName && help) {
		up = g_strdup (help);
		g_strup (up);
		show = strstr (up, plug->filter) != NULL;
		g_free (up);
	}

	return show;
}

/*********************************************************************
  Display one row in the plugin display.
*********************************************************************/
static void cb_update_plugs (plugInfoPlug *info, pguiPlug *plug)
{
	static GtkCTreeNode *parent;
	static gboolean showHelp;
	GtkCTreeNode *node;
	const char *text[2] = {NULL, NULL};

	text[0] = info->name;
	if (!info->plug) {
		gboolean show = TRUE;

		if (pgui_plug.filter) {
			pguiHelp *help = plug_get_help (plug, text[0]);
			showHelp = filter_show (plug, NULL, help->help);
			show = showHelp || filter_show (plug, text[0], NULL);
		}
		if (show) {
			node = gtk_ctree_insert_node (GTK_CTREE(plug->ctree), NULL, NULL,
										  (char**)text, 0,
										  NULL, NULL, NULL, NULL,
										  FALSE, TRUE);
			parent = node;
		} else
			parent = NULL;
	} else if (parent && (!pgui_plug.filter ||
						  showHelp ||
						  filter_show (plug, text[0], NULL))) {
		node = gtk_ctree_insert_node (GTK_CTREE(plug->ctree), parent, NULL,
									  (char**)text, 0,
									  NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
		gtk_ctree_node_set_row_data (GTK_CTREE(plug->ctree), node, info->plug);
		gui_tree_show_bool (plug->ctree, node, 1, info->plug && info->enabled);
	}
}

/*********************************************************************
  Update the CTree on the plugins page.
*********************************************************************/
void plug_update_plugs (void)
{
	static int first = TRUE;
	GtkCList *clist = GTK_CLIST(pgui_plug.ctree);
	GtkRequisition req;
	int width;

	if (first) {
		/* Set initial paned position after the session file was applied */
		gtk_paned_set_position (GTK_PANED(pgui_plug.paned), 9999);
		first = FALSE;
	}

	gtk_clist_freeze (clist);

	gtk_clist_clear (clist);
	plug_info_plugs ((plugInfoPlugFunc)cb_update_plugs, &pgui_plug);
	if (pgui_plug.inst_names) {
		char *text[2] = {ROOT_INSTALLED, NULL};
		GtkCTreeNode *parent;
		int i;

		parent = gtk_ctree_insert_node (GTK_CTREE(clist), NULL, NULL,
										text, 0,
										NULL, NULL, NULL, NULL,
										FALSE, TRUE);
		for (i=0; pgui_plug.inst_names[i][INST_FILE]; i++) {
			if (!pgui_plug.filter
				|| filter_show (&pgui_plug,
								pgui_plug.inst_names[i][INST_FILE],
								pgui_plug.inst_names[i][INST_HELP])) {
				text[0] = pgui_plug.inst_names[i][INST_FILE];
				gtk_ctree_insert_node (GTK_CTREE(clist), parent, NULL,
									   text, 0,
									   NULL, NULL, NULL, NULL,
									   TRUE, TRUE);
			}
		}
	}

	gtk_widget_size_request (pgui_plug.window, &req);
	width = gtk_clist_optimal_column_width (clist, 0);
	if (width > req.width/1.5) width = req.width/1.5;
	gtk_clist_set_column_width (clist, 0, width);

	gtk_clist_select_row (clist, 0, -1);

	gtk_clist_thaw (clist);
}

static void plug_toggle_enabled (GtkWidget *ctree, GtkCTreeNode *node)
{
	if (node) {
		plugPlugin *plug = gtk_ctree_node_get_row_data (GTK_CTREE(ctree), node);
		if (plug) {
			plug_set_enable (plug, !plug->enabled);
			gui_tree_show_bool (ctree, node, 1, plug->enabled);
		}
	}
}

/*********************************************************************
  If clicked on the "Enabled"-Row, enable/disable the plugin.
*********************************************************************/
static gint cb_plug_ctree_press (GtkWidget *ctree, GdkEventButton *event, gpointer data)
{
	gint row, column;
	if (event->type == GDK_2BUTTON_PRESS) {
		if (gtk_clist_get_selection_info (GTK_CLIST(ctree), event->x, event->y,
										  &row, &column)) {
			GtkCTreeNode *node = gtk_ctree_node_nth (GTK_CTREE(ctree), row);
			plug_toggle_enabled (ctree, node);
		}
		return TRUE;
	}
	return FALSE;
}

/*********************************************************************
  Toggle enabled in context menu for plugins selected.
*********************************************************************/
static void cb_plug_enable (GtkWidget *widget, GtkWidget *ctree)
{
	GtkCTreeNode *node;

	if (GTK_CLIST (ctree)->focus_row < 0) return;
	node = gtk_ctree_node_nth (GTK_CTREE(ctree),
							   GTK_CLIST(ctree)->focus_row);
	plug_toggle_enabled (ctree, node);
}

static void cb_plug_add_instance_init (void *id)
{
	/* Only plugins which are not initalised are called
	   by plug_init_all() -> only the new one(s) are called */
	plug_init_all (NULL);
	plug_init_options_all();
	plug_main_unregister (id);
}

/*********************************************************************
  Add instance in context menu for plugins selected.
*********************************************************************/
static void cb_plug_add_instance (GtkWidget *widget, GtkWidget *ctree)
{
	plugPlugin *plug;
	GtkCTreeNode *node;
	char *name;
	char **argv = NULL, *args = NULL;
	int argc = 0;

	if (GTK_CLIST (ctree)->focus_row < 0) return;
	node = gtk_ctree_node_nth (GTK_CTREE(ctree),
							   GTK_CLIST(ctree)->focus_row);
	if (node) {
		if (pgui_plug.scroll) {
			const char *str = gtk_entry_get_text (GTK_ENTRY(pgui_plug.entryArgs));
			if (str && *str) {
				args = strdup (str);
				if (!iw_load_args (args, 0, NULL, &argc, &argv)) {
					gui_message_show ("Error",
									  "Unable to parse arguments for the new plugin instance!");
					goto error;
				}
			}
		}

		gtk_ctree_get_node_info (GTK_CTREE(ctree), node, &name,
								 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if ((plug = plug_load (name, FALSE))) {
			plug_init_prepare (plug, argc, argv);

			/* Prepare deferred plugin-init (to init it from the main thread) */
			plug_main_register_before (cb_plug_add_instance_init, NULL);
			plug_update();
		} else {
			gui_message_show ("Error", "Unable to init plugin from %s!\n", name);
			goto error;
		}
	}
	return;
 error:
	if (args)
		free (args);
	if (argv)
		free (argv);
}

/*********************************************************************
  Open the clist context menu for plugins.
*********************************************************************/
static gint cb_plug_menu_popup (GtkCList *clist, GdkEventButton *event, GtkMenu *menu)
{
	gint row, column;

	if (!event || event->window != clist->clist_window || event->button != 3)
		return FALSE;
	if (!gtk_clist_get_selection_info (clist, event->x, event->y, &row, &column))
		return FALSE;

	GTK_CLIST(clist)->focus_row = row;
	gtk_clist_select_row (clist, row, column);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);

	gtk_signal_emit_stop_by_name (GTK_OBJECT(clist), "button_press_event");
	return TRUE;
}

/*********************************************************************
  Show plugin name, arguments, and help information.
*********************************************************************/
static void plug_show_details (pguiPlug *plug)
{
	gtk_widget_set_sensitive (plug->m_instance, plug->selFile!=NULL);
	gtk_widget_set_sensitive (plug->m_enable, plug->selPlug!=NULL);

	if (plug->scroll) {
		gboolean dofree = FALSE;
		char *label;

		gtk_widget_set_sensitive (plug->instance, plug->selFile!=NULL);
		gtk_widget_set_sensitive (plug->enable, plug->selPlug!=NULL);

		if (!plug->selPlug || plug->selPlug->argc <= 0) {
			label = "";
		} else {
			int i;

			label = g_malloc (ARGV_MAXLEN+5);
			dofree = TRUE;
			*label = '\0';
			for (i = 0; i < plug->selPlug->argc; i++) {
				if (strlen(label) + strlen(plug->selPlug->argv[i]) > ARGV_MAXLEN)
					break;
				if (strchr (plug->selPlug->argv[i], ' ')) {
					strcat (label, "\"");
					strcat (label, plug->selPlug->argv[i]);
					strcat (label, "\" ");
				} else {
					strcat (label, plug->selPlug->argv[i]);
					strcat (label, " ");
				}
			}
		}
		gtk_entry_set_text (GTK_ENTRY(plug->entryArgs), label);
		gtk_editable_set_position (GTK_EDITABLE(plug->entryArgs), -1);
		if (dofree) g_free (label);

		if (plug->selFile) {
			pguiHelp *help = plug_get_help (plug, plug->selFile);
			char *file =  strrchr (plug->selFile, '/');
			if (file)
				file++;
			else
				file = plug->selFile;

			label = g_strconcat (file, "(plugin)  ", help->name, "(first instance)", NULL);
			gtk_label_set_text (GTK_LABEL(plug->labName), label);
			g_free (label);

			gtk_label_set_text (GTK_LABEL(plug->labHelp), help->help);
		} else {
			gtk_label_set_text (GTK_LABEL(plug->labName), "N/A");
			gtk_label_set_text (GTK_LABEL(plug->labHelp), "N/A");
		}
	}
}

/*********************************************************************
  Plugin row selected -> show plugin details.
*********************************************************************/
static void cb_plug_select_row (GtkWidget *ctree, gint row, gint column,
								GdkEventButton *event, pguiPlug *gplug)
{
	GtkCTreeNode *node;
	GtkCTreeRow *tree_row;
	plugPlugin *plug;
	char *file;

	node = gtk_ctree_node_nth (GTK_CTREE(ctree), row);
	tree_row = GTK_CTREE_ROW(node);
	plug = gtk_ctree_node_get_row_data (GTK_CTREE(ctree), node);
	if (plug)
		node = tree_row->parent;
	gtk_ctree_get_node_info (GTK_CTREE(ctree), node, &file,
							 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (plug || strcmp(ROOT_INSTALLED, file))
		gplug->selFile = file;
	else
		gplug->selFile = NULL;
	gplug->selPlug = plug;
	plug_show_details (gplug);
}

/*********************************************************************
  Show/Hide the plugin arguments and help information.
*********************************************************************/
static void cb_plug_details (GtkWidget *widget, pguiPlug *plug)
{
	if (!plug->scroll) {
		GtkWidget *label, *table;
		int pos, width, height;

		plug->scroll = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(plug->scroll),
										GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX(plug->vbox), plug->scroll, TRUE, TRUE, 0);

		table = gtk_table_new (5, 2, FALSE);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(plug->scroll),
											   table);

		label = gtk_label_new ("Name: ");
		gtk_misc_set_alignment (GTK_MISC(label), 1.0, 0.0);
		gtk_table_attach (GTK_TABLE(table), label,
						  0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
		plug->labName = gtk_label_new ("--");
		gtk_label_set_selectable (GTK_LABEL(plug->labName), TRUE);
		gtk_misc_set_alignment (GTK_MISC(plug->labName), 0.0, 0.5);
		gtk_table_attach (GTK_TABLE(table), plug->labName,
						  1, 2, 0, 1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 3);

		label = gtk_hseparator_new();
		gtk_table_attach (GTK_TABLE(table), label,
						  0, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

		label = gtk_label_new ("Args: ");
		gtk_misc_set_alignment (GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach (GTK_TABLE(table), label,
						  0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
		plug->entryArgs = gtk_entry_new_with_max_length (ARGV_MAXLEN);
		gtk_table_attach (GTK_TABLE(table), plug->entryArgs,
						  1, 2, 2, 3, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 3);

		label = gtk_hseparator_new();
		gtk_table_attach (GTK_TABLE(table), label,
						  0, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

		label = gtk_label_new ("Help: ");
		gtk_misc_set_alignment (GTK_MISC(label), 1.0, 0.0);
		gtk_table_attach (GTK_TABLE(table), label,
						  0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);

		plug->labHelp = gtk_label_new ("--");
		gtk_label_set_selectable (GTK_LABEL(plug->labHelp), TRUE);
		gtk_label_set_justify (GTK_LABEL(plug->labHelp), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC(plug->labHelp), 0.0, 0.5);
		gtk_table_attach (GTK_TABLE(table), plug->labHelp,
						  1, 2, 4, 5, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 3);

		/* Try to guess a good divider position.*/
		pos = gtk_clist_optimal_column_width (GTK_CLIST(plug->ctree), 0);
		width = GTK_CLIST(plug->ctree)->column[0].width;
		if (pos > width)
			pos = width;

		if (plug->window) {
			GtkRequisition req;
			gtk_widget_size_request (plug->window, &req);
			if (pos > req.width/1.5)
				pos = req.width/1.5;
			gdk_window_get_size (plug->window->window, &width, &height);
		}
		pos += 60;
		if (pos+350 > width && plug->window)
			gtk_window_resize (GTK_WINDOW(plug->window), pos+350, height);
		gtk_paned_set_position (GTK_PANED(plug->paned), pos);

		gtk_label_set_text (GTK_LABEL(GTK_BUTTON(plug->wshow)->bin.child), ">");
		gtk_box_pack_start (GTK_BOX(pgui_plug.hbox), pgui_plug.hbox2, TRUE, TRUE, 0);

		plug_show_details (plug);
	} else {
		gtk_widget_destroy (plug->scroll);
		plug->scroll = NULL;

		gtk_container_remove (GTK_CONTAINER(pgui_plug.hbox), pgui_plug.hbox2);

		gtk_label_set_text (GTK_LABEL(GTK_BUTTON(plug->wshow)->bin.child), "<");
		gtk_paned_set_position (GTK_PANED(plug->paned), 9999);
	}
	gtk_widget_show_all (plug->vbox);
}

static void filter_get (pguiPlug *plug)
{
	const char *str = gtk_entry_get_text (GTK_ENTRY(pgui_plug.entryFilter));

	if (plug->filter)
		g_free (plug->filter);
	if (str && *str) {
		plug->filter = g_strdup (str);
		g_strup (plug->filter);
	} else
		plug->filter = NULL;
}
/*********************************************************************
  Filter by name selected.
*********************************************************************/
static void cb_filter_name (GtkWidget *widget, pguiPlug *plug)
{
	filter_get (plug);
	plug->filterName = TRUE;
	plug_update_plugs();
}

/*********************************************************************
  Filter by help selected.
*********************************************************************/
static void cb_filter_help (GtkWidget *widget, pguiPlug *plug)
{
	filter_get (plug);
	plug->filterName = FALSE;
	plug_update_plugs();
}

/*********************************************************************
  Open the plugins tab on the plugin info window.
*********************************************************************/
void plug_open_plugs (GtkWidget *notebook, GtkTooltips *tooltips,
					  GtkAccelGroup *ag)
{
	static char *plugins[] = {"Plugins", "Enabled", NULL};

	GtkWidget *menu, *scroll, *widget, *vbox, *hbox, *label;

	pgui_plug.inst_hash = NULL;
	pgui_plug.inst_names = NULL;
	pgui_plug.scroll = NULL;
	pgui_plug.filter = NULL;

	pgui_plug.window = gtk_widget_get_toplevel (notebook);

	pgui_plug.paned = gtk_hpaned_new ();
	widget = gtk_label_new ("Plugins");
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pgui_plug.paned, widget);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack1 (GTK_PANED(pgui_plug.paned), vbox, FALSE, FALSE);

	  /* Plugin ctree */
	  scroll = gtk_scrolled_window_new (NULL, NULL);
	  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
									  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

	  pgui_plug.ctree = gtk_ctree_new_with_titles (2, 0, plugins);
	  gtk_ctree_set_indent (GTK_CTREE(pgui_plug.ctree), 15);
	  gtk_clist_column_titles_passive (GTK_CLIST(pgui_plug.ctree));
	  gtk_clist_set_selection_mode (GTK_CLIST(pgui_plug.ctree),
									GTK_SELECTION_BROWSE);
	  gtk_container_add (GTK_CONTAINER (scroll), pgui_plug.ctree);

	  gtk_signal_connect (GTK_OBJECT(pgui_plug.ctree), "button_press_event",
						  GTK_SIGNAL_FUNC(cb_plug_ctree_press), NULL);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.ctree), "select_row",
						  GTK_SIGNAL_FUNC(cb_plug_select_row),
						  &pgui_plug);

	  /* Plugin context menu */
	  menu = gtk_menu_new();
	  pgui_plug.m_instance = gtk_menu_item_new_with_label ("Add instance");
	  gtk_menu_append (GTK_MENU (menu), pgui_plug.m_instance);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.m_instance), "activate",
						  GTK_SIGNAL_FUNC(cb_plug_add_instance), pgui_plug.ctree);
	  pgui_plug.m_enable = gtk_menu_item_new_with_label ("Toggle enabled");
	  gtk_menu_append (GTK_MENU (menu), pgui_plug.m_enable);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.m_enable), "activate",
						  GTK_SIGNAL_FUNC(cb_plug_enable), pgui_plug.ctree);
	  gtk_widget_show_all (menu);

	  gtk_signal_connect (GTK_OBJECT(pgui_plug.ctree), "button_press_event",
						  GTK_SIGNAL_FUNC (cb_plug_menu_popup),
						  menu);

	  /* Filter button bar */
	  hbox = gtk_hbox_new (FALSE, 2);
	  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

		label = gtk_label_new ("Filter:");
		gtk_label_set_pattern (GTK_LABEL(label), "_");
		gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, TRUE, 0);

		pgui_plug.entryFilter = gtk_entry_new ();
		gtk_tooltips_set_tip (tooltips, pgui_plug.entryFilter,
							  "Filter tree content by plugin name and/or plugin help", NULL);
		gtk_box_pack_start (GTK_BOX(hbox), pgui_plug.entryFilter, TRUE, TRUE, 0);

		widget = gui_button_new_with_accel ("_Name", &ag);
		gtk_tooltips_set_tip (tooltips, widget,
							  "Filter tree content by plugin name", NULL);
		gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							GTK_SIGNAL_FUNC(cb_filter_name), &pgui_plug);
		gtk_box_pack_start (GTK_BOX(hbox), widget, FALSE, TRUE, 0);

		widget = gui_button_new_with_accel ("_Help", &ag);
		gtk_tooltips_set_tip (tooltips, widget,
							  "Filter tree content by plugin name and plugin help", NULL);
		gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							GTK_SIGNAL_FUNC(cb_filter_help), &pgui_plug);
		gtk_box_pack_start (GTK_BOX(hbox), widget, FALSE, TRUE, 0);

		/* Accel group ag is now initialised */
		gtk_widget_add_accelerator (pgui_plug.entryFilter, "grab_focus", ag,
									gdk_keyval_to_lower('f'), GDK_MOD1_MASK, 0);

	/* Plugin details pane */
	pgui_plug.vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack2 (GTK_PANED(pgui_plug.paned), pgui_plug.vbox, TRUE, FALSE);

	  pgui_plug.hbox = gtk_hbox_new (FALSE, 0);
	  gtk_box_pack_end (GTK_BOX(pgui_plug.vbox), pgui_plug.hbox, FALSE, FALSE, 0);

	  pgui_plug.wshow = gtk_button_new_with_label("<");
	  gtk_container_set_border_width (GTK_CONTAINER(pgui_plug.wshow), 2);
	  gtk_tooltips_set_tip (tooltips, pgui_plug.wshow,
							"Show/Hide plugin details", NULL);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.wshow), "clicked",
						  GTK_SIGNAL_FUNC(cb_plug_details), &pgui_plug);
	  gtk_box_pack_start (GTK_BOX(pgui_plug.hbox), pgui_plug.wshow, FALSE, TRUE, 0);

	  pgui_plug.hbox2 = gtk_hbox_new (TRUE, 5);
	  gtk_container_set_border_width (GTK_CONTAINER(pgui_plug.hbox2), 2);
	  gtk_widget_show (pgui_plug.hbox2);
	  gtk_object_ref (GTK_OBJECT(pgui_plug.hbox2));

	  pgui_plug.instance = gtk_button_new_with_label("Add instance");
	  gtk_tooltips_set_tip (tooltips, pgui_plug.instance,
							"Add a new instance of the selected plugin type "
							"with command line arguments as specified in the text entry", NULL);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.instance), "clicked",
						  GTK_SIGNAL_FUNC(cb_plug_add_instance), pgui_plug.ctree);
	  gtk_box_pack_start (GTK_BOX(pgui_plug.hbox2), pgui_plug.instance, TRUE, TRUE, 0);

	  pgui_plug.enable = gtk_button_new_with_label("Toggle enabled");
	  gtk_tooltips_set_tip (tooltips, pgui_plug.enable,
							"Toggle the calling of the plugin process function", NULL);
	  gtk_signal_connect (GTK_OBJECT(pgui_plug.enable), "clicked",
						  GTK_SIGNAL_FUNC(cb_plug_enable), pgui_plug.ctree);
	  gtk_box_pack_start (GTK_BOX(pgui_plug.hbox2), pgui_plug.enable, TRUE, TRUE, 0);

	plug_load_help (&pgui_plug);
}
