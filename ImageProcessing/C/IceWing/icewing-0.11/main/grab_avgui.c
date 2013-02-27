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
#include <ctype.h>
#include <string.h>

#include "gui/Gtools.h"
#include "gui/Goptions.h"
#include "grab_prop.h"
#include "grab_avgui.h"

#define PAGE_NAME		".Properties"
#define STR_LEN_NUM	20
#define STR_LEN		200

typedef struct grabAvVals {
	iwGrabPropertyType type; /* Type of the property */
	iwGrabPropertyFlags flags;
	char *name;
	union {
		BOOL b;
		int i;
		float f;
		char *s;
	} data[2];
	int num;
	struct grabValues *parent;
} grabAvVals;

typedef struct grabValues {
	plugDataFunc *propFunc;
	int page;					/* Id of the option page */
	BOOL rescan;				/* Button 'Rescan properties' */
	grabAvVals *avVals;			/* Values for all properties */
	pthread_mutex_t changed;	/* Mutex for the next two vars */
	int minChanged, maxChanged; /* Min/max number of changed properties */
} grabValues;

/*********************************************************************
  Get a tool tip string in buffer for property prop.
*********************************************************************/
static char *get_ttip (iwGrabProperty *prop, char *buffer, int bufLen)
{
	char *pos = buffer;
	*pos = '\0';
	if (prop->flags & IW_GRAB_READONLY)
		strcpy (pos, "Readonly");
	if (prop->type == IW_GRAB_INT || prop->type == IW_GRAB_DOUBLE) {
		pos += strlen (pos);
		if (pos != buffer) {
			*pos++ = ',';
			*pos++ = ' ';
		}
		if (prop->type == IW_GRAB_INT)
			sprintf(pos, "Min: %"G_GINT64_FORMAT" Max: %"G_GINT64_FORMAT,
					(GINT64_PRINTTYPE)prop->data.i.min, (GINT64_PRINTTYPE)prop->data.i.max);
		else if (prop->type == IW_GRAB_DOUBLE)
			sprintf(pos, "Min: %g Max: %g", prop->data.d.min, prop->data.d.max);
	}
	if (prop->description) {
		pos += strlen(pos);
		if (pos != buffer) *pos++ = '\n';
		gui_strlcpy (pos, prop->description, bufLen-(pos-buffer));
	}
	if (*buffer)
		return buffer;
	return NULL;
}

/*********************************************************************
  Called when one of the property widgets changed.
*********************************************************************/
static void cb_widget_changed (optsWidget *widget, void *newValue, void *data)
{
	grabAvVals *avVal = data;
	grabValues *vals = avVal->parent;

	pthread_mutex_lock (&vals->changed);
	if (avVal->num < vals->minChanged) vals->minChanged = avVal->num;
	if (avVal->num > vals->maxChanged) vals->maxChanged = avVal->num;
	pthread_mutex_unlock (&vals->changed);
}

/*********************************************************************
  Create vals->avVals and corresponding widgets from props.
*********************************************************************/
static void grab_init_properties (grabValues *vals, iwGrabProperty *props)
{
	char buffer[1024];
	int p = vals->page;
	int cnt, i;

	for (cnt=0; props[cnt].name; cnt++) /*empty*/;
	if (cnt <= 0) return;

	vals->avVals = calloc (1, sizeof(grabAvVals)*(cnt+1));
	vals->avVals[cnt].type = -2; /* Mark the last entry */

	for (i=0; props[i].name; i++) {
		iwGrabProperty *prop = &props[i];
		grabAvVals *avVal = &vals->avVals[i];

		avVal->type = -1;
		switch (prop->type) {
			case IW_GRAB_INT:
				if (prop->data.i.max < prop->data.i.min) continue;
				break;
			case IW_GRAB_DOUBLE:
				if (prop->data.d.max < prop->data.d.min) continue;
				break;
			case IW_GRAB_ENUM:
				if (!prop->data.e.enums || !prop->data.e.enums[0].name) continue;
				break;
			default: break;
		}

		if (prop->type != IW_GRAB_CATEGORY)
			avVal->name = strdup (prop->name);
		avVal->flags = prop->flags;
		avVal->type = prop->type;
		switch (prop->type) {
			case IW_GRAB_CATEGORY: {
				char *name;
				for (name = prop->name; *name; name++) {
					if (*name == '<') {
					} else if (!isspace((int)*name)) {
						avVal->name = strdup (name);
						opts_separator_create (p, name);
						break;
					}
				}
				break;
			}
			case IW_GRAB_COMMAND:
				opts_button_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)), &avVal->data[0].i);
				break;
			case IW_GRAB_BOOL:
				avVal->data[0].b = prop->data.b.value;
				opts_toggle_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)), &avVal->data[0].b);
				break;
			case IW_GRAB_INT: {
				gint64 min = prop->data.i.min;
				gint64 max = prop->data.i.max;
				if (min >= -100000 && max <= 100000 && max-min <= 100000) {
					avVal->data[0].i = prop->data.i.value;
					opts_entscale_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)),
										  &avVal->data[0].i, min, max);
				} else {
					avVal->data[0].s = malloc (STR_LEN_NUM);
					sprintf (avVal->data[0].s, "%"G_GINT64_FORMAT, (GINT64_PRINTTYPE)prop->data.i.value);
					opts_stringenter_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)),
											 avVal->data[0].s, STR_LEN_NUM);
					avVal->type += 100;
				}
				break;
			}
			case IW_GRAB_DOUBLE: {
				double min = prop->data.d.min;
				double max = prop->data.d.max;
				if (min >= -100000 && max <= 100000 && max-min <= 100000) {
					avVal->data[0].f = prop->data.d.value;
					opts_float_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)),
									   &avVal->data[0].f, min, max);
				} else {
					avVal->data[0].s = malloc (STR_LEN_NUM);
					sprintf (avVal->data[0].s, "%g", prop->data.d.value);
					opts_stringenter_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)),
											 avVal->data[0].s, STR_LEN_NUM);
					avVal->type += 100;
				}
				break;
			}
			case IW_GRAB_ENUM: {
				char *opts[prop->data.e.max+2];
				int e;
				for (e = 0; prop->data.e.enums[e].name; e++)
					opts[e] = prop->data.e.enums[e].name;
				opts[e] = NULL;
				avVal->data[0].i = prop->data.e.value;
				opts_option_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)), opts, &avVal->data[0].i);
				break;
			}
			case IW_GRAB_STRING: {
				avVal->data[0].s = malloc (STR_LEN);
				if (prop->data.s.value)
					gui_strlcpy (avVal->data[0].s, prop->data.s.value, STR_LEN);
				else
					avVal->data[0].s[0] = '\0';
				opts_stringenter_create (p, prop->name, get_ttip(prop, buffer, sizeof(buffer)), avVal->data[0].s, STR_LEN);
				break;
			}
		}
		if (prop->type != IW_GRAB_CATEGORY) {
			avVal->num = i;
			avVal->parent = vals;
			opts_signal_connect (p, prop->name, OPTS_SIG_CHANGED,
								 cb_widget_changed, avVal);
		}

		if (avVal->type == IW_GRAB_STRING || avVal->type > 100) {
			if (avVal->type == IW_GRAB_STRING)
				avVal->data[1].s = malloc(STR_LEN);
			else if (avVal->type > 100)
				avVal->data[1].s = malloc(STR_LEN_NUM);
			strcpy (avVal->data[1].s, avVal->data[0].s);
		} else
			avVal->data[1] = avVal->data[0];
	}
}

/*********************************************************************
  Create a GUI for setting the grabber properties.
*********************************************************************/
void grab_process_avgui (plugDefinition *plug_d, void **data)
{
	grabValues *vals = *data;
	iwGrabProperty *props = NULL;
	char buffer[1024], *pos;
	int i;

	if (!vals) {
		/* Init the interface */
		*data = vals = calloc (1, sizeof(grabValues));
		pthread_mutex_init (&vals->changed, NULL);
		vals->minChanged = G_MAXINT;
		vals->maxChanged = G_MININT;
		vals->propFunc = plug_function_get_full (IW_GRAB_IDENT_PROPERTIES,
												 NULL, plug_d->name);

		if (vals->propFunc)
			((iwGrabPropertiesFunc)vals->propFunc->func) (vals->propFunc->plug,
														  IW_GRAB_GET_PROP, &props, IW_GRAB_LAST);
		if (!props || !props[0].name) return;

		vals->page = opts_page_append (plug_name(plug_d, PAGE_NAME));
		opts_button_create (vals->page, "Rescan properties",
							"Rescan all properties and show there current values", &vals->rescan);
		grab_init_properties (vals, props);
	}
	if (vals->rescan) {
		/* Re-create the property widgets */
		char *namePos;

		strcpy (buffer, plug_d->name);
		strcat (buffer, PAGE_NAME);
		namePos = buffer+strlen(buffer);
		*namePos++ = '.';
		*namePos = '\0';

		if (vals->avVals) {
			for (i=0; vals->avVals[i].type != -2; i++) {
				grabAvVals *avVal = &vals->avVals[i];
				if (avVal->name) {
					gui_strlcpy (namePos, avVal->name, sizeof(buffer)-(namePos-buffer));
					opts_widget_remove (buffer);
					free (avVal->name);
				}
				if (avVal->type == IW_GRAB_STRING || avVal->type > 100) {
					free (avVal->data[0].s);
					free (avVal->data[1].s);
				}
			}
			free (vals->avVals);
			vals->avVals = NULL;
		}

		((iwGrabPropertiesFunc)vals->propFunc->func) (vals->propFunc->plug,
													  IW_GRAB_GET_PROPVALUES, &props, IW_GRAB_LAST);
		if (!props || !props[0].name) return;
		grab_init_properties (vals, props);

		vals->rescan = 0;
	}

	if (!vals->avVals || vals->maxChanged < 0) return;

	/* Check for changed widget values and set the corresponding properties */
	pthread_mutex_lock (&vals->changed);
	buffer[0] = '\0';
	pos = buffer;
	for (i=vals->minChanged;
		 i<=vals->maxChanged && vals->avVals[i].type != -2 && pos-buffer < sizeof(buffer)-40;
		 i++) {
		grabAvVals *avVal = &vals->avVals[i];

		if (avVal->flags & IW_GRAB_READONLY) continue;

		switch ((int)avVal->type) {
			case IW_GRAB_COMMAND:
				if (!avVal->data[0].i) continue;

				avVal->data[0].i = 0;
				sprintf (pos, ":prop%d", i);
				break;
			case IW_GRAB_BOOL:
				if (avVal->data[0].b == avVal->data[1].b) continue;

				avVal->data[1].b = avVal->data[0].b;
				sprintf (pos, ":prop%d=%d", i, avVal->data[1].b);
				break;
			case IW_GRAB_INT:
			case IW_GRAB_ENUM:
				if (avVal->data[0].i == avVal->data[1].i) continue;

				avVal->data[1].i = avVal->data[0].i;
				sprintf (pos, ":prop%d=%d", i, avVal->data[1].i);
				break;
			case IW_GRAB_DOUBLE:
				if (avVal->data[0].f == avVal->data[1].f) continue;

				avVal->data[1].f = avVal->data[0].f;
				sprintf (pos, ":prop%d=%g", i, avVal->data[1].f);
				break;
			case IW_GRAB_STRING:
			case IW_GRAB_INT+100:
			case IW_GRAB_DOUBLE+100:
				if (! strcmp(avVal->data[0].s, avVal->data[1].s)) continue;

				gui_strcpy_back (avVal->data[1].s, avVal->data[0].s,
								 avVal->type==IW_GRAB_STRING ? STR_LEN : STR_LEN_NUM);
				sprintf (pos, ":prop%d=", i);
				pos += strlen(pos);
				gui_strlcpy (pos, avVal->data[1].s, sizeof(buffer)-(pos-buffer));
				break;
			default:
				continue;
		}
		pos += strlen(pos);
	}
	vals->minChanged = G_MAXINT;
	vals->maxChanged = G_MININT;
	pthread_mutex_unlock (&vals->changed);
	if (buffer[0])
		((iwGrabPropertiesFunc)vals->propFunc->func) (vals->propFunc->plug, IW_GRAB_SET_PROP,
													  buffer+1, IW_GRAB_LAST);
}
