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
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__sun)
#include <ieeefp.h>
#endif

#include "Gtools.h"
#include "Grender.h"
#include "Goptions.h"
#include "Gpreview.h"
#include "Gcolor.h"
#include "GrenderImg.h"

#define HIST_OFF		0
#define HIST_LIN		1
#define HIST_LOG		2

#define HISTHIGH		80

#define PLANE_LENGTH	10

typedef struct prevOptsImage {
	gboolean show;				/* Show metainfo ? */
	int histogram;				/* One of HIST_xxx, see above */
	gboolean lines;				/* Lines / Bars for the histogram */
	gboolean extrema;			/* Include min/max in histogram scaling? */
	float scale_min, scale_max;	/* Range scale factors */
	char planes[PLANE_LENGTH];	/* Planes which should be shown */
} prevImageOpts;

typedef struct prevImageData {
	double min, max;			/* Min/max image value */
	double scale_min, scale_max;/* See prevOptsImage */
	int histogram;				/* See prevOptsImage */
	gboolean lines;				/* See prevOptsImage */
	gboolean extrema;			/* See prevOptsImage */
	int planes[3];				/* See prevOptsImage */
	int hist[3][IW_COLCNT];
} prevImageData;

/*********************************************************************
  Initialize the user interface for image rendering.
*********************************************************************/
void prev_render_image_opts (prevBuffer *b)
{
	static char *histogram[] = {"Off", "Linear", "Logarithmic", NULL};
	prevImageOpts *opts = calloc (1, sizeof(prevImageOpts));
	int p;

	opts->show = FALSE;
	opts->histogram = HIST_LIN;
	opts->lines = FALSE;
	opts->extrema = TRUE;
	opts->planes[0] = '\0';
	opts->scale_min = 0;
	opts->scale_max = 0;
	prev_opts_store (b, PREV_IMAGE, opts, TRUE);

	opts_toggle_create ((long)b, "Show MetaInfo",
						"Display histogram and further information about the image?",
						&opts->show);
	p = prev_get_page(b);
	opts_option_create (p, "Histogram:",
						"Show a linear/logarithmic scaled histogram?",
						histogram, &opts->histogram);
	opts_toggle_create (p, "Use lines for histogram",
						"Use lines instead of bars for the histogram?",
						&opts->lines);
	opts_toggle_create (p, "Include min/max value in histogram?",
						"Include min/max in histogram scaling?"
						" Useful to exclude if a lot of the image is black/white.",
						&opts->extrema);
	opts_string_create (p, "Planes which should be shown [0 1 2]",
						"Which planes of the images should be shown, "
						"default 0 (gray image) or 0 1 2 (color image)",
						opts->planes, PLANE_LENGTH);
	opts_float_create (p, "scaleMin",
					   "Scale factor in % for min image value towards max."
					   "   -1: Clamp values to 0-255   -2: Shift values to the range 0-255",
					   &opts->scale_min, -2, 100);
	opts_float_create (p, "scaleMax",
					   "Scale factor in % for max image value towards min",
					   &opts->scale_max, 0, 100);

	prev_opts_append (b, PREV_TEXT, -1);
}

/*********************************************************************
  Free the image 'data'.
*********************************************************************/
void prev_render_image_free (void *data)
{
	prevDataImage *img = data;
	iw_img_free (img->i, IW_IMG_FREE_ALL);
	g_free (img);
}

/*********************************************************************
  Copy the image 'data' and return it.
*********************************************************************/
void* prev_render_image_copy (const void *data)
{
	const prevDataImage *src = data;
	prevDataImage *dst;

	dst = g_malloc (sizeof(prevDataImage));
	*dst = *src;
	dst->i = iw_img_duplicate (src->i);

	return dst;
}

/*********************************************************************
  Return information (a new string) about dImg at position x,y.
*********************************************************************/
char* prev_render_image_info (prevBuffer *b, const prevDataImage *dImg,
							  int disp_mode, int x, int y, int radius)
{
	static int digits[] = {3, 5, 11, 12, 12};
	iwImage *img = dImg->i;
	char *string, *s_pos;
	int p;

	x -= dImg->x;
	y -= dImg->y;
	if (x < 0 || y < 0 || x >= img->width || y >= img->height)
		return NULL;

	string = g_malloc (10 + img->planes * (1+digits[img->type]));

	strcpy (string,
			(img->planes < 3 && img->ctab <= IW_COLFORMAT_MAX) ?
			"GRAY" : IW_COLOR_TEXT(img));
	s_pos = string+strlen(string);

	for (p=0; p<img->planes; p++) {
		int cnt = 0, sx, sy;
		guchar *data;
		gdouble val = 0;

		for (sy=y-radius; sy<=y+radius; sy++) {
			for (sx=x-radius; sx<=x+radius; sx++) {
				if (sx < 0 || sy < 0 || sx >= img->width || sy >= img->height)
					continue;

				if (img->rowstride)
					data = img->data[0] + img->rowstride*sy + (sx*img->planes + p)*IW_TYPE_SIZE(img);
				else
					data = img->data[p] + (img->width*sy+sx)*IW_TYPE_SIZE(img);
				switch (img->type) {
					case IW_8U    : val += *data; break;
					case IW_16U   : val += *(guint16*)data; break;
					case IW_32S   : val += *(gint32*)data; break;
					case IW_FLOAT : val += *(gfloat*)data; break;
					case IW_DOUBLE: val += *(gdouble*)data; break;
				}
				cnt++;
			}
		}
		val /= cnt;
		switch (img->type) {
			case IW_8U:
			case IW_16U:
			case IW_32S:
				sprintf (s_pos, " %ld", lrint(val));
				break;
			case IW_FLOAT:
			case IW_DOUBLE:
				sprintf (s_pos, " %g", val);
				break;
			default:
				iw_warning ("Unknown image type %d, can't get color info", img->type);
		}
		s_pos += strlen(s_pos);
	}
	return string;
}

/*********************************************************************
  Calc values to show img->i in b at position (img->x, img->y).
  zoom   : Zoom value
  buf    : Starting position in b->buffer
  offset : Offset for upper left corner in img->i->data[x]
  d_width, d_height: Number of bytes which must be copied to the
					 destination buffer.
*********************************************************************/
static void prev_get_values (prevBuffer *b, const prevDataImage *img,
							 float *zoom,
							 guchar **buf, int *offset,
							 int *d_width, int *d_height)
{
	int bpp = (b->gray ? 1:3);
	int start, end, rowstride, pixstride;

	if (img->i->rowstride) {
		rowstride = img->i->rowstride;
		pixstride = img->i->planes;
	} else {
		rowstride = img->i->width;
		pixstride = 1;
	}

	*zoom = prev_get_zoom (b);

	*buf = b->buffer;
	/* Offset for first visible pixel in the upper left corner */
	if (img->x > b->x) *buf += (long)((img->x - b->x) * (*zoom)) * bpp;
	if (img->y > b->y) *buf += (long)((img->y - b->y) * (*zoom)) * b->width*bpp;

	/* Offset for upper left corner in the source */
	*offset = 0;
	if (img->x < b->x) *offset += (b->x - img->x) * pixstride;
	if (img->y < b->y) *offset += (b->y - img->y) * rowstride;

	/* Destination width and height */
	start = MAX (b->x, img->x);
	end = MIN (img->x + img->i->width, b->width/(*zoom)+b->x);
	*d_width = (end - start)*(*zoom);

	start = MAX (b->y, img->y);
	end = MIN (img->y + img->i->height, b->height/(*zoom)+b->y);
	*d_height = (end - start)*(*zoom);
}

/*********************************************************************
  Display interleaved image dImg in buffer b.
*********************************************************************/
static void prev_render_interleaved (prevBuffer *b, const prevDataImage *dImg, prevImageData *dat)
{
	iwImage *img = dImg->i;
	int *pnr = dat->planes;
	iwColtab ctab = img->ctab;
	int planes = img->planes;
	guchar *buf, *y;
	int xs, ys, bpp = (b->gray ? 1:3);
	int d_width, d_height;
	float zoom;
	void (*c_trans) (guchar s1, guchar s2, guchar s3, guchar *r, guchar *g, guchar *b) = NULL;

	prev_get_values (b, dImg, &zoom, &buf, &xs, &d_width, &d_height);
	if (d_width < 1 || d_height < 1) return;

	y = img->data[0] + xs;

	if (pnr[1] < 0 && ctab <= IW_COLFORMAT_MAX)
		ctab = IW_GRAY;
	else if (ctab == IW_HSV)
		c_trans = prev_hsvToRgb;
	if (b->gray || ctab == IW_GRAY || ctab > IW_COLFORMAT_MAX)
		y += pnr[0];

	if (iw_isequalf (zoom, 1.0)) {
		BOOL copy = b->gray && planes == 1;
		if (!copy && planes == 3) {
			if (ctab == IW_RGB)
				copy = pnr[0] == 0 && pnr[1] == 1 && pnr[2] == 2;
			else if (ctab == IW_BGR)
				copy = pnr[0] == 2 && pnr[1] == 1 && pnr[2] == 0;
		}
		/* Image must not be zoomed */
		for (ys=0; ys<d_height; ys++) {
			if (copy) {
				memcpy (buf, y, d_width*planes);
				buf += d_width*bpp;
				y += d_width*planes;
			} else if (b->gray) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					y += planes;
				}
			} else if (ctab == IW_BGR) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = y[pnr[2]];
					*buf++ = y[pnr[1]];
					*buf++ = y[pnr[0]];
					y += planes;
				}
			} else if (ctab == IW_YUV) {
				for (xs=0; xs<d_width; xs++) {
					prev_inline_yuvToRgbVis (y[pnr[0]], y[pnr[1]], y[pnr[2]],
											 buf, buf+1, buf+2);
					buf += 3;
					y += planes;
				}
			} else if (ctab == IW_GRAY) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					*buf++ = *y;
					*buf++ = *y;
					y += planes;
				}
			} else if (ctab > IW_COLFORMAT_MAX) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = ctab[*y][0];
					*buf++ = ctab[*y][1];
					*buf++ = ctab[*y][2];
					y += planes;
				}
			} else if (c_trans) {
				for (xs=0; xs<d_width; xs++) {
					c_trans (y[pnr[0]], y[pnr[1]], y[pnr[2]], buf, buf+1, buf+2);
					buf += 3;
					y += planes;
				}
			} else {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = y[pnr[0]];
					*buf++ = y[pnr[1]];
					*buf++ = y[pnr[2]];
					y += planes;
				}
			}
			buf += (b->width-d_width)*bpp;
			y += img->rowstride - d_width*planes;
		}
	} else if (zoom < 1.0) {
		/* Image must be scaled down */
		int down = 1.0/zoom+0.5;							/* Downsample factor */
		int line = ((down-1)*img->rowstride +				/* Offset end of line */
					img->rowstride - d_width*down*planes);	/*   to start of next line */

		for (ys=0; ys<d_height; ys++) {
			if (b->gray) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					y += down * planes;
				}
			} else if (ctab == IW_YUV) {
				for (xs=0; xs<d_width; xs++) {
					prev_inline_yuvToRgbVis (y[pnr[0]], y[pnr[1]], y[pnr[2]],
											 buf, buf+1, buf+2);
					y += down * planes;
					buf += 3;
				}
			} else if (ctab == IW_BGR) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = y[pnr[2]];
					*buf++ = y[pnr[1]];
					*buf++ = y[pnr[0]];
					y += down * planes;
				}
			} else if (ctab == IW_GRAY) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					*buf++ = *y;
					*buf++ = *y;
					y += down * planes;
				}
			} else if (ctab > IW_COLFORMAT_MAX) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = ctab[*y][0];
					*buf++ = ctab[*y][1];
					*buf++ = ctab[*y][2];
					y += down * planes;
				}
			} else if (c_trans) {
				for (xs=0; xs<d_width; xs++) {
					c_trans (y[pnr[0]], y[pnr[1]], y[pnr[2]], buf, buf+1, buf+2);
					y += down * planes;
					buf += 3;
				}
			} else {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = y[pnr[0]];
					*buf++ = y[pnr[1]];
					*buf++ = y[pnr[2]];
					y += down * planes;
				}
			}
			y += line;
			buf += (b->width-d_width)*bpp;
		}
	} else {
		/* Image must be scaled up */
		int i, j, zoomI = zoom+0.5;

		d_width /= zoomI;
		d_height /= zoomI;
		for (ys=0; ys<d_height; ys++) {
			for (j=0; j<zoomI; j++) {
				if (b->gray) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++)
							*buf++ = *y;
						y += planes;
					}
				} else if (ctab == IW_YUV) {
					for (xs=0; xs<d_width; xs++) {
						guchar r, g, b;
						prev_inline_yuvToRgbVis (y[pnr[0]], y[pnr[1]], y[pnr[2]],
												 &r, &g, &b);
						for (i=0; i<zoomI; i++) {
							*buf++ = r;
							*buf++ = g;
							*buf++ = b;
						}
						y += planes;
					}
				} else if (ctab == IW_BGR) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = y[pnr[2]];
							*buf++ = y[pnr[1]];
							*buf++ = y[pnr[0]];
						}
						y += planes;
					}
				} else if (ctab == IW_GRAY) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = *y;
							*buf++ = *y;
							*buf++ = *y;
						}
						y += planes;
					}
				} else if (ctab > IW_COLFORMAT_MAX) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = ctab[*y][0];
							*buf++ = ctab[*y][1];
							*buf++ = ctab[*y][2];
						}
						y += planes;
					}
				} else if (c_trans) {
					for (xs=0; xs<d_width; xs++) {
						guchar r, g, b;
						c_trans (y[pnr[0]], y[pnr[1]], y[pnr[2]], &r, &g, &b);
						for (i=0; i<zoomI; i++) {
							*buf++ = r;
							*buf++ = g;
							*buf++ = b;
						}
						y += planes;
					}
				} else {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = y[pnr[0]];
							*buf++ = y[pnr[1]];
							*buf++ = y[pnr[2]];
						}
						y += planes;
					}
				}
				buf += (b->width-d_width*zoomI)*bpp;
				y -= d_width*planes;
			}
			y += img->rowstride;
		}
	}
}

/*********************************************************************
  Display planed image dImg in buffer b.
*********************************************************************/
static void prev_render_planed (prevBuffer *b, const prevDataImage *dImg, prevImageData *dat)
{
	iwImage *img = dImg->i;
	iwColtab ctab = img->ctab;
	guchar *buf, *y, *u, *v;
	int d_width, d_height, xs, ys;
	int bpp = (b->gray ? 1:3);
	float zoom;
	void (*c_trans) (guchar s1, guchar s2, guchar s3, guchar *r, guchar *g, guchar *b) = NULL;

	prev_get_values (b, dImg, &zoom,
					 &buf, &xs, &d_width, &d_height);
	if (d_width < 1 || d_height < 1) return;

	y = img->data[dat->planes[0]] + xs;
	u = v = NULL;
	if (dat->planes[1] >= 0)
		u = img->data[dat->planes[1]] + xs;
	if (dat->planes[2] >= 0)
		v = img->data[dat->planes[2]] + xs;

	if ((!u || !v) && ctab <= IW_COLFORMAT_MAX)
		ctab = IW_GRAY;
	else if (ctab == IW_HSV)
		c_trans = prev_hsvToRgb;

	if (iw_isequalf (zoom, 1.0)) {
		/* Image must not be zoomed */
		for (ys=0; ys<d_height; ys++) {
			if (b->gray) {
				memcpy (buf, y, d_width);
				buf += d_width;
				y += d_width;
			} else if (ctab == IW_YUV) {
				for (xs=0; xs<d_width; xs++) {
					prev_inline_yuvToRgbVis (*y++, *u++, *v++, buf, buf+1, buf+2);
					buf += 3;
				}
			} else if (ctab == IW_BGR) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *v++;
					*buf++ = *u++;
					*buf++ = *y++;
				}
			} else if (ctab == IW_GRAY) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					*buf++ = *y;
					*buf++ = *y++;
				}
			} else if (ctab > IW_COLFORMAT_MAX) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = ctab[*y][0];
					*buf++ = ctab[*y][1];
					*buf++ = ctab[*y++][2];
				}
			} else if (c_trans) {
				for (xs=0; xs<d_width; xs++) {
					c_trans (*y++, *u++, *v++, buf, buf+1, buf+2);
					buf += 3;
				}
			} else {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y++;
					*buf++ = *u++;
					*buf++ = *v++;
				}
			}
			buf += (b->width-d_width)*bpp;
			y += img->width - d_width;
			u += img->width - d_width;
			v += img->width - d_width;
		}
	} else if (zoom < 1.0) {
		/* Image must be scaled down */
		int down = 1.0/zoom+0.5;				/* Downsample factor */
		int line = ((down-1)*img->width +
					img->width - d_width*down);	/* Offset end of line to start of next line */

		for (ys=0; ys<d_height; ys++) {
			if (b->gray) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					y += down;
				}
			} else if (ctab == IW_YUV) {
				for (xs=0; xs<d_width; xs++) {
					prev_inline_yuvToRgbVis (*y, *u, *v, buf, buf+1, buf+2);
					y += down;
					u += down;
					v += down;
					buf += 3;
				}
			} else if (ctab == IW_BGR) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *v;
					*buf++ = *u;
					*buf++ = *y;
					y += down;
					u += down;
					v += down;
				}
			} else if (ctab == IW_GRAY) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					*buf++ = *y;
					*buf++ = *y;
					y += down;
				}
			} else if (ctab > IW_COLFORMAT_MAX) {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = ctab[*y][0];
					*buf++ = ctab[*y][1];
					*buf++ = ctab[*y][2];
					y += down;
				}
			} else if (c_trans) {
				for (xs=0; xs<d_width; xs++) {
					c_trans (*y, *u, *v, buf, buf+1, buf+2);
					y += down;
					u += down;
					v += down;
					buf += 3;
				}
			} else {
				for (xs=0; xs<d_width; xs++) {
					*buf++ = *y;
					*buf++ = *u;
					*buf++ = *v;
					y += down;
					u += down;
					v += down;
				}
			}
			y += line;
			u += line;
			v += line;
			buf += (b->width-d_width)*bpp;
		}
	} else {
		/* Image must be scaled up */
		int i, j, zoomI = zoom+0.5;

		d_width /= zoomI;
		d_height /= zoomI;
		for (ys=0; ys<d_height; ys++) {
			for (j=0; j<zoomI; j++) {
				if (b->gray) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++)
							*buf++ = *y;
						y++;
					}
				} else if (ctab == IW_YUV) {
					for (xs=0; xs<d_width; xs++) {
						guchar r, g, b;
						prev_inline_yuvToRgbVis (*y++, *u++, *v++, &r, &g, &b);
						for (i=0; i<zoomI; i++) {
							*buf++ = r;
							*buf++ = g;
							*buf++ = b;
						}
					}
				} else if (ctab == IW_BGR) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = *v;
							*buf++ = *u;
							*buf++ = *y;
						}
						y++; u++; v++;
					}
				} else if (ctab == IW_GRAY) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = *y;
							*buf++ = *y;
							*buf++ = *y;
						}
						y++;
					}
				} else if (ctab > IW_COLFORMAT_MAX) {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = ctab[*y][0];
							*buf++ = ctab[*y][1];
							*buf++ = ctab[*y][2];
						}
						y++;
					}
				} else if (c_trans) {
					for (xs=0; xs<d_width; xs++) {
						guchar r, g, b;
						c_trans (*y++, *u++, *v++, &r, &g, &b);
						for (i=0; i<zoomI; i++) {
							*buf++ = r;
							*buf++ = g;
							*buf++ = b;
						}
					}
				} else {
					for (xs=0; xs<d_width; xs++) {
						for (i=0; i<zoomI; i++) {
							*buf++ = *y;
							*buf++ = *u;
							*buf++ = *v;
						}
						y++; u++; v++;
					}
				}
				buf += (b->width-d_width*zoomI)*bpp;
				y -= d_width;
				u -= d_width;
				v -= d_width;
			}
			y += img->width;
			u += img->width;
			v += img->width;
		}
	}
}

/*********************************************************************
  Scale dat->min/max according to dat->scale_min/max.
*********************************************************************/
static void prev_scale_minmax (prevImageData *dat, iwImage *img,
							   double *min, double *max)
{
	if (dat->scale_min >= 0) {
		*min = dat->min + (dat->max - dat->min)*(dat->scale_min/100);
		*max = dat->max - (dat->max - dat->min)*(dat->scale_max/100);
		if (*max < *min) *max = *min;
	} else if (dat->scale_min < -1.1) {
		*min = IW_TYPE_MIN(img);
		*max = IW_TYPE_MAX(img);
	} else {
		*min = 0;
		*max = IW_COLMAX;
	}
}

/*********************************************************************
  Check if IW_COLMAX*(max-min) can overflow.
*********************************************************************/
static BOOL prev_overflow (double gmax, double min, double max)
{
	if (gmax < G_MAXINT-10)
		return FALSE;

	if (iw_isequal(G_MAXINT, gmax)) {
		if (min < 0) {
			if (max < G_MAXINT/IW_COLMAX + min)
				return FALSE;
		} else {
			if (max-min < G_MAXINT/IW_COLMAX)
				return FALSE;
		}
	} else if (iw_isequal(G_MAXFLOAT, gmax)) {
		if (min < 0) {
			if (max < G_MAXFLOAT/IW_COLMAX + min)
				return FALSE;
		} else {
			if (max-min < G_MAXFLOAT/IW_COLMAX)
				return FALSE;
		}
	}

	return TRUE;
}

#define CONV_PLANE(type, gmin, gmax)	\
{ \
	type *spos, max = gmin, min = gmax; \
	double dif; \
	int p, x; \
	BOOL overflow; \
	guchar *dpos, val; \
 \
	/* Get min/max values */ \
	for (p=0; p<img->planes; p++) { \
		spos =  (type*)src->data[dat->planes[p]]; \
		for (x = src->width*src->height; x>0; x--) { \
			if (*spos < min) min = *spos; \
			if (*spos > max) max = *spos; \
			spos++; \
		} \
	} \
	dat->min = min; dat->max = max; \
	overflow = prev_overflow (gmax, min, max); \
 \
	/* Scale min/max values */ \
	if (dat->scale_min >= 0) { \
		dif = (double)max-min; \
		min = min + dif*(dat->scale_min/100); \
		max = max - dif*(dat->scale_max/100); \
		if (max < min) max = min; \
	} \
	dif = (double)max - min; \
 \
	/* Convert image */ \
	for (p=0; p<img->planes; p++) { \
		dpos = d[p]; \
		spos = (type*)src->data[dat->planes[p]]; \
		if (dat->scale_min < -1.1) { \
			type div; \
			if (gmin < 0) {\
				div = ((double)gmax+IW_COLCNT/2) / (IW_COLCNT/2); \
				for (x = src->width*src->height; x>0; x--) { \
					val = (guchar)(*spos++ / div + IW_COLCNT/2); \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
			} else {\
				div = ((double)gmax+IW_COLCNT) / IW_COLCNT; \
				for (x = src->width*src->height; x>0; x--) { \
					val = (guchar)(*spos++ / div); \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
			} \
		} else if (dat->scale_min < -0.1) { \
			for (x = src->width*src->height; x>0; x--) { \
				if (*spos <= 0) \
					val = 0; \
				else if (*spos >= IW_COLMAX) \
					val = IW_COLMAX; \
				else \
					val = *spos; \
				spos++; \
				dat->hist[p][val]++; \
				*dpos++ = val; \
			} \
		} else if (iw_iszero(dif)) { \
			memset (d[p], 0, src->width*src->height); \
			dat->hist[p][0] += src->width*src->height; \
		} else if (!overflow) { \
			type dif = max-min; \
			for (x = src->width*src->height; x>0; x--) { \
				if (*spos <= min) \
					val = 0; \
				else if (*spos >= max) \
					val = IW_COLMAX; \
				else \
					val = (guchar)(IW_COLMAX * (*spos - min) / dif); \
				spos++; \
				dat->hist[p][val]++; \
				*dpos++ = val; \
			} \
		} else { \
			for (x = src->width*src->height; x>0; x--) { \
				if (*spos <= min) \
					val = 0; \
				else if (*spos >= max) \
					val = IW_COLMAX; \
				else \
					val = (guchar)(IW_COLMAX * (((double)(*spos) - min) / dif)); \
				spos++; \
				dat->hist[p][val]++; \
				*dpos++ = val; \
			} \
		} \
	} \
}

#define CONV_INTERLEAVED(type, gmin, gmax) \
{ \
	type sval, *spos, max = gmin, min = gmax; \
	double dif; \
	int x, y, p; \
	int *pnr = dat->planes; \
	BOOL overflow; \
	guchar *dpos, val; \
 \
	/* Get min/max values */ \
	for (y=0; y<src->height; y++) { \
		spos = (type*)(src->data[0] + y*src->rowstride); \
		for (x=src->width; x>0; x--) { \
			for (p=img->planes; p>0; p--) { \
				sval = spos[pnr[p]]; \
				if (sval < min) min = sval; \
				if (sval > max) max = sval; \
			} \
			spos += src->planes; \
		} \
	} \
	dat->min = min; dat->max = max; \
	overflow = prev_overflow (gmax, min, max); \
 \
	/* Scale min/max values */ \
	if (dat->scale_min >= 0) { \
		dif = (double)max-min; \
		min = min + dif*(dat->scale_min/100); \
		max = max - dif*(dat->scale_max/100); \
		if (max < min) max = min; \
	} \
	dif = (double)max - min; \
 \
	/* Convert image */ \
	if (dat->scale_min < -1.1) { \
		int off; \
		type div; \
		if (gmin < 0) { \
			div = ((double)gmax+IW_COLCNT/2) / (IW_COLCNT/2); \
			off = IW_COLCNT/2; \
		} else { \
			div = ((double)gmax+IW_COLCNT) / IW_COLCNT; \
			off = 0; \
		} \
		for (y=0; y<src->height; y++) { \
			spos = (type*)(src->data[0] + y*src->rowstride); \
			dpos = d[0] + y * img->rowstride; \
 \
			for (x=src->width; x>0; x--) { \
				for (p = 0; p<img->planes; p++) { \
					val = (guchar)(spos[pnr[p]] / div + off); \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
				spos += src->planes; \
			} \
		} \
	} else if (dat->scale_min < -0.1) { \
		for (y=0; y<src->height; y++) { \
			spos = (type*)(src->data[0] + y*src->rowstride); \
			dpos = d[0] + y * img->rowstride; \
 \
			for (x=src->width; x>0; x--) { \
				for (p = 0; p<img->planes; p++) { \
					sval = spos[pnr[p]]; \
					if (sval <= 0) \
						val = 0; \
					else if (sval >= IW_COLMAX) \
						val = IW_COLMAX; \
					else \
						val = sval; \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
				spos += src->planes; \
			} \
		} \
	} else if (iw_iszero(dif)) { \
		memset (d[0], 0, src->height*img->rowstride); \
		for (p = 0; p<img->planes; p++) \
			dat->hist[p][0] += src->width*src->height; \
	} else if (!overflow) { \
		type dif = max-min; \
		for (y=0; y<src->height; y++) { \
			spos = (type*)(src->data[0] + y*src->rowstride); \
			dpos = d[0] + y * img->rowstride; \
 \
			for (x=src->width; x>0; x--) { \
				for (p = 0; p<img->planes; p++) { \
					sval = spos[pnr[p]]; \
					if (sval <= min) \
						val = 0; \
					else if (sval >= max) \
						val = IW_COLMAX; \
					else \
						val = (guchar)(IW_COLMAX * (sval - min) / dif); \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
				spos += src->planes; \
			} \
		} \
	} else { \
		for (y=0; y<src->height; y++) { \
			spos = (type*)(src->data[0] + y*src->rowstride); \
			dpos = d[0] + y * img->rowstride; \
 \
			for (x=src->width; x>0; x--) { \
				for (p = 0; p<img->planes; p++) { \
					sval = spos[pnr[p]]; \
					if (sval <= min) \
						val = 0; \
					else if (sval >= max) \
						val = IW_COLMAX; \
					else \
						val = (guchar)(IW_COLMAX * (((double)sval - min) / dif)); \
					dat->hist[p][val]++; \
					*dpos++ = val; \
				} \
				spos += src->planes; \
			} \
		} \
	} \
}

/*********************************************************************
  Return a new image of the size of src and with the number of planes
  needed for the display of src. The image must be freed by calling
  gui_release_buffer().
*********************************************************************/
static iwImage *prev_copy_image (iwImage *src, prevImageData *dat)
{
	iwImage *img;
	int p;

	p = 3;
	if (dat->planes[1] < 0) p = 1;
	img = gui_get_buffer (sizeof(iwImage) + sizeof(guchar*)*p +
						  src->width*src->height*p);

	*img = *src;
	img->planes = p;
	img->type = IW_8U;
	img->data = (guchar**)((guchar*)img + sizeof(iwImage));
	img->data[0] = (guchar*)img->data + sizeof(guchar*)*p;
	if (src->rowstride) {
		img->rowstride = img->width * p;
		memset (&img->data[1], 0, (p-1)*sizeof(guchar*));
	} else {
		for (p = 1; p < img->planes; p++)
			img->data[p] = img->data[p-1] + src->width*src->height;
	}

	return img;
}

/*********************************************************************
  Convert src->data to IW_8U and return it as a new image. The image
  must be freed by calling gui_release_buffer(). min/max-values and
  the image histogram is returned in dat.
*********************************************************************/
static iwImage *prev_convert (iwImage *src, prevImageData *dat)
{
	iwImage *img;
	guchar **d;
	int p;

	img = prev_copy_image (src, dat);
	d = img->data;

	if (src->rowstride) {
		switch (src->type) {
			case IW_8U:
				CONV_INTERLEAVED (guchar, 0, IW_COLMAX);
				break;
			case IW_16U:
				CONV_INTERLEAVED (guint16, 0, (G_MAXSHORT*2+1));
				break;
			case IW_32S:
				CONV_INTERLEAVED (gint32, G_MININT, G_MAXINT);
				break;
			case IW_FLOAT:
				CONV_INTERLEAVED (gfloat, (-G_MAXFLOAT), G_MAXFLOAT);
				break;
			case IW_DOUBLE:
				CONV_INTERLEAVED (gdouble, (-G_MAXDOUBLE), G_MAXDOUBLE);
				break;
			default:
				iw_warning ("Unable to convert %d to uchar", src->type);
		}
	} else {
		switch (src->type) {
			case IW_8U:
				CONV_PLANE (guchar, 0, IW_COLMAX);
				break;
			case IW_16U:
				CONV_PLANE (guint16, 0, (G_MAXSHORT*2+1));
				break;
			case IW_32S:
				CONV_PLANE (gint32, G_MININT, G_MAXINT);
				break;
			case IW_FLOAT:
				CONV_PLANE (gfloat, (-G_MAXFLOAT), G_MAXFLOAT);
				break;
			case IW_DOUBLE:
				CONV_PLANE (gdouble, (-G_MAXDOUBLE), G_MAXDOUBLE);
				break;
			default:
				iw_warning ("Unable to convert %d to uchar", src->type);
		}
	}
	for (p=0; p<3 && dat->planes[p]>=0; p++)
		dat->planes[p] = p;
	return img;
}

static void prev_draw_histtip (int xs, int ys, double max, int x, int y, gboolean histlog)
{
	int h = 0;
	if (y != 0) {
		if (histlog)
			h = HISTHIGH*log(y)/max;
		else
			h = HISTHIGH*y/max;
		if (h > HISTHIGH) h = HISTHIGH;
	}
	prev_drawPoint (xs+x, ys+HISTHIGH-h);
}

static void prev_draw_histval (int xs, int ys, int max, int x, int y,
							   int ylast, gboolean lines)
{
	int h = HISTHIGH*y/max, hlast;
	if (h > HISTHIGH) h = HISTHIGH;

	if (!lines)
		prev_drawFRect (xs+x, ys+HISTHIGH-h, 1, 1 + h);
	else if (ylast >= 0) {
		hlast = HISTHIGH*ylast/max;
		if (hlast > HISTHIGH) hlast = HISTHIGH;
		prev_drawLine (xs+x, ys+HISTHIGH-h, xs+x-1, ys+HISTHIGH-hlast);
	}
}

static void prev_draw_histval_log (int xs, int ys, double max, int x, int y,
								   int ylast, gboolean lines)
{
	int h = 0, hlast = 0;

	if (y != 0) {
		h = HISTHIGH*log(y)/max;
		if (h > HISTHIGH) h = HISTHIGH;
	}
	if (!lines)
		prev_drawFRect (xs+x, ys+HISTHIGH-h, 1, 1 + h);
	else if (ylast >= 0) {
		if (ylast != 0) {
			hlast = HISTHIGH*log(ylast)/max;
			if (hlast > HISTHIGH) hlast = HISTHIGH;
		}
		prev_drawLine (xs+x, ys+HISTHIGH-h, xs+x-1, ys+HISTHIGH-hlast);
	}
}

/*********************************************************************
  Show histogram dat->hist in buffer b at upper left position x,y.
  dat->hist contains img->planes different histograms.
*********************************************************************/
static void prev_draw_hist (prevBuffer *b, int x, int y, iwImage *img,
							prevImageData *dat)
{
	int i, p, max, sum, last, offset, planes;
	double vmin, vmax;
	char str[20];

	if (dat->histogram == HIST_OFF) return;

	planes = 3;
	if (dat->planes[1] < 0) planes = 1;

	/* Get max histogram value */
	if (dat->extrema)
		offset = 0;
	else
		offset = 1;
	max = 0;
	for (p=0; p<planes; p++)
		for (i=offset; i<IW_COLCNT-offset; i++)
			if (max < dat->hist[p][i])
				max = dat->hist[p][i];

	if ((dat->histogram == HIST_LOG && max < 2) ||
		(dat->histogram != HIST_LOG && max < 1)) {
		prev_drawInit (b, 255, 255, 255);
		prev_drawLine (x, y+HISTHIGH, x+IW_COLCNT, y+HISTHIGH);
	} else {
		double logmax = log(max);

		/* Draw histograms of the different planes */
		if (planes > 1) {
			for (p=planes-1; p>=0; p--) {
				prev_drawInit (b, (p==0)*255, (p==1)*255, (p==2)*255);
				last = -1;
				for (i=0; i<IW_COLCNT; i++) {
					if (dat->histogram == HIST_LOG)
						prev_draw_histval_log (x, y, logmax, i, dat->hist[p][i], last, dat->lines);
					else
						prev_draw_histval (x, y, max, i, dat->hist[p][i], last, dat->lines);
					last = dat->hist[p][i];
				}
			}
		}

		/* Draw intensity histogram */
		prev_drawInit (b, 255, 255, 255);
		last = -1;
		for (i=0; i<IW_COLCNT; i++) {
			sum = dat->hist[0][i];
			for (p=1; p<planes; p++) sum += dat->hist[p][i];
			sum /= planes;
			if (dat->histogram == HIST_LOG)
				prev_draw_histval_log (x, y, logmax, i, sum, last, dat->lines);
			else
				prev_draw_histval (x, y, max, i, sum, last, dat->lines);
			last = sum;
		}

		/* Draw histogram tips */
		if (!dat->lines && planes > 1) {
			prev_drawInit (b, 160, 160, 164);
			if (dat->histogram != HIST_LOG) logmax = max;
			for (p=planes-1; p>=0; p--) {
				for (i=0; i<IW_COLCNT; i++)
					prev_draw_histtip (x, y, logmax, i, dat->hist[p][i], dat->histogram == HIST_LOG);
			}

			/* Draw tips for the intensity histogram */
			for (i=0; i<IW_COLCNT; i++) {
				sum = dat->hist[0][i];
				for (p=1; p<planes; p++) sum += dat->hist[p][i];
				prev_draw_histtip (x, y, logmax, i, sum/planes, dat->histogram == HIST_LOG);
			}
		}
	}
	prev_drawInit (b, 255, 255, 255);
	prev_drawLine (x+IW_COLCNT, y, x+IW_COLCNT, y+HISTHIGH+1);
	prev_drawLine (x, y+HISTHIGH+1, x+IW_COLCNT-1, y+HISTHIGH+1);

	/* Show histogram-min/max values */
	if (img->type != IW_8U || dat->scale_min > 0 || dat->scale_max > 0) {
		prev_scale_minmax (dat, img, &vmin, &vmax);
	} else {
		vmin = 0;
		vmax = IW_COLMAX;
	}
	if (!finite(vmax - vmin) || iw_isequal(vmin, vmax) || vmax - vmin >= 100 ||
		(img->type != IW_FLOAT && img->type != IW_DOUBLE)) {
		p = 0;
	} else {
		int i = 1/(0.101*(vmax-vmin));
		p = 1;
		while (i > 0) {
			p++;
			i = i/10;
		}
	}

	sprintf (str, "%d", max);
	prev_drawString (x+IW_COLCNT+1, y, str);

	y += HISTHIGH+2;
	if (fabs(vmin) > 99999999999.0)
		sprintf (str, "%e", vmin);
	else
		sprintf (str, "%.*f", p, vmin);
	prev_drawString (x, y, str);

	if (fabs(vmax) > 99999999999.0)
		sprintf (str, "%e", vmax);
	else
		sprintf (str, "%.*f", p, vmax);
	prev_drawString (x+IW_COLCNT, y, str);
}

static void parse_planes (prevBuffer *b, const prevDataImage *dImg,
						  char *splanes, int *dplanes)
{
	iwImage *img = dImg->i;
	int pcnt = img->planes;
	int i;

	dplanes[0] = 0;
	dplanes[1] = 1;
	dplanes[2] = 2;

	i = sscanf (splanes, "%d %d %d", &dplanes[0], &dplanes[1], &dplanes[2]);
	if (   i == 1
		|| b->gray || img->ctab == IW_GRAY || pcnt < 2
		|| img->ctab > IW_COLFORMAT_MAX) {
		dplanes[1] = dplanes[2] = -1;
	} else if (i == 2) {
		/* Render two planes by repeating the second */
		dplanes[2] = dplanes[1];
	}
	for (i=0; i<3 && dplanes[i]>=0; i++) {
		if (dplanes[i] >= pcnt)
			dplanes[i] = pcnt-1;
		if (!img->rowstride && !img->data[dplanes[i]])
			dplanes[i] = -1;
	}
	if (dplanes[0] < 0)
		dplanes[0] = 0;
}

/*********************************************************************
  Display image dImg in buffer b.
*********************************************************************/
void prev_render_image (prevBuffer *b, const prevDataImage *dImg, int disp_mode)
{
	prevDataImage dImg_b;
	prevImageData dat;
	prevImageOpts **opts;
	iwImage *img_old = NULL;

	if (!dImg || !dImg->i || dImg->i->planes <= 0 || dImg->i->width <= 0 || dImg->i->height <= 0)
		return;

	memset (&dat, 0, sizeof(prevImageData));
	opts = (prevImageOpts**)prev_opts_get(b, PREV_IMAGE);
	if (opts) {
		dat.min = 1;
		dat.max = 0;
		dat.scale_min = (*opts)->scale_min;
		dat.scale_max = (*opts)->scale_max;
		dat.histogram = (*opts)->histogram;
		dat.lines = (*opts)->lines;
		dat.extrema = (*opts)->extrema;
		parse_planes (b, dImg, (*opts)->planes, dat.planes);
	}

	dImg_b = *dImg;
	if (dImg_b.i->type != IW_8U || dat.scale_min > 0 || dat.scale_max > 0) {
		img_old = dImg_b.i;
		dImg_b.i = prev_convert (dImg_b.i, &dat);
	}

	if (dImg_b.i->rowstride)
		prev_render_interleaved (b, &dImg_b, &dat);
	else
		prev_render_planed (b, &dImg_b, &dat);

	if (img_old) {
		gui_release_buffer (dImg_b.i);
		dImg_b.i = img_old;
	}

	if (opts && (*opts)->show) {
		int xs, ys, x, y, p;
		float zoom;
		char str[60];

		/* Show histogramn and meta info */
		zoom = prev_drawInitFull (b, 255,255,255, disp_mode);

		/* Place info at a visible position inside the image */
		xs = ys = 5;

		x = (dImg_b.x + dImg_b.i->width - b->x)*zoom;
		y = (dImg_b.y + dImg_b.i->height - b->y)*zoom;
		if (xs + IW_COLCNT >= x) xs = x-IW_COLCNT-1;
		if (ys + HISTHIGH+1 >= y) ys = y-HISTHIGH-2;

		x = (dImg_b.x - b->x)*zoom;
		y = (dImg_b.y - b->y)*zoom;
		if (xs < x) xs = x;
		if (ys < y) ys = y;

		/* Anything inside the window? */
		if (xs+IW_COLCNT > 0 && ys+HISTHIGH > 0 && xs < b->width && ys < b->height) {
			if (dImg_b.i->type == IW_8U && dat.max < dat.min) {
				/* Get min/max values and histogram for uchar images */
				int *pnr = dat.planes;
				guchar max = 0, min = 255, *spos;
				int planes = 3;
				if (pnr[1] < 0) planes = 1;
				if (dImg_b.i->rowstride) {
					for (y=0; y<dImg_b.i->height; y++) {
						spos = (guchar*)(dImg_b.i->data[0] + y*dImg_b.i->rowstride);
						for (x=dImg_b.i->width; x>0; x--) {
							for (p=0; p<planes; p++) {
								guchar sval = spos[pnr[p]];
								if (sval < min) min = sval;
								if (sval > max) max = sval;
								dat.hist[p][sval]++;
							}
							spos += dImg_b.i->planes;
						}
					}
				} else {
					for (p=0; p<planes; p++) {
						spos = (guchar*)dImg_b.i->data[pnr[p]];
						for (x=dImg_b.i->width*dImg_b.i->height; x>0; x--) {
							if (*spos < min) min = *spos;
							if (*spos > max) max = *spos;
							dat.hist[p][*spos]++;
							spos++;
						}
					}
				}
				dat.min = min;
				dat.max = max;
			}

			prev_draw_hist (b, xs, ys, dImg_b.i, &dat);

			prev_drawInit (b, 255,255,255);
			sprintf (str, "%dx%d %s\nmin %g max %g",
					 dImg_b.i->width, dImg_b.i->height,
					 (dImg_b.i->planes < 3 && dImg_b.i->ctab <= IW_COLFORMAT_MAX)
					 ? "GRAY" : IW_COLOR_TEXT(dImg_b.i),
					 dat.min, dat.max);
			prev_drawString (xs, ys, str);
		}
	}
}

#if defined(WITH_PNG) || defined(WITH_JPEG)
/*********************************************************************
  Convert IW_8U image src to RGB / 1 plane gray and return it.
  If copy_img src is not changed, otherwise src must have 3 planes.
*********************************************************************/
static iwImage *prev_convert_color (iwImage *src, prevImageData *dat, BOOL copy_img)
{
	int *pnr = dat->planes;
	iwImage *img;
	int p, x, y;
	void (*c_trans) (guchar s1, guchar s2, guchar s3, guchar *r, guchar *g, guchar *b) = NULL;

	if (copy_img) {
		img = prev_copy_image (src, dat);
	} else
		img = src;

	if (src->ctab == IW_HSV)
		c_trans = prev_hsvToRgb;

	if (img->rowstride) {
		guchar *s = src->data[0], *d = img->data[0];
		p = src->planes;
		if (img->planes < 3) s += pnr[0];

		for (y = 0; y < img->height; y++) {
			if (img->planes < 3) {
				for (x = 0; x < img->width; x++) {
					*d++ = *s;
					s += p;
				}
			} else if (src->ctab == IW_YUV) {
				for (x = 0; x < img->width; x++) {
					prev_inline_yuvToRgbVis (s[pnr[0]], s[pnr[1]], s[pnr[2]],
											 d, d+1, d+2);
					s += p;
					d += 3;
				}
			} else if (src->ctab == IW_BGR) {
				for (x = 0; x < img->width; x++) {
					*d++ = s[pnr[2]];
					*d++ = s[pnr[1]];
					*d++ = s[pnr[0]];
					s += p;
				}
			} else if (c_trans) {
				for (x = 0; x < img->width; x++) {
					c_trans (s[pnr[0]], s[pnr[1]], s[pnr[2]], d, d+1, d+2);
					s += p;
					d += 3;
				}
			} else {
				for (x = 0; x < img->width; x++) {
					*d++ = s[pnr[0]];
					*d++ = s[pnr[1]];
					*d++ = s[pnr[2]];
					s += p;
				}
			}
			s += img->rowstride - img->width*p;
		}
	} else {
		guchar *rs = src->data[pnr[0]], *gs = src->data[pnr[1]], *bs = src->data[pnr[2]];
		guchar *rd = img->data[0], *gd = img->data[1], *bd = img->data[2];

		if (img->planes < 3) {
			memcpy (rd, rs, img->width*img->height);
		} else if (src->ctab == IW_YUV) {
			for (x = img->width*img->height; x>0; x--)
				prev_inline_yuvToRgbVis (*rs++, *gs++, *bs++, rd++, gd++, bd++);
		} else if (src->ctab == IW_BGR) {
			for (x = img->width*img->height; x>0; x--) {
				*rd++ = *bs++;
				*gd++ = *gs++;
				*bd++ = *rs++;
			}
		} else if (c_trans) {
			for (x = img->width*img->height; x>0; x--)
				c_trans (*rs++, *gs++, *bs++, rd++, gd++, bd++);
		} else {
			memcpy (rd, rs, img->width*img->height);
			memcpy (gd, gs, img->width*img->height);
			memcpy (bd, bs, img->width*img->height);
		}
	}
	img->ctab = img->planes<3 ? IW_GRAY:IW_RGB;

	return img;
}
#endif

/*********************************************************************
  Render the image img according the data in save.
*********************************************************************/
iwImgStatus prev_render_image_vector (prevBuffer *b, prevVectorSave *save,
									  const prevDataImage *dImg, int disp_mode)
{
#if defined(WITH_PNG) || defined(WITH_JPEG)
	float zoom = save->zoom;
	char *fname, *pos, *fpart;
	int *num, *pnr;
	iwImgStatus status = IW_IMG_STATUS_OK;
	prevImageData dat;
	prevImageOpts **opts;
	iwImage *img;

	if (save->format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (!dImg || !dImg->i || dImg->i->planes <= 0 || dImg->i->width <= 0 || dImg->i->height <= 0)
		return IW_IMG_STATUS_OK;

	if (! (num = (int*)gui_data_get (save->rdata, "image"))) {
		num = (int*)gui_data_set (&save->rdata, "image",
								  GINT_TO_POINTER(0), NULL);
	}
	(*num)++;

	fname = malloc (strlen(save->fname)+20);
	strcpy (fname, save->fname);
	if ((fpart = strrchr (fname, '/')))
		fpart++;
	else
		fpart = fname;

	memset (&dat, 0, sizeof(prevImageData));
	opts = (prevImageOpts**)prev_opts_get(b, PREV_IMAGE);
	if (opts) {
		dat.min = 1;
		dat.max = 0;
		dat.scale_min = (*opts)->scale_min;
		dat.scale_max = (*opts)->scale_max;
		dat.histogram = (*opts)->histogram;
		dat.lines = (*opts)->lines;
		dat.extrema = (*opts)->extrema;
		parse_planes (b, dImg, (*opts)->planes, dat.planes);
	}
	if (dImg->i->type != IW_8U || dat.scale_min > 0 || dat.scale_max > 0)
		img = prev_convert (dImg->i, &dat);
	else
		img = dImg->i;

	pnr = dat.planes;
	if ( ( img->planes > 1 || pnr[1] >= 0 ) &&
		 ( img->ctab != IW_RGB || img->planes != 3 ||
		   pnr[0] != 0 || pnr[1] != 1 || pnr[2] != 2 ) )
		img = prev_convert_color (img, &dat, img == dImg->i);

	if (!(pos = strrchr (fpart, '.')))
		pos = fname+strlen(fname);
#if defined(WITH_PNG)
	sprintf (pos, "-svg%d.png", *num);
	status = iw_img_save (img, IW_IMG_FORMAT_PNG, fname, NULL);
#else
	sprintf (pos, "-svg%d.jpg", *num);
	status = iw_img_save (img, IW_IMG_FORMAT_JPEG, fname, NULL);
#endif
	if (status == IW_IMG_STATUS_OK)
		fprintf (save->file,
				 "  <image x=\"%ld\" y=\"%ld\" width=\"%ld\" height=\"%ld\"\n"
				 "         xlink:href=\"%s\" />\n",
				 gui_lrint(dImg->x*zoom), gui_lrint(dImg->y*zoom),
				 gui_lrint(img->width*zoom), gui_lrint(img->height*zoom),
				 fpart);

	if (img != dImg->i)
		gui_release_buffer (img);
	free (fname);
	return status;
#else
	return IW_IMG_STATUS_FORMAT;
#endif
}
