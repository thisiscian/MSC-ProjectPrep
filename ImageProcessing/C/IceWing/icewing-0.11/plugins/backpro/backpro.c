/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Daniel Westhoff and Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Applied Computer Science, Faculty of Technology, Bielefeld University, Germany
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
#include <math.h>
#include "gui/Goptions.h"
#include "gui/Grender.h"
#include "gui/Ggui.h"
#include "main/image.h"
#include "gsimple/skin.h"
#include "tracking/handtrack.h"

#define HISTSIZE		64
#define HIST_RG2IND		(SKIN_SCALE / HISTSIZE)
#define ZOOM			4

typedef struct {
	BOOL init_bpro;
	BOOL dynamic_update;
	int region_stretch;		/* Pixels by which the an update region should be stretched */
	int rect_x;
	int rect_y;
} bproValues;

/* Variables which can be modified in the gui */
static bproValues opt_values;

/* Buffer for an image,
   init with prev_new_window(), display with prev_...() */
static prevBuffer *b_histogram;
static prevBuffer *b_skin_histogram;
static prevBuffer *b_ratio_histogram;

/* 2D-Histogram for the image */
static int histogram[HISTSIZE][HISTSIZE];

/* 2D-Histogram for all kalman-regions */
static int skin_histogram[HISTSIZE][HISTSIZE];

/* Ratio-histogram for backprojection */
static int ratio_histogram[HISTSIZE][HISTSIZE];

static skinOptions *skin_options;

/*********************************************************************
  Draw 2D-histogram.
  norm == TRUE: normalize hist to the range 0-255
*********************************************************************/
static void bpro_draw_histogram (int *hist, prevBuffer *buf, BOOL norm)
{
	int r, g, *pos;
	int max = 255;

	if (buf->window) {
		/* Get maximum in histogram */
		if (norm) {
			max = 0;
			pos = hist;
			r = HISTSIZE*HISTSIZE;
			while (r--) {
				if (*pos>max) max = *pos;
				pos++;
			}
			if (!max) max = 255;
		}

		prev_buffer_lock();

		/* Draw the contrast scaled histogram */
		if (buf->width >= HISTSIZE*ZOOM && buf->height >= HISTSIZE*ZOOM) {
			pos = hist;
			for (r=0; r<HISTSIZE; r++)
				for (g=0; g<HISTSIZE; g++) {
					if (r+g == HISTSIZE)		/* Draw Line */
						prev_drawInit (buf, 127,0,0);
					else
						prev_drawInit (buf, (*pos)*255/max,0,0);
					prev_drawFRect_gray_nc (r*ZOOM, g*ZOOM, ZOOM, ZOOM);
					pos++;
				}
		}
		prev_buffer_unlock();

		prev_draw_buffer (buf);
	}
}

/*********************************************************************
 Convert YUV-Pixel to an index into a histogram.
*********************************************************************/
static void bpro_yuv2ind (uchar y, uchar u, uchar v, int *rd ,int *gd)
{
	float rgb;

	/* prev_yuvToRgbVis (y,u,v,&r,&g,&b); */
	float Y, U, V;
	int r, g, b;

	Y = 1.164*(y - 16);
	U = u - 128;
	V = v - 128;

	r = (int)(Y+1.597*V+0.5);
	g = (int)(Y-0.392*U-0.816*V+0.5);
	b = (int)(Y+2.018*U+0.5);

	r = r < 0 ? 0 : (r > 255 ? 255: r);
	g = g < 0 ? 0 : (g > 255 ? 255: g);
	b = b < 0 ? 0 : (b > 255 ? 255: b);

	/* Calculate rg-values */
	rgb = r+g+b;
	if (rgb > 0) {
		*rd = r*(HISTSIZE-1) / rgb;
		*gd = g*(HISTSIZE-1) / rgb;
	} else {
		*rd = 0;
		*gd = 0;
	}
}

/*********************************************************************
  Generate 2D-histogram for the image '**s'
*********************************************************************/
static void bpro_calc_histogram (uchar **s, int width, int height)
{
	int r, g, count;
	uchar *y=s[0], *u=s[1], *v=s[2];

	iw_debug (4,"Calculating 2d-histogramm...");

	/* Add all rg-values to histogram */
	memset (histogram, 0, HISTSIZE*HISTSIZE*sizeof(int));
	for (count = width*height; count>0; count--) {
		bpro_yuv2ind (*y++, *u++, *v++, &r, &g);
		histogram[r][g]++;
	}
}

/*********************************************************************
  Calculate the ratio histogram by deviding skin_histogram through
  histogram.
*********************************************************************/
static void bpro_calc_ratio_histogram (void)
{
	int r, g, max = 0;
	float max_norm;

	/* Calculate ratio-histogram and find maximum */
	memset (ratio_histogram, 0, HISTSIZE*HISTSIZE*sizeof(int));
	for (r=0;r<HISTSIZE;r++)
		for (g=0;g<HISTSIZE;g++)
			if (histogram[r][g] > 0) {
				ratio_histogram[r][g] = (skin_histogram[r][g]*255) / histogram[r][g];
				if (ratio_histogram[r][g] > max)
					max = ratio_histogram[r][g];
			}

	/* Normalize ratio-histogram to 0..255 */
	max_norm = (float)max / 255;
	if (max > 0) {
		for (r=0;r<HISTSIZE;r++)
			for (g=0;g<HISTSIZE;g++)
				ratio_histogram[r][g] = ratio_histogram[r][g]/max_norm;
	}
}

/*********************************************************************
  Called before any other bpro_*_mix()-calls.
  RETURN: FALSE: bpro_*_mix() should not be called.
*********************************************************************/
static BOOL bpro_do_mix (void)
{
	if (!opt_values.dynamic_update)
		return FALSE;

	memset (skin_histogram, 0, HISTSIZE*HISTSIZE*sizeof(int));
	return TRUE;
}

/*********************************************************************
  Update skin-histogram
*********************************************************************/
static void bpro_update_mix (iwImage *img, uchar *y, uchar *u, uchar *v, int cnt, long kal_nr)
{
	t_Pixel rg;
	int i;

	for (i=0; i<cnt; i++) {					/* Go through the given line */
		iw_skin_yuv2rg (*y++, *u++, *v++, &rg);
		if (iw_skin_pixelIsSkin (skin_options, &rg)) {
			/* Add to the histogramm */
			skin_histogram[(int)(rg.r/HIST_RG2IND)][(int)(rg.g/HIST_RG2IND)]++;
		}
	}
}

/*********************************************************************
  Callback function for the b_region window.
*********************************************************************/
static void cb_button_event (prevBuffer *b, prevEvent signal, int x, int y, void *data)
{
	if (!opt_values.init_bpro) return;

	opt_values.rect_x = x;
	opt_values.rect_y = y;
}

/*********************************************************************
  Trivial initialisation of skin-histogram by showing some skin
  to the camera
*********************************************************************/
static void bpro_init_skin_histogram (uchar **s, int width, int height)
{
	int radius = 20;
	int i, j, c, r, g, count = width*height;
	prevBuffer *input = grab_get_inputBuf();
	uchar *y=s[0], *u=s[1], *v=s[2];
	int xl, xr, yl, yr;

	if (opt_values.rect_x < 0)
		xl = width/2 - radius;
	else
		xl = opt_values.rect_x - radius;
	if (xl<0) xl = 0;
	else if (xl+radius*2 >= width) xl = width-radius*2-1;

	if (opt_values.rect_y < 0)
		yl = height/2 - radius;
	else
		yl = opt_values.rect_y - radius;
	if (yl<0) yl = 0;
	else if (yl+radius*2 >= height) yl = height-radius*2-1;

	xr = xl + radius*2;
	yr = yl + radius*2;

	iw_debug (4,"Initialisation of skin-histogram: X: %d - %d, Y: %d - %d",
			  xl, xr, yl, yr);

	if (input->window) {
		prevDataRect rect;

		rect.ctab = IW_INDEX;
		rect.r = rot;
		rect.x1 = xl;
		rect.y1 = yl;
		rect.x2 = xr;
		rect.y2 = yr;

		prev_render_rects (input, &rect, 1, 0, width, height);
	}

	memset (skin_histogram, 0, HISTSIZE*HISTSIZE*sizeof(int));
	for (c=1; c<=count; c++) {
		i = c / width;
		j = c - (i * width);
		if ((j > xl) && (j < xr) && (i < yr) && (i > yl)) {
			/* YUV -> RG calculation */
			bpro_yuv2ind (*y, *u, *v, &r, &g);

			/* Add rg to skin-histogram */
			skin_histogram[r][g]++;
		}
		/* Next yuv-value */
		y++;
		u++;
		v++;
	}
}

/*********************************************************************
  Classify one pixel.
*********************************************************************/
static uchar pixelclassify (uchar y, uchar u, uchar v)
{
	t_Pixel rg;

	/* Calculate rg-value of yuv-pixel */
	iw_skin_yuv2rg (y, u, v, &rg);

	if (iw_skin_pixelIsSkin (skin_options, &rg)) {
		return (ratio_histogram[(int)(rg.r/HIST_RG2IND)][(int)(rg.g/HIST_RG2IND)]);
	} else
		return 0;
}

/*********************************************************************
  Color classification of img. Write median() smoothed result
  (mask size: size*2+1) to d.
  pix_cnt=0,1,2 -> calculate average over 1,4,5 pixels
*********************************************************************/
static BOOL bpro_classify (iwImage *img, uchar *d, int pix_cnt, int size, int thresh)
{
	uchar *y=img->data[0], *u=img->data[1], *v=img->data[2], *dpos, *buffer;
	int width = img->width, height = img->height;
	int count = width*height;

	dpos = buffer = iw_img_get_buffer (count);

	/* Trivial initialisation for skin-histogramm */
	if (opt_values.init_bpro)
		bpro_init_skin_histogram (img->data, width, height);

	/* Calculate ratio-histogram for classification */
	bpro_calc_ratio_histogram();

	/* Draw histogram visualisation */
	bpro_draw_histogram ((int*)histogram, b_histogram, TRUE);
	bpro_draw_histogram ((int*)skin_histogram, b_skin_histogram, TRUE);
	bpro_draw_histogram ((int*)ratio_histogram, b_ratio_histogram, FALSE);

	if (size == 0)
		dpos = d;

	if (pix_cnt==0) {
		while (count--) {
			*dpos++ = pixelclassify (*y, *u, *v);
			y++;
			u++;
			v++;
		}
	} else {
		int i,k, r,g,b;
		if (!size) iw_img_border (dpos,width,height,1);
		y+=width+1; u+=width+1; v+=width+1; dpos+=width+1;
		for (i=1; i<height-1; i++) {
			for (k=1; k<width-1; k++) {
				if (pix_cnt==2) {
					r = (*y + *(y-1) + *(y+1) + *(y-width) + *(y+width)) / 5;
					g = (*u + *(u-1) + *(u+1) + *(u-width) + *(u+width)) / 5;
					b = (*v + *(v-1) + *(v+1) + *(v-width) + *(v+width)) / 5;
				} else {
					r = (*(y-1) + *(y+1) + *(y-width) + *(y+width)) / 4;
					g = (*(u-1) + *(u+1) + *(u-width) + *(u+width)) / 4;
					b = (*(v-1) + *(v+1) + *(v-width) + *(v+width)) / 4;
				}
				*dpos++ = pixelclassify (r,g,b);
				y++; u++; v++;
			}
			y+=2; u+=2; v+=2; dpos+=2;
		}
	}
	if (size>0)
		iw_img_median (buffer, d, width, height, size);

	iw_img_release_buffer();

	/* Generate 2D-histogram for 'img' */
	bpro_calc_histogram (img->data, width, height);

	return TRUE;
}

/*********************************************************************
  Updating the BackProjection classifier taking into account the
  classified regions.
*********************************************************************/
static void bpro_update (iwImage *img, int *ireg, iwRegion *regions, int numregs)
{
	int i, j;
	BOOL first = TRUE;

	/* Update the gauss/bpro-classifier with the image pixels
	   which are identified as hand pixels */
	for (i=0; i<numregs; i++) {
		if (track_is_valid (regions[i].judge_kalman)) {
			Polygon_t *p = &regions[i].r.polygon;
			prevDataPoint *pts;

			if (first) {
				if (!bpro_do_mix()) break;
				first = FALSE;
			}

			iw_reg_stretch (img->width, img->height, &regions[i], opt_values.region_stretch);
			pts = malloc(sizeof(prevDataPoint)*p->n_punkte);
			for (j=0; j<p->n_punkte; j++) {
				pts[j].x = p->punkt[j]->x;
				pts[j].y = p->punkt[j]->y;
			}
			iw_img_fillPoly (img, p->n_punkte, pts, FALSE,
							 (imgLineFunc)bpro_update_mix, (void*)(long)regions[i].id);
			free (pts);
		}
	}
}

/*********************************************************************
  Free the resources allocated during bpro_???().
*********************************************************************/
static void bpro_cleanup (plugDefinition *plug) {}

static void help (plugDefinition *plug)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by Daniel Westhoff/Frank Loemker\n"
			 "Perform color classification based on back projection.\n"
			 "Needs the skinclass plugin.\n"
			 "\n"
			 "Usage of the %s plugin: no options (yet)\n",
			 plug->name, ICEWING_NAME, plug->name);
	gui_exit (1);
}

/*********************************************************************
  Initialise the alternative classification.
  'para': command line parameter
  argc, argv: plugin specific options
*********************************************************************/
#define ARG_TEMPLATE "-H:H"
static void bpro_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
{
	void *arg;
	char ch;
	int nr = 0;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	plug_function_register (plug, "classify", (plugFunc)bpro_classify);
	plug_function_register (plug, "update", (plugFunc)bpro_update);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int bpro_init_options (plugDefinition *plug)
{
	int page;

	opt_values.init_bpro = FALSE;
	opt_values.dynamic_update = TRUE;
	opt_values.region_stretch = 10;
	opt_values.rect_x = -1;
	opt_values.rect_y = -1;

	page = opts_page_append ("BackPro");
	opts_toggle_create (page,"Init BackPro", "Show some skin to the camera!",
						&opt_values.init_bpro);
	opts_toggle_create (page,"Dynamic update",
						"Update the skin histogram continuously",
						&opt_values.dynamic_update);
	opts_entscale_create (page,"Region stretch",
						  "Number of pixels by which an update region should be stretched",
						  &opt_values.region_stretch, 0, 40);
	skin_options = iw_skin_init (page);

	b_histogram       = prev_new_window ("BackPro 2D-Histogram",
										 ZOOM*HISTSIZE, ZOOM*HISTSIZE, TRUE, FALSE);
	b_skin_histogram  = prev_new_window ("BackPro Skin-Histogram",
										 ZOOM*HISTSIZE, ZOOM*HISTSIZE, TRUE, FALSE);
	b_ratio_histogram = prev_new_window ("BackPro Ratio-Histogram",
										 ZOOM*HISTSIZE, ZOOM*HISTSIZE, TRUE, FALSE);

	prev_signal_connect (grab_get_inputBuf(), PREV_BUTTON_PRESS|PREV_BUTTON_MOTION,
						 cb_button_event, NULL);

	memset (skin_histogram, 0, HISTSIZE*HISTSIZE*sizeof(int));

	return page;
}

plugDefinition plug_backpro = {
	"BackPro",
	PLUG_ABI_VERSION,
	bpro_init,
	bpro_init_options,
	bpro_cleanup,
	NULL
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_bpro_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_backpro;
}
