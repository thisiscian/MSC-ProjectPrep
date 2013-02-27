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

#ifndef iw_grab_prop_H
#define iw_grab_prop_H

#include "tools/tools.h"

/* Ident of the function for setting grabbing properties */
#define IW_GRAB_IDENT_PROPERTIES	"setProperties"

/* Possible values for the iwGrabPropertiesFunc() */
typedef enum {
	IW_GRAB_LAST = 0,	/* Must be given as last argument to finalize the argument list */
	IW_GRAB_SET_ROI,	/* Args: int x, int y, int width, int height
						   Set ROI. If a value is -1, the current value is not changed. */
	IW_GRAB_GET_PROP,	/* Args: iwGrabProperty **prop
						   Get an array of all properties, finished with (*prop)[last].name == NULL */
	IW_GRAB_GET_PROPVALUES,	/* Args: iwGrabProperty **prop
						   Get an array of all properties and reacquire there values.
						   The array is finished with (*prop)[last].name == NULL */
	IW_GRAB_SET_PROP	/* Args: char *propvals
						   Set properties to new values, argument format: 'propX=val:propY=val:...' */
} iwGrabControl;

/* Possible results for setting grabbing properties */
typedef enum {
	IW_GRAB_STATUS_OK			= 0,
	IW_GRAB_STATUS_UNSUPPORTED	= -1, /* Unsupported property */
	IW_GRAB_STATUS_ERR			= -2, /* Another error */
	IW_GRAB_STATUS_NOTOPEN		= -3, /* No grabbing device is open */
	IW_GRAB_STATUS_ARGUMENT		= -4, /* Invalid argument */
} iwGrabStatus;

/* Type of the property, which is stored in iwGrabProperty */
typedef enum {
	IW_GRAB_CATEGORY, /* Arranging the props in a tree structure.
						 A sub tree is finished if name starts with '<'. */
	IW_GRAB_COMMAND,
	IW_GRAB_BOOL,
	IW_GRAB_INT,
	IW_GRAB_DOUBLE,
	IW_GRAB_ENUM,
	IW_GRAB_STRING
} iwGrabPropertyType;

/* Characteristics of properties */
typedef enum {
	IW_GRAB_READONLY	/* Property is read only */
} iwGrabPropertyFlags;

typedef struct {
	char *name;
	gint64 priv;
} iwGrabPropertyEnum;

typedef struct {
	char *name;				/* Name of the property e.g. "Contrast", array finished with a NULL entry */
	char *description;		/* Optional help text */
	iwGrabPropertyFlags flags;
	iwGrabPropertyType type; /* Type of the property */
	union {
		struct {
			BOOL def;		/* Default value */
			BOOL value;
		} b;
		struct {
			gint64 min;
			gint64 max;
			gint64 def;		/* Default value */
			gint64 value;
		} i;
		struct {
			double min;
			double max;
			double def;		/* Default value */
			double value;
		} d;
		struct {
			iwGrabPropertyEnum *enums;	/* Possible values, finished with a NULL name */
			int max;		/* Max allowed value */
			int def;		/* Default value, index into enums */
			int value;		/* Index into enums */
		} e;
		struct {
			char *def;		/* Default value */
			char *value;
		} s;
	} data;
	gint64 priv;			/* Private driver data */
	void *reserved[4];		/* For future expansion */
} iwGrabProperty;

/* To prevent inclusion of "plugin.h" */
typedef struct plugDefinition _plugDefinition;
typedef iwGrabStatus (*iwGrabPropertiesFunc) (_plugDefinition *plug_d, ...);

#endif /* iw_grab_prop_H */
