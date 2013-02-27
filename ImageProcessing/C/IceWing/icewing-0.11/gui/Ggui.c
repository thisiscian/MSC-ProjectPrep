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
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <pthread.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "tools/tools.h"
#include "Gpreferences.h"
#include "Goptions_i.h"
#include "Gpreview_i.h"
#include "Gsession.h"
#include "Gcolor.h"
#include "Gtools.h"
#include "images/prefs.xpm"
#include "Ggui_i.h"

char *GUI_PRG_NAME = "Unknown";
char *GUI_VERSION = "0.0";

typedef enum {
	GUI_RUNNING,
	GUI_PENDING,
	GUI_PENDING_IMM,
	GUI_EXITING
} guiExitState;

typedef struct guiPage {
	GtkWidget *window;			/* Window for the detached page */
	GtkWidget *container;		/* The to be detached page */
	GtkWidget *focus_widget;	/* Widget, which gets focus on hotkey press */
	GtkCTreeNode *node;			/* CTree node belonging to container */
	char *title;
	guint hotkey;
	struct guiWindow *guiWindow;

	struct guiPage *last;
} guiPage;

typedef struct guiWindow {
	GtkWidget *window;			/* Main window */
	GtkAccelGroup *accel;		/* Main menu accelerator group */
	GtkWidget *ctree;
	GtkWidget *notebook;
	guiPage *pages;				/* Last entry in the page list */
} guiWindow;

static gboolean gui_iconified = FALSE;

pthread_t gui_thread = (pthread_t) NULL;

static int gui_exit_pipe[2] = {-1, -1};
static guiExitState gui_exit_state = GUI_RUNNING;
static int gui_exit_status;
static gboolean gui_immediate_exit = TRUE;

static char **gui_icon = NULL;
static GtkTooltips *gui_tooltips;

/*********************************************************************
  Bring window to front and activate it.
*********************************************************************/
static void gui_window_focus (GtkWidget *window)
{
	if (window->window) {
		gdk_window_raise (window->window);

		/* Prevent BadMatch, if window is not viewable */
		gdk_error_trap_push ();
		XSetInputFocus (GDK_WINDOW_XDISPLAY(window->window),
						GDK_WINDOW_XWINDOW(window->window),
						RevertToParent, CurrentTime);
		gdk_flush();
		gdk_error_trap_pop ();
	}
}

/*********************************************************************
  Set a tooltip for widget 'widget'.
*********************************************************************/
void gui_tooltip_set (GtkWidget *widget, const gchar *tip_text)
{
	if (tip_text)
		gtk_tooltips_set_tip (gui_tooltips, widget, tip_text, NULL);
}

/*********************************************************************
  Use icon for all new windows as a window icon. Should be called
  before gui_init().
*********************************************************************/
void gui_set_icon (char **icon)
{
#if GTK_CHECK_VERSION(2,0,0)
	GdkPixbuf  *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**)icon);
	GList *list = NULL;
	list = g_list_append (list, pixbuf);
	gtk_window_set_default_icon_list (list);
	g_list_free (list);
#else
	gui_icon = icon;
#endif
}

/*********************************************************************
  Install the default window icon for the window win.
*********************************************************************/
void gui_apply_icon (GtkWidget *win)
{
	static GdkPixmap *icon;
	static GdkBitmap *mask;
	GdkAtom icon_atom;
	glong data[2];

	if (!gui_icon) return;

	gtk_widget_realize (win);

	if (!icon)
		icon = gdk_pixmap_create_from_xpm_d (win->window, &mask,
											 &win->style->bg[GTK_STATE_NORMAL], gui_icon);
	data[0] = GDK_WINDOW_XWINDOW(icon);
	data[1] = GDK_WINDOW_XWINDOW(mask);

	icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
	gdk_property_change (win->window, icon_atom, icon_atom, 32,
						 GDK_PROP_MODE_REPLACE, (guchar *)data, 2);
	gdk_window_set_icon (win->window, NULL, icon, mask);
}

/*********************************************************************
  Terminate the program and return 'status' to the system.
  Must be called instead of exit() if the program should exit.
*********************************************************************/
void gui_exit (int status)
{
	gui_exit_status = status;
	gui_exit_state = GUI_EXITING;

	/* If the pipe is not open, exit normally */
	if (gui_exit_pipe[1] == -1)
		exit (status);

	if (GUI_IS_GUI_THREAD()) {
		prev_close_avi_all();
		if (iw_prefs_get()->save_atexit)
			iw_session_save();
		gtk_exit (status);
	} else {
		gchar state = status;
		write (gui_exit_pipe[1], &state, 1);
		while (1) sleep(1);
	}
}

static pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

/*********************************************************************
  Lock/Unlock gui_try_exit().
*********************************************************************/
void gui_exit_lock (void)
{
	pthread_mutex_lock (&exit_mutex);
}
void gui_exit_unlock (void)
{
	pthread_mutex_unlock (&exit_mutex);
	if (gui_exit_state == GUI_PENDING_IMM)
		gui_exit (gui_exit_status);
}
/*********************************************************************
  If gui_exit_lock() is not active, call gui_exit().
  Otherwise schedule an exit after gui_exit_unlock() and return.
*********************************************************************/
void gui_try_exit (int status)
{
	if (pthread_mutex_trylock (&exit_mutex) == 0)
		gui_exit (status);
	gui_exit_status = status;
	gui_exit_state = GUI_PENDING_IMM;
}

/*********************************************************************
  If immediate_exit is TRUE exit immediately. Otherwise schedule an
  exit and return. The exit will be performed when gui_check_exit()
  is called.
*********************************************************************/
void gui_schedule_exit (int status)
{
	if (gui_immediate_exit) {
		gui_exit (status);
	} else if (gui_exit_state < GUI_PENDING) {
		gui_exit_status = status;
		gui_exit_state = GUI_PENDING;
	}
}

/*********************************************************************
  Check if an exit is pending (from a click on the Quit-Button or a
  call to gui_schedule_exit()) and exit.
  immediate_exit==TRUE (default): On click on the Quit-button the
      program exits immediately, otherwise it exits only if gui_exit()
      or gui_check_exit() is called.
*********************************************************************/
void gui_check_exit (gboolean immediate_exit)
{
	if (gui_exit_state == GUI_PENDING || gui_exit_state == GUI_PENDING_IMM)
		gui_exit (gui_exit_status);
	else if (gui_exit_state == GUI_EXITING) {
		if (GUI_IS_GUI_THREAD())
			gui_exit (gui_exit_status);
		else
			while (1) sleep(1);
	}
	gui_immediate_exit = immediate_exit;
}

static void cb_exit_read (gpointer data, gint source, GdkInputCondition condition)
{
	gchar state;

	if (condition==GDK_INPUT_READ && source==gui_exit_pipe[0])
		if (read (gui_exit_pipe[0], &state, 1) == 1)
			gui_exit (state);
}

/*********************************************************************
  Can be used as a GtkSignalFunc() for a callback to exit the program.
*********************************************************************/
gboolean gui_delete_cb (GtkWidget *widget, GdkEvent *event, gpointer *data)
{
	/* This callback is always called with the gdk mutex held.
	   For exiting iceWing this must be dropped so that
	   the main thread can handle the quit events. */
	gui_threads_leave();
	gui_schedule_exit (0);
	return TRUE;
}

static void parse_rcfiles (void)
{
	char file[PATH_MAX];

	gui_convert_prg_rc (file, "gtkrc", TRUE);
	gtk_rc_parse (file);

	gui_convert_prg_rc (file, "rc", FALSE);
	gtk_rc_parse (file);
}

/*********************************************************************
  Set version and name of programm. Used for files and windows.
*********************************************************************/
void gui_set_prgname (char *prgname, char *version)
{
	if (!gui_thread) gui_thread = pthread_self();

	GUI_PRG_NAME = g_strdup (prgname);
	GUI_VERSION = g_strdup (version);
}

/*********************************************************************
  Start main window iconified? Should be called before gui_init().
*********************************************************************/
void gui_set_iconified (gboolean iconified)
{
	gui_iconified = iconified;
}

/*********************************************************************
  Activate page in the notebook / bring window page to front.
*********************************************************************/
static void listbook_select (guiPage *page)
{
	if (page) {
		GtkNotebook *notebook = GTK_NOTEBOOK(page->guiWindow->notebook);
		if (page->window)
			gdk_window_raise (page->window->window);
		else
			gtk_notebook_set_page (notebook,
								   gtk_notebook_page_num (notebook, page->container));
	}
}

static void cb_listbook_select (GtkWidget *ctree, gint row, gint column,
								GdkEventButton *event, gpointer data)
{
	guiPage *page = gtk_clist_get_row_data (GTK_CLIST(ctree), row);
	listbook_select (page);
}

/*********************************************************************
  Handle the hotkeys for the notebook pages.
*********************************************************************/
static gboolean cb_listbook_key (GtkWidget *gtkWindow, GdkEvent *event, gpointer data)
{
	guiWindow *window = data;
	guiPage *page = window->pages;
	guint keyval = gdk_keyval_to_lower (event->key.keyval);

	while (page) {
		if (keyval == page->hotkey
			&& (event->key.state & GDK_MOD1_MASK)
			&& !(event->key.state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK))) {
			listbook_select (page);
			if (page->window)
				gui_window_focus (page->window);
			else
				gui_window_focus (window->window);
			gtk_widget_grab_focus (page->focus_widget);
			gtk_ctree_select (GTK_CTREE(window->ctree), page->node);

			return TRUE;
		}
		page = page->last;
	}
	return FALSE;
}

/*********************************************************************
  Return the keyval of hotkey, it hotkey is unique. Otherwise return
  an entry from label
*********************************************************************/
static guint listbook_set_hotkey (GtkLabel *label, int hotkey)
{
	static char *keys = NULL;
	static char keys_len = 1;

	int len = strlen(label->label), hk, round;
	char *pattern, *lab = label->label;

	/* Find a good hotkey which is not already taken */
	round = 0;
	hk = hotkey;
	while (round <= 1 && keys && strchr(keys, tolower((int)lab[hk]))) {
		do {
			hk = (hk+1) % len;
			if (hk == hotkey)
				round++;
		} while (! ((round == 0 &&
					 ( isupper((int)lab[hk]) ||
					   hk==0 ||
					   (!isalpha((int)lab[hk-1]) && isalpha((int)lab[hk])) ||
					   (isalpha((int)lab[hk-1]) && isdigit((int)lab[hk])) ) ) ||
					(round == 1 && isalpha((int)lab[hk])) ||
					(round > 1)) );
	}
	if (round <= 1) {
		keys_len++;
		keys = realloc (keys, sizeof(char)*keys_len);
		keys[keys_len-1] = '\0';
		keys[keys_len-2] = tolower((int)lab[hk]);

		pattern = malloc (sizeof(char)*(len+1));
		memset (pattern, ' ', len);
		pattern[len] = '\0';
		pattern[hk] = '_';
		gtk_label_set_pattern (label, pattern);
		free (pattern);

		return gdk_keyval_to_lower (lab[hk]);
	} else
		return 0;
}

static gboolean cb_detach_attach_delete (GtkWidget *wid, GdkEvent *event, gpointer data);

static void cb_detach_attach (GtkWidget *wid, gpointer data)
{
	guiPage *page = data, *pos;
	GtkNotebook *notebook = GTK_NOTEBOOK(page->guiWindow->notebook);
	int i;

	if (page->window) {			/* Is detached, so attach */
		GtkCTree *ctree = GTK_CTREE(page->guiWindow->ctree);

		gtk_widget_ref (page->container);
		gtk_container_remove (GTK_CONTAINER (page->window), page->container);

		i = 0;
		for (pos=page; pos; pos=pos->last)
			i += pos->window ? 0 : 1;
		gtk_notebook_insert_page (notebook, page->container, NULL, i);

		if (GTK_CLIST(ctree)->selection) {
			guiPage *page_sel =
				gtk_ctree_node_get_row_data (ctree,
											 GTK_CLIST(ctree)->selection->data);
			if (page == page_sel)
				gtk_notebook_set_page (notebook, i);
		}

		gtk_widget_unref (page->container);

		iw_session_remove (page->title);
		gtk_widget_destroy (page->window);
		page->window = NULL;
	} else {						/* Is attached, so detach */
		page->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_add_accel_group (GTK_WINDOW(page->window), page->guiWindow->accel);
		gtk_window_set_title (GTK_WINDOW (page->window), GUI_PRG_NAME);
		gtk_signal_connect (GTK_OBJECT (page->window), "key_press_event",
							GTK_SIGNAL_FUNC (cb_listbook_key),
							page->guiWindow);
		gtk_signal_connect (GTK_OBJECT (page->window), "delete_event",
							GTK_SIGNAL_FUNC (cb_detach_attach_delete), page);

		gtk_widget_ref (page->container);
		gtk_notebook_remove_page (notebook,
								  gtk_notebook_page_num (notebook, page->container));
		gtk_container_add (GTK_CONTAINER (page->window), page->container);
		gtk_widget_unref (page->container);

		if (GTK_IS_SCROLLED_WINDOW(page->container)) {
			GtkWidget *w = GTK_BIN(page->container)->child;
			if (w) {
				int height = gdk_screen_height() * 2/3;
				if (w->requisition.height < height)
					height = w->requisition.height;
				gtk_window_set_default_size (GTK_WINDOW (page->window), -1, height);
			}
		}

		iw_session_set (page->window, page->title, TRUE, TRUE);
		gui_apply_icon (page->window);
		gtk_widget_show (page->window);
	}
}

static gboolean cb_detach_attach_delete (GtkWidget *wid, GdkEvent *event, gpointer data)
{
	cb_detach_attach (wid, data);
	return FALSE;
}

typedef struct {
	GtkCTreeNode *parent;			/* Parent node in the ctree */
	char *title;
	int children;					/* Number of children */
} lbParent;
/*********************************************************************
  PRIVATE: Append a new page to a ctree/notebook-combination.
*********************************************************************/
GtkWidget *gui_listbook_append (GtkWidget *ctree, const gchar *label)
{
	static GHashTable *parents_hash = NULL;
	static int use_tree = -1;	/* widget style: uninitialised  */
	static int use_scrolled = -1;
	guiWindow *window;
	GtkWidget *out_vbox, *hbox, *vbox, *frame, *widget;
	gchar *titles[1];
	guiPage *page;
	GtkCTreeNode *node = NULL;
	int hotkey;

	iw_assert (ctree && label && *label, "Wrong arguments in gui_listbook_append()");

	window = gtk_object_get_user_data (GTK_OBJECT (ctree));

	page = g_malloc0 (sizeof(guiPage));
	page->title = g_strdup (label);
	page->guiWindow = window;
	page->last = window->pages;

	window->pages = page;

	/* Scrolled window for the categories pages? */
	if (use_scrolled < 0)
		use_scrolled = iw_prefs_get()->use_scrolled;

	/* Initialise widget style */
	if (use_tree < 0) {
		use_tree = iw_prefs_get()->use_tree;
		use_tree = (use_tree == PREF_TREE_CATEGORIES || use_tree == PREF_TREE_BOTH);
		if (use_tree)
			gtk_ctree_set_line_style (GTK_CTREE(ctree), GTK_CTREE_LINES_SOLID);
		else
			gtk_ctree_set_line_style (GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	}

	/* Add categories entry */
	titles[0] = page->title;
	hotkey = 0;
	if (!use_tree) {
		node = gtk_ctree_insert_node (GTK_CTREE(ctree), NULL, NULL,
									  titles, 0, NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
		gtk_ctree_node_set_shift (GTK_CTREE(ctree), node, 0, 0, -20);
	} else {
		lbParent *parent, *parent_new = NULL;
		gchar *pos, pos_ch;
		gboolean is_leaf;

		pos = titles[0];
		while (*pos && *pos!=' ' && *pos!='.') pos++;
		pos_ch = *pos;
		*pos = '\0';
		is_leaf = (pos_ch == '\0') || (*(pos+1) == '\0');

		if (!parents_hash)
			parents_hash = g_hash_table_new (g_str_hash, g_str_equal);
		parent = g_hash_table_lookup (parents_hash, titles[0]);
		if (!parent || (parent && parent->children == 0)) {
			node = gtk_ctree_insert_node (GTK_CTREE(ctree), NULL, NULL,
										  titles, 0, NULL, NULL, NULL, NULL,
										  is_leaf, TRUE);
			if (!parent) {
				parent_new = g_malloc (sizeof(lbParent));
				parent_new->children = 0;
				parent_new->title = g_strdup(titles[0]);
				parent_new->parent = node;
				g_hash_table_insert (parents_hash, parent_new->title, parent_new);
			} else if (parent->children == 0) {
				if (is_leaf)
					iw_error ("Page with name %s already created", label);
				gtk_ctree_move (GTK_CTREE(ctree), parent->parent, node, NULL);
				parent->parent = node;
				parent->children++;
			}
		}
		if (parent || !is_leaf) {
			if (!parent)
				parent = parent_new;
			if (!is_leaf) {
				hotkey = pos+1-titles[0];
				titles[0] = pos+1;
			}
			node = gtk_ctree_insert_node (GTK_CTREE(ctree), parent->parent, NULL,
										  titles, 0, NULL, NULL, NULL, NULL,
										  TRUE, TRUE);
			parent->children++;
		}
		*pos = pos_ch;
	}

	gtk_ctree_node_set_row_data (GTK_CTREE(ctree), node, page);
	page->node = node;

	/* Add notebook page */
	out_vbox = gtk_vbox_new (FALSE, 0);
	if (use_scrolled) {
		widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
										GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (widget),
											   out_vbox);
#if GTK_CHECK_VERSION(2,0,0)
		/* Hack: Otherwise widgets are missing sometimes,
		         erroneous size request/allocation? */
		gtk_container_set_resize_mode (GTK_CONTAINER(GTK_BIN(widget)->child),
									   GTK_RESIZE_PARENT);
#endif
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN(widget)->child),
									  GTK_SHADOW_NONE);
		page->container = widget;
	} else
		page->container = out_vbox;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (out_vbox), hbox, FALSE, TRUE, 0);

		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
		gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

			widget = gtk_label_new (label);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			gtk_misc_set_padding (GTK_MISC (widget), 2, 1);
			gtk_container_add (GTK_CONTAINER (frame), widget);
			page->hotkey = listbook_set_hotkey (GTK_LABEL(widget), hotkey);

		widget = gtk_button_new_with_label ("D/A");
		gui_tooltip_set (widget, "Detach/attach the page from/to the main window");
		gtk_signal_connect (GTK_OBJECT(widget), "clicked",
							GTK_SIGNAL_FUNC(cb_detach_attach),
							(gpointer)page);
		gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		page->focus_widget = widget;

	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER (out_vbox), vbox);

	gtk_notebook_append_page (GTK_NOTEBOOK(window->notebook), page->container, NULL);
	gtk_widget_show_all (page->container);

	if (iw_session_find (page->title))
		cb_detach_attach (NULL, page);

	return vbox;
}

static void cb_preferences (GtkWidget *widget, void *data)
{
	iw_prefs_open();
}

/*********************************************************************
  Initialise/Create the load/save buttons of the options interface.
*********************************************************************/
static void gui_init_io (GtkBox *vbox)
{
	GtkWidget *hbox, *hbox2, *widget, *pixwid;
	GdkPixmap *pix;
	GdkBitmap *mask;
	GdkColormap *colormap;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	  widget = gtk_button_new();
	  gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(cb_preferences), NULL);
	  gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 0);

		colormap = gtk_widget_get_colormap (widget);
		pix = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
													 NULL, prefs_xpm);
		pixwid = gtk_pixmap_new (pix, mask);
		gdk_pixmap_unref (pix);
		gdk_bitmap_unref (mask);
		gtk_container_add (GTK_CONTAINER(widget), pixwid);

	hbox2 = gtk_hbox_new (TRUE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (hbox2), 5);
	gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);

	  widget = gtk_button_new_with_label("Load");
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(opts_load_cb), (gpointer)FALSE);
	  gtk_box_pack_start(GTK_BOX(hbox2), widget, TRUE, TRUE, 0);

	  widget = gtk_button_new_with_label("Load Def");
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(opts_load_cb), (gpointer)TRUE);
	  gtk_box_pack_start(GTK_BOX(hbox2), widget, TRUE, TRUE, 0);

	  widget = gtk_button_new_with_label("Save");
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(opts_save_cb), (gpointer)FALSE);
	  gtk_box_pack_start(GTK_BOX(hbox2), widget, TRUE, TRUE, 0);

	  widget = gtk_button_new_with_label("Save Def");
	  gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						  GTK_SIGNAL_FUNC(opts_save_cb), (gpointer)TRUE);
	  gtk_box_pack_start(GTK_BOX(hbox2), widget, TRUE, TRUE, 0);
}

#define ID_LOAD			1
#define ID_LOAD_DEF		2
#define ID_SAVE			3
#define ID_SAVE_DEF		4
#define ID_QUIT			5
#define ID_PREFS		6

static void cb_menu (GtkWidget *widget, int button, void *data)
{
	button = GPOINTER_TO_INT(data);
	switch (button) {
		case ID_LOAD:
			opts_load_cb (widget, GINT_TO_POINTER(FALSE));
			break;
		case ID_LOAD_DEF:
			opts_load_cb (widget, GINT_TO_POINTER(TRUE));
			break;
		case ID_SAVE:
			opts_save_cb (widget, GINT_TO_POINTER(FALSE));
			break;
		case ID_SAVE_DEF:
			opts_save_cb (widget, GINT_TO_POINTER(TRUE));
			break;
		case ID_QUIT:
			gui_delete_cb (widget, NULL, NULL);
			break;
		case ID_PREFS:
			iw_prefs_open();
			break;
	}
}

typedef struct guiMenuItem {	/* Definition of the main menu items */
	char *title;
	char *ttip;
	optsCbackFunc cback;
	void *data;
} guiMenuItem;

static guiMenuItem main_items[] = {
	{"MainMenu._File/_Load...<<control>l><stock gtk-open>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_LOAD)},
	{"MainMenu._File/Load _default<<control><shift>l><stock gtk-open>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_LOAD_DEF)},
	{"MainMenu._File/_Save...<<control>s><stock gtk-save-as>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_SAVE)},
	{"MainMenu._File/S_ave default<<control><shift>s><stock gtk-save>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_SAVE_DEF)},
	{"MainMenu._File/Sep", NULL, NULL, NULL},
	{"MainMenu._File/_Quit<<control>q><stock gtk-quit>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_QUIT)},
	{"MainMenu._Edit/_Preferences<<control>p><stock gtk-preferences>",
	 NULL, cb_menu, GINT_TO_POINTER(ID_PREFS)},
	{NULL, NULL, NULL, NULL}
};

/*********************************************************************
  Initialises the gui_(), prev_(), and opts_() - functions and
  packs into the vbox a user interface for the selection of images
  and option widgets.
  prev_width, prev_height: default size for a preview window
  rcfile: additional settings files for opts_load_default()
*********************************************************************/
static void gui_init_interface (GtkWidget *mainWindow, GtkWidget *vbox,
								char **rcfile, int prev_width, int prev_height)
{
	GtkWidget *widget, *notebook, *paned, *ctree;
	GtkItemFactory *factory;
	GtkAccelGroup *accel;
	guiMenuItem *item;
	gchar *titles[1];
	guiWindow *window;
	prevBuffer *b_menu;

	prev_col_init();

	/* Initialize the gui_exit() system */
	gui_thread = pthread_self();
	if (pipe (gui_exit_pipe) == 0)
		gdk_input_add (gui_exit_pipe[0], GDK_INPUT_READ, cb_exit_read, NULL);

	parse_rcfiles();
	gui_tooltips = gtk_tooltips_new();

	iw_session_load();

	accel = gtk_accel_group_new ();
	factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<mmenu>",  accel);

	/* Enable opts_...() calls for the main menu */
	b_menu = g_malloc0 (sizeof(prevBuffer));
	b_menu->factory = factory;
	opts_page_add__nl (NULL, b_menu, "MainMenu");

	/* Init main menu items */
	for (item = main_items; item->title; item++) {
		if (item->cback)
			opts_buttoncb_create (-1, item->title, item->ttip, item->cback, item->data);
		else
			opts_separator_create (-1, item->title);
	}

	widget = gtk_item_factory_get_widget (factory, "<mmenu>");
	gtk_box_pack_start (GTK_BOX(vbox), widget, FALSE, TRUE, 0);
	gtk_window_add_accel_group (GTK_WINDOW(mainWindow), accel);

	paned = gtk_hpaned_new ();
	gtk_container_add (GTK_CONTAINER(vbox), paned);

	/* Pane 1: The categories ctree */
	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
									GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_paned_pack1 (GTK_PANED (paned), widget, FALSE, FALSE);

	titles[0] = "Categories";
	ctree = gtk_ctree_new_with_titles (1, 0, titles);
	gtk_ctree_set_indent (GTK_CTREE(ctree), 15);
	gtk_clist_column_titles_passive (GTK_CLIST (ctree));
	gtk_clist_set_selection_mode (GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_container_add (GTK_CONTAINER (widget), ctree);

	/* Pane 2: The options notebook */
	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_paned_pack2 (GTK_PANED (paned), widget, TRUE, FALSE);
	gtk_paned_set_position (GTK_PANED (paned),
							gui_text_width(ctree, titles[0]) + 20);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_container_add (GTK_CONTAINER (widget), notebook);

	window = malloc (sizeof(guiWindow));
	window->window = mainWindow;
	window->accel = accel;
	window->notebook = notebook;
	window->ctree = ctree;
	window->pages = NULL;
	gtk_object_set_user_data (GTK_OBJECT (ctree), window);

	gtk_signal_connect (GTK_OBJECT (ctree), "select_row",
						GTK_SIGNAL_FUNC (cb_listbook_select), NULL);
	gtk_signal_connect (GTK_OBJECT (mainWindow), "key_press_event",
						GTK_SIGNAL_FUNC (cb_listbook_key), window);

	opts_init (ctree);
	iw_prefs_init();

	/* Additional buttons in the main window */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);
	gui_init_io (GTK_BOX (vbox));

	opts_load_default (rcfile);

	widget = gui_listbook_append (ctree, "Images");
	prev_init (GTK_BOX(widget), prev_width, prev_height);
	gtk_widget_show_all (widget);
	gtk_clist_select_row (GTK_CLIST(ctree), 0, -1);

#if GTK_CHECK_VERSION(2,0,0)
	g_object_set (gtk_settings_get_default(),
				  "gtk-can-change-accels", TRUE, NULL);
#endif
}

/*********************************************************************
  Initialises the gui_(), prev_(), and opts_() - functions. Opens a
  new toplevel window with a user interface for the selection of
  images and option widgets.
  prev_width, prev_height: default size for a preview window
  rcfile: additional settings file for opts_load_default()
*********************************************************************/
void gui_init (char **rcfile, int prev_width, int prev_height)
{
	GtkWidget *window, *vbox;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);
	gtk_window_set_title (GTK_WINDOW (window), GUI_PRG_NAME);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
						GTK_SIGNAL_FUNC (gui_delete_cb), NULL);

	vbox = gtk_vbox_new (FALSE, 0);

	gui_init_interface (window, vbox, rcfile, prev_width, prev_height);

	gtk_container_add (GTK_CONTAINER (window), vbox);

	iw_session_set (window, GUI_PRG_NAME, TRUE, TRUE);
	gui_apply_icon (window);

	if (gui_iconified) {
#if GTK_CHECK_VERSION(2,0,0)
		gtk_window_iconify (GTK_WINDOW (window));
#else
		XWMHints *xwmh;

		gtk_widget_realize (window);
		xwmh = XGetWMHints (GDK_WINDOW_XDISPLAY(GTK_WIDGET(window)->window),
							GDK_WINDOW_XWINDOW(GTK_WIDGET(window)->window));
		if (!xwmh) xwmh = XAllocWMHints();
		if (xwmh) {
			xwmh->initial_state = IconicState;
			xwmh->flags |= StateHint;
			XSetWMHints (GDK_WINDOW_XDISPLAY(GTK_WIDGET(window)->window),
						 GDK_WINDOW_XWINDOW(GTK_WIDGET(window)->window), xwmh);
			XFree (xwmh);
		}
#endif
	}

	gtk_widget_show_all (window);
}
