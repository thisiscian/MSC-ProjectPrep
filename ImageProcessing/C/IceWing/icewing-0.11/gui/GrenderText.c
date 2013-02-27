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
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "Grender.h"
#include "Goptions.h"
#include "Gtools.h"
#include "GrenderText.h"

static int rotate_x[][2] = {{1,0},{0,1},{-1,0},{0,-1}};
static int rotate_y[][2] = {{0,1},{1,0},{0,-1},{-1,0}};

/* Font 'Bitstream Vera Sans Mono' used for SVG export */
static textFont vector_fonts[3] = {
	/* width spacing size baseline data rowstride */
	{ 6, 11, 9.953,  9, NULL, 0},
	{ 8, 15, 13.27, 12, NULL, 0},
	{12, 25, 19.92, 18, NULL, 0}};

#define ATT_MAXLEN		20
#define VALUE_MAXLEN	100

typedef enum {
	ATT_CLOSE,
	ATT_TEXT,
	ATT_ANCHOR,
	ATT_FG,
	ATT_BG,
	ATT_FONT,
	ATT_ROTATE,
	ATT_ZOOM,
	ATT_ADJUST,
	ATT_SHADOW
} textAttTag;

typedef enum {	/* Return values */
	ATT_BOOL,	/* int   : 0 | 1 */
	ATT_INT,	/* int   : value */
	ATT_STRING,	/* string: "value" */
	ATT_LABEL,	/* int   : 0 ... */
	ATT_COLOR	/* int   : r g b */
} textAttType;

typedef enum {
	ATT_TL,
	ATT_TR,
	ATT_BR,
	ATT_BL,
	ATT_C
} textAttAnchor;

typedef enum {
	ATT_LEFT,
	ATT_CENTER,
	ATT_RIGHT
} textAttAdjust;

/*********************************************************************
  Parse one attribute/value pair from the string pos and
  return if any more may follow.
*********************************************************************/
static gboolean attribute_parse (const char **pos, textAttTag *ratt, char *rvalue)
{
	static struct {
		char *att;
		textAttType type;
		char *data;
		textAttTag tag;
	} textAtt[] = {
		{"<", ATT_STRING, "<", ATT_TEXT},
		{"ANCHOR", ATT_LABEL, "TL TR BR BL C ", ATT_ANCHOR},
		{"FG", ATT_COLOR, NULL, ATT_FG},
		{"BG", ATT_COLOR, NULL, ATT_BG},
		{"FONT", ATT_LABEL, "SMALL MED BIG ", ATT_FONT},
		{"ROTATE", ATT_LABEL, "0 90 180 270", ATT_ROTATE},
		{"ZOOM", ATT_BOOL, "0", ATT_ZOOM},
		{"ADJUST", ATT_LABEL, "LEFT CENTER RIGHT ", ATT_ADJUST},
		{"SHADOW", ATT_BOOL, "1", ATT_SHADOW},
		{NULL, ATT_BOOL, NULL, ATT_ANCHOR}
	};
	int i;
	char delim = 0;
	char att[ATT_MAXLEN+1], value[VALUE_MAXLEN+1], *val;

	/* Get attribute from *pos (att '=' ['"'|"'"] value ['"'|"'"]) */
	while (isspace((int)**pos)) (*pos)++;
	if (**pos == '/') {
		(*pos)++;
		*ratt = ATT_CLOSE;
		return TRUE;
	}
	i = 0;
	while (isalpha((int)**pos) || **pos == '<') {
		if (i < ATT_MAXLEN)
			att[i++] = **pos;
		(*pos)++;
	}
	att[i] = '\0';

	/* Get value from *pos */
	while (isspace((int)**pos)) (*pos)++;
	i = 0;
	if (**pos == '=') {
		(*pos)++;
		while (isspace((int)**pos)) (*pos)++;
		if (**pos == '"' || **pos == '\'') {
			delim = **pos;
			(*pos)++;
		}
		while (**pos && ( (delim && **pos != delim) ||
						  (!delim && !isspace((int)**pos) && **pos != '>') ) ) {
			if (i < VALUE_MAXLEN)
				value[i++] = **pos;
			(*pos)++;
		}
		if (delim && **pos)
			(*pos)++;
	}
	value[i] = '\0';

	if (!**pos || !*att) return FALSE;

	/* Parse value according to the attribute */
	for (i=0; textAtt[i].att; i++) {
		if (!g_strcasecmp (att, textAtt[i].att)) {
			*ratt = textAtt[i].tag;
			switch (textAtt[i].type) {
				case ATT_BOOL:
					if (*value)
						val = value;
					else
						val = textAtt[i].data;
					if (*val == '1' ||
						!g_strcasecmp (val, "YES") ||
						!g_strcasecmp (val, "ON"))
						*(int*)rvalue = 1;
					else
						*(int*)rvalue = 0;
					break;
				case ATT_INT:
					*(int*)rvalue = 0;
					if (*value)
						val = value;
					else
						val = textAtt[i].data;
					if (val && *val) {
						char *end = NULL;
						int int_arg = strtol (val, &end, 10);
						if (end && !*end)
							*(int*)rvalue = int_arg;
					}
					break;
				case ATT_STRING:
					if (*value)
						val = value;
					else if (textAtt[i].data)
						val = textAtt[i].data;
					else
						val = NULL;
					if (val && *val)
						strncpy (rvalue, val, VALUE_MAXLEN);
					else
						*rvalue = '\0';
					break;
				case ATT_LABEL:
					val = textAtt[i].data;
					*(int*)rvalue = 0;
					if (*value && val) {
						int label = 0;
						while (*val) {
							i = 0;
							while (val[i] && val[i] != ' ') {
								if (toupper((int)val[i]) != toupper((int)value[i]))
									break;
								i++;
							}
							if (!value[i] && (!val[i] || val[i] == ' ')) {
								*(int*)rvalue = label;
								break;
							} else {
								val += i;
								while (*val && *val != ' ')
									val++;
								while (*val == ' ')
									val++;
							}
							label++;
						}
					}
					break;
				case ATT_COLOR:
					*(int*)rvalue = 0;
					*((int*)rvalue+1) = 0;
					*((int*)rvalue+2) = 0;
					if (*value) {
						iwColtab ctab = IW_RGB;
						gint r, g, b;

						i = 0;
						if (isalpha((int)*value)) {
							if (!g_strncasecmp (value, "RGB", 3)) {
								ctab = IW_RGB;
							} else if (!g_strncasecmp (value, "YUV", 3)) {
								ctab = IW_YUV;
							} else if (!g_strncasecmp (value, "BGR", 3)) {
								ctab = IW_BGR;
							} else if (!g_strncasecmp (value, "BW", 2)
									   || !g_strncasecmp (value, "GRAY", 4)) {
								ctab = IW_BW;
							} else if (!g_strncasecmp (value, "INDEX", 5)) {
								ctab = IW_INDEX;
							} else
								ctab = (iwColtab)-1;
							while (value[i] && !isdigit((int)value[i]))
								i++;
						}
						if ( ((ctab == IW_BW || ctab == IW_INDEX) &&
							  sscanf (&value[i], "%d", &r) == 1) ||
							 ((ctab != (iwColtab)-1 && ctab != IW_BW && ctab != IW_INDEX) &&
							  sscanf (&value[i], "%d %d %d", &r, &g, &b) == 3) ) {
							prev_colConvert (ctab, r, g, b, &r, &g, &b);
							*(int*)rvalue = r;
							*((int*)rvalue+1) = g;
							*((int*)rvalue+2) = b;
						}
					}
					break;
			}
			return TRUE;
		}
	}
	return FALSE;
}

typedef struct textLine {
	textFont *font;		/* Biggest font of line / NULL if paragraph */
	int height;			/* Height of paragraph */
	int width;			/* Width of line */
	int start;			/* Offset from left border */
	short adjust;		/* ATT_LEFT | ATT_CENTER | ATT_RIGHT */
	short rotate;		/* 0 | 90 | 180 | 270 */
	struct textLine *prev, *next;
} textLine;

static textLine* text_line_append (textLine *line)
{
	textLine *new = calloc (1, sizeof(textLine));
	while (line && line->next)
		line = line->next;
	if (line) {
		line->next = new;
		new->prev = line;
	}
	return new;
}

/*********************************************************************
  Create and return a new text context as a copy of the last one.
*********************************************************************/
textContext* prev_text_context_push (textContext *top)
{
	textContext *new = malloc (sizeof(textContext));
	if (top) {
		*new = *top;
		new->next = top;
	} else {
		memset (new, 0, sizeof(textContext));
	}
	return new;
}

static textContext* prev_text_context_pop (textContext *top)
{
	textContext *new = NULL;
	if (top) {
		new = top->next;
		free (top);
	}
	return new;
}

/*********************************************************************
  Calculate line positions for the complete string str.
*********************************************************************/
static void text_calc_position (textFont *fonts, textContext *tc, const char *str,
								textLine *line, int *compwidth, int *compheight,
								textAttAnchor *anchor, int *zoom)
{
	textLine *lstart = line, *lpos;
	int x, xpix, chrs, width, height;
	int rotate = 0;
	textFont *fontmax = NULL;
	gboolean paragraph, end = FALSE;

	/* Width/height of complete text string */
	*compwidth  = *compheight = 0;
	/* Width/height of one paragraph */
	width = height = 0;
	/* Line width in pixel/chars */
	xpix = x = 0;

	paragraph = FALSE;
	line->rotate = 0;
	line->adjust = tc->adjust;

	for (; ; str++) {
		chrs = 0;
		if (*str == '<') {
			textAttTag att;
			char value[VALUE_MAXLEN];

			str++;
			tc = prev_text_context_push (tc);
			while (attribute_parse (&str, &att, value)) {
				switch (att) {
					case ATT_CLOSE:
						tc = prev_text_context_pop (tc);
						tc = prev_text_context_pop (tc);
						break;
					case ATT_TEXT:
						chrs = strlen(value);
						break;
					case ATT_ANCHOR:
						*anchor = *(int*)value;
						break;
					case ATT_FONT:
						tc->font = &fonts[*(int*)value];
						break;
					case ATT_ROTATE:
						rotate = *(int*)value * 90;
						paragraph = TRUE;
						break;
					case ATT_ZOOM:
						*zoom = *(int*)value;
						break;
					case ATT_ADJUST:
						tc->adjust = *(int*)value;
						break;
					default:
						break;
				}
			}
		} else if (*str == '\n' || !*str) {
			if (paragraph || end) {
				/* Fix line starts according to the now known paragraph width */
				lpos = line;
				while (lpos && lpos != lstart) {
					switch (lpos->adjust) {
						case ATT_LEFT:
							lpos->start = 0;
							break;
						case ATT_CENTER:
							lpos->start = (width - lpos->width)/2;
							break;
						case ATT_RIGHT:
							lpos->start = width - lpos->width;
							break;
					}
					if (lstart->rotate == 180 || lstart->rotate == 270)
						lpos->start += lpos->width;
					lpos = lpos->prev;
				}

				if (lstart->rotate == 90 || lstart->rotate == 270) {
					x = width;
					width = height;
					height = x;
				}
				if (width > *compwidth) *compwidth = width;
				*compheight += height;

				lstart->height = height;
				lstart->width = width;

				if (end) break;

				if (line != lstart)
					lstart = line = text_line_append (line);
				else
					lstart = line;
				lstart->rotate = rotate;
				lstart->adjust = tc->adjust;
				width = 0;
				height = 0;
			}
			if (!*str) {
				if (!end) str--;
				end = TRUE;
			}
			if (fontmax < tc->font) fontmax = tc->font;

			line = text_line_append (line);
			line->rotate = rotate;
			line->font = fontmax;
			line->adjust = tc->adjust;
			line->width = xpix;
			line->start = tc->adjust + (xpix << 2);

			if (xpix > width) width = xpix;
			height += fontmax->spacing;

			xpix = x = 0;
			fontmax = NULL;
			paragraph = FALSE;
		} else if (*str == '\t') {
			chrs = (1+x/8)*8-x;
		} else
			chrs = 1;
		if (chrs > 0) {
			if (fontmax < tc->font) fontmax = tc->font;
			x += chrs;
			xpix += chrs*tc->font->width;
		}
	}

	/* Adjust paragraphs */
	while (line) {
		if (!line->font) {
			switch (line->adjust) {
				case ATT_LEFT:
					line->start = 0;
					break;
				case ATT_CENTER:
					line->start = (*compwidth - line->width)/2;
					break;
				case ATT_RIGHT:
					line->start = *compwidth - line->width;
					break;
			}
			if (line->rotate == 90)
				line->start += line->width;
		}
		line = line->prev;
	}

	while (tc->next) tc = prev_text_context_pop (tc);
}

typedef struct textVector {
	textData *data;
	int xstart;				/* Start position for text */
	int ystart;

	int xpara;				/* Start paragraph position */
	int ypara;
	gboolean group;			/* TRUE if there is an open '<g>' */

	gboolean tspan_end;
	gboolean newline;
	gboolean paragraph;

	gboolean chars;			/* True if current tspan contains text */

	int bg_chars;			/* Number of chars in current bg-rectangle */
	float bg_start;			/* Start x-coordinate of current bg-rectangle */
	textFont *bg_font;		/* Font (==height) for current bg-rectangle */
	int bg_r, bg_g, bg_b;
	int bg_len, bg_max;
	char *bg_line;			/* Background color line */

	gboolean shadows;		/* Shadows in the current paragraph? */
	gboolean shadow_line;	/* Shadows in the current line? */
	int shadow_start;		/* Start of current line in sh_line */
	int sh_len, sh_max;
	char *sh_line;			/* Shadow line */

	int te_len, te_max;
	char *te_line;			/* Text line */
} textVector;

static void vector_append_str (char **line, int *len, int *max, char *str, ...)
{
	va_list args;

	if (*len + 500 > *max) {
		*max += 500;
		*line = realloc (*line, *max);
	}

	va_start (args, str);
	vsprintf (&(*line)[*len], str, args);
	va_end (args);

	*len = strlen (*line);
}

static void vector_append_chars (char **line, int *len, int *max, char *str)
{
	char *conv, conv_ch[3];
	int conv_len;

	while (*str) {
		switch (*str) {
			case '"':
				conv = "&quot;";
				break;
			case '<':
				conv = "&lt;";
				break;
			case '>':
				conv = "&gt;";
				break;
			case '&':
				conv = "&amp;";
				break;
			default:
				/* Latin-1 -> UTF-8 */
				if ((uchar)*str <= 0x7f) {
					conv_ch[0] = *str;
					conv_ch[1] = '\0';
				} else {
					conv_ch[0] = 0xc0 | ((uchar)*str >> 6);
					conv_ch[1] = 0x80 | ((uchar)*str & 0x3f);
					conv_ch[2] = '\0';
				}
				conv = conv_ch;
				break;
		}
		conv_len = strlen(conv);
		if (*len + conv_len >= *max) {
			*max += 500;
			*line = realloc (*line, *max);
		}
		strcpy (&(*line)[*len], conv);
		*len += conv_len;
		str++;
	}
}

/*********************************************************************
  End of tspan (changed parameter), line, or paragraph.
*********************************************************************/
static void vector_endline (textVector *vec)
{
	if ((vec->tspan_end || vec->newline) && vec->chars) {
		vector_append_str (&vec->sh_line, &vec->sh_len, &vec->sh_max,
						   "</tspan>");
		vector_append_str (&vec->te_line, &vec->te_len, &vec->te_max,
						   "</tspan>");
		vec->chars = FALSE;
		if (vec->newline) {
			if (!vec->shadow_line) {
				vec->sh_len = vec->shadow_start;
				vec->sh_line[vec->sh_len] = '\0';
			}
			vec->shadow_line = FALSE;
			vec->bg_start = 0;
		}
	}
	if (vec->paragraph) {
		if (vec->te_len) {
			vector_append_str (&vec->sh_line, &vec->sh_len, &vec->sh_max,
							   "</text>\n");
			vector_append_str (&vec->te_line, &vec->te_len, &vec->te_max,
							   "</text>\n");
			if (vec->bg_len)
				fputs (vec->bg_line, vec->data->save->file);
			if (vec->shadows)
				fputs (vec->sh_line, vec->data->save->file);
			if (vec->te_len)
				fputs (vec->te_line, vec->data->save->file);
		}
		if (vec->group)
			fputs ("</g>\n", vec->data->save->file);

		vec->sh_len = vec->te_len = vec->bg_len = 0;
		vec->shadows = FALSE;
		vec->paragraph = FALSE;
		vec->group = FALSE;
	}
}

static textVector* vector_init (textData *data, textContext *tc, int xstart, int ystart)
{
	textVector *vec = calloc (1, sizeof(textVector));
	vec->data = data;
	vec->xstart = xstart;
	vec->ystart = ystart;
	vec->bg_r = tc->bg_r;
	vec->bg_g = tc->bg_g;
	vec->bg_b = tc->bg_b;
	vec->bg_font = tc->font;
	vec->newline = vec->paragraph = TRUE;
	return vec;
}

static void vector_free (textVector *vec)
{
	if (!vec) return;

	vec->newline = vec->paragraph = TRUE;
	vector_endline (vec);

	if (vec->bg_line) free (vec->bg_line);
	if (vec->sh_line) free (vec->sh_line);
	if (vec->te_line) free (vec->te_line);
	free (vec);
}

/*********************************************************************
  Rotate (x,y) around (centx, centy) by angle (0, 90, 180, 270).
*********************************************************************/
static void point_rot (int centx, int centy, int angle, int *x, int *y)
{
	int xn;

	angle = (angle/90) % 4;
	*x -= centx;
	*y -= centy;
	xn = centx + *x*rotate_x[angle][0] - *y*rotate_x[angle][1];
	*y = centy + *x*rotate_y[angle][0] + *y*rotate_y[angle][1];
	*x = xn;
}

/*********************************************************************
  str == 0  : Change text attributes.
  *str == \n: End of line.
  *str == \f: End of paragraph -> Output text.
  else      : Apend str to the current text lines.
*********************************************************************/
static void render_vector (textVector *vec, char *str,
						   textContext *tc, textLine *line, int x, int y)
{
	textData *data;
	float zoom;
	char **l_str;
	int i, *len, *max;
	float xdest, ydest;

	if (!vec) return;

	data = vec->data;
	zoom = data->zoom;

	if (vec->paragraph) {
		vec->xpara = x;
		vec->ypara = y;
	}
	/* Remove rotation (is done with a transformation group) */
	point_rot (vec->xpara, vec->ypara, 360-line->rotate, &x, &y);

	/* Update bg-rectangles */
	if ((str && *str == '\n') ||
		tc->font != vec->bg_font ||
		tc->bg_r != vec->bg_r || tc->bg_g != vec->bg_g || tc->bg_b != vec->bg_b) {

		float x1;
		float w = vec->bg_chars*vec->bg_font->width*zoom;
		if (vec->bg_start != 0)
			x1 = vec->bg_start;
		else
			x1 = vec->xstart + x*zoom;
		vec->bg_start = x1 + w;

		if (w > 0 && (vec->bg_r >= 0 || vec->bg_g >= 0 || vec->bg_b >= 0)) {
			float y1 = vec->ystart + (y + line->font->baseline - vec->bg_font->baseline)*zoom;
			float h = vec->bg_font->spacing*zoom;

			vector_append_str (&vec->bg_line, &vec->bg_len, &vec->bg_max,
							   "  <rect style=\"fill:%s\" "
							   "x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
							   prev_colString(IW_RGB, vec->bg_r, vec->bg_g, vec->bg_b),
							   x1, y1, w, h);
		}
		vec->bg_r = tc->bg_r;
		vec->bg_g = tc->bg_g;
		vec->bg_b = tc->bg_b;
		vec->bg_chars = 0;
		vec->bg_font = tc->font;
	}

	if (!str) {
		vec->tspan_end = TRUE;
		return;
	} else if (*str == '\n') {
		vec->newline = TRUE;
		return;
	} else if (*str == '\f') {
		vec->paragraph = TRUE;
		return;
	} else if (!*str)
		return;

	vector_endline (vec);

	/* Start a new tspan */
	if (!vec->chars) {
		xdest = vec->xstart + x*zoom;
		ydest = vec->ystart + y*zoom;
		if (line->adjust == ATT_CENTER)
			xdest += line->width*zoom/2;
		else if (line->adjust == ATT_RIGHT)
			xdest += line->width*zoom;

		if (vec->te_len == 0 && line->rotate > 0) {
			fprintf (vec->data->save->file, "<g transform=\"rotate(%d, %f, %f)\">\n",
					 line->rotate, xdest, ydest);
			vec->group = TRUE;
		}
		l_str = &vec->sh_line;
		len = &vec->sh_len;
		max = &vec->sh_max;
		for (i=1; i>=0; i--) {
			if (!*len)
				vector_append_str (l_str, len, max,
								   "  <text xml:space=\"preserve\""
								   " style=\"font-family:Bitstream Vera Sans Mono,Monospace\">");
			if (i == 1 && vec->newline)
				vec->shadow_start = vec->sh_len;

			vector_append_str (l_str, len, max, "<tspan\n   ");
			if (line->adjust != ATT_LEFT)
				vector_append_str (l_str, len, max,
								   " text-anchor=\"%s\"",
								   line->adjust == ATT_CENTER ? "middle" : "end");
			if (i == 1) {
				if (tc->shadow) {
					vec->shadows = vec->shadow_line = TRUE;
					vector_append_str (l_str, len, max, " visibility=\"visible\"");
				} else
					vector_append_str (l_str, len, max, " visibility=\"hidden\"");
			}
			if (vec->newline) {
				int r = (4 - line->rotate/90) % 4;
				float x = i*zoom;
				vector_append_str (l_str, len, max, " x=\"%f\" y=\"%f\"",
								   xdest + x*rotate_x[r][0] - x*rotate_x[r][1],
								   ydest + line->font->baseline + x*rotate_y[r][0] + x*rotate_y[r][1]);
			}
			vector_append_str (l_str, len, max,
							   " style=\"fill:%s;font-size:%.2f\"\n    >",
							   i==1 ? "black":prev_colString(IW_RGB, tc->r, tc->g, tc->b),
							   tc->font->size*zoom);

			l_str = &vec->te_line;
			len = &vec->te_len;
			max = &vec->te_max;
		}
		vec->tspan_end = FALSE;
		vec->newline = FALSE;
	}
	/* Append the real text */
	vector_append_chars (&vec->sh_line, &vec->sh_len, &vec->sh_max, str);
	vector_append_chars (&vec->te_line, &vec->te_len, &vec->te_max, str);

	vec->chars = TRUE;
	vec->bg_chars += strlen(str);
}

/*********************************************************************
  Render one char at (xstart,ystart).
*********************************************************************/
static void render_ch (textData *data, char ch, int xstart, int ystart,
					   int rotate, textFont *font, int r, int g, int b)
{
	const unsigned char *fdata;
	unsigned int bit;
	int y, x, width, height;
	void (*putPoint) (int, int);
	int zoomI, down = 1;

	rotate /= 90;
	if (iw_isequalf (data->zoom, 1.0)) {
		zoomI = 1;
		width = font->width;
		height = font->size;
	} else if (data->zoom < 1.0) {
		zoomI = 0;
		down = 1.0/data->zoom+0.5;
		width = font->width/down;
		height = font->size/down;
	} else {
		zoomI = data->zoom+0.5;
		width = font->width*zoomI;
		height = font->size*zoomI;
	}

	x = xstart+(width-1)*rotate_x[rotate][0]-(height-1)*rotate_x[rotate][1];
	y = ystart+(width-1)*rotate_y[rotate][0]+(height-1)*rotate_y[rotate][1];
	if (xstart>=0 && ystart>=0 &&
		xstart<data->b->width && ystart<data->b->height &&
		x>=0 && y>=0 && x<data->b->width && y<data->b->height) {
		if (data->dispMode & RENDER_XOR) {
			if (data->b->gray)
				putPoint = prev_drawPoint_gray_nc_xor;
			else
				putPoint = prev_drawPoint_color_nc_xor;
		} else {
			if (data->b->gray)
				putPoint = prev_drawPoint_gray_nc;
			else if (r>=0 && g>=0 && b>=0)
				putPoint = prev_drawPoint_color_nc_nm;
			else
				putPoint = prev_drawPoint_color_nc;
		}
	} else
		putPoint = prev_drawPoint;

	fdata = font->data + font->rowstride * gui_lrint(font->size) * (uchar)ch;
	prev_drawInit (data->b, r, g, b);
	if (zoomI == 1) {
		for (y=0; y<height; y++) {
			bit = 1;
			for (x=0; x<width; x++) {
				if (bit > 128) {
					bit = 1;
					fdata++;
				}
				if (*fdata & bit)
					putPoint (xstart+x*rotate_x[rotate][0]-y*rotate_x[rotate][1],
							  ystart+x*rotate_y[rotate][0]+y*rotate_y[rotate][1]);
				bit <<= 1;
			}
			fdata++;
		}
	} else if (zoomI == 0) {
		int tx;
		for (y=0; y<height; y++) {
			bit = 1;
			tx = 0;
			for (x=0; x<width; x++) {
				if (fdata[tx/8] & bit)
					putPoint (xstart+x*rotate_x[rotate][0]-y*rotate_x[rotate][1],
							  ystart+x*rotate_y[rotate][0]+y*rotate_y[rotate][1]);
				tx += down;
				bit <<= down;
				while (bit > 128) bit >>= 8;
			}
			fdata += font->rowstride*down;
		}
	} else {
		int i, j, sx, sy;
		height /= zoomI;
		width /= zoomI;
		sy = 0;
		for (y=0; y<height; y++) {
			for (i=0; i<zoomI; i++) {
				bit = 1;
				sx = 0;
				for (x=0; x<width; x++) {
					if (fdata[x/8] & bit) {
						for (j=0; j<zoomI; j++) {
							putPoint (xstart+sx*rotate_x[rotate][0]-sy*rotate_x[rotate][1],
									  ystart+sx*rotate_y[rotate][0]+sy*rotate_y[rotate][1]);
							sx++;
						}
					} else
						sx += zoomI;
					bit <<= 1;
					if (bit > 128) bit = 1;
				}
				sy++;
			}
			fdata += font->rowstride;
		}
	}
}

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
  ATTENTION: Low-Level routine without buffer locking or check for
			 open windows.
*********************************************************************/
void prev_drawString_do (textData *data, int xstart, int ystart,
						 const char *str, textContext *tc)
{
	char *latin1 = NULL;
	char value[VALUE_MAXLEN], curstr[VALUE_MAXLEN];
	int xpix, xind, ypix, ypara;
	int width, height;
	textLine *line = NULL, *lstart = NULL, *lparagraph;
	textVector *vec = NULL;
	textFont *fonts = iw_fonts;

	int z_do = FALSE;
	textAttAnchor anchor = ATT_TL;

	latin1 = gui_get_latin1 (str);
	str = latin1;

	width = 0;
	height = 0;
	if (data->save) {
		/* Substitute the vector fonts */
		tc->font = &vector_fonts[tc->font - iw_fonts];
		fonts = vector_fonts;
	}

	/* Calculate line positions */
	lstart = text_line_append (lstart);
	text_calc_position (fonts, tc, str, lstart, &width, &height, &anchor, &z_do);

	if (!z_do) data->zoom = 1;

	/* Adjust text position according to the anchor */
	switch (anchor) {
		case ATT_TL:
			break;
		case ATT_TR:
			xstart -= width*data->zoom;
			break;
		case ATT_BR:
			xstart -= width*data->zoom;
			ystart -= height*data->zoom;
			break;
		case ATT_BL:
			ystart -= height*data->zoom;
			break;
		case ATT_C:
			xstart -= width*data->zoom / 2;
			ystart -= height*data->zoom / 2;
			break;
	}

	if (xstart+(width+1)*data->zoom<0 || xstart>=data->b->width ||
		ystart+(height+1)*data->zoom<0 || ystart>=data->b->height) {
		/* Text is completely outside, nothing to draw */
		g_free (latin1);
		return;
	}

	lparagraph = lstart;
	line = lparagraph->next;

	/* Draw the string */
	ypara = 0;
	if (lparagraph->rotate == 180 || lparagraph->rotate == 270)
		ypix = lparagraph->height;
	else
		ypix = 0;
	xpix = lparagraph->start;
	if (lparagraph->rotate == 0 || lparagraph->rotate == 180)
		xpix += line->start;
	xind = 0;

	if (data->save)
		vec = vector_init (data, tc, xstart, ystart);

	for (; *str; str++) {
		int ch_cnt = 0, ch_pos;

		if (*str == '<') {
			textAttTag att;

			str++;
			tc = prev_text_context_push (tc);
			while (attribute_parse (&str, &att, value)) {
				switch (att) {
					case ATT_CLOSE:
						tc = prev_text_context_pop (tc);
						tc = prev_text_context_pop (tc);
						break;
					case ATT_TEXT:
						ch_cnt = strlen(value);
						memcpy (curstr, value, ch_cnt);
						break;
					case ATT_FONT:
						tc->font = &fonts[*(int*)value];
						break;
					case ATT_ADJUST:
						tc->adjust = *(int*)value;
						break;
					case ATT_SHADOW:
						tc->shadow = *(int*)value;
						break;
					case ATT_FG:
						tc->r = *(int*)value;
						tc->g = *((int*)value+1);
						tc->b = *((int*)value+2);
						break;
					case ATT_BG:
						tc->bg_r = *(int*)value;
						tc->bg_g = *((int*)value+1);
						tc->bg_b = *((int*)value+2);
						break;
					default:
						break;
				}
			}
			if (vec)
				render_vector (vec, NULL, tc, line, xpix, ypix);
		} else if (*str == '\n') {
			if (vec)
				render_vector (vec, "\n", tc, line, xpix, ypix);

			switch (lparagraph->rotate) {
				case   0: ypix += line->font->spacing; break;
				case  90: xpix -= line->font->spacing; break;
				case 180: ypix -= line->font->spacing; break;
				case 270: xpix += line->font->spacing; break;
			}
			line = line->next;
			if (!line->font) {
				ypara += lparagraph->height;
				lparagraph = line;
				line = line->next;
				ypix = ypara;
				if (lparagraph->rotate == 180 || lparagraph->rotate == 270)
					ypix += lparagraph->height;
				xpix = lparagraph->start;
				if (vec)
					render_vector (vec, "\f", tc, line, xpix, ypix);
			}

			if (lparagraph->rotate == 0 || lparagraph->rotate == 180)
				xpix = lparagraph->start + line->start;
			else
				ypix = ypara + line->start;
			xind = 0;
		} else if (*str == '\t') {
			ch_cnt = (1+xind/8)*8-xind;
			memset (curstr, ' ', ch_cnt);
		} else {
			*curstr = *str;
			ch_cnt = 1;
		}
		if (vec) {
			curstr[ch_cnt] = '\0';
			render_vector (vec, curstr, tc, line, xpix, ypix);
		} else {
			for (ch_pos=0; ch_pos < ch_cnt; ch_pos++) {
				int y = ypix + line->font->baseline - tc->font->baseline;

				if (tc->bg_r >= 0 || tc->bg_g >= 0 || tc->bg_b >= 0) {
					int x1 = xstart + xpix*data->zoom, y1 = ystart+y*data->zoom;
					int w = tc->font->width * data->zoom;
					int h = tc->font->spacing * data->zoom;
					int i;

					prev_drawInit (data->b, tc->bg_r, tc->bg_g, tc->bg_b);
					switch (lparagraph->rotate) {
						case  90: i=w; w=h; h=i; x1 -= w-1; break;
						case 180: x1 -= w-1; y1 -= h-1; break;
						case 270: i=w; w=h; h=i; y1 -= h-1; break;
					}
					prev_drawFRect (x1, y1, w, h);
				}
				if (curstr[ch_pos] != ' ') {
					if (tc->shadow)
						render_ch (data, curstr[ch_pos],
								   xstart+(xpix+1)*data->zoom, ystart+(y+1)*data->zoom,
								   lparagraph->rotate, tc->font, 0, 0, 0);

					render_ch (data, curstr[ch_pos],
							   xstart+xpix*data->zoom, ystart+y*data->zoom,
							   lparagraph->rotate, tc->font, tc->r, tc->g, tc->b);
				}
				switch (lparagraph->rotate) {
					case   0: xpix += tc->font->width; break;
					case  90: ypix += tc->font->width; break;
					case 180: xpix -= tc->font->width; break;
					case 270: ypix -= tc->font->width; break;
				}
			}
		}
		xind += ch_cnt;
	}
	/* Clean up */
	if (vec)
		vector_free (vec);
	while (lstart) {
		line = lstart;
		lstart = lstart->next;
		free (line);
	}
	while (tc) tc = prev_text_context_pop(tc);
	g_free (latin1);
}

/*********************************************************************
  Initialize the user interface for text rendering.
*********************************************************************/
void prev_render_text_opts (prevBuffer *b)
{
	static char *fontOpts[] = {"small","med","big",NULL};
	textOpts *opts = malloc (sizeof(textOpts));
	opts->font = FONT_MED - 1;
	opts->shadow = FALSE;
	prev_opts_store (b, PREV_TEXT, opts, TRUE);

	opts_radio_create ((long)b, "Font", "Font size to use",
					   fontOpts, &opts->font);
	opts_toggle_create ((long)b, "Font Shadows", "Display text with shadows?",
						&opts->shadow);
}

/*********************************************************************
  Free the text 'data'.
*********************************************************************/
void prev_render_text_free (void *data)
{
	prevDataText *text = (prevDataText*)data;

	if (text->text) g_free (text->text);
	g_free (text);
}

/*********************************************************************
  Copy the text 'data' and return it.
*********************************************************************/
void* prev_render_text_copy (const void *data)
{
	prevDataText *text = (prevDataText*)data;
	prevDataText *t = g_malloc (sizeof(prevDataText));

	*t = *text;
	if (text->text)
		t->text = g_strdup (text->text);
	return t;
}

/*********************************************************************
  Display text in buffer b.
*********************************************************************/
void prev_render_text_do (prevBuffer *b, const prevDataText *text,
						  int disp_mode)
{
	float zoom = prev_colInit (b, disp_mode, text->ctab,
							   text->r, text->g, text->b, text->r);
	prev_drawString ((text->x-b->x)*zoom,
					 (text->y-b->y)*zoom, text->text);
}

/*********************************************************************
  Render the text text according the data in save.
*********************************************************************/
iwImgStatus prev_render_text_vector (prevBuffer *b, prevVectorSave *save,
									 prevDataText *text, int disp_mode)
{
	textContext *tc = prev_text_context_push(NULL);
	textData data;
	textOpts **opts;

	if (save->format != IW_IMG_FORMAT_SVG) {
		prev_text_context_pop (tc);
		return IW_IMG_STATUS_FORMAT;
	}

	opts = (textOpts **)prev_opts_get(b, PREV_TEXT);
	if (disp_mode & FONT_MASK)
		tc->font = &iw_fonts[(disp_mode & FONT_MASK) - 1];
	else if (opts) {
		tc->font = &iw_fonts[(((*opts)->font+1) & FONT_MASK) - 1];
	} else
		tc->font = &iw_fonts[1];
	prev_colConvert (text->ctab, text->r, text->g, text->b,
					 &tc->r, &tc->g, &tc->b);
	tc->bg_r = -1;
	tc->bg_g = -1;
	tc->bg_b = -1;
	tc->shadow = opts && (*opts)->shadow;

	data.b = b;
	data.dispMode = disp_mode;
	data.zoom = save->zoom;
	data.save = save;
	prev_drawString_do (&data, text->x*data.zoom, text->y*data.zoom, text->text, tc);

	return IW_IMG_STATUS_OK;
}
