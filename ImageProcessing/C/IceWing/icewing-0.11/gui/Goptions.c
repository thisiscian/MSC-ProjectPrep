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

#include "tools/tools.h"
#include "Gtools.h"
#include "Gdialog.h"
#include "Gpreferences.h"
#include "Gpreview.h"
#include "Gpreview_i.h"
#include "Ggui_i.h"
#include "Gsession.h"
#include "Glist.h"
#include "Goptions_i.h"
#include "images/key_enter.xpm"

#define MENU_ID			"MENURC"
#define ENTRY_WIDTH		30
#define SCALE_WIDTH		100

#define MAX_PAGES		1000

typedef struct {
	optsSignal sigset;			/* Signals this cback should be called for */
	optsSignalFunc cback;		/* The function which should be called */
	void *data;					/* Data passed to the callback */
} optsCBack;

/* Values, which were loaded from a file,
   but could up to now not be assigned to a widget */
typedef struct optsDefValue {
	char *title;
	char *value;
} optsDefValue;
static GHashTable *opts_def_value = NULL;
static GHashTable *opts_def_value_rem = NULL;

static GHashTable *opts_save_rem = NULL;

static GPtrArray *opts_pages = NULL;
static GHashTable *opts_pages_hash = NULL;
static optsValue *opts_values = NULL;

static GtkWidget *opts_parent = NULL;

/*********************************************************************
  Remove the page/buffer "title" from the opts_pages array.
*********************************************************************/
void opts_page_remove__nl (char *title)
{
	optsPage *p = g_hash_table_lookup (opts_pages_hash, title);
	if (p) {
		g_hash_table_remove (opts_pages_hash, title);
		if (p->widget)
			g_ptr_array_remove (opts_pages, p);
		g_free (p->title);
		g_free (p);
	}
}

/*********************************************************************
  Add a page(== widget) / buffer to the opts_pages array.
*********************************************************************/
int opts_page_add__nl (GtkWidget *widget, prevBuffer *buffer, const char *title)
{
	optsPage *p = g_malloc (sizeof(optsPage));

	p->buffer = buffer;
	p->widget = widget;
	if (widget)
		p->page = opts_pages->len;
	else
		p->page = -1;
	p->title = g_strdup(title);

	if (!opts_pages_hash)
		opts_pages_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (opts_pages_hash, p->title, p);

	if (widget)
		g_ptr_array_add (opts_pages, p);

	return p->page;
}

/*********************************************************************
  Return a pointer to the page belonging to 'page'.
*********************************************************************/
optsPage* opts_page_get__nl (long page)
{
	if (page >= opts_pages->len)
		iw_error ("Page number %ld not available, only 0 - %d",
				  page, opts_pages->len-1);
	return g_ptr_array_index (opts_pages, page);
}

/*********************************************************************
  Search for a widget with name title. If 'page' != -1 search only for
  widgets on page 'page'. If fqname, assume that title contains
  'page.widget'.
*********************************************************************/
static optsValue *title_search (long page, const char *title, gboolean fqname)
{
	optsValue *pos = NULL;
	char *fqtitle = NULL;

	if (page != -1) {
		optsPage *p;
		if (page >= 0 && page < MAX_PAGES) {
			if (page >= opts_pages->len)
				iw_error ("Page number %ld not available, only 0 - %d",
						  page, opts_pages->len-1);
			p = g_ptr_array_index (opts_pages, page);
		} else {
			p = g_hash_table_lookup (opts_pages_hash, ((prevBuffer*)page)->title);
		}
		fqtitle = g_malloc (strlen(p->title) + strlen(title) + 2);
		strcpy (fqtitle, p->title);
		strcat (fqtitle, ".");
		strcat (fqtitle, title);
		title = fqtitle;
	}

	/* First search for full qualified name */
	for (pos = opts_values; pos; pos = pos->next)
		if (!strcmp (title, pos->w.title))
			break;

	/* Not found ->
	   First try pattern matching, than match only the widget name part */
	if (!pos && !fqname) {
		if (strchr (title, '*') || strchr (title, '?'))
			for (pos = opts_values; pos; pos = pos->next)
				if (gui_pattern_match (title, pos->w.title))
					return pos;
		for (pos = opts_values; pos; pos = pos->next)
			if (!strcmp (title, pos->w.wtitle))
				return pos;
	}

	if (fqtitle) g_free (fqtitle);
	return pos;
}

/*********************************************************************
  Return position of v->top_widget in p->widget or p->buffer->menu.
*********************************************************************/
static int opts_get_box_pos (optsPage *p, optsValue *v)
{
	GtkWidget *search = v->top_widget;
	GList *list;
	int pos = 0;

	if (search && search->parent) {
		if (p->widget) {
			GtkBoxChild *child;
			GtkBox *box = GTK_BOX(p->widget);

			for (list = box->children; list; list = list->next) {
				child = list->data;
				if (search == child->widget)
					return pos;
				pos++;
			}
		} else {
			if (GTK_IS_MENU (search->parent)) {
				GtkMenuShell *menu = GTK_MENU_SHELL(search->parent);
				GtkWidget *menu_item;

				for (list = menu->children; list; list = list->next) {
					menu_item = GTK_WIDGET (list->data);

					if (search == menu_item)
						return pos;
					pos++;
				}
			}
		}
	}
	iw_error ("Widget not found in box/menu");
	return -1;
}

/*********************************************************************
  Check if widget title on page page (if -1 title must be full
  qualified) already exists, remove the widget, and return
    a) the widget name without the page part in title
    b) a pointer to the page
    c) the index of the widget in the page, or -1
*********************************************************************/
void opts_create_prepare (long page, const char **title, optsPage **opage, int *index)
{
	const char *wtitle;
	char *page_title, *fqname = NULL;
	optsValue *vals;
	optsPage *p = NULL;

	if (page == -1) {
		char *pos;

		/* If '.' would not appear in the widget title: strrchr (fqname, '.')
		   Current workaround: Search for longest page title */
		fqname = strdup (*title);
		for (pos=fqname+strlen(fqname)-1; pos != fqname; pos--) {
			if (*pos == '.') {
				*pos = '\0';
				p = g_hash_table_lookup (opts_pages_hash, fqname);
				*pos = '.';
				if (p) {
					*title = *title + (int)(pos+1-fqname);
					break;
				}
			}
		}
		if (!p)
			iw_error ("No valid page number or page/buffer name given");
	} else {
		int len;

		if (page >= 0 && page < MAX_PAGES) {
			if (page >= opts_pages->len)
				iw_error ("Page number %ld not available, only 0 - %d",
						  page, opts_pages->len-1);
			p = g_ptr_array_index (opts_pages, page);
			page_title = p->title;
		} else {
			page_title = ((prevBuffer*)page)->title;
			p = g_hash_table_lookup (opts_pages_hash, page_title);
		}

		len = strlen (page_title);
		if (!strncmp (page_title, *title, len) && (*title)[len] == '.')
			wtitle = *title = *title + len + 1;
		else
			wtitle = *title;
		if (!wtitle) wtitle = "";

		fqname = g_malloc (len + strlen(wtitle) + 2);
		strcpy (fqname, page_title);
		strcat (fqname, ".");
		strcat (fqname, wtitle);
	}
	if ((vals = title_search(-1, fqname, TRUE))) {
		*index = opts_get_box_pos (p, vals);
		opts_widget_remove__nl (fqname);
	} else
		*index = -1;
	free (fqname);

	*opage = p;
}

/*********************************************************************
  Concatenate strings for a path to a menu item and extract the path,
  the accelerator components, and the stock icon according to
  'path <accel> <"stock" icon>'.
*********************************************************************/
static void opts_get_path (GtkItemFactoryEntry *item, const char *str1, ...)
{
	va_list args;
	char *s, *end;
	int l = 1, deep;

	/* Get the length of the path */
	va_start (args, str1);
	s = (char*)str1;
	while (s) {
		l += strlen (s) + 1;
		s = va_arg (args, gchar*);
	}
	va_end (args);

	/* Concatenate the path */
	item->path = g_malloc0 (l);
	if (str1[0] != '/')
		strcpy (item->path, "/");
	strcat (item->path, str1);

	va_start (args, str1);
	s = va_arg (args, gchar*);
	while (s) {
		l = strlen(item->path);
		if (s[0] != '/' && item->path[l-1] != '/')
			strcat (item->path, "/");
		strcat (item->path, s);
		s = va_arg (args, gchar*);
	}
	va_end (args);

	/* Check for accelerator and/or stock icon at the end of the path */
	item->accelerator = NULL;
	s = &item->path[strlen(item->path)-1];

	while (*s == '>' && s >= item->path) {
		end = s;
		deep = 0;
		for (; s >= item->path; s--) {
			if (*s == '>') {
				deep++;
			} else if (*s == '<') {
				deep--;
				if (deep == 0) {
					*end = '\0';
					if (!strncasecmp(s+1, "stock ", 6) &&
						!strncasecmp(item->item_type, "<item>", 6)) {
#if GTK_CHECK_VERSION(2,0,0)
						end = s+6;
						while (*end == ' ')
							end++;
						item->extra_data = end;
						item->item_type = "<StockItem>";
#endif
					} else {
						item->accelerator = s+1;
					}
					while (s > item->path && *(s-1) == ' ')
						s--;
					*s = '\0';
					s--;
					break;
				}
			}
		}
	}
}

/*********************************************************************
  Remove any '_' from a path to a menu item.
*********************************************************************/
static char *opts_clean_path (char *path)
{
	char *d = path, *s = path;

	for (; *s; s++) {
		if (*s == '_') {
			if (s[1] == '_')
				*d++ = *s++;
		} else
			*d++ = *s;
	}
	*d = '\0';
	return path;
}

/*********************************************************************
  Return name of the default rcFile. Returned value is a pointer to a
  static variable.
*********************************************************************/
char *opts_get_default_file (void)
{
	static char file[PATH_MAX] = "";
	if (!file[0])
		gui_convert_prg_rc (file, "values", TRUE);
	return file;
}

/*********************************************************************
  Apply the value 'value' to the widget from opts.
*********************************************************************/
static void opts_value_set_char (optsValue *opts, char *value)
{
	int i_value;
	long l_value;
	float f_value;
	double d_value;
	optsListData list_value;

	switch (opts->wtype) {
		case WID_STRING:
		case WID_FILESEL:
		case WID_VAR_STRING:
			opts->set_value (opts, (void*)value);
			break;
		case WID_ENTSCALE:
		case WID_TOGGLE:
		case WID_OPTION:
		case WID_RADIO:
		case WID_BUTTON:
		case WID_VAR_BOOL:
		case WID_VAR_INT:
			i_value = -1;
			sscanf (value, "%d", &i_value);
			opts->set_value (opts, (void*)(long)i_value);
			break;
		case WID_VAR_LONG:
			l_value = -1;
			sscanf (value, "%ld", &l_value);
			opts->set_value (opts, &l_value);
			break;
		case WID_LIST:
			opts_list_sscanf (opts, value, &list_value);
			opts->set_value (opts, (void*)&list_value);
			opts_list_entry_free (list_value.entries);
			break;
		case WID_FLOAT:
		case WID_VAR_FLOAT:
			f_value = -1;
			sscanf (value, "%f", &f_value);
			opts->set_value (opts, &f_value);
			break;
		case WID_VAR_DOUBLE:
			d_value = -1;
			sscanf (value, "%lf", &d_value);
			opts->set_value (opts, &d_value);
			break;
		default: break;
	}
}

/*********************************************************************
  Append a new loaded default value to the default value list.
*********************************************************************/
static void opts_defvalue_append (char *title, char *value)
{
	optsDefValue *new;

	if (!opts_def_value)
		opts_def_value = g_hash_table_new (g_str_hash, g_str_equal);

	new = g_hash_table_lookup (opts_def_value, title);
	if (!new) {
		new = g_malloc (sizeof(optsDefValue));
		new->title = g_strdup (title);
		g_hash_table_insert (opts_def_value, new->title, new);
	} else {
		if (new->value) g_free (new->value);
	}
	new->value = g_strdup (value);
}

/*********************************************************************
  Search for val->w.title in the opts_def_value-list and apply the
  found value.
*********************************************************************/
static void opts_defvalue_apply (optsValue *val)
{
	optsDefValue *pos;
	char *title;
	gboolean set;

	if (!opts_def_value) return;

	/* First search for full qualified name */
	title = val->w.title;
	pos = g_hash_table_lookup (opts_def_value, title);
	if (!pos) {
		title = val->w.wtitle;
		pos = g_hash_table_lookup (opts_def_value, title);
	}
	if (pos) {
		g_hash_table_remove (opts_def_value, title);

		/* Check if value was removed from the def_value-list
		   by a call to opts_defvalue_remove() */
		set = TRUE;
		if (opts_def_value_rem) {
			if (g_hash_table_lookup (opts_def_value_rem, val->w.title) ||
				g_hash_table_lookup (opts_def_value_rem, val->w.wtitle))
				set = FALSE;
		}
		if (set)
			opts_value_set_char (val, pos->value);

		if (pos->title) g_free (pos->title);
		if (pos->value) g_free (pos->value);
		g_free (pos);
	}
}

/*********************************************************************
  Prevent any values from default config files to be set for the
  widget referenced by title.
*********************************************************************/
void opts_defvalue_remove (const char *title)
{
	char *new;

	if (!opts_def_value_rem)
		opts_def_value_rem = g_hash_table_new (g_str_hash, g_str_equal);

	new = g_hash_table_lookup (opts_def_value_rem, title);
	if (!new) {
		new = g_strdup (title);
		g_hash_table_insert (opts_def_value_rem, new, new);
	}
}

optsValue* opts_value_add1__nl (optsPage *p, optsWidType wtype, const char *wtitle,
								setValueFkt setFkt, void *value, void *data)
{
	optsValue *new = g_malloc (sizeof(optsValue));
	int wtitle_start;

	if (wtype < WID_VAR_START) {
		wtitle_start = strlen(p->title)+1;
		new->w.title = g_malloc (wtitle_start + strlen(wtitle) + 1);
		strcpy (new->w.title, p->title);
		strcat (new->w.title, ".");
		strcat (new->w.title, wtitle);
	} else {
		wtitle_start = 9;		/* == strlen("variable.") */
		new->w.title = g_malloc (wtitle_start + strlen(wtitle) + 1);
		strcpy (new->w.title, "variable.");
		strcat (new->w.title, wtitle);
	}
	new->w.wtitle = new->w.title + wtitle_start;
	new->wtype = wtype;
	new->cback = NULL;
	new->w.value = value;

	new->top_widget = NULL;
	new->set_widget = NULL;
	new->set_value = setFkt;
	new->free_value = NULL;
	new->data = data;
	new->next = opts_values;
	opts_values = new;

	return new;
}

void opts_value_add2__nl (optsValue *opts, optsPage *p, int index,
						  GtkWidget *top_widget, GtkWidget *set_widget)
{
	if (opts->wtype < WID_VAR_START) {
		if (index >= 0) {
			if (p->widget)
				gtk_box_reorder_child (GTK_BOX(p->widget), top_widget, index);
			else
				gtk_menu_reorder_child (GTK_MENU(top_widget->parent),
										top_widget, index);
		}
	}
	opts->top_widget = top_widget;
	opts->set_widget = set_widget;

	/* Search for loaded but not applied default value */
	if (opts->set_value) opts_defvalue_apply (opts);
}

/*********************************************************************
  Prepare all registered signal handlers for removal.
*********************************************************************/
static void opts_signal_remove (optsValue *opts)
{
	GSList *cback = opts->cback;
	if (!cback) return;

	for (; cback; cback = cback->next) {
		if (cback->data)
			g_free (cback->data);
	}
	g_slist_free (opts->cback);
	opts->cback = NULL;
}

/*********************************************************************
  Call cback with data as last argument if one of the signals
  specified with sigset occured for the widget referenced by title.
  page: - Return value of opts_page_append() or
        - Return value of prev_get_page() or
        - Partly a pointer to a prevBuffer or
        - -1  -> title must have the form 'pageTitle.widgetTitle'
  title: Name of a widget or 'pageTitle.widgetTitle'
  Return TRUE on success.
*********************************************************************/
gboolean opts_signal_connect (long page, const char *title, optsSignal sigset,
							  optsSignalFunc cback, void *data)
{
	gboolean ret = FALSE;
	optsValue *opts;

	gui_threads_enter();
	if ((opts = title_search(page, title, FALSE)) && opts->set_value) {
		optsCBack *cb = g_malloc (sizeof(optsCBack));

		cb->sigset = sigset;
		cb->cback = cback;
		cb->data = data;
		opts->cback = g_slist_append (opts->cback, cb);
		ret = TRUE;
	}
	gui_threads_leave();

	return ret;
}

/*********************************************************************
  Call all widget cbacks which have registered for signal.
*********************************************************************/
void opts_signal (optsValue *opts, optsSignal signal, void *newValue)
{
	GSList *cback;
	optsCBack *cb;

	for (cback = opts->cback; cback; cback = cback->next) {
		cb = cback->data;
		if (cb->cback && (signal & cb->sigset))
			cb->cback (&opts->w, newValue, cb->data);
	}
}

/*********************************************************************
  Remove widget with title 'title' from options tab.
  Use 'pageTitle.widgetTitle' for the title argument.
  Return: Did the widget exist?
*********************************************************************/
gboolean opts_widget_remove__nl (const char *title)
{
	optsValue *pos, *old = NULL;

	if ((pos = title_search(-1, title, FALSE))) {
		opts_signal (pos, OPTS_SIG_REMOVE, NULL);
		opts_signal_remove (pos);
		if (pos == opts_values) {
			opts_values = pos->next;
		} else {
			for (old = opts_values; old->next!=pos; old = old->next) /* empty */;
			old->next = pos->next;
		}
		if (pos->free_value)
			pos->free_value (pos);
		if (pos->top_widget)
			gtk_widget_destroy (pos->top_widget);
		if (pos->w.title)
			g_free (pos->w.title);
		g_free (pos);

		return TRUE;
	}
	return FALSE;
}
gboolean opts_widget_remove (const char *title)
{
	gboolean ret;

	gui_threads_enter();
	ret = opts_widget_remove__nl (title);
	gui_threads_leave();

	return ret;
}

/*********************************************************************
  Return a pointer to the current value from the widget referenced by
  title.
  title: 'pageTitle.widgetTitle'
*********************************************************************/
void *opts_value_get (const char *title)
{
	void *ret = NULL;
	optsValue *pos;

	gui_threads_enter();
	if ((pos = title_search(-1, title, FALSE)) && pos->set_value)
		ret = pos->w.value;
	gui_threads_leave();

	return ret;
}

/*********************************************************************
  Set new value and display it in widget referenced by title.
  title: 'pageTitle.widgetTitle'
  value: strings, floats, list: a pointer to the value
         otherwise            : the value itself
  Return: Old value on sucess, OPTS_SET_ERROR otherwise (no widget
          found or value does not fit into one long(e.g. a string)).
*********************************************************************/
long opts_value_set (const char *title, void *value)
{
	long ret = OPTS_SET_ERROR;
	optsValue *pos;

	gui_threads_enter();
	if ((pos = title_search(-1, title, FALSE)) && pos->set_value)
		ret = pos->set_value (pos, value);
	gui_threads_leave();

	return ret;
}

/*********************************************************************
  Check if *value is inside of bounds and clamp if necessary.
*********************************************************************/
static void value_clamp_int (char *fkt, gint *value, gint left, gint right)
{
	iw_assert (left <= right, "left value (%d) > right value (%d)", left, right);

	if (*value >= left && *value <= right)
		return;
	iw_warning ("Value %d in %s() out of bounds [%d..%d], clamping...",
				*value, fkt, left, right);
	if (*value < left)
		*value = left;
	else
		*value = right;
}
static void value_clamp_float (char *fkt, gfloat *value, gfloat left, gfloat right)
{
	iw_assert (left <= right, "left value (%f) > right value (%f)", left, right);

	if (*value >= left && *value <= right)
		return;
	iw_warning ("Value %f in %s() out of bounds [%f..%f], clamping...",
				*value, fkt, left, right);
	if (*value < left)
		*value = left;
	else
		*value = right;
}

typedef struct optsVarData {
	optsSetFunc setval;
	void *data;
	int length;
} optsVarData;

static void opts_variable_free (optsValue *opts)
{
	if (opts && opts->data)
		free (opts->data);
}

static long opts_variable_set (optsValue *opts, void *value)
{
	optsVarData *vdata = opts->data;
	long old = OPTS_SET_ERROR;
	gboolean cont = TRUE;

	switch (opts->wtype) {
		case WID_VAR_INT:
		case WID_VAR_BOOL:
			old = (long)(*(int*)(opts->w.value));
			break;
		case WID_VAR_LONG:
			old = *(long*)(opts->w.value);
			break;
		case WID_VAR_FLOAT:
			old = (long)(*(float*)(opts->w.value));
			break;
		case WID_VAR_DOUBLE:
			old = (long)(*(double*)(opts->w.value));
			break;
		default: break;
	}
	if (vdata->setval)
		cont = vdata->setval (opts->w.value, value, vdata->data);
	if (cont) {
		switch (opts->wtype) {
			case WID_VAR_BOOL: {
				int val = GPOINTER_TO_INT(value);
				if (val==FALSE || val==TRUE) {
					opts_signal (opts, OPTS_SIG_CHANGED, &val);
					*(int*)opts->w.value = val;
				} else {
					iw_warning ("Value %d for '%s' out of bounds, ignoring...",
								val, opts->w.title);
					old = OPTS_SET_ERROR;
				}
				break;
			}
			case WID_VAR_INT:
				opts_signal (opts, OPTS_SIG_CHANGED, &value);
				*(int*)opts->w.value = GPOINTER_TO_INT(value);
				break;
			case WID_VAR_LONG:
				opts_signal (opts, OPTS_SIG_CHANGED, value);
				*(long*)opts->w.value = *(long*)value;
				break;
			case WID_VAR_FLOAT:
				opts_signal (opts, OPTS_SIG_CHANGED, value);
				*(float*)opts->w.value = *(float*)value;
				break;
			case WID_VAR_DOUBLE:
				opts_signal (opts, OPTS_SIG_CHANGED, value);
				*(double*)opts->w.value = *(double*)value;
				break;
			case WID_VAR_STRING: {
				char *new_str = value;
				opts_signal (opts, OPTS_SIG_CHANGED, value);
				gui_strcpy_back (opts->w.value, new_str, vdata->length);
				break;
			}
			default: break;
		}
	}
	return old;
}

/*********************************************************************
  Add a non graphical value, which gets loaded/saved in the same way
  as the graphical values. If the value should be set to a new value,
  setval() is called with the old value, the new value and data as
  arguments. If setval() returns TRUE or if setval()==NULL the value
  is set automatically in the background.
*********************************************************************/
static void opts_variable_add_full (const char *title, optsSetFunc setval, void *data,
									optsType type, void *value, int length)
{
	optsValue *opts;
	optsVarData *vdata = g_malloc (sizeof(optsVarData));
	vdata->setval = setval;
	vdata->data = data;
	vdata->length = length;

	if (type == OPTS_BOOL)
		value_clamp_int ("opts_variable_add", value, FALSE, TRUE);
	opts = opts_value_add1__nl (NULL, type+WID_VAR_START, title,
								opts_variable_set, value, vdata);
	opts_value_add2__nl (opts, NULL, -1,  NULL, NULL);
	opts->free_value = opts_variable_free;
}
void opts_variable_add (const char *title, optsSetFunc setval, void *data,
						optsType type, void *value)
{
	opts_variable_add_full (title, setval, data, type, value, -1);
}
void opts_varstring_add (const char *title, optsSetFunc setval, void *data,
						 void *value, int length)
{
	opts_variable_add_full (title, setval, data, OPTS_STRING, value, length);
}

/*********************************************************************
  Create a new separator with a label.
  page can be a pointer to a prevBuffer (-> new menu separator).
*********************************************************************/
void opts_separator_create (long page, const char *title)
{
	optsValue *opts;
	GtkWidget *hbox, *wid;
	optsPage *p;
	int index;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);
	if (p->widget) {
		hbox = gtk_hbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (p->widget), hbox, FALSE, FALSE, 0);

		if (title) {
#if GTK_CHECK_VERSION(2,0,0)
			char *buf = g_markup_printf_escaped ("<b>%s</b>", title);
			wid = gtk_label_new (NULL);
			gtk_label_set_markup (GTK_LABEL(wid), buf);
			free (buf);
#else
			wid = gtk_label_new (title);
#endif
			gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);
		}

		wid = gtk_hseparator_new();
		gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);
	} else {
		GtkItemFactoryEntry item = {NULL, NULL, NULL, 0, "<Separator>"};
		GtkItemFactory *factory = p->buffer->factory;
		opts_get_path (&item, title, NULL);
		gtk_item_factory_create_item (factory, &item, NULL, 2);
		hbox = gtk_item_factory_get_widget (factory,
											opts_clean_path(item.path));
		g_free (item.path);
	}

	gtk_widget_show_all (hbox);
	opts = opts_value_add1__nl (p, WID_SEPARATOR, title, NULL, NULL, NULL);
	opts_value_add2__nl (opts, p, index, hbox, NULL);
	gui_threads_leave();
}

typedef struct optsButtonData {
	optsCbackFunc cback;
	void *data;
	int cnt;
	GtkWidget **buttons;
} optsButtonData;

/*********************************************************************
  Free all values associated with a button-cb widget.
*********************************************************************/
static void opts_buttoncb_free (optsValue *opts)
{
	if (opts->data) {
		optsButtonData *bdata = opts->data;
		if (bdata->buttons)
			free (bdata->buttons);
		free (bdata);
	}
}

static void cb_buttoncb (GtkWidget *widget, gpointer data)
{
	optsValue *opts = gtk_object_get_user_data(GTK_OBJECT(widget));
	optsButtonData *bdata = opts->data;
	opts_signal (opts, OPTS_SIG_CHANGED, &data);
	if (bdata->cback)
		bdata->cback (widget, (long)data, bdata->data);
}

/*********************************************************************
  Set new value -> emulate button click.
*********************************************************************/
static long opts_buttoncb_set (optsValue *opts, void *value)
{
	int b_val = GPOINTER_TO_INT(value);
	optsButtonData *bdata = opts->data;

	if (b_val >= 0 && b_val <= bdata->cnt) {
		if (b_val > 0)
			cb_buttoncb (bdata->buttons[b_val-1], value);
	} else
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					b_val, opts->w.title);

	return OPTS_SET_ERROR;
}

static void cb_button (GtkWidget *widget, gpointer data)
{
	optsValue *opts = gtk_object_get_user_data(GTK_OBJECT(widget));
	opts_signal (opts, OPTS_SIG_CHANGED, &data);
	if (opts->w.value)
		*(int*)opts->w.value = (long)data;
}

/*********************************************************************
  Set new value -> emulate button click.
*********************************************************************/
static long opts_button_set (optsValue *opts, void *value)
{
	long old = OPTS_SET_ERROR;
	int b_val = GPOINTER_TO_INT(value);

	if (b_val >= 0 && b_val <= GPOINTER_TO_INT(opts->data)) {
		opts_signal (opts, OPTS_SIG_CHANGED, &b_val);
		old = (long)(*(int*)opts->w.value);
		*(int*)opts->w.value = b_val;
	} else
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					b_val, opts->w.title);
	return old;
}

/*********************************************************************
  Create new button widgets. On button click value is set to button
  number starting at 1 (if != NULL) or cback is called with the number.
  title and ttip are '|'-separated.
  page can be a pointer to a prevBuffer (-> new menu item).
*********************************************************************/
static void button_create (long page, const char *title, const char *ttip,
						   GtkSignalFunc sigfunc, gint *value,
						   optsCbackFunc cback, void *data)
{
	optsValue *opts;
	GtkWidget *hbox = NULL, *button = NULL, **buttons = NULL;
	const char *pos;
	char *label, *tip;
	optsPage *p;
	int index, i, j, read;
	optsButtonData *bdata;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);
	if (cback) {
		bdata = malloc (sizeof(optsButtonData));
		bdata->cback = cback;
		bdata->data = data;
		opts = opts_value_add1__nl (p, WID_BUTTON, title, opts_buttoncb_set,
									value, bdata);
		opts->free_value = opts_buttoncb_free;
	} else {
		opts = opts_value_add1__nl (p, WID_BUTTON, title, opts_button_set,
									value, NULL);
	}

	if (p->widget) {
		hbox = gtk_hbox_new (TRUE, 5);
		gtk_box_pack_start (GTK_BOX (p->widget), hbox, FALSE, FALSE, 0);
	}
	i = 1;
	pos = title;
	while (pos && *pos) {
		j = gui_sscanf (pos, "%a|%n", &label, &read);
		if (j <= 0) break;

		if (j == 1)
			pos = NULL;
		else
			pos += read;
		if (p->widget) {
			button = gtk_button_new_with_label (label);
			gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
			gtk_signal_connect (GTK_OBJECT(button), "clicked",
								sigfunc, GINT_TO_POINTER(i));
		} else {
			GtkItemFactoryEntry item = {NULL, NULL, NULL, 0, "<Item>"};
			item.callback = sigfunc;
			opts_get_path (&item, label, NULL);
			gtk_item_factory_create_item (p->buffer->factory, &item, GINT_TO_POINTER(i), 2);
			button = gtk_item_factory_get_widget (p->buffer->factory,
												  opts_clean_path(item.path));
			g_free (item.path);
		}
		g_free (label);

		if (ttip) {
			j = gui_sscanf (ttip, "%a|%n", &tip, &read);
			if (j > 0) {
				if (j == 1)
					ttip = NULL;
				else
					ttip += read;
				gui_tooltip_set (button, tip);
				g_free (tip);
			}
		}
		if (cback) {
			buttons = realloc (buttons, sizeof(GtkWidget*)*i);
			buttons[i-1] = button;
		}
		gtk_object_set_user_data (GTK_OBJECT(button), opts);
		i++;
	}
	if (p->widget)
		gtk_widget_show_all (hbox);
	if (cback) {
		bdata->cnt = i-1;
		bdata->buttons = buttons;
	} else {
		opts->data = GINT_TO_POINTER(i-1);
	}
	opts_value_add2__nl (opts, p, index, p->widget ? hbox:button, button);
	gui_threads_leave();
}

/*********************************************************************
  Create new button widgets. On button click value is set to the
  button number starting at 1. title and ttip are '|'-separated for
  the single buttons, e.g. "title1|title2" for two buttons.
  page can be a pointer to a prevBuffer (-> new menu items).
*********************************************************************/
void opts_button_create (long page, const char *title, const char *ttip, gint *value)
{
	button_create (page, title, ttip, GTK_SIGNAL_FUNC(cb_button),
				   value, NULL, NULL);
}

/*********************************************************************
  Create new button widgets. On button click cback is called with the
  button number starting at 1 and 'data'. title and ttip are
  '|'-separated for the single buttons, e.g. "title1|title2" for
  two buttons.
  page can be a pointer to a prevBuffer (-> new menu items).
*********************************************************************/
void opts_buttoncb_create (long page, const char *title, const char *ttip,
						   optsCbackFunc cback, void *data)
{
	button_create (page, title, ttip, GTK_SIGNAL_FUNC(cb_buttoncb),
				   NULL, cback, data);
}

static void cb_string_entry (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	const char *new_value;

	if ((new_value = gtk_entry_get_text (GTK_ENTRY(widget)))) {
		opts_signal (opts, OPTS_SIG_CHANGED, (void*)new_value);
		/* Copy from back to front ->
		   value is always terminated by '\0' */
		gui_strcpy_back (opts->w.value, new_value, -1);
	}
}

/*********************************************************************
  Set new value and display it in string widget.
*********************************************************************/
static long opts_string_set (optsValue *opts, void *value)
{
	gtk_entry_set_text (GTK_ENTRY(opts->set_widget), value);
	gtk_editable_set_position (GTK_EDITABLE(opts->set_widget), -1);

	return OPTS_SET_ERROR;
}

/*********************************************************************
  Create new string widget. Max allowed length of string is length.
*********************************************************************/
void opts_string_create (long page, const char *title, const char *ttip,
						 char *value, int length)
{
	optsValue *opts;
	GtkWidget *box, *label, *entry;
	optsPage *p;
	int index;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_STRING, title,
								opts_string_set, value, NULL);

	box = gtk_vbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (p->widget), box, FALSE, FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	entry = gtk_entry_new_with_max_length (length-1);
	gui_tooltip_set (entry, ttip);
	gtk_entry_set_text (GTK_ENTRY(entry), value);
	gtk_editable_set_position (GTK_EDITABLE(entry), -1);
	gtk_signal_connect (GTK_OBJECT(entry), "changed",
						GTK_SIGNAL_FUNC(cb_string_entry), opts);
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);

	gtk_widget_show_all (box);

	opts_value_add2__nl (opts, p, index, box, entry);

	gui_threads_leave();
}

static void cb_stringenter_clicked (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	const char *new_value;

	if ((new_value = gtk_entry_get_text (GTK_ENTRY(opts->set_widget)))) {
		opts_signal (opts, OPTS_SIG_CHANGED, (void*)new_value);
		/* Copy from back to front ->
		   value is always terminated by '\0' */
		gui_strcpy_back (opts->w.value, new_value, -1);
	}
	gtk_widget_set_sensitive (widget, FALSE);
}

static void cb_stringenter_activate (GtkWidget *widget, void *button)
{
	const char *new_value;

	if ((new_value = gtk_entry_get_text (GTK_ENTRY(widget)))) {
		optsValue *opts = gtk_object_get_user_data(GTK_OBJECT(widget));
		opts_signal (opts, OPTS_SIG_CHANGED, (void*)new_value);
		/* Copy from back to front ->
		   value is always terminated by '\0' */
		gui_strcpy_back (opts->w.value, new_value, -1);
	}
	gtk_widget_set_sensitive (GTK_WIDGET(button), FALSE);
}

static void cb_stringenter_changed (GtkWidget *widget, void *button)
{
	gtk_widget_set_sensitive (GTK_WIDGET(button), TRUE);
}

/*********************************************************************
  Set new value and display it in string widget.
*********************************************************************/
static long opts_stringenter_set (optsValue *opts, void *value)
{
	gtk_entry_set_text (GTK_ENTRY(opts->set_widget), value);
	gtk_editable_set_position (GTK_EDITABLE(opts->set_widget), -1);

	gtk_signal_emit_by_name (GTK_OBJECT(opts->set_widget), "activate");

	return OPTS_SET_ERROR;
}

/*********************************************************************
  Create new string widget with an "enter" button. The string is
  updated only on enter press or button click. Max allowed length of
  string is length.
*********************************************************************/
void opts_stringenter_create (long page, const char *title, const char *ttip,
							  char *value, int length)
{
	GtkWidget *box, *hbox, *label, *pixwid, *entry;
	optsValue *opts;
	optsPage *p;
	int index;
	GdkPixmap *pix;
	GdkBitmap *mask;
	GdkColormap *colormap;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_STRING, title,
								opts_stringenter_set, value, NULL);

	box = gtk_vbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (p->widget), box, FALSE, FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

	  entry = gtk_entry_new_with_max_length (length-1);
	  gui_tooltip_set (entry, ttip);
	  gtk_entry_set_text (GTK_ENTRY(entry), value);
	  gtk_editable_set_position (GTK_EDITABLE(entry), -1);
	  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	  label = gtk_button_new();
	  gtk_widget_set_sensitive (label, FALSE);
	  colormap = gtk_widget_get_colormap (label);
	  pix = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
												   NULL, key_enter_xpm);
	  pixwid = gtk_pixmap_new (pix, mask);
	  gdk_pixmap_unref (pix);
	  gdk_bitmap_unref (mask);
	  gtk_container_add (GTK_CONTAINER(label), pixwid);

	  gui_tooltip_set (label, ttip);
	  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	  gtk_signal_connect (GTK_OBJECT(entry), "changed",
						  GTK_SIGNAL_FUNC(cb_stringenter_changed), label);
	  gtk_signal_connect (GTK_OBJECT(entry), "activate",
						  GTK_SIGNAL_FUNC(cb_stringenter_activate), label);
	  gtk_signal_connect (GTK_OBJECT(label), "clicked",
						  GTK_SIGNAL_FUNC(cb_stringenter_clicked), opts);

	gtk_widget_show_all (box);

	gtk_object_set_user_data (GTK_OBJECT(entry), opts);
	gtk_object_set_user_data (GTK_OBJECT(label), opts);
	opts_value_add2__nl (opts, p, index, box, entry);

	gui_threads_leave();
}

static void cb_filesel_fs_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	optsValue *opts = gtk_object_get_user_data(GTK_OBJECT(fs));
	const char *new_value = gtk_file_selection_get_filename (fs);

	if (!strcmp(opts->w.value, new_value))
		opts_signal (opts, OPTS_SIG_CHANGED, (void*)new_value);
	gtk_entry_set_text (GTK_ENTRY(opts->set_widget), new_value);
	gtk_editable_set_position (GTK_EDITABLE(opts->set_widget), -1);
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void cb_filesel_button (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	void **window = &opts->data;

	gui_file_selection_open ((GtkWidget**)window, "Select a file",
							 gtk_entry_get_text (GTK_ENTRY(opts->set_widget)),
							 GTK_SIGNAL_FUNC(cb_filesel_fs_ok));
	gtk_object_set_user_data (GTK_OBJECT(opts->data), opts);
}

static void cb_filesel_entry (GtkWidget *widget, void *data)
{
	const char *new_value;

	if ((new_value = gtk_entry_get_text (GTK_ENTRY(widget)))) {
		optsValue *opts = data;
		opts_signal (opts, OPTS_SIG_CHANGED, (void*)new_value);
		/* Copy from back to front ->
		   value is always terminated by '\0' */
		gui_strcpy_back (opts->w.value, new_value, -1);
	}
}

/*********************************************************************
  Set new value and display it in string widget.
*********************************************************************/
static long opts_filesel_set (optsValue *opts, void *value)
{
	gtk_entry_set_text (GTK_ENTRY(opts->set_widget), value);
	gtk_editable_set_position (GTK_EDITABLE(opts->set_widget), -1);

	return OPTS_SET_ERROR;
}

/*********************************************************************
  Create new string widget with an attached button to open a file
  selection widget. Max allowed length of string is length.
*********************************************************************/
void opts_filesel_create (long page, const char *title, const char *ttip,
						  char *value, int length)
{
	GtkWidget *box, *hbox, *label, *entry;
	optsValue *opts;
	optsPage *p;
	int index;

	gui_threads_enter();

	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_FILESEL, title,
								opts_filesel_set, value, NULL);

	box = gtk_vbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (p->widget), box, FALSE, FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

	  entry = gtk_entry_new_with_max_length (length-1);
	  gui_tooltip_set (entry, ttip);
	  gtk_entry_set_text (GTK_ENTRY(entry), value);
	  gtk_editable_set_position (GTK_EDITABLE(entry), -1);
	  gtk_signal_connect (GTK_OBJECT(entry), "changed",
						  GTK_SIGNAL_FUNC(cb_filesel_entry), opts);
	  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	  label = gtk_button_new_with_label (" ... ");
	  gui_tooltip_set (label, ttip);
	  gtk_signal_connect (GTK_OBJECT(label), "clicked",
						  GTK_SIGNAL_FUNC(cb_filesel_button), opts);
	  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all (box);
	opts_value_add2__nl (opts, p, index, box, entry);

	gui_threads_leave();
}

static void cb_entscale_entry (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	GtkAdjustment *adjustment;
	gint new_value, *value = (gint*)opts->w.value;

	new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

	if (*value != new_value) {
		adjustment = gtk_object_get_user_data(GTK_OBJECT(widget));

		if (new_value >= adjustment->lower && new_value <= adjustment->upper) {
			opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
			*value            = new_value;
			adjustment->value = new_value;

			gtk_signal_emit_by_name (GTK_OBJECT(adjustment), "value_changed");
		}
	}
}

static void cb_entscale_scale (GtkAdjustment *adjustment, void *data)
{
	optsValue *opts = data;
	GtkWidget *entry;
	char buf[20];
	gint *value = opts->w.value;
	gint new_value = adjustment->value;

	if (*value != new_value) {
		opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
		*value = new_value;

		entry = gtk_object_get_user_data(GTK_OBJECT(adjustment));
		sprintf(buf, "%d", *value);

		gtk_signal_handler_block_by_data(GTK_OBJECT(entry), opts);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(entry), opts);
	}
}

/*********************************************************************
  Set new value and display it in entry-scale widget.
*********************************************************************/
static long opts_entscale_set (optsValue *opts, void *value)
{
	long old = OPTS_SET_ERROR;
	char buf[20];
	gint *act_value = opts->w.value, i_val = GPOINTER_TO_INT(value);
	GtkAdjustment *adjustment;

	adjustment = gtk_object_get_user_data(GTK_OBJECT(opts->set_widget));

	if (i_val>=adjustment->lower && i_val<=adjustment->upper) {
		old = (long)(*act_value);

		/* Ensure that both the entry and the scale are updated */
		adjustment->value = i_val;
		gtk_signal_emit_by_name (GTK_OBJECT(adjustment), "value_changed");

		sprintf (buf, "%d", i_val);
		gtk_entry_set_text (GTK_ENTRY(opts->set_widget), buf);
	} else
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					i_val, opts->w.title);
	return old;
}

/*********************************************************************
  Create new integer scale widget with an attached entry widget.
*********************************************************************/
void opts_entscale_create (long page, const char *title, const char *ttip,
						   gint *value, gint left, gint right)
{
	optsValue *opts;
	GtkWidget *hbox, *label, *scale, *entry;
	GtkObject *scale_data;
	optsPage *p;
	char buf[20];
	int index, step, max;

	gui_threads_enter();

	value_clamp_int ("opts_entscale_create", value, left, right);
	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_ENTSCALE, title,
								opts_entscale_set, value, NULL);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (p->widget), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	step = (int)((right-left+1)/20);
	if (step < 1) step = 1;
	scale_data = gtk_adjustment_new (*value, left, right, 1.0, step, 0.0);

	scale = gtk_hscale_new (GTK_ADJUSTMENT(scale_data));
	gui_tooltip_set (scale, ttip);

	gtk_widget_set_usize (scale, SCALE_WIDTH, 0);
	gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
	gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
	gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 5);

	sprintf (buf, "%d", left);
	max = strlen(buf);
	sprintf (buf, "%d", right);
	if (strlen(buf) > max) max = strlen(buf);
	for (step=0; step < max && step < 9; step++)
		buf[step] = '8';
	buf[step] = '\0';

	entry = gtk_entry_new_with_max_length (max);
	gui_tooltip_set (entry, ttip);
	gtk_object_set_user_data (GTK_OBJECT(entry), scale_data);
	gtk_object_set_user_data (scale_data, entry);
	gui_entry_set_width (entry, buf, ENTRY_WIDTH);
	sprintf (buf, "%d", *value);
	gtk_entry_set_text (GTK_ENTRY(entry), buf);
	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT(scale_data), "value_changed",
						GTK_SIGNAL_FUNC(cb_entscale_scale), opts);
	gtk_signal_connect (GTK_OBJECT(entry), "changed",
						GTK_SIGNAL_FUNC(cb_entscale_entry), opts);

	gtk_widget_show_all (hbox);

	opts_value_add2__nl (opts, p, index, hbox, entry);

	gui_threads_leave();
}

/*********************************************************************
  Return useful number of digits after the comma for a range of values
  between left and right.
*********************************************************************/
static int float_get_digits (float left, float right)
{
	int digits = 2;
	int i = 1/(1.01*(right-left));

	if (right-left >= 100) return 1;
	while (i > 0) {
		digits++;
		i = i/10;
	}
	return digits;
}

/*********************************************************************
  Set buf to value. If buf gets outside of [left,right] due to
  rounding errors change value.
*********************************************************************/
static void float_set_text (GtkEntry *entry, char *buf, float value, float left, float right)
{
	int digits = float_get_digits(left, right);
	float step = 0.00001;
	float v = value;

	while (1) {
		if (entry) {
			do {
				sprintf (buf, "%0.*f", digits+1, v);

				/* Remove any trailing '0' (except first '0' after a comma) */
				if (strchr(buf, '.')) {
					int i = strlen(buf)-1;
					while (i>1 && buf[i]=='0' && buf[i-1]!='.') {
						buf[i] = '\0';
						i--;
					}
				}
				digits--;
			} while (digits >= 0 && strlen(buf) > entry->text_max_length);
		} else
			sprintf (buf, "%0.*f", digits, v);
		v = atof (buf);
		if ((v>=left && v<=right) || step > 10)
			break;
		if (v < left)
			v += step;
		else
			v -= step;
		step *= 10;
	}
	if (entry)
		gtk_entry_set_text (entry, buf);
}

static void cb_float_entry (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	GtkAdjustment *adjustment;
	gfloat new_value, *value = (gfloat*)opts->w.value;

	new_value = atof(gtk_entry_get_text(GTK_ENTRY(widget)));

	if (*value != new_value) {
		adjustment = gtk_object_get_user_data(GTK_OBJECT(widget));

		if (new_value >= adjustment->lower && new_value <= adjustment->upper) {
			opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
			*value            = new_value;
			adjustment->value = new_value;

			gtk_signal_emit_by_name (GTK_OBJECT(adjustment), "value_changed");
		}
	}
}

static void cb_float_scale (GtkAdjustment *adjustment, void *data)
{
	optsValue *opts = data;
	GtkWidget *entry;
	char buf[20];
	gfloat *value = opts->w.value;
	gfloat new_value = adjustment->value;

	if (*value != new_value) {
		opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
		entry = gtk_object_get_user_data(GTK_OBJECT(adjustment));
		float_set_text (NULL, buf, adjustment->value, adjustment->lower, adjustment->upper);
		*value = atof(buf);

		gtk_signal_handler_block_by_data(GTK_OBJECT(entry), opts);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(entry), opts);
	}
}

/*********************************************************************
  Set new value and display it in entry-scale widget.
*********************************************************************/
static long opts_float_set (optsValue *opts, void *value)
{
	long old = OPTS_SET_ERROR;
	char buf[20];
	gfloat *act_value = opts->w.value, f_val = *(gfloat*)value;
	GtkAdjustment *adjustment;

	adjustment = gtk_object_get_user_data(GTK_OBJECT(opts->set_widget));

	if (f_val>=adjustment->lower && f_val<=adjustment->upper) {
		old = (long)(*act_value);

		/* Ensure that both the entry and the scale are updated */
		adjustment->value = f_val;
		gtk_signal_emit_by_name (GTK_OBJECT(adjustment), "value_changed");

		float_set_text (GTK_ENTRY(opts->set_widget), buf, f_val, adjustment->lower, adjustment->upper);
	} else
		iw_warning ("Value %f for '%s' out of bounds, ignoring...",
					f_val, opts->w.title);
	return old;
}

/*********************************************************************
  Create new scale widget with an attached entry widget (for floats).
*********************************************************************/
void opts_float_create (long page, const char *title, const char *ttip,
						gfloat *value, gfloat left, gfloat right)
{
	optsValue *opts;
	GtkWidget *hbox, *label, *scale, *entry;
	GtkObject *scale_data;
	optsPage *p;
	char buf[20];
	gfloat max;
	int index, digits, start;

	gui_threads_enter();

	value_clamp_float ("opts_float_create", value, left, right);
	digits = float_get_digits (left, right);
	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_FLOAT, title,
								opts_float_set, value, NULL);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (p->widget), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	scale_data = gtk_adjustment_new (*value, left, right,
									 (right-left)/100, (right-left)/20, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT(scale_data));
	gui_tooltip_set (scale, ttip);

	gtk_widget_set_usize (scale, SCALE_WIDTH, 0);
	gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
	gtk_scale_set_digits (GTK_SCALE (scale), digits);
	gtk_range_set_update_policy (GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
	gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 5);

	if (right <= 0)
		max = left;
	else if (left >= 0)
		max = right;
	else if (-10*left+9.99999 >= right)
		max = left;
	else
		max = right;
	sprintf (buf, "%d", (int)ABS(max));
	digits += strlen(buf) + (max<0);
	entry = gtk_entry_new_with_max_length (digits+1);

	start = 0;
	if (max < 0) {
		digits--;
		buf[start++] = '-';
	}
	digits = CLAMP (digits, 2, 9);
	buf[start+digits+1] = '\0';
	buf[start+digits] = '8';
	buf[start+digits-1] = '.';
	for (; digits > 1; digits--)
		buf[start+digits-2] = '8';
	gui_entry_set_width (entry, buf, 20);

	gui_tooltip_set (entry, ttip);
	gtk_object_set_user_data (GTK_OBJECT(entry), scale_data);
	gtk_object_set_user_data (scale_data, entry);
	float_set_text (GTK_ENTRY(entry), buf, *value, left, right);
	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT(scale_data), "value_changed",
						GTK_SIGNAL_FUNC(cb_float_scale), opts);
	gtk_signal_connect (GTK_OBJECT(entry), "changed",
						GTK_SIGNAL_FUNC(cb_float_entry), opts);

	gtk_widget_show_all (hbox);
	opts_value_add2__nl (opts, p, index, hbox, entry);

	gui_threads_leave();
}

static void cb_toggle (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	int *value = (int*)opts->w.value;
	int new_value;

	if ((GTK_IS_TOGGLE_BUTTON(widget) && GTK_TOGGLE_BUTTON (widget)->active) ||
		(GTK_IS_CHECK_MENU_ITEM(widget) && GTK_CHECK_MENU_ITEM (widget)->active))
		new_value = TRUE;
	else
		new_value = FALSE;
	opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
	*value = new_value;
}

/*********************************************************************
  Set new value and display it in toggle widget.
*********************************************************************/
static long opts_toggle_set (optsValue *opts, void *value)
{
	long old = OPTS_SET_ERROR;
	int b_val = GPOINTER_TO_INT(value);

	if (b_val==FALSE || b_val==TRUE) {
		old = (long)(*(int*)opts->w.value);

		if (GTK_IS_TOGGLE_BUTTON(opts->set_widget))
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opts->set_widget), b_val);
		else
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (opts->set_widget), b_val);
		*(int*)opts->w.value = b_val;
	} else
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					b_val, opts->w.title);
	return old;
}

/*********************************************************************
  Create new toggle widget.
  page can be a pointer to a prevBuffer (-> new check menu item).
*********************************************************************/
void opts_toggle_create (long page, const char *title, const char *ttip,
						 gboolean *value)
{
	optsValue *opts;
	GtkWidget *toggle;
	optsPage *p;
	int index;

	gui_threads_enter();

	value_clamp_int ("opts_toggle_create", value, FALSE, TRUE);
	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_TOGGLE, title,
								opts_toggle_set, value, NULL);

	if (p->widget) {
		toggle = gtk_check_button_new_with_label (title);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), *value);
		gtk_box_pack_start (GTK_BOX (p->widget), toggle, FALSE, FALSE, 0);
		gtk_signal_connect (GTK_OBJECT (toggle), "toggled",
							GTK_SIGNAL_FUNC(cb_toggle), opts);
		gtk_widget_show (toggle);
	} else {
		GtkItemFactory *factory = p->buffer->factory;
		GtkItemFactoryEntry item = {NULL, NULL, cb_toggle, 0, "<CheckItem>"};

		opts_get_path (&item, title, NULL);
		gtk_item_factory_create_item (factory, &item, opts, 2);
		toggle = gtk_item_factory_get_widget (factory,
											  opts_clean_path(item.path));
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (toggle), *value);
		g_free (item.path);
	}

	gui_tooltip_set (toggle, ttip);
	opts_value_add2__nl (opts, p, index, toggle, toggle);

	gui_threads_leave();
}

static void cb_option (GtkWidget *widget, void *data)
{
	optsValue *opts = data;
	int *value = (int*)opts->w.value;
	int new_value = GPOINTER_TO_INT(gtk_object_get_user_data (GTK_OBJECT(widget)));

	opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
	*value = new_value;
}

/*********************************************************************
  Set new value and display it in option widget.
*********************************************************************/
static long opts_option_set (optsValue *opts, void *value)
{
	long old = OPTS_SET_ERROR;
	int *act_val = opts->w.value;
	int i_val = GPOINTER_TO_INT(value);

	if (i_val >= 0) {
		old = *act_val;
		if (i_val != *act_val)
			opts_signal (opts, OPTS_SIG_CHANGED, &i_val);
		gtk_option_menu_set_history (GTK_OPTION_MENU (opts->set_widget), i_val);
		*act_val = i_val;
	} else
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					i_val, opts->w.title);
	return old;
}

/*********************************************************************
  Create new option-menu widget with selectable values from label.
  The const array label must be terminated with NULL.
*********************************************************************/
void opts_option_create (long page, const char *title, const char *ttip,
						 char **label, gint *value)
{
	optsValue *opts;
	GtkWidget *o_menu, *menu, *item, *box;
	optsPage *p;
	int index, i;

	gui_threads_enter();

	for (i=0; label[i]; i++) /* empty */;
	value_clamp_int ("opts_option_create", value, 0, i-1);
	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_OPTION, title,
								opts_option_set, value, NULL);

	box = gtk_hbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (p->widget), box, FALSE, FALSE, 0);

	menu = gtk_label_new (title);
	gtk_box_pack_start (GTK_BOX (box), menu, FALSE, FALSE, 0);

	o_menu = gtk_option_menu_new ();
	gui_tooltip_set (o_menu, ttip);
	gtk_box_pack_start (GTK_BOX (box), o_menu, TRUE, TRUE, 0);

	menu = gtk_menu_new ();
	for (i=0; label[i]; i++) {
		item = gtk_menu_item_new_with_label (label[i]);
		gtk_object_set_user_data (GTK_OBJECT(item), GINT_TO_POINTER(i));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
							GTK_SIGNAL_FUNC(cb_option), opts);
		gtk_menu_append (GTK_MENU (menu), item);
	}
	gtk_widget_show_all (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (o_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (o_menu),*value);

	gtk_widget_show_all (box);
	opts_value_add2__nl (opts, p, index, box, o_menu);

	gui_threads_leave();
}

static void cb_radio (GtkWidget *widget, void *data)
{
	if ((GTK_IS_RADIO_BUTTON(widget) && GTK_TOGGLE_BUTTON (widget)->active) ||
		(GTK_IS_RADIO_MENU_ITEM(widget) && GTK_CHECK_MENU_ITEM (widget)->active)) {
		optsValue *opts = data;
		int *value = opts->w.value;
		int new_value = GPOINTER_TO_INT(gtk_object_get_user_data (GTK_OBJECT(widget)));
		opts_signal (opts, OPTS_SIG_CHANGED, &new_value);
		*value = new_value;
	}
}

/*********************************************************************
  Set new value and display it in radio widget.
*********************************************************************/
static long opts_radio_set (optsValue *opts, void *value)
{
	long old = (long)(*(int*)opts->w.value);
	int i_val = GPOINTER_TO_INT(value), nr;
	GSList *group;

	if (GTK_IS_RADIO_BUTTON(opts->set_widget)) {
		group = gtk_radio_button_group(GTK_RADIO_BUTTON(opts->set_widget));

		nr = 0;
		for (; group; group = group->next) {
			int index = GPOINTER_TO_INT(gtk_object_get_user_data (GTK_OBJECT(group->data)));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(group->data), index == i_val);
			nr++;
		}
	} else {
		group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(opts->set_widget));

		nr = 0;
		for (; group; group = group->next) {
			int index = GPOINTER_TO_INT(gtk_object_get_user_data (GTK_OBJECT(group->data)));
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(group->data), index == i_val);
			nr++;
		}
	}

	if (i_val>=0 && i_val<=nr) {
		*(int*)opts->w.value = i_val;
	} else {
		iw_warning ("Value %d for '%s' out of bounds, ignoring...",
					i_val, opts->w.title);
		old = OPTS_SET_ERROR;
	}
	return old;
}

/*********************************************************************
  Create new radio widget with selectable values from label.
  The const array label must be terminated with NULL.
  page can be a pointer to a prevBuffer (-> new radio menu item).
*********************************************************************/
void opts_radio_create (long page, const char *title, const char *ttip,
						char **label, gint *value)
{
	optsValue *opts;
	GtkWidget *button, *box, *sub = NULL;
	optsPage *p;
	GtkItemFactoryEntry branch = {NULL, NULL, NULL, 0, "<Branch>"};
	GtkItemFactoryEntry item = {NULL, NULL, cb_radio, 0, "<RadioItem>"};
	int index, i, val_int;

	gui_threads_enter();

	for (i=0; label[i]; i++) /* empty */;
	value_clamp_int ("opts_radio_create", value, 0, i-1);
	val_int = *value;

	opts_create_prepare (page, &title, &p, &index);
	opts = opts_value_add1__nl (p, WID_RADIO, title,
								opts_radio_set, value, NULL);

	if (p->widget) {
		box = gtk_hbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (p->widget), box, FALSE, FALSE, 0);

		button = gtk_label_new (title);
		gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

		sub = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (box), sub, FALSE, FALSE, 0);
	} else {
		opts_get_path (&branch, title, NULL);
		gtk_item_factory_create_item (p->buffer->factory, &branch, value, 2);
		box = gtk_item_factory_get_widget (p->buffer->factory,
										   opts_clean_path(branch.path));
		gui_tooltip_set (box, ttip);
	}

	button = NULL;
	for (i=0; label[i]; i++) {
		if (p->widget) {
			button = gtk_radio_button_new_with_label
				((button==NULL) ? NULL : gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
				 label[i]);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), val_int==i);
			gtk_box_pack_start (GTK_BOX (sub), button, FALSE, FALSE, 0);
			gtk_signal_connect (GTK_OBJECT (button), "toggled",
								GTK_SIGNAL_FUNC(cb_radio), opts);
			gtk_object_set_user_data (GTK_OBJECT(button), GINT_TO_POINTER(i));
		} else {
			if (item.path)
				item.item_type = item.path;
			else
				item.item_type = g_strdup ("<RadioItem>");
			opts_get_path (&item, branch.path, label[i], NULL);

			gtk_item_factory_create_item (p->buffer->factory, &item, opts, 2);
			button = gtk_item_factory_get_widget (p->buffer->factory,
												  opts_clean_path(item.path));
			gtk_object_set_user_data (GTK_OBJECT(button), GINT_TO_POINTER(i));
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(button), val_int==i);

			g_free (item.item_type);
		}
		gui_tooltip_set (button, ttip);
	}
	if (item.path)
		g_free (item.path);
	if (branch.path)
		g_free (branch.path);

	gtk_widget_show_all (box);
	opts_value_add2__nl (opts, p, index, box, button);

	gui_threads_leave();
}

static void opts_load_menu (char *menu)
{
#if GTK_CHECK_VERSION(2,0,0)
	GScanner *scanner = g_scanner_new (NULL);
	g_scanner_input_text (scanner, menu, strlen(menu));
	gtk_accel_map_load_scanner (scanner);
	g_scanner_destroy (scanner);
#else
	gtk_item_factory_parse_rc_string (menu);
#endif
	g_free (menu);
}

/*********************************************************************
  fkt!=NULL: Get settings for widgets and display them by periodically
			 calling fkt(data,buffer,buf_len).
  fkt==NULL: Load settings of all widgets from file data (a file name
             of type char*) and display them.
*********************************************************************/
static void opts_load__nl (gboolean deffile, optsLoadFunc fkt, void *data)
{
	optsValue *opts;
	FILE *file = NULL;
	char *line, *id, *value;
	int line_len = 10000, len;
	char *menu = NULL;
	int menulen = 0, menumax = 0;

	if (!fkt) {
		if (!(file=fopen((char*)data, "r"))) {
			if (!deffile)
				gui_message_show__nl ("Error",
									  "Unable to open options file '%s' for loading!",
									  (char*)data);
			return;
		}
	}

	line = g_malloc (line_len);
	while (1) {
		/* Read one line */
		if (fkt) {
			if (!fkt(data, line, line_len))
				break;
		} else {
			if (!gui_fgets (&line, &line_len, file))
				break;
		}
		if (line[0] && line[strlen(line)-1]=='\n')
			line[strlen(line)-1] = '\0';

		/* Parse the line */
		if (menu) {
			if (!strncmp (MENU_ID, line, strlen(MENU_ID))) {
				opts_load_menu (menu);
				menu = NULL;
			} else {
				len = strlen(line);
				if (menulen+len+1 >= menumax) {
					menumax += MAX(4000, len+2);
					menu = g_realloc (menu, menumax);
				}
				strcat (&menu[menulen], line);
				menulen += len;
				strcat (&menu[menulen], "\n");
				menulen++;
			}
		} else if (!strncmp (MENU_ID, line, strlen(MENU_ID))) {
			menulen = 0;
			menumax = 4000;
			menu = g_malloc (menumax);
			menu[0] = '\0';
		} else if (gui_parse_line (line, &id, &value)) {
			if ((opts = title_search(-1, id, TRUE)) && opts->set_value) {
				opts_value_set_char (opts, value);
			} else if (deffile)
				opts_defvalue_append (id, value);
		}
	}
	g_free (line);
	if (menu)
		opts_load_menu (menu);
	if (file)
		fclose (file);
}

/*********************************************************************
  fkt!=NULL: Get settings for widgets and display them by periodically
			 calling fkt(data,buffer,buf_len).
  fkt==NULL: Load settings of all widgets from file data (a file name
             of type char*) and display and display them.
*********************************************************************/
void opts_load (optsLoadFunc fkt, void *data)
{
	gui_threads_enter();
	opts_load__nl (FALSE, fkt, data);
	gui_threads_leave();
}

static gboolean cb_opts_load_default (void *data, char *buffer, int size)
{
	char **string = data, *pos;

	while (**string == '\n') (*string)++;
	pos = *string;
	while (*pos && *pos != '\n') pos++;

	if (pos != *string) {
		if (pos-(*string) >= size)
			pos = (*string)+size-1;
		if (pos-*string > 0)
			memcpy (buffer, *string, pos-*string);
		buffer[pos-*string] = '\0';
		*string = pos;
		return TRUE;
	} else
		return FALSE;
}

/*********************************************************************
  PRIVATE: Load settings of all widgets from the default file and
  from file and display them.
*********************************************************************/
void opts_load_default (char **file)
{
	gui_threads_enter();
	opts_load__nl (TRUE, NULL, opts_get_default_file());
	if (file) {
		while (*file) {
			if (strchr (*file, '=')) {
				char *data = *file;
				opts_load__nl (TRUE, cb_opts_load_default, &data);
			} else
				opts_load__nl (TRUE, NULL, *file);
			file++;
		}
	}
	gui_threads_leave();
}

/*********************************************************************
  If opts_save() is called, do not save the settings for the widget
  referenced by title.
*********************************************************************/
void opts_save_remove (const char *title)
{
	char *new;

	if (!opts_save_rem)
		opts_save_rem = g_hash_table_new (g_str_hash, g_str_equal);

	new = g_hash_table_lookup (opts_save_rem, title);
	if (!new) {
		new = g_strdup (title);
		g_hash_table_insert (opts_save_rem, new, new);
	}
}

static gboolean cb_opts_save (void *data, char *string)
{
	FILE *file = data;

	if (fputs (string, file) == EOF)
		return FALSE;
	else
		return TRUE;
}

typedef struct optsSaveMenuData {
	optsSaveFunc fkt;
	void *data;
} optsSaveMenuData;
#if GTK_CHECK_VERSION(2,0,0)
static void cb_opts_save_menu (gpointer data, const gchar *path, guint key,
							   GdkModifierType mods, gboolean changed)
{
	optsSaveMenuData *dat = (optsSaveMenuData*)data;
	gchar *tmp, *name, *epath;

	epath = g_strescape (path, NULL);
	tmp = gtk_accelerator_name (key, mods);
	name = g_strescape (tmp, NULL);
	g_free (tmp);

	tmp = g_strconcat (changed ? "" : "; ",
					   "(gtk_accel_path \"", epath, "\" \"", name, "\")\n", NULL);
	dat->fkt (dat->data, tmp);

	g_free (tmp);
	g_free (name);
	g_free (epath);
}
#else
static void cb_opts_save_menu (gpointer *data, gchar *string)
{
	optsSaveMenuData *dat = (optsSaveMenuData*)data;

	dat->fkt (dat->data, string);
	dat->fkt (dat->data, "\n");
}
#endif

/*********************************************************************
  fkt!=NULL: Save settings of all widgets by periodically calling
             fkt(data,string) for all to be saved strings.
  fkt==NULL: Save settings of all widgets to file data (a file name
             of type char*).
*********************************************************************/
static gboolean opts_save__nl (optsSaveFunc fkt, void *data, gboolean showerr)
{
	optsValue *opts;
	gboolean save;
	char *string = NULL, *err = NULL;
	FILE *file = NULL;
	optsSaveMenuData menu;

	if (!fkt) {
		if (!(file = fopen((char*)data, "w"))) {
			err = "Unable to open '%s' for saving the options!";
			goto cleanup;
		}
		data = file;
		fkt = cb_opts_save;
	}
	string = g_strdup_printf ("# Options for %s V%s\n\n", GUI_PRG_NAME, GUI_VERSION);
	if (!fkt (data, string)) {
		err = "Error saving options in '%s'!";
		goto cleanup;
	}
	g_free (string);
	string = NULL;

	for (opts = opts_values; opts; opts = opts->next) {
		save = TRUE;
		if (opts_save_rem) {
			if (g_hash_table_lookup (opts_save_rem, opts->w.title) ||
				g_hash_table_lookup (opts_save_rem, opts->w.wtitle))
				save = FALSE;
		}
		if (save && opts->set_value) {
			switch (opts->wtype) {
				case WID_STRING:
				case WID_FILESEL:
				case WID_VAR_STRING:
					string = g_strdup_printf ("\"%s\" = %s\n",
											  opts->w.title, (char*)opts->w.value);
					break;
				case WID_ENTSCALE:
				case WID_TOGGLE:
				case WID_OPTION:
				case WID_RADIO:
				case WID_VAR_BOOL:
				case WID_VAR_INT:
					string = g_strdup_printf ("\"%s\" = %d\n",
											  opts->w.title, *(int*)opts->w.value);
					break;
				case WID_BUTTON:
					/* Save buttons to have a complete list of all widgets */
					if (opts->w.value)
						string = g_strdup_printf ("\"%s\" = %d\n",
												  opts->w.title, *(int*)opts->w.value);
					else
						string = g_strdup_printf ("\"%s\" = 0\n", opts->w.title);
					break;
				case WID_VAR_LONG:
					string = g_strdup_printf ("\"%s\" = %ld\n",
											  opts->w.title, *(long*)opts->w.value);
					break;
				case WID_LIST:
					string = opts_list_sprintf (opts);
					break;
				case WID_FLOAT:
				case WID_VAR_FLOAT:
					string = g_strdup_printf ("\"%s\" = %f\n",
											  opts->w.title, *(float*)opts->w.value);
					break;
				case WID_VAR_DOUBLE:
					string = g_strdup_printf ("\"%s\" = %f\n",
											  opts->w.title, *(double*)opts->w.value);
					break;
				default: break;
			}
			if (string) {
				if (!fkt (data, string)) {
					err = "Error saving options in '%s'!";
					goto cleanup;
				}
				g_free (string);
				string = NULL;
			}
		}
	}

	if (!fkt (data, MENU_ID" START\n")) {
		err = "Error saving options in '%s'!";
		goto cleanup;
	}
	menu.fkt = fkt;
	menu.data = data;
#if GTK_CHECK_VERSION(2,0,0)
	gtk_accel_map_foreach (&menu, cb_opts_save_menu);
#else
	gtk_item_factory_dump_items (NULL, TRUE,
								 (GtkPrintFunc)cb_opts_save_menu, &menu);
#endif
	if (!fkt (data, MENU_ID" END\n")) {
		err = "Error saving options in '%s'!";
		goto cleanup;
	}
 cleanup:
	if (string)
		g_free (string);
	if (file)
		fclose (file);
	if (err) {
		if (showerr)
			gui_message_show__nl ("Error", err, (char*)data);
		return FALSE;
	} else
		return TRUE;
}

/*********************************************************************
  fkt!=NULL: Save settings of all widgets by periodically calling
             fkt(data,string) for all to be saved strings.
  fkt==NULL: Save settings of all widgets to file data (a file name
             of type char*).
  Return: TRUE if no error occurred.
*********************************************************************/
gboolean opts_save (optsSaveFunc fkt, void *data)
{
	gboolean ret;

	gui_threads_enter();
	ret = opts_save__nl (fkt, data, FALSE);
	gui_threads_leave();

	return ret;
}

static GtkWidget *opts_file_win = NULL;
static char opts_filename[PATH_MAX];

static void cb_file_save_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	gui_strlcpy (opts_filename, gtk_file_selection_get_filename (fs), PATH_MAX);
	opts_save__nl (NULL, opts_filename, TRUE);
	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void cb_file_load_ok (GtkWidget *widget, GtkFileSelection *fs)
{
	gui_strlcpy (opts_filename, gtk_file_selection_get_filename (fs), PATH_MAX);
	opts_load__nl (FALSE, NULL, opts_filename);
	gtk_widget_destroy (GTK_WIDGET (fs));
}


/*********************************************************************
  PRIVATE: GtkSignalFunc() callback to load the default settings
  file / a settings file via a file selector.
*********************************************************************/
void opts_load_cb (GtkWidget *widget, gpointer def_file)
{
	if ((gboolean)(long)def_file)
		opts_load__nl (FALSE, NULL, opts_get_default_file());
	else
		gui_file_selection_open (&opts_file_win, "Load options", opts_filename,
								 GTK_SIGNAL_FUNC(cb_file_load_ok));
}

/*********************************************************************
  PRIVATE: GtkSignalFunc() callback to save to the default settings
  file / to a settings file via a file selector.
*********************************************************************/
void opts_save_cb (GtkWidget *widget, gpointer def_file)
{
	if ((gboolean)(long)def_file)
		opts_save__nl (NULL, opts_get_default_file(), TRUE);
	else
		gui_file_selection_open (&opts_file_win, "Save options", opts_filename,
								 GTK_SIGNAL_FUNC(cb_file_save_ok));
}

/*********************************************************************
  Append a new page to the options notebook. If title contains '.' or
  ' ' (a'.'b or a' 'b) the page is shown in the categories list under
  node a with entry b.
  Return: the number of the new page
*********************************************************************/
int opts_page_append (const char *title)
{
	int p = 0;

	iw_assert (opts_parent != NULL, "Goptions not initialized");

	gui_threads_enter();
	p = opts_page_add__nl (gui_listbook_append (opts_parent, title),
						   NULL, title);
	gui_threads_leave();

	return p;
}

/*********************************************************************
  PRIVATE: Initialise the options interface.
*********************************************************************/
void opts_init (GtkWidget *parent)
{
	opts_parent = parent;
	if (!opts_pages)
		opts_pages = g_ptr_array_new();
}
