/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/* PRIVATE HEADER */

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

#ifndef iw_Goptions_i_H
#define iw_Goptions_i_H

#include "Goptions.h"
#include "Gpreview.h"

typedef enum {
	WID_SEPARATOR,
	WID_BUTTON,
	WID_STRING,
	WID_FILESEL,
	WID_ENTSCALE,
	WID_FLOAT,
	WID_TOGGLE,
	WID_OPTION,
	WID_LIST,
	WID_RADIO,
	WID_VAR_BOOL,
	WID_VAR_INT,
	WID_VAR_LONG,
	WID_VAR_FLOAT,
	WID_VAR_DOUBLE,
	WID_VAR_STRING
} optsWidType;

#define WID_VAR_START	WID_VAR_BOOL

typedef struct optsValue {
	optsWidget w;
	optsWidType wtype;			/* Type of the widget */
	GSList *cback;				/* List of callbacks for this widget */
	GtkWidget *top_widget, *set_widget;
	/* Function to set the value and change the displayed value */
	long (*set_value) (struct optsValue *opts, void *value);
	void (*free_value) (struct optsValue *opts);
	struct optsValue *next;
	void *data;					/* Widget specific data */
} optsValue;

typedef long (*setValueFkt) (struct optsValue *opts, void *value);
typedef void (*freeValueFkt) (struct optsValue *opts);

typedef struct optsPage {
	prevBuffer *buffer;
	GtkWidget *widget;			/* If Page: Top page widget */
	int page;					/* If Page: page index, If Buffer: -1 */
	char *title;				/* Title of the page/buffer */
} optsPage;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Call all widget cbacks which have registered for signal.
*********************************************************************/
void opts_signal (optsValue *opts, optsSignal signal, void *newValue);

/*********************************************************************
  Remove the page/buffer "title" from the opts_pages array.
*********************************************************************/
void opts_page_remove__nl (char *title);

/*********************************************************************
  Add a page/buffer to the opts_pages array.
*********************************************************************/
int opts_page_add__nl (GtkWidget *widget, prevBuffer *buffer, const char *title);

/*********************************************************************
  Return a pointer to the page belonging to 'page'.
*********************************************************************/
optsPage* opts_page_get__nl (long page);

optsValue* opts_value_add1__nl (optsPage *p, optsWidType wtype, const char *wtitle,
								setValueFkt setFkt, void *value, void *data);
void opts_value_add2__nl (optsValue *opts, optsPage *p, int index,
						  GtkWidget *top_widget, GtkWidget *set_widget);

/*********************************************************************
  Check if widget title on page page (if -1 title must be full
  qualified) already exists, remove the widget, and return
    a) the widget name without the page part in title
    b) a pointer to the page
    c) the index of the widget in the page, or -1
*********************************************************************/
void opts_create_prepare (long page, const char **title, optsPage **opage, int *index);

/*********************************************************************
  Remove widget with title 'title' from options tab.
  Use 'pageTitle.widgetTitle' for the title argument.
  Return: Did the widget exist?
*********************************************************************/
gboolean opts_widget_remove__nl (const char *title);

/*********************************************************************
  Load settings of all widgets from the default file and from file
  and display them.
*********************************************************************/
void opts_load_default (char **file);

/*********************************************************************
  PRIVATE: GtkSignalFunc() callback to load the default settings
  file / a settings file via a file selector.
*********************************************************************/
void opts_load_cb (GtkWidget *widget, gpointer def_file);

/*********************************************************************
  PRIVATE: GtkSignalFunc() callback to save to the default settings
  file / to a settings file via a file selector.
*********************************************************************/
void opts_save_cb (GtkWidget *widget, gpointer def_file);

/*********************************************************************
  Initialise the options interface.
*********************************************************************/
void opts_init (GtkWidget *parent);

#ifdef __cplusplus
}
#endif

#endif /* iw_Goptions_i_H */
