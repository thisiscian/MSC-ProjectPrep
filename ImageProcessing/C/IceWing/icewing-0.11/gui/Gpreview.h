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

#ifndef iw_Gpreview_H
#define iw_Gpreview_H

#include <stdio.h>
#include <gtk/gtk.h>
#include "Gcolor.h"
#include "Gimage.h"
#include "Gdata.h"

/* Identifies data for rendering, e.g.
   in prev_render_data(), prev_render_list(), and prev_render_set() */
typedef enum {			/* Types containing the data: */
	PREV_IMAGE,			/*   prevDataImage */
	PREV_TEXT,			/*   prevDataText */
	PREV_LINE,			/*   prevDataLine */
	PREV_RECT,			/*   prevDataRect */
	PREV_FRECT,			/*   prevDataRect (filled rectangle) */
	PREV_REGION,		/*   iwRegion */
	PREV_POLYGON,		/*   prevDataPoly */
	PREV_FPOLYGON,		/*   prevDataPoly (filled polygon) */
	PREV_CIRCLE,		/*   prevDataCircle */
	PREV_FCIRCLE,		/*   prevDataCircle (filled circle) */
	PREV_ELLIPSE,		/*   prevDataEllipse */
	PREV_COMINFO,		/*   iwRegCOMinfo */
	PREV_LINE_F,		/*   prevDataLineF */
	PREV_RECT_F,		/*   prevDataRectF */
	PREV_FRECT_F,		/*   prevDataRectF (filled rectangle) */
	PREV_POLYGON_F,		/*   prevDataPolyF */
	PREV_FPOLYGON_F,	/*   prevDataPolyF (filled polygon) */
	PREV_CIRCLE_F,		/*   prevDataCircleF */
	PREV_FCIRCLE_F,		/*   prevDataCircleF (filled circle) */
	PREV_ELLIPSE_F,		/*   prevDataEllipseF */
	PREV_NEW = 100		/* For prev_renderdb_register() to add a new type,
						   (100 to have some free values before to add new PREV_...) */
} prevType;

/* User event in a preview window,
   see prev_signal_connect() and prev_signal_connect2() */
typedef enum {
	PREV_BUTTON_PRESS		= 1 << 0,
	PREV_BUTTON_RELEASE		= 1 << 1,
	PREV_BUTTON_MOTION		= 1 << 2,
	PREV_KEY_PRESS			= 1 << 3,
	PREV_KEY_RELEASE		= 1 << 4
} prevEvent;

typedef enum {
	SAVE_NONE,
	SAVE_RENDER,
	SAVE_ORIG,
	SAVE_SEQ,
	SAVE_ORIGSEQ
} prevSave;

typedef struct _prevSettings prevSettings;
typedef struct _prevBuffer prevBuffer;

typedef struct prevEventAny {
	prevEvent type;
} prevEventAny;
typedef struct prevEventButton {
	prevEvent type;
	guint32 time;				/* Time this event happend */
	guint state;				/* State of modifier keys (GDK_SHIFT_MASK, ...) */
	int x, y;					/* Coordinates without zooming and panning */
	int x_orig, y_orig;			/* Original screen coordinates */
	guint button;				/* Number of pressed button */
} prevEventButton;
typedef struct prevEventMotion {
	prevEvent type;
	guint32 time;				/* Time this event happend */
	guint state;				/* State of modifier keys (GDK_SHIFT_MASK, ...) */
	int x, y;					/* Coordinates without zooming and panning */
	int x_orig, y_orig;			/* Original screen coordinates */
	guint button;				/* Number of pressed button */
} prevEventMotion;
typedef struct prevEventKey{
	prevEvent type;
	guint32 time;				/* Time this event happend */
	guint state;				/* State of modifier keys (GDK_SHIFT_MASK, ...) */
	guint keyval;				/* Pressed key (GDK_A, ..., GDK_Shift_L, ...) */
	int length;					/* Length of string */
	char *string;				/* Text result from keyval */
} prevEventKey;
typedef union prevEventData {
	prevEventAny any;
	prevEventButton button;
	prevEventMotion motion;
	prevEventKey key;
} prevEventData;

typedef void (*prevButtonFunc) (prevBuffer *b, prevEvent signal, int x, int y, void *data);
typedef void (*prevSignalFunc) (prevBuffer *b, prevEventData *event, void *data);

/* PRIVATE */
typedef struct {
	int width, height;			/* Size of largest rendering since last window clear */
	guchar bg_r, bg_g, bg_b;	/* Color for background clearing */
	int thickness;
} prevGC;

/* A window which can be used to render/visualize any results.
   Created by prev_new_window(). */
struct _prevBuffer {
	guchar *buffer;				/* Buffer for the to be displayed image */
	int width, height;			/* Size of buffer */
	gboolean gray;				/* Color or Grayscale */
	char *title;				/* Title of the window */

	int x, y;					/* Left upper edge of part displayed in the window */
	float zoom;					/* Zoom factor, if 0 fit to window is used */

	GtkWidget *window;			/* Window for the to be displayed buffer,
								   if !=NULL the preview window is open */
	/* PRIVATE */
	gboolean save_rdata;		/* Save original render data? */
	GArray *rdata;				/* Saved render data */

	GtkWidget *drawing;			/* Destination for gdk_draw_rgb_image() */
	int draw_width, draw_height;/* Size of drawable */
	GdkGC *g_gc;				/* gc for gdk_draw_rgb_image() */

	GHashTable *opts;			/* Options data for this window, see prev_opts_xxx() */

	GtkWidget *menu;			/* Context menu */
	GtkItemFactory *factory;	/* The item factory for the context menu */
	GtkAccelGroup *accel;		/* The window accelerator group */
	prevSettings *settings;		/* Data for the settings window */

	GSList *cback;				/* List of callbacks for this window */

	prevGC gc;					/* Render options */

	guint expose_id;			/* ID of the expose idle-handler */
	prevSave save;				/* !=SAVE_NONE -> prev_render() saves the image */
	GtkWidget *save_widget;		/* Pointer to the active menu entry (for SAVE SEQ) */
	iwMovie *save_avifile;
	gboolean save_done;			/* Set in prev_render() if save!=SAVE_NONE, */
								/* reseted in prev_draw_buffer() */
	int save_cnt;				/* How many times the buffer was saved? */

	prevBuffer *next;
};

typedef struct {
	char *fname;				/* Destination file name */
	FILE *file;					/* Destination file */
	float zoom;
	guiData rdata;				/* Entry for the renderer to store persistent data */
	iwImgFormat format;
} prevVectorSave;

typedef void (*renderDbOptsFunc) (prevBuffer *b);
typedef void (*renderDbFreeFunc) (void *data);
typedef void* (*renderDbCopyFunc) (const void *data);
typedef void (*renderDbRenderFunc) (prevBuffer *b, const void *data, int disp_mode);
typedef char* (*renderDbInfoFunc) (prevBuffer *b, const void *data, int disp_mode, int x, int y, int radius);
typedef iwImgStatus (*renderDbVectorFunc) (prevBuffer *b, prevVectorSave *save,
										   const void *data, int disp_mode);

/* prev_set_render_size(prevBuffer *b, int width, int height)
		Update render_{width|height} in buffer b. */
#define prev_set_render_size(b,width,height)	G_STMT_START {	\
	if ((width) >= 0) (b)->gc.width = (width);									\
	if ((height) >= 0) (b)->gc.height = (height);									\
} G_STMT_END

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Must be called on access of a prevBuffer->buffer.
  Attention: gdk-/X11-mutex must not be locked before locking this
             mutex.
*********************************************************************/
void prev_buffer_lock (void);
void prev_buffer_unlock (void);

/*********************************************************************
  Register a new render type 'type'.
  If 'type' == PREV_NEW use the first unused value and return it.
*********************************************************************/
prevType prev_renderdb_register (prevType type, renderDbOptsFunc o_fkt,
								 renderDbFreeFunc f_fkt, renderDbCopyFunc c_fkt,
								 renderDbRenderFunc r_fkt);

/*********************************************************************
  Register a function which is called for the "Info Window".
*********************************************************************/
void prev_renderdb_register_info (prevType type, renderDbInfoFunc i_fkt);

/*********************************************************************
  Register a function for saving in a vector format.
*********************************************************************/
void prev_renderdb_register_vector (prevType type, renderDbVectorFunc v_fkt);

/*********************************************************************
  Store a value associated to type and return its address.
  free_data==TRUE: If the window b is freed, data is freed by calling
				   free().
  Used in renderDbOptsFunc() to store the value of the menu entry.
*********************************************************************/
void** prev_opts_store (prevBuffer *b, prevType type,
						void *data, gboolean free_data);
/*********************************************************************
  Get the value associated to type stored with prev_opts_store().
*********************************************************************/
void** prev_opts_get (prevBuffer *b, prevType type);
/*********************************************************************
  Append option widgets to the window specific menu according to
  the given types. Last argument must be '-1'.
*********************************************************************/
void prev_opts_append (prevBuffer *b, prevType type, ...);

/*********************************************************************
  Return zoom factor to get something of size
  b->gc.width x b->gc.height into buffer b.
*********************************************************************/
float prev_get_zoom (prevBuffer *b);

/*********************************************************************
  If b->save!=SAVE_NONE:
      img!=NULL: Save image img.
      img==NULL: Save image b->buffer (size: b->width x b->height).
  Attention: prev_buffer_(un)lock is called.
*********************************************************************/
void prev_chk_save (prevBuffer *b, const iwImage *img);

/*********************************************************************
  If b->window is open, display b->buffer.
*********************************************************************/
void prev_draw_buffer (prevBuffer *b);

/*********************************************************************
  If zoom >= 0, change zoom level of the preview.
  If x >= 0 or y >= 0 pan the preview to that position.
*********************************************************************/
void prev_pan_zoom (prevBuffer *b, int x, int y, float zoom);

/*********************************************************************
  Call cback with data as last argument if one of the signals
  specified with sigset occured.
*********************************************************************/
void prev_signal_connect (prevBuffer *b, prevEvent sigset,
						  prevButtonFunc cback, void *data);
void prev_signal_connect2 (prevBuffer *b, prevEvent sigset,
						   prevSignalFunc cback, void *data);

/*********************************************************************
  Return the page number for the preview settings window. Allows to
  create any widgets with the help of the opts_() functions.
*********************************************************************/
int prev_get_page (prevBuffer *b);

/*********************************************************************
  Close preview window b (if necessary), remove it from the CList and
  free its memory.
*********************************************************************/
void prev_free_window (prevBuffer *b);

/*********************************************************************
  Initialise a new preview window (Title: title, size: width x height,
  depth: 24bit (gray==FALSE) or 8 Bit (gray==TRUE)).
  show == TRUE: The window is shown immediately.
  width, heigth < 0 : Default width, height is used.
  title contains '.' (a'.'b): Window is shown in the list of windows
                              under node a with entry b.
*********************************************************************/
prevBuffer *prev_new_window (const char *title, int width, int height,
							 gboolean gray, gboolean show);

/*********************************************************************
  Copy the content of a preview window into an iceWing image. The
  preview window needs to be open (buffer->window != NULL), otherwise
  NULL is returned. If save_full the full visible preview window size
  is used, otherwise the size of the displayed data is used.
*********************************************************************/
iwImage *prev_copy_to_image (prevBuffer *b, gboolean save_full);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gpreview_H */
