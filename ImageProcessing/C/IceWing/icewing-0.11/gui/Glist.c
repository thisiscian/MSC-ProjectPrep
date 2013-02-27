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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gdk/gdkkeysyms.h>

#include "tools/tools.h"
#include "Gtools.h"
#include "Ggui_i.h"
#include "Goptions_i.h"
#include "Glist.h"

#define LIST_DATA_LEN	256

/* Data attached to a clist widget */
typedef struct listData {
	optsListFlags flags;
	optsListData *d;

	char **label;
	char **ttips;
	int nentries;		/* Number of entries in d (including the last -1 entry) */
	int maxentries;		/* Number of allocated entries */
	GtkWidget *w_clist;
	GtkWidget *w_label;
	GtkWidget *w_entry;
	GtkWidget *w_menu;
	optsValue *opts;
} listData;

void opts_list_entry_free (optsListEntry *entries)
{
	optsListEntry *e;

	for (e = entries; e->oldindex >= 0; e++) {
		if (e->data)
			g_free (e->data);
	}
	g_free (entries);
}

static void list_entry_init (optsListEntry *entry)
{
	entry->oldindex = -1;
	entry->selected = FALSE;
	entry->data = g_malloc0 (LIST_DATA_LEN);
}

static void list_entry_reset (optsListEntry *entry)
{
	entry->oldindex = -1;
	entry->selected = FALSE;
	entry->data[0] = '\0';
}

/*********************************************************************
  Read optsListEntry-entries from string str and return them.
*********************************************************************/
void opts_list_sscanf (optsValue *opts, char *str, optsListData *odata)
{
	listData *data = opts->w.value;
	optsListEntry *entries = NULL;
	int read, nentries, index, selected;
	char *label, *buf, format[20];
	gboolean add = (data->flags & OPTS_ADD ? TRUE:FALSE);

	nentries = 1;

	/* If OPTS_ADD, list can contain any number of entries,
	   otherwise there are always the same number */
	if (add)
		entries = g_malloc (sizeof(optsListEntry)*nentries);
	else
		entries = g_malloc (sizeof(optsListEntry)*data->nentries);

	sprintf (format, "%%a%%d%%%da|%%n", LIST_DATA_LEN);
	while ((nentries < data->nentries || add) && *str) {
		buf = NULL;
		if (gui_sscanf (str, format, &label, &selected, &buf, &read) == 4) {
			for (index = 0; data->label[index] && strcmp(label, data->label[index]); index++)
				/* empty */;
			if (!data->label[index]) {
				iw_warning ("Value '%s' for list widget not available, ignoring...",
							label);
				if (buf) g_free (buf);
				str += read;
				continue;
			}
			entries[nentries-1].oldindex = index;
			entries[nentries-1].selected = selected;
			entries[nentries-1].data = buf;

			str += read;
			nentries++;
			if (add)
				entries = g_realloc (entries, sizeof(optsListEntry)*nentries);
		} else {
			if (buf) g_free (buf);
			break;
		}
	}

	if (!add) {
		/* !add && to few new entries -> fill with missing entries */
		int i, unused = 0;
		index = nentries;
		for (; nentries < data->nentries; nentries++) {
			/* Find unused entry */
			i = 0;
			while (i < index) {
				if (entries[i].oldindex == unused) {
					unused++;
					i = -1;
				}
				i++;
			}
			entries[nentries-1].oldindex = unused;
			entries[nentries-1].selected = FALSE;
			entries[nentries-1].data = NULL;
			unused++;
		}
	}
	entries[nentries-1].oldindex = -1;
	entries[nentries-1].selected = FALSE;
	entries[nentries-1].data = NULL;

	odata->entries = entries;
	odata->current = -1;
}

/*********************************************************************
  Append ch to string (allocated memory of a length len), where
  strpos is a pointer to the last position of string.
*********************************************************************/
static void sprintf_add (char **string, char **strpos, int *len, char ch)
{
	if (*strpos - *string >= *len) {
		char *oldstr = *string;
		*len += 50;
		*string = realloc (*string, *len);
		*strpos = *strpos - oldstr + *string;
	}
	**strpos = ch;
	(*strpos)++;
}

/*********************************************************************
  Return a string containing the values from opts.
*********************************************************************/
char* opts_list_sprintf (optsValue *opts)
{
	listData *data = opts->w.value;
	optsListEntry *entries = data->d->entries;
	char *string, *strpos, *pos;
	int i, len;

	len = strlen(opts->w.title)+400;
	string = malloc (len);

	sprintf (string, "\"%s\" = ", opts->w.title);
	strpos = string+strlen(string);

	for (i=0; entries[i].oldindex >= 0; i++) {
		sprintf_add (&string, &strpos, &len, '"');
		pos = data->label[entries[i].oldindex];
		while (pos && *pos) {
			if (*pos == '"' || *pos == '\\')
				sprintf_add (&string, &strpos, &len, '\\');
			sprintf_add (&string, &strpos, &len, *pos);
			pos++;
		}
		sprintf_add (&string, &strpos, &len, '"');

		sprintf_add (&string, &strpos, &len, ' ');
		sprintf_add (&string, &strpos, &len, '0' + entries[i].selected);
		sprintf_add (&string, &strpos, &len, ' ');

		sprintf_add (&string, &strpos, &len, '"');
		pos = entries[i].data;
		while (pos && *pos) {
			if (*pos == '"' || *pos == '\\')
				sprintf_add (&string, &strpos, &len, '\\');
			sprintf_add (&string, &strpos, &len, *pos);
			pos++;
		}
		sprintf_add (&string, &strpos, &len, '"');

		sprintf_add (&string, &strpos, &len, '|');
	}
	/* Mark empty lists with content, otherwiese opts_load() ignores them */
	if (i == 0)
		sprintf_add (&string, &strpos, &len, '|');
	sprintf_add (&string, &strpos, &len, '\n');
	sprintf_add (&string, &strpos, &len, '\0');

	return string;
}

/*********************************************************************
  Free all values associated with a list widget.
*********************************************************************/
static void opts_list_free (optsValue *opts)
{
	listData *data = opts->w.value;
	int i;

	opts_list_entry_free (data->d->entries);
	data->d->entries = NULL;

	for (i=0; data->label[i]; i++) {
		if (data->label[i]) g_free (data->label[i]);
		if (data->ttips[i]) g_free (data->ttips[i]);
	}
	if (data->label) g_free (data->label);
	if (data->ttips) g_free (data->ttips);
	if (data->w_menu) gtk_widget_destroy (data->w_menu);
	g_free (data);
}

/*********************************************************************
  Return column entry strings for 'entry'.
*********************************************************************/
static char** opts_list_get_column (listData *data, int entry)
{
	static char *column[3] = {"", "", ""};

	optsListEntry *entries = data->d->entries;
	int opts_select = (data->flags & OPTS_SELECT ? 1:0);

	if (opts_select) {
		column[0] = NULL;
		column[1] = data->label[entries[entry].oldindex];
	} else {
		column[0] = data->label[entries[entry].oldindex];
	}
	if (data->flags & OPTS_DATA)
		column[1 + opts_select] = entries[entry].data;

	return column;
}

/*********************************************************************
  Set selected column for row entry.
*********************************************************************/
static void opts_list_show_selected (listData *data, int entry)
{
	if (data->flags & OPTS_SELECT)
		gui_list_show_bool (data->w_clist, entry, 0,
							((optsListEntry *)data->d->entries)[entry].selected);
}

static void opts_list_fill (GtkWidget *clist, listData *data,
							gboolean clear, gboolean defaults)
{
	int i, cnt, width, select, current;
	optsListEntry *entries = data->d->entries;

	/* Reset old entries? */
	if (clear && entries) {
		for (i=0; entries[i].oldindex >= 0; i++)
			list_entry_reset (&entries[i]);
	}

	if (entries) {
		for (i=0; entries[i].oldindex >= 0; i++) /* empty */;
		data->nentries = i+1;
	}

	if (!entries || data->nentries <= 0 || clear) {
		/* No entries given
		   -> create defaults (every label one time, unselected) */

		cnt = 1;
		if (defaults)
			for (i=0; data->label[i]; i++)
				if (data->label[i][0]) cnt++;

		data->nentries = cnt;
		if (data->nentries > data->maxentries) {
			data->d->entries = g_realloc (data->d->entries,
										  sizeof(optsListEntry) * data->nentries);
			entries = data->d->entries;

			i = data->maxentries;
			data->maxentries = data->nentries;

			for (; i<data->maxentries; i++)
				list_entry_init (&entries[i]);
		}

		cnt = 0;
		for (i=0; i<data->nentries-1; i++) {
			while (!data->label[cnt][0])
				cnt++;
			entries[i].oldindex = cnt;
			cnt++;
		}
	}

	gtk_clist_freeze (GTK_CLIST(clist));
	gtk_clist_clear (GTK_CLIST(clist));

	select = -1;
	current = data->d->current;

	for (i=0; entries[i].oldindex >= 0; i++) {
		if (!(data->flags & OPTS_SELECT) && entries[i].selected)
			select = i;

		gtk_clist_append (GTK_CLIST(clist),
						  opts_list_get_column(data, i));
		opts_list_show_selected (data, i);
	}

	if (defaults && i > 0 && select < 0)
		select = 0;
	else if (clear || current >= i)
		data->d->current = -1;
	else if (select < 0)
		select = current;

	if (select >= 0) {
		GTK_CLIST(clist)->focus_row = select;
		gtk_clist_select_row (GTK_CLIST(clist), select, -1);
	}

	i = (data->flags & OPTS_SELECT) ? 1:0;
	width = gtk_clist_optimal_column_width (GTK_CLIST(clist), i);
	gtk_clist_set_column_width (GTK_CLIST(clist), i, width);

	gtk_clist_thaw (GTK_CLIST(clist));
}

/*********************************************************************
  Add entry in context menu selected (for OPTS_ADD).
  entry: Number of the entry, which should be added.
*********************************************************************/
static void opts_list_add (listData *data, int entry)
{
	int i, current;
	char *str;

	current = data->d->current;
	if (current < 0) current = data->nentries-1;

	data->nentries++;
	if (data->nentries > data->maxentries) {
		data->d->entries = g_realloc (data->d->entries,
									  sizeof(optsListEntry)*data->nentries);
		data->maxentries = data->nentries;
		list_entry_init (&data->d->entries[data->maxentries-1]);
	}
	str = data->d->entries[data->nentries-1].data;
	for (i=data->nentries-1; i>current; i--)
		data->d->entries[i] = data->d->entries[i-1];
	data->d->entries[i].oldindex = entry;
	data->d->entries[i].selected = TRUE;
	str[0] = '\0';
	data->d->entries[i].data = str;

	gtk_clist_insert (GTK_CLIST(data->w_clist), current,
					  opts_list_get_column(data, i));
	opts_list_show_selected (data, i);

	GTK_CLIST(data->w_clist)->focus_row = current;
	gtk_clist_select_row (GTK_CLIST(data->w_clist), current, -1);
}

/*********************************************************************
  Add entry in context menu selected (for OPTS_ADD).
  p_entry: Number of the selected entry.
*********************************************************************/
static void cb_list_add (GtkWidget *widget, void *p_entry)
{
	int entry = GPOINTER_TO_INT(p_entry);
	GtkWidget *menu = gtk_widget_get_ancestor(widget, GTK_TYPE_MENU);
	listData *data = gtk_object_get_user_data(GTK_OBJECT(menu));

	opts_list_add (data, entry);
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Duplicate entry in context menu selected (for OPTS_ADD).
*********************************************************************/
static void cb_list_duplicate (GtkWidget *widget, listData *data)
{
	if (data->d->current < 0) return;
	opts_list_add (data, (data->d->entries)[data->d->current].oldindex);
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

static void opts_list_delete (listData *data, int entry)
{
	optsListEntry del, *entries = data->d->entries;
	int i;

	del = entries[entry];
	for (i=entry; i<data->nentries-1; i++)
		entries[i] = entries[i+1];
	entries[i] = del;

	list_entry_reset (&entries[i]);
	data->nentries--;

	gtk_clist_remove (GTK_CLIST(data->w_clist), entry);

	if (data->d->current > entry)
		data->d->current--;
}

/*********************************************************************
  Delete entry in context menu selected (for OPTS_ADD).
*********************************************************************/
static void cb_list_delete (GtkWidget *widget, listData *data)
{
	if (data->d->current < 0) return;
	opts_list_delete (data, data->d->current);
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Remove unselected in context menu selected (for OPTS_ADD).
*********************************************************************/
static void cb_list_remunsel (GtkWidget *widget, listData *data)
{
	int i;

	gtk_clist_freeze (GTK_CLIST(data->w_clist));

	/* -2: do not delete the last -1 entry */
	for (i = data->nentries-2; i>=0; i--)
		if (!data->d->entries[i].selected)
			opts_list_delete (data, i);

	gtk_clist_thaw (GTK_CLIST(data->w_clist));
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Init list in context menu selected (for OPTS_ADD).
*********************************************************************/
static void cb_list_init (GtkWidget *widget, listData *data)
{
	opts_list_fill (data->w_clist, data, TRUE, TRUE);
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Clear list in context menu selected (for OPTS_ADD).
*********************************************************************/
static void cb_list_clear (GtkWidget *widget, listData *data)
{
	opts_list_fill (data->w_clist, data, TRUE, FALSE);
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Open the clist context menu (for OPTS_ADD).
*********************************************************************/
static gint cb_list_menu_popup (GtkCList *clist, GdkEventButton *event, GtkMenu *menu)
{
	gint row, column;

	if (!event || event->window != clist->clist_window || event->button != 3)
		return FALSE;

	if (gtk_clist_get_selection_info (clist, event->x, event->y, &row, &column)) {
		clist->focus_row = row;
		gtk_clist_select_row (clist, row, -1);
	}
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);

	gtk_signal_emit_stop_by_name (GTK_OBJECT(clist), "button_press_event");
	return TRUE;
}

/*********************************************************************
  Handle key presses inside the entry widget to control the list
  widget (OPTS_DATA).
*********************************************************************/
static gboolean cb_list_entry_key (GtkWidget *widget, GdkEventKey *event,
								   listData *data)
{
	GtkScrollType type = GTK_SCROLL_NONE;

	switch (event->keyval) {
		case GDK_Up:
			type = GTK_SCROLL_STEP_BACKWARD;
			break;
		case GDK_Down:
			type = GTK_SCROLL_STEP_FORWARD;
			break;
		case GDK_Page_Up:
			type = GTK_SCROLL_PAGE_BACKWARD;
			break;
		case GDK_Page_Down:
			type = GTK_SCROLL_PAGE_FORWARD;
			break;
		case GDK_Return:
			if (data->d->current >= 0) {
				GTK_CLIST(data->w_clist)->focus_row = data->d->current;
				gtk_clist_select_row (GTK_CLIST(data->w_clist),
									  data->d->current, 0);
			}
	}
	if (type != GTK_SCROLL_NONE) {
		gtk_signal_emit_by_name (GTK_OBJECT(data->w_clist), "scroll-vertical",
								 type, 0.0);
#if !GTK_CHECK_VERSION(2,0,0)
		gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_press_event");
#endif
		return TRUE;
	}
	return FALSE;
}

/*********************************************************************
  The text in the clist entry widget was changed (for OPTS_DATA).
  Update list widget and value.
*********************************************************************/
static void cb_list_entry (GtkWidget *widget, listData *data)
{
	const char *new_value;
	optsListEntry *entries = data->d->entries;
	int current = data->d->current;

	if (current < 0) return;

	if ((new_value = gtk_entry_get_text (GTK_ENTRY(widget)))) {
		gui_strlcpy (entries[current].data, new_value, LIST_DATA_LEN);
		gtk_clist_set_text (GTK_CLIST(data->w_clist),
							current, 1+(data->flags & OPTS_SELECT ? 1:0),
							new_value);
		opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
	}
}

/*********************************************************************
  Unselect in list widget -> Clear entry widget (for OPTS_DATA).
*********************************************************************/
static void cb_list_unsel (GtkCList *clist, gint row, gint column,
						   GdkEventButton *event, listData *data)
{
	if (data->d->current != -1) {
		data->d->current = -1;
		opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
	}

	if (data->w_label)
		gtk_label_set_text (GTK_LABEL(data->w_label), "Data");
	if (data->w_entry)
		gtk_entry_set_text (GTK_ENTRY(data->w_entry), "");
}

/*********************************************************************
  Select in list widget.
*********************************************************************/
static void cb_list_sel (GtkCList *clist, gint row, gint column,
						 GdkEventButton *event, listData *data)
{
	optsListEntry *entries = data->d->entries;
	BOOL changed = FALSE;
	int i;

	/* Update selection status */
	if (data->flags & OPTS_SELECT) {
		if (column == 0 || (event && event->type == GDK_2BUTTON_PRESS)) {
			entries[row].selected = !entries[row].selected;
			gui_list_show_bool (GTK_WIDGET(clist), row, 0, entries[row].selected);
			changed = TRUE;
		}
	} else {
		changed = !entries[row].selected;
		for (i=0; entries[i].oldindex>=0 ; i++)
			entries[i].selected = FALSE;
		entries[row].selected = TRUE;
	}

	/* Update entry widget (for OPTS_DATA). */
	if (clist->selection) {
		changed = changed || data->d->current != row;
		data->d->current = row;
		if (data->w_label && data->ttips[entries[row].oldindex])
			gtk_label_set_text (GTK_LABEL(data->w_label),
								data->ttips[entries[row].oldindex]);
		if (data->w_entry) {
			if (entries[row].data)
				gtk_entry_set_text (GTK_ENTRY(data->w_entry),
									entries[row].data);
			if (GTK_WIDGET_MAPPED(data->w_entry))
				gtk_widget_grab_focus (data->w_entry);
		}
	}
	if (changed)
		opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  Entry in list widget was moved (DnD).
*********************************************************************/
static void cb_list_move (GtkWidget *clist, gint src, gint dest, listData *data)
{
	optsListEntry *entries = data->d->entries, help;
	int i;

	if (data->d->current == src)
		data->d->current = dest;

	help = entries[src];
	if (src<dest)
		for (i=src; i<dest; i++)
			entries[i] = entries[i+1];
	else
		for (i=src; i>dest; i--)
			entries[i] = entries[i-1];
	entries[dest] = help;
	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
}

/*********************************************************************
  DnD in list widget was finished.

  This is a workaround for a GtkCList bug. The internal
  CList-data-element associated to the DnD context is not freed. If
  during a later DnD action the new context gets allocated again at
  the same address, the wrong old data element would be used. So
  free it here.
*********************************************************************/
static void cb_list_dragend (GtkWidget *clist, GdkDragContext *context, listData *data)
{
	g_dataset_remove_data (context, "gtk-clist-drag-source");
}

/*********************************************************************
  Set new values and display them in the list widget.
*********************************************************************/
static long opts_list_set (optsValue *opts, void *value)
{
	GtkWidget *clist = opts->set_widget;
	listData *data = opts->w.value;

	optsListData *newval = value;
	optsListEntry *entry = value;
	int i, newcnt;

	/* BIG HACK try to guess if an optsListData or optsListEntry
	   was passed to the function */
	if ( ((long)newval->entries > 1000 &&
		  newval->current >= -1 && newval->current < 1000) ||
		 entry->oldindex < -1 || entry->oldindex > 1000 ||
		 entry->selected < 0 || entry->selected > 1 ) {
		if (newval && data->d != newval) {
			gboolean add = (data->flags & OPTS_ADD ? TRUE:FALSE);

			for (newcnt=0; newval->entries[newcnt].oldindex >= 0; newcnt++) /* empty */;
			newcnt++;

			if (newcnt > data->maxentries) {
				if (add) {
					data->d->entries = g_realloc (data->d->entries,
												  sizeof(optsListEntry)*newcnt);
					i = data->maxentries;
					data->maxentries = newcnt;
					for (; i<data->maxentries; i++)
						list_entry_init (&data->d->entries[i]);
				} else
					newcnt = data->maxentries;
			}

			/* Copy new entries */
			for (i=0; i < newcnt; i++) {
				data->d->entries[i].oldindex = newval->entries[i].oldindex;
				data->d->entries[i].selected = newval->entries[i].selected;
				if (newval->entries[i].data)
					gui_strlcpy (data->d->entries[i].data, newval->entries[i].data,
								 LIST_DATA_LEN);
				else
					data->d->entries[i].data[0] = '\0';
			}
		}
		opts_list_fill (clist, data, FALSE, TRUE);
	} else if (data->d->current >= 0) {
		int current = data->d->current;
		char **column;

		if (entry->data)
			gui_strlcpy (data->d->entries[current].data, entry->data, LIST_DATA_LEN);
		else
			data->d->entries[current].data[0] = '\0';
		data->d->entries[current].selected = entry->selected;
		data->d->entries[current].oldindex = entry->oldindex;

		column = opts_list_get_column (data, current);
		opts_list_show_selected (data, current);
		if ((data->flags & OPTS_DATA) || (data->flags & OPTS_SELECT))
			gtk_clist_set_text (GTK_CLIST(data->w_clist), current, 1,
								column[1]);
		if ((data->flags & OPTS_DATA) && (data->flags & OPTS_SELECT))
			gtk_clist_set_text (GTK_CLIST(data->w_clist), current, 2,
								column[2]);

		GTK_CLIST(clist)->focus_row = current;
		gtk_clist_select_row (GTK_CLIST(clist), current, -1);
	} else
		return OPTS_SET_ERROR;

	opts_signal (data->opts, OPTS_SIG_CHANGED, NULL);
	return OPTS_SET_ERROR;
}

typedef struct addMenuData {
	int lnum;			/* Current parsing position: label[lnum][lpos] */
	int lpos;
	gboolean newsub;	/* Current entry is a sub menu */
} addMenuData;
/*********************************************************************
  Create menu hierarchy for labels.
  Allowed meta characters at start of label:
    '>': label is a sub menu
    '<': Back one hierarchy
*********************************************************************/
static GtkWidget* list_create_add_menu_do (char **label, addMenuData *d, listData *data)
{
	GtkWidget *menu_item, *sub, *new_sub;

	sub = gtk_menu_new();
	gtk_menu_append (GTK_MENU(sub), gtk_tearoff_menu_item_new());
	gtk_object_set_user_data (GTK_OBJECT(sub), data);

	while (label[d->lnum]) {
		while (label[d->lnum][d->lpos]) {
			if (label[d->lnum][d->lpos] == '>') {
				d->newsub = TRUE;
			} else if (label[d->lnum][d->lpos] == '<') {
				d->lpos++;	/* Overread '<' */
				return sub;
			} else if (!isspace((int)label[d->lnum][d->lpos])) {
				/* Start of real label text */
				char *l = &label[d->lnum][d->lpos];
				char name[101];
				int npos = 0;

				name[0] = '\0';
				while (*l && *l != '|' && npos < 100) {
					if (*l == '\\' && *(l+1) == '|')
						l++;
					name[npos++] = *l++;
				}
				name[npos] = '\0';

				menu_item = gtk_menu_item_new_with_label (name);
				gtk_menu_append (GTK_MENU(sub), menu_item);
				d->lnum++;

				if (d->newsub) {
					d->newsub = FALSE;
					d->lpos = 0;
					new_sub = list_create_add_menu_do (label, d, data);
					gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu_item), new_sub);
				} else {
					gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
										GTK_SIGNAL_FUNC(cb_list_add), GINT_TO_POINTER(d->lnum - 1));
					d->lpos = 0;
				}
				break;
			}
			d->lpos++;
		}
	}

	return sub;
}
static GtkWidget* list_create_add_menu (char **label, listData *data)
{
	addMenuData d = {0, 0, FALSE};
	return list_create_add_menu_do (label, &d, data);
}

/*********************************************************************
  Create new list widget with selectable values from label. The const
  array label must be terminated with NULL. Allowed meta characters
  at start of label (for 'Add entry' menu, only usefull if flags
  contain OPTS_ADD):
      '>': label is a sub menu
      '<': Back one hierarchy
  In label (only usefull if flags contain OPTS_DATA):
      '|': Separator between label and string help.
  e.g. {"> Sub1", "Entry1|Help1", "<> Sub2", "Entry2|Help2", NULL}
       -> Two submenues with two entries, these two can be added to
          the list widget.
  value->entries can be NULL (-> list is filled with default values).
  If not, oldindex of last entry must be -1.
*********************************************************************/
void opts_list_create (long page, const char *title, const char *ttip,
					   char **label, optsListFlags flags, optsListData *value)
{
	const char *column[3] = {"", "", ""};
	GtkWidget *clist, *scroll, *event, *box;
	int index, cnt, i, read;
	char *pos, *name;
	listData *data;
	optsPage *p;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);

	data = g_malloc0 (sizeof(listData));
	data->flags = flags;
	data->d = value;

	for (i=0; label[i]; i++) /* empty */;
	data->label = g_malloc (sizeof(char*)*(i+1));
	data->ttips = g_malloc (sizeof(char*)*(i+1));

	for (i=0; label[i]; i++) {
		pos = label[i];

		/* Overread any meta character */
		while (*pos == '<' || isspace((int)*pos))
			pos++;

		data->ttips[i] = NULL;
		if (*pos == '>') {
			data->label[i] = g_strdup("");
		} else {
			cnt = gui_sscanf (pos, "%a|%n", &name, &read);
			if (cnt > 1)
				data->ttips[i] = g_strdup (pos+read);
			if (cnt > 0)
				data->label[i] = name;
		}
	}
	data->label[i] = NULL;
	data->ttips[i] = NULL;
	data->opts = opts_value_add1__nl (p, WID_LIST, title,
									  opts_list_set, data, NULL);

	box = gtk_vbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (p->widget), box, TRUE, TRUE, 0);

	/* Event box to get tool tips working */
	event = gtk_event_box_new();
	gtk_box_pack_start (GTK_BOX (box), event, TRUE, TRUE, 0);
	gui_tooltip_set (event, ttip);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
									GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (event), scroll);

	if (flags & OPTS_SELECT) {
		column[1] =  title;
		cnt = 2;
	} else {
		column[0] =  title;
		cnt = 1;
	}
	if (flags & OPTS_DATA) {
		column[cnt] = "Data";
		cnt++;
	}
	data->w_clist = clist = gtk_clist_new_with_titles (cnt, (char**)column);
	gtk_clist_column_titles_passive (GTK_CLIST(clist));
	gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);
	if (flags & OPTS_SELECT) {
		gtk_clist_set_column_justification (GTK_CLIST(clist), 0, GTK_JUSTIFY_CENTER);
		gtk_clist_set_column_width (GTK_CLIST(clist), 0, 20);
	}
	if (flags & OPTS_REORDER)
		gtk_clist_set_reorderable (GTK_CLIST(clist), TRUE);
	gtk_container_add (GTK_CONTAINER (scroll), clist);

	if (flags & OPTS_DATA) {
		data->w_label = gtk_label_new ("Data");
		gtk_misc_set_alignment(GTK_MISC(data->w_label), 0.0, 0.5);
		gtk_label_set_justify (GTK_LABEL(data->w_label), GTK_JUSTIFY_LEFT);
		gtk_box_pack_start (GTK_BOX (box), data->w_label, FALSE, FALSE, 0);

		data->w_entry = gtk_entry_new_with_max_length (LIST_DATA_LEN-1);
		gtk_signal_connect (GTK_OBJECT(data->w_entry), "changed",
							GTK_SIGNAL_FUNC(cb_list_entry), data);
		gtk_signal_connect (GTK_OBJECT(data->w_entry), "key_press_event",
							GTK_SIGNAL_FUNC(cb_list_entry_key), data);
		gtk_box_pack_start (GTK_BOX (box), data->w_entry, FALSE, FALSE, 0);
	}
	if (flags & OPTS_ADD) {
		GtkWidget *menu, *menu_item, *sub;

		menu = gtk_menu_new();
		gtk_menu_append (GTK_MENU(menu), gtk_tearoff_menu_item_new());
		data->w_menu = menu;

		menu_item = gtk_menu_item_new_with_label ("Add entry");
		gtk_menu_append (GTK_MENU(menu), menu_item);

		sub = list_create_add_menu (label, data);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu_item), sub);

		menu_item = gtk_menu_item_new_with_label ("Duplicate entry");
		gtk_menu_append (GTK_MENU(menu), menu_item);
		gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
							GTK_SIGNAL_FUNC(cb_list_duplicate), data);

		menu_item = gtk_menu_item_new_with_label ("Delete entry");
		gtk_menu_append (GTK_MENU(menu), menu_item);
		gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
							GTK_SIGNAL_FUNC(cb_list_delete), data);

		menu_item = gtk_menu_item_new();
		gtk_menu_append (GTK_MENU(menu), menu_item);

		menu_item = gtk_menu_item_new_with_label ("Init list");
		gtk_menu_append (GTK_MENU(menu), menu_item);
		gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
							GTK_SIGNAL_FUNC(cb_list_init), data);

		menu_item = gtk_menu_item_new_with_label ("Clear list");
		gtk_menu_append (GTK_MENU(menu), menu_item);
		gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
							GTK_SIGNAL_FUNC(cb_list_clear), data);

		menu_item = gtk_menu_item_new_with_label ("Remove unselected");
		gtk_menu_append (GTK_MENU(menu), menu_item);
		gtk_signal_connect (GTK_OBJECT(menu_item), "activate",
							GTK_SIGNAL_FUNC(cb_list_remunsel), data);

		gtk_widget_show_all (menu);

		gtk_signal_connect (GTK_OBJECT(clist), "button_press_event",
							GTK_SIGNAL_FUNC (cb_list_menu_popup),
							menu);
	}

	gtk_signal_connect (GTK_OBJECT(clist), "select_row",
						GTK_SIGNAL_FUNC(cb_list_sel), data);
	gtk_signal_connect (GTK_OBJECT(clist), "unselect_row",
						GTK_SIGNAL_FUNC(cb_list_unsel), data);
	gtk_signal_connect (GTK_OBJECT(clist), "row_move",
						GTK_SIGNAL_FUNC(cb_list_move), data);
	gtk_signal_connect (GTK_OBJECT(clist), "drag_end",
						GTK_SIGNAL_FUNC(cb_list_dragend), data);

	opts_list_fill (clist, data, FALSE, TRUE);
	gtk_widget_show_all (box);

	opts_value_add2__nl (data->opts, p, index, box, clist);
	data->opts->free_value = opts_list_free;

	gui_threads_leave();
}
