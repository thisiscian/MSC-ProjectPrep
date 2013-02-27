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

#ifndef iw_GrenderText_H
#define iw_GrenderText_H

#include "Gpreview.h"
#include "Grender.h"

typedef struct textData {
	prevBuffer *b;
	float zoom;
	int dispMode;
	prevVectorSave *save;
} textData;

typedef struct textFont {
	int width;			/* Width of a char */
	int spacing;		/* Line spacing */
	float size;			/* Font size */
	int baseline;
	const unsigned char *data;
	int rowstride;		/* Width of a row in bytes */
} textFont;

/* Options for rendering text */
typedef struct textContext {
	int r, g, b;
	int bg_r, bg_g, bg_b;
	gboolean shadow;
	textFont *font;
	short adjust;
	struct textContext *next;
} textContext;

typedef struct {
	int font;
	gboolean shadow;
} textOpts;

extern textFont iw_fonts[3];

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Create and return a new text context as a copy of the last one.
*********************************************************************/
textContext* prev_text_context_push (textContext *top);

/*********************************************************************
  Show string str in b at position (xstart, ystart) with render
  options from tc.
*********************************************************************/
void prev_drawString_do (textData *data, int xstart, int ystart,
						 const char *str, textContext *tc);

/*********************************************************************
  Initialize the user interface for text rendering.
*********************************************************************/
void prev_render_text_opts (prevBuffer *b);

/*********************************************************************
  Free the text 'data'.
*********************************************************************/
void prev_render_text_free (void *data);

/*********************************************************************
  Copy the text 'data' and return it.
*********************************************************************/
void* prev_render_text_copy (const void *data);

/*********************************************************************
  Display text in buffer b.
*********************************************************************/
void prev_render_text_do (prevBuffer *b, const prevDataText *text,
						  int disp_mode);

/*********************************************************************
  Render the text text according the data in save.
*********************************************************************/
iwImgStatus prev_render_text_vector (prevBuffer *b, prevVectorSave *save,
									 prevDataText *text, int disp_mode);

#ifdef __cplusplus
}
#endif

#endif /* iw_GrenderText_H */
