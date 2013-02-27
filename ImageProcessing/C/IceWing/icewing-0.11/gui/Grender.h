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

#ifndef iw_Grender_H
#define iw_Grender_H

#include "Gpreview.h"
#include "main/region.h"

#define FONT_SMALL			0x01	/* Font to use for displaying text */
#define FONT_MED			0x02	/* " */
#define FONT_BIG			0x03	/* " */
#define FONT_MASK			0x03	/* All bits of the fonts */
#define FONT_SHIFT			0x02	/* Number of bits used by the fonts */

#define REGION_NONE			(1<<FONT_SHIFT)		/* Display no text */
#define REGION_COM			(2<<FONT_SHIFT)		/* Display center of mass */
#define REGION_PCOUNT		(3<<FONT_SHIFT)		/* Display pixel count */
#define REGION_LENGTH		(4<<FONT_SHIFT)		/* Display length of polygon */
#define REGION_ECCENTRICITY	(5<<FONT_SHIFT)		/* Display eccentricity */
#define REGION_COMPACTNESS	(6<<FONT_SHIFT)		/* Display compactness */
#define REGION_AVGCONF		(7<<FONT_SHIFT)		/* Display avg. confidenceValue */
#define REGION_RATING		(8<<FONT_SHIFT)		/* Display rating of the region */
#define REGION_RAT_KALMAN	(9<<FONT_SHIFT)		/* Display kalman rating of the region */
#define REGION_RAT_MOTION	(10<<FONT_SHIFT)	/* Display motion rating of the region */
#define REGION_ID			(11<<FONT_SHIFT)	/* Display the ID */
#define REGION_MASK			0x3C				/* All bits above */

#define RENDER_XOR			(0x20000000)
#define RENDER_THICK		(0x40000000)	/* Use thickness for line oriented drawings */
#define RENDER_CLEAR		(0x80000000)	/* Clear the buffer and free all old render
											   data (IMPORTANT to prevent mem leaks) */

typedef struct {
	prevType type;
	void *data;
} prevData;

/* ctab   : Color table, supported are IW_YUV, IW_RGB, IW_BGR, IW_BW (only r is used),
			IW_INDEX and any other colortable (r is used as an index into the table)\n
   r, g, b: Color components,
			if -1 the corresponding channel is not changed for color images */
typedef struct {
	iwColtab ctab;
	int r,g,b;
	int x, y;		/* Position of anchor (default left upper edge) of displayed text */
	char *text;				/* The to be displayed text */
} prevDataText;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int x1, y1, x2, y2;		/* Start / end points */
} prevDataLine;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int x1, y1, x2, y2;		/* Upper Left / lower right points */
} prevDataRect;

typedef struct {
	int x,y;
} prevDataPoint;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int npts;				/* Number of points in next array */
	prevDataPoint *pts;		/* The points of the polygon */
} prevDataPoly;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int x, y;				/* Center point */
	int radius;				/* Radius of the circle */
} prevDataCircle;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int x, y;				/* Center of ellipse */
	int width, height;		/* Major / minor radius of ellipse */
	int angle;				/* Rotation angle in grad */
	int start, end;			/* Start / end angle in grad */
	gboolean filled;
} prevDataEllipse;

typedef struct {
	iwImage *i;
	int x, y;				/* Position where i should be displayed */
} prevDataImage;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	float x1, y1, x2, y2;	/* Start / end points */
} prevDataLineF;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	float x1, y1, x2, y2;	/* Upper Left / lower right points */
} prevDataRectF;

typedef struct {
	float x,y;
} prevDataPointF;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	int npts;				/* Number of points in next array */
	prevDataPointF *pts;	/* The points of the polygon */
} prevDataPolyF;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	float x, y;				/* Center point */
	float radius;			/* Radius of the circle */
} prevDataCircleF;

typedef struct {
	iwColtab ctab;			/* See prevDataText */
	int r,g,b;
	float x, y;				/* Center of ellipse */
	float width, height;	/* Major / minor radius of ellipse */
	int angle;				/* Rotation angle in grad */
	int start, end;			/* Start / end angle in grad */
	gboolean filled;
} prevDataEllipseF;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Set color and dest buffer for the primitive drawing functions
  prev_drawPoint() to prev_drawString(). If a color component is set
  to -1, the corresponding channel is not changed.
  prev_drawInitFull(): Return zoom factor.
*********************************************************************/
void prev_drawInit (prevBuffer *buf, gint r, gint g, gint b);
float prev_drawInitFull (prevBuffer *buf, gint r, gint g, gint b, gint disp_mode);

/*********************************************************************
  PRIVATE: Convert from ctab to the rgb color space.
*********************************************************************/
void prev_colConvert (iwColtab ctab, gint r, gint g, gint b,
					  gint *rd, gint *gd, gint *bd);

/*********************************************************************
  Calls prev_colConvert() and returns a pointer to a static string
  '#rrggbb'.
*********************************************************************/
char *prev_colString (iwColtab ctab, gint r, gint g, gint b);

/*********************************************************************
  Set color and buffer for following drawing primitives.
  Calls prev_drawInitFull() and returns zoom factor.
*********************************************************************/
float prev_colInit (prevBuffer *buf, int disp_mode, iwColtab model,
					gint r, gint g, gint b, gint colindex);

/*********************************************************************
  Draw a point/filled rectangle/line/polygon/filled polygon/
  convex filled polygon/circle/filled cirecle/ellipse in the
  buffer set with drawInit().
  .._nc()           : No clipping (-> no check of the coordinates).
  .._gray|color_..(): Assume buffer is gray|color.
  ATTENTION: Low-Level functions without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawPoint (int x, int y);
void prev_drawPoint_gray_nc (int x, int y);
void prev_drawPoint_gray_nc_xor (int x, int y);
void prev_drawPoint_color_nc (int x, int y);
void prev_drawPoint_color_nc_xor (int x, int y);
void prev_drawPoint_color_nc_nm (int x, int y);

void prev_drawFRect (int x, int y, int width, int height);
void prev_drawFRect_gray_nc (int x, int y, int width, int height);
void prev_drawFRect_color_nc (int x, int y, int width, int height);

void prev_drawLine (int x, int y, int X, int Y);
void prev_drawPoly (int npts, const prevDataPoint *pts);
void prev_drawFPoly (int npts, const prevDataPoint *pts);
void prev_drawConvexPoly (int npts, const prevDataPoint *pts);
void prev_drawCircle (int x, int y, int radius);
void prev_drawFCircle (int x, int y, int radius);
void prev_drawEllipse (int x, int y, int width, int height,
					   int angle, int arc_start, int arc_end, gboolean filled);

/*********************************************************************
  Draw a text at pos (x,y) with font given with prev_drawInitFull() or
  via the menu appended with prev_opts_append().
  Differnt in-string formating options and '\n','\t' are supported:
  Format: <tag1=value tag2="value" tag3> with
  tag    value                                               result
  <                                                    '<' - string
  anchor tl|tr|br|bl|c     (x,y) at topleft,..., center pos of text
  fg     [rgb:|yuv:|bgr:|bw:|index:]r g b           forground color
  bg     [rgb:|yuv:|bgr:|bw:|index:]r g b          background color
  font   small|med|big                                  font to use
  adjust left|center|right                          text adjustment
  rotate 0|90|180|270   rotate text beginning with the current line
  shadow 1|0                                          shadow on/off
  zoom   1|0                                       zoom text on/off
  /                                 return to values before last <>
  ATTENTION: Low-Level function without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawString (int x, int y, const char *str);

/*********************************************************************
  PRIVATE: Fill gc struct with default values.
*********************************************************************/
void prev_gc_init (prevGC *gc);

/*********************************************************************
  Fill b with the bg color.
  ATTENTION: Low-Level function without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawBackground (prevBuffer *b);

/*********************************************************************
  Set color which is used for clearing the buffer (RENDER_CLEAR).
*********************************************************************/
void prev_set_bg_color (prevBuffer *buf, uchar r, uchar g, uchar b);
/*********************************************************************
  Set thickness for line oriented drawings.
*********************************************************************/
void prev_set_thickness (prevBuffer *buf, int thickness);

/*********************************************************************
  Some random things which must be done before drawing
  (is already done for all prev_render_...()):
    - lock the buffer
    - if RENDER_CLEAR is set:
        - clear the buffer
        - free all old render data
        - store the render size (width,height) in b->gc (if >= 0)
  Returns TRUE if anything should be done (-> if the window is open).
*********************************************************************/
gboolean prev_prepare_draw (prevBuffer *b, int disp_mode, int width, int height);

/*********************************************************************
  Draw the data (
	- one element of type type
	- a list, length: cnt, size of one element: size, type:type
	- a set of different elements with cnt elements)
  in buffer b, such that an area of size width x height fits into the
  buffer. If width/height < 0 or RENDER_CLEAR is not set, the last
  values for this buffer b are used. Mode of display disp_mode
  defined by flags in header file. If no flags are set and option
  widgets are appended to the window, these widgets are used.
*********************************************************************/
void prev_render_data (prevBuffer *b, prevType type, const void *data,
					   int disp_mode, int width, int height);
void prev_render_list (prevBuffer *b, prevType type, const void *data,
					   int size, int cnt,
					   int disp_mode, int width, int height);
void prev_render_set (prevBuffer *b, const prevData *data, int cnt,
					  int disp_mode, int width, int height);

/*********************************************************************
  Wrapper around prev_render_list().
  _texts(): Formating options of prev_drawString() are supported.
*********************************************************************/
void prev_render_texts (prevBuffer *b, const prevDataText *texts, int cnt,
						int disp_mode, int width, int height);
void prev_render_lines (prevBuffer *b, const prevDataLine *lines, int cnt,
						int disp_mode, int width, int height);
void prev_render_rects (prevBuffer *b, const prevDataRect *rects, int cnt,
						int disp_mode, int width, int height);
void prev_render_frects (prevBuffer *b, const prevDataRect *frects, int cnt,
						 int disp_mode, int width, int height);
void prev_render_polys (prevBuffer *b, const prevDataPoly *polys, int cnt,
						int disp_mode, int width, int height);
void prev_render_fpolys (prevBuffer *b, const prevDataPoly *polys, int cnt,
						 int disp_mode, int width, int height);
void prev_render_circles (prevBuffer *b, const prevDataCircle *circ, int cnt,
						  int disp_mode, int width, int height);
void prev_render_fcircles (prevBuffer *b, const prevDataCircle *circ, int cnt,
						   int disp_mode, int width, int height);
void prev_render_ellipses (prevBuffer *b, const prevDataEllipse *ellipse, int cnt,
						   int disp_mode, int width, int height);
void prev_render_regions (prevBuffer *b, const iwRegion *regions, int cnt,
						  int disp_mode, int width, int height);
void prev_render_COMinfos (prevBuffer *b, const iwRegCOMinfo *regions, int cnt,
						   int disp_mode, int width, int height);
void prev_render_imgs (prevBuffer *b, const prevDataImage *imgs, int cnt,
					   int disp_mode, int width, int height);

/*********************************************************************
  Render planes of size width x height and color mode/table ctab
  (IW_YUV, IW_RGB, ..., or an own table) in preview window b.
*********************************************************************/
void prev_render (prevBuffer *b, guchar **planes, int width, int height, iwColtab ctab);

/*********************************************************************
  Render the img src (size: w x h) in buffer b by using the function
  dest = src * col_step % 256.
  If col_step > 255 it is interpreted as a color table for rendering
  the dest image (and a col_Step of 1 is used).
*********************************************************************/
void prev_render_int (prevBuffer *b, const gint32 *src, int w, int h, long col_step);

/*********************************************************************
  Render str in b at position (x,y). Printf style arguments and the
  formating options of prev_drawString() are supported.
*********************************************************************/
void prev_render_text (prevBuffer *b, int disp_mode, int width, int height,
					   int x, int y, const char *format, ...) G_GNUC_PRINTF(7, 8);

/*********************************************************************
  PRIVATE: Called from prev_init() to initialise the render functions.
*********************************************************************/
void prev_render_init (void);

#ifdef __cplusplus
}
#endif

#endif /* iw_Grender_H */
