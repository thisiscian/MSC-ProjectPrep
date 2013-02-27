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

#ifndef iw_Goptions_H
#define iw_Goptions_H

#include <gtk/gtk.h>

/* Return value for opts_value_set() if error occurs */
#define OPTS_SET_ERROR	(-9999)

/* Flags for a list widget: */
typedef enum {
	OPTS_SELECT			= 1 << 0,	/* Toggle entries (otherwise only one entry is selected) */
	OPTS_REORDER		= 1 << 1,	/* Entries can be reordered */
	OPTS_ADD			= 1 << 2,	/* Entries can be added/removed */
	OPTS_DATA			= 1 << 3	/* For every entry a string can be entered */
} optsListFlags;

/* Data type identifier for opts_variable_add() */
typedef enum {
	OPTS_BOOL,
	OPTS_INT,
	OPTS_LONG,
	OPTS_FLOAT,
	OPTS_DOUBLE,
	OPTS_STRING
} optsType;

/* Both optsListEntry and optsListData can be passed to opts_value_set() */

/* Entries of the list widget, can be passed to opts_value_set() */
typedef struct optsListEntry {
	int oldindex;				/* Index of the entry before reordering, -1 marks last entry */
	gboolean selected;			/* Is the entry selected ? */
	char *data;					/* Any data entered for this entry (see OPTS_DATA) */
} optsListEntry;

/* Data for a list entry, can be passed to opts_value_set() */
typedef struct optsListData {
	optsListEntry *entries;		/* Array of all entries */
	int current;				/* Currently selected entry */
} optsListData;

typedef struct optsWidget {
	char *title;				/* Full qualified name */
	char *wtitle;				/* Pointer inside title, widget name part */
	void *value;				/* Pointer to the widgets current value/variable */
} optsWidget;

/* Signal for a widget, see opts_signal_connect() */
typedef enum {
	OPTS_SIG_CHANGED	= 1 << 0,	/* The value of the widget has changed */
	OPTS_SIG_REMOVE		= 1 << 1	/* The widget is removed */
} optsSignal;

typedef gboolean (*optsLoadFunc) (void *data, char *buffer, int size);
typedef gboolean (*optsSaveFunc) (void *data, char *string);

typedef void (*optsCbackFunc) (GtkWidget *widget, int number, void *data);

typedef gboolean (*optsSetFunc) (void *value, void *newValue, void *data);
typedef void (*optsSignalFunc) (optsWidget *widget, void *newValue, void *data);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return name of the default rcFile. Returned value is a pointer to a
  static variable.
*********************************************************************/
char *opts_get_default_file (void);

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
							  optsSignalFunc cback, void *data);

/*********************************************************************
  Remove widget with title 'title' from options tab.
  Use 'pageTitle.widgetTitle' for the title argument.
  Return: Did the widget exist?
*********************************************************************/
gboolean opts_widget_remove (const char *title);

/*********************************************************************
  Prevent any values, which are loaded from default config files to
  be set for the widget referenced by title.
*********************************************************************/
void opts_defvalue_remove (const char *title);

/*********************************************************************
  Return a pointer to the current value from the widget referenced by
  title.
  title: 'pageTitle.widgetTitle'
*********************************************************************/
void *opts_value_get (const char *title);

/*********************************************************************
  Set new value and display it in widget referenced by title.
  title: 'pageTitle.widgetTitle'
  value: strings, floats, list: a pointer to the value
         otherwise            : the value itself
  Return: Old value on sucess, OPTS_SET_ERROR otherwise (no widget
          found or value does not fit into one long(e.g. a string)).
*********************************************************************/
long opts_value_set (const char *title, void *value);

/*********************************************************************
  Add a non graphical value, which gets loaded/saved in the same way
  as the graphical values. If the value should be set to a new value,
  setval() is called with the old value, the new value and data as
  arguments. If setval() returns TRUE or if setval()==NULL the value
  is set automatically in the background.
*********************************************************************/
void opts_variable_add (const char *title, optsSetFunc setval, void *data,
						optsType type, void *value);
void opts_varstring_add (const char *title, optsSetFunc setval, void *data,
						 void *value, int length);

/*
 * For all opts_..._create() functions:
 * - page: - return value of opts_page_append() or
 *         - return value of prev_get_page() or
 *         - partly a pointer to a prevBuffer
 *               -> widget is added to the context menu or
 *         - -1  -> title must have the form 'pageTitle.widgetTitle'
 * - title: Name for the widget or 'pageTitle.widgetTitle'
 * - If a widget with the name 'pageTitle.widgetTitle' already exists,
 *   the newly created one replaces the old one.
 */

/*********************************************************************
  Create a new separator with a label.
  page can be a pointer to a prevBuffer (-> new menu separator).
*********************************************************************/
void opts_separator_create (long page, const char *title);

/*********************************************************************
  Create new button widgets. On button click value is set to the
  button number starting at 1. title and ttip are '|'-separated for
  the single buttons, e.g. "title1|title2" for two buttons.
  page can be a pointer to a prevBuffer (-> new menu items).
*********************************************************************/
void opts_button_create (long page, const char *title, const char *ttip,
						 gint *value);

/*********************************************************************
  Create new button widgets. On button click cback is called with the
  button number starting at 1 and 'data'. title and ttip are
  '|'-separated for the single buttons, e.g. "title1|title2" for
  two buttons.
  page can be a pointer to a prevBuffer (-> new menu items).
*********************************************************************/
void opts_buttoncb_create (long page, const char *title, const char *ttip,
						   optsCbackFunc cback, void *data);

/*********************************************************************
  Create new string widget. Max allowed length of string is length.
*********************************************************************/
void opts_string_create (long page, const char *title, const char *ttip,
						 char *value, int length);

/*********************************************************************
  Create new string widget with an "enter" button. The string is
  updated only on enter press or button click.  Max allowed length of
  string is length.
*********************************************************************/
void opts_stringenter_create (long page, const char *title, const char *ttip,
							  char *value, int length);

/*********************************************************************
  Create new string widget with an attached button to open a file
  selection widget. Max allowed length of string is length.
*********************************************************************/
void opts_filesel_create (long page, const char *title, const char *ttip,
						  char *value, int length);

/*********************************************************************
  Create new integer scale widget with an attached entry widget.
*********************************************************************/
void opts_entscale_create (long page, const char *title, const char *ttip,
						   gint *value, gint left, gint right);

/*********************************************************************
  Create new scale widget with an attached entry widget (for floats).
*********************************************************************/
void opts_float_create (long page, const char *title, const char *ttip,
						gfloat *value, gfloat left, gfloat right);

/*********************************************************************
  Create new toggle widget.
  page can be a pointer to a prevBuffer (-> new check menu item).
*********************************************************************/
void opts_toggle_create (long page, const char *title, const char *ttip,
						 gboolean *value);

/*********************************************************************
  Create new option-menu widget with selectable values from label.
  The const array label must be terminated with NULL.
*********************************************************************/
void opts_option_create (long page, const char *title, const char *ttip,
						 char **label, gint *value);

/*********************************************************************
  Create new radio widget with selectable values from label.
  The const array label must be terminated with NULL.
  page can be a pointer to a prevBuffer (-> new radio menu item).
*********************************************************************/
void opts_radio_create (long page, const char *title, const char *ttip,
						char **label, gint *value);

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
					   char **label, optsListFlags flags, optsListData *value);

/*********************************************************************
  fkt!=NULL: Get settings for widgets and display them by periodically
			 calling fkt(data,buffer,buf_len).
  fkt==NULL: Load settings of all widgets from file data (a file name
             of type char*) and display them.
*********************************************************************/
void opts_load (optsLoadFunc fkt, void *data);

/*********************************************************************
  If opts_save() is called, do not save the settings for the widget
  referenced by title.
*********************************************************************/
void opts_save_remove (const char *title);

/*********************************************************************
  fkt!=NULL: Save settings of all widgets by periodically calling
             fkt(data,string) for all to be saved strings.
  fkt==NULL: Save settings of all widgets to file data (a file name
             of type char*).
  Return: TRUE if no error occurred.
*********************************************************************/
gboolean opts_save (optsSaveFunc fkt, void *data);

/*********************************************************************
  Append a new page to the options notebook. If title contains '.' or
  ' ' (a'.'b or a' 'b) the page is shown in the categories list under
  node a with entry b.
  Return: the number of the new page
*********************************************************************/
int opts_page_append (const char *title);

#ifdef __cplusplus
}
#endif

#endif /* iw_Goptions_H */
