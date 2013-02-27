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
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "gui/Goptions.h"
#include "gui/Ggui.h"
#include "tools/tools.h"
#include "main/image.h"
#include "main/output.h"
#include "main/plugin.h"
#include "skin.h"
#include "skinclass/skinclass.h"
#include "gsimple_image.h"

#define COLCNT				256
#define LOOK_SIZE			2097152
#define GSIMPLE_FILE		"gauss.data"
#define GSIMPLE_FILE_UPDATE	"gauss_update.data"

#define RECTSIZE	50

/* String used for status reporting */
#define STATUS_UPDATE		"classUpdate"

typedef enum {
	GSIMPLE_UPDATE,
	GSIMPLE_CLASS
} gsimpleState;

typedef struct {
	BOOL init_gsimple;
	BOOL calc_lookup;
	int save;
	int class_mode;
	float var_scale;
	float var_scale_kov;
	float var_add;
	int threshold;

	int fill_minsize;
	int iterations;
	int reg_shrink;
	BOOL background;
	int diff_thresh;
	int diff_back;
	int rect_x;
	int rect_y;
	BOOL update_wait;
	int update_cont;
} gsimpleValues;

static gsimpleValues opt_values;
static prevBuffer *b_region, *b_gauss, *b_background, *b_diff;

typedef struct {
	double mean_r;
	double mean_g;
	double inv_cov_mat_11;
	double inv_cov_mat_22;
	double inv_cov_mat_12;
	double sig_11;
	double sig_22;
	double sig_12;
	double r, g, rr, gg, rg;
	int count;
} t_gauss;

static t_gauss gs_gauss;
static uchar gs_lookup[LOOK_SIZE];
static gsimpleState gs_state = GSIMPLE_CLASS;

static skinOptions *skin_options;

static uchar gauss_calc_probability (t_gauss *g, t_Pixel rg)
{
	double dev_r = rg.r - g->mean_r;
	double dev_g = rg.g - g->mean_g;
	double help1 = (dev_r * g->inv_cov_mat_11) + (dev_g * g->inv_cov_mat_12);
	double help2 = (dev_r * g->inv_cov_mat_12) + (dev_g * g->inv_cov_mat_22);
	double w = expf ( -0.5 * (help1 * dev_r + help2 * dev_g) );
	if (w >= 0 && w <= 1)
		return (uchar)(w*255);
	else
		return 0;
}

/*********************************************************************
  Init gaussian parameters with zeros.
*********************************************************************/
static void gauss_clear (t_gauss *g)
{
	g->r = 0;
	g->g = 0;
	g->rr = 0;
	g->gg = 0;
	g->rg = 0;
	g->count = 0;
}

static void gauss_add_pixel (t_gauss *g, t_Pixel rg)
{
	g->count++;
	g->r += rg.r;
	g->g += rg.g;
	g->rr += rg.r * rg.r;
	g->gg += rg.g * rg.g;
	g->rg += rg.r * rg.g;
}

/*********************************************************************
  Calculate the invers of the covariance matrix:
  1. Check det(covariance_matrix) != 0
  2. If TRUE, matrix is invertible, calculate the invers
	 (here: use of equation 2.11.6, 'Numerical Recipes in C',
			p. 103, some simplifications because of
			(sig_12 == sig_21) => (inv_cov_mat_12 == inv_cov_mat_21))
*********************************************************************/
static void gauss_invert (t_gauss *g)
{
	double det = g->sig_11 * g->sig_22 - g->sig_12 * g->sig_12;

	if (det != 0.0 && g->sig_11 != 0.0) {
		det = 1.0/det;

		g->inv_cov_mat_12 = - g->sig_12 * det;
		g->inv_cov_mat_11 = (1.0 + g->sig_12 * g->sig_12 * det) / g->sig_11;
		g->inv_cov_mat_22 = g->sig_11 * det;
		/* Print the invers of the covariance matrix */
		/* iw_debug (3, "invers =\n"
					 "            %2f %2f\n"
					 "            %2f %2f",
					 g->inv_cov_mat_11, g->inv_cov_mat_12, g->inv_cov_mat_12, g->inv_cov_mat_22);
		*/
	} else if (g->inv_cov_mat_11 == 0.0)
		iw_warning ("Covariance matrix not invertible (det(A)=0)");
}

static void gauss_calc (t_gauss *g)
{
	if (g->count != 0) {
		/* Calculate mean-value */
		g->mean_r = g->r/g->count;
		g->mean_g = g->g/g->count;

		/* Calculate covarianz matrix */
		g->sig_11 = g->rr/g->count - g->mean_r * g->mean_r;
		g->sig_12 = g->rg/g->count - g->mean_r * g->mean_g;
		g->sig_22 = g->gg/g->count - g->mean_g * g->mean_g;

		g->sig_11 = g->sig_11 * opt_values.var_scale + opt_values.var_add;
		g->sig_12 = g->sig_12 * opt_values.var_scale_kov;
		g->sig_22 = g->sig_22 * opt_values.var_scale + opt_values.var_add;

		/* Print the covariance matrix */
		iw_debug (4, "mean: %f %f", g->mean_r,  g->mean_g);
		iw_debug (4, "count: %d varianz =\n"
				  "                %2f %2f\n"
				  "                %2f %2f",
				  g->count, g->sig_11, g->sig_12, g->sig_12, g->sig_22);

		/* Invert covariance matrix */
		gauss_invert (g);
	} else
		iw_warning ("Number of pixels == 0");
}

/*********************************************************************
  Classify one pixel.
*********************************************************************/
static uchar pixelclassify (t_gauss *g, uchar y, uchar u, uchar v)
{
	t_Pixel rg;

	iw_skin_yuv2rg (y, u, v, &rg);

	/* Calculate probability for non-white pixel in skinlocus */
	if (iw_skin_pixelIsSkin (skin_options, &rg))
		return gauss_calc_probability (g, rg);
	else
		return 0;
}

static void calc_lookup (t_gauss *g)
{
	int i;

	for (i=0; i<LOOK_SIZE; ++i)
		gs_lookup[i] = pixelclassify (g, ((i & 0x1fc000) >> 14)*2,
									  ((i & 0x3f80) >> 7)*2,
									  (i & 0x7f)*2);
}


static BOOL draw_started = FALSE;
static int draw_zoom = 0;
static prevBuffer *draw_buf;
static BOOL gauss_draw_start (prevBuffer *buf)
{
	if (buf->window) {
		int z;

		prev_buffer_lock();

		z = buf->height / COLCNT;
		draw_zoom = buf->width / COLCNT;
		if (z < draw_zoom) draw_zoom = z;

		if (draw_zoom > 0) {
			draw_started = TRUE;
			draw_buf = buf;
			return TRUE;
		}

		prev_buffer_unlock();
	}
	return FALSE;
}
static void gauss_draw_stop (void)
{
	if (draw_started) {
		prev_buffer_unlock();
		draw_started = FALSE;
		prev_draw_buffer (draw_buf);
	}
}
static void gauss_draw_lock (void)
{
	if (draw_started) prev_buffer_lock();
}
static void gauss_draw_unlock (void)
{
	if (draw_started) prev_buffer_unlock();
}
static void gauss_draw_clear (void)
{
	if (draw_started) memset (draw_buf->buffer, 0,
							  draw_buf->width*draw_buf->height*3);
}
static void gauss_draw_pixel (t_Pixel rg, uchar col)
{
	if (draw_started) {
		prev_drawInit (draw_buf, col, -1, -1);
		prev_drawFRect_color_nc (rg.r*draw_zoom*COLCNT/SKIN_SCALE,
								 rg.g*draw_zoom*COLCNT/SKIN_SCALE,
								 draw_zoom, draw_zoom);
	}
}
static void gauss_draw_dist (t_gauss *g)
{
	int x, y, max;
	t_Pixel rg;

	if (draw_started) {
		max = draw_zoom*COLCNT;
		for (y=0; y<max; y++) {
			rg.g = y*SKIN_SCALE/max;
			for (x=0; x<max; x++) {
				rg.r = x*SKIN_SCALE/max;
				if (rg.r+rg.g >= SKIN_SCALE) {
					prev_drawInit (draw_buf, 0, 0, 0);
				} else if (iw_skin_pixelIsSkin (skin_options, &rg)) {
					prev_drawInit (draw_buf, -1, gauss_calc_probability (g, rg),0);
				} else if (rg.r*100/SKIN_SCALE < 16 || rg.g*100/SKIN_SCALE < 16) {
					int R = rg.r * 255/1024;
					int G = rg.g * 255/1024;
					prev_drawInit (draw_buf, R, G, 255-R-G);
				} else
					prev_drawInit (draw_buf, -1, gauss_calc_probability (g, rg),150);
				prev_drawFRect_color_nc (rg.r*draw_zoom*COLCNT/SKIN_SCALE,
										 rg.g*draw_zoom*COLCNT/SKIN_SCALE,
										 draw_zoom, draw_zoom);
			}
		}
	}
}

/*********************************************************************
  Read lookup table and gauss parameter from file name.
*********************************************************************/
static BOOL gsimple_read (char *name)
{
	FILE *file;
	BOOL ok = TRUE;

	if (!(file = fopen (name, "r"))) {
		iw_warning ("Unable to open '%s'", name);
		return FALSE;
	}
	ok = ok && fread (&gs_gauss, 1, sizeof(t_gauss), file) == sizeof(t_gauss);
	ok = ok && fread (gs_lookup, 1, LOOK_SIZE, file) == LOOK_SIZE;
	if (!ok)
		iw_warning ("Unable to read from '%s'", name);
	fclose (file);
	return ok;
}

/*********************************************************************
  Write lookup table and gauss parameter to file name.
*********************************************************************/
static BOOL gsimple_write (char *name)
{
	FILE *file;
	BOOL ok = TRUE;

	if (!(file = fopen(name, "w"))) {
		iw_warning ("Unable to open '%s'", name);
		return FALSE;
	}

	ok = ok && fwrite (&gs_gauss, 1, sizeof(t_gauss), file) == sizeof(t_gauss);
	ok = ok && fwrite (gs_lookup, 1, LOOK_SIZE, file) == LOOK_SIZE;
	if (!ok)
		iw_warning ("Unable to write to '%s'", name);
	fclose (file);
	return ok;
}

/*********************************************************************
  Callback function for the InputBufWindow to move the init rectangle.
*********************************************************************/
static void cb_button_event (prevBuffer *b, prevEvent signal, int x, int y, void *data)
{
	if (!opt_values.init_gsimple) return;

	opt_values.rect_x = x;
	opt_values.rect_y = y;
}

static void set_update_mode (BOOL update, int thresh_new)
{
	static long face = OPTS_SET_ERROR, ret;
	static int thresh = OPTS_SET_ERROR;
	static int dthresh = OPTS_SET_ERROR;
	static int comb = OPTS_SET_ERROR;
	static int diff = OPTS_SET_ERROR;
	static int mhi = OPTS_SET_ERROR;

	if (update) {
		gsimple_read (GSIMPLE_FILE_UPDATE);

		ret = opts_value_set (OPT_DIFF_THRESH, GINT_TO_POINTER(0));
		if (dthresh == OPTS_SET_ERROR) dthresh = ret;

		ret = opts_value_set (OPT_DIFF_DO, GINT_TO_POINTER(1));
		if (diff == OPTS_SET_ERROR) diff = ret;

		ret = opts_value_set (OPT_COMB_MODE, GINT_TO_POINTER(SCLAS_COMB_MUL));
		if (comb == OPTS_SET_ERROR) comb = ret;

		ret = opts_value_set (OPT_CLASS_THRESH, GINT_TO_POINTER(0));
		if (thresh == OPTS_SET_ERROR) thresh = ret;

		ret = opts_value_set ("Motion History Image", GINT_TO_POINTER(0));
		if (mhi == OPTS_SET_ERROR) mhi = ret;

		ret = opts_value_set ("Face detection:", GINT_TO_POINTER(0));
		if (face == OPTS_SET_ERROR) face = ret;

		gs_state = GSIMPLE_UPDATE;
	} else {
		opts_value_set (OPT_DIFF_THRESH, GINT_TO_POINTER(dthresh));
		dthresh = OPTS_SET_ERROR;
		opts_value_set (OPT_COMB_MODE, GINT_TO_POINTER(comb));
		comb = OPTS_SET_ERROR;
		opts_value_set (OPT_DIFF_DO, GINT_TO_POINTER(diff));
		diff = OPTS_SET_ERROR;

		if (thresh_new >= 0) thresh = thresh_new;
		if (thresh == OPTS_SET_ERROR) thresh = 60;
		opts_value_set (OPT_CLASS_THRESH, GINT_TO_POINTER(thresh));
		thresh = OPTS_SET_ERROR;

		if (mhi != OPTS_SET_ERROR) {
			opts_value_set ("Motion History Image", GINT_TO_POINTER(mhi));
			mhi = OPTS_SET_ERROR;
		}
		if (face != OPTS_SET_ERROR) {
			opts_value_set ("Face detection:", (void*)face);
			face = OPTS_SET_ERROR;
		}
		gs_state = GSIMPLE_CLASS;
	}
}

/*********************************************************************
  Initialisation of gauss distribution by showing some skin to the
  camera.
*********************************************************************/
static void gsimple_init_interactive (uchar **s, int width, int height)
{
	prevBuffer *input = grab_get_inputBuf();
	int x, y, i, j, of;
	t_Pixel rg;

	if (opt_values.rect_x < 0)
		x = (width - RECTSIZE) / 2;
	else
		x = opt_values.rect_x - RECTSIZE/2;
	if (x<0) x = 0;
	else if (x+RECTSIZE >= width) x = width-RECTSIZE-1;

	if (opt_values.rect_y < 0)
		y = (height - RECTSIZE) / 2;
	else
		y = opt_values.rect_y - RECTSIZE/2;
	if (y<0) y = 0;
	else if (y+RECTSIZE >= height) y = height-RECTSIZE-1;

	iw_debug (4,"Initialisation of gauss distibution: (%d, %d) size: %d - %d",
			  x, y, RECTSIZE, RECTSIZE);

	if (input->window) {
		prevDataRect rect;

		rect.ctab = IW_INDEX;
		rect.r = rot;
		rect.x1 = x-1;
		rect.y1 = y-1;
		rect.x2 = x+RECTSIZE+1;
		rect.y2 = y+RECTSIZE+1;

		prev_render_rects (input, &rect, 1, 0, width, height);
	}

	gauss_clear (&gs_gauss);
	gauss_draw_start (b_gauss);
	gauss_draw_clear();

	of = y*width + x;
	for (i=RECTSIZE; i>0; i--) {
		for (j=RECTSIZE; j>0; j--) {
			iw_skin_yuv2rg (*(s[0]+of), *(s[1]+of), *(s[2]+of), &rg);

			if (iw_skin_pixelIsSkin (skin_options, &rg)) {
				gauss_add_pixel (&gs_gauss, rg);
				gauss_draw_pixel (rg, 255);
			} else
				gauss_draw_pixel (rg, 127);

			of++;
		}
		of += width - RECTSIZE;
	}
	gauss_calc (&gs_gauss);
	gauss_draw_dist (&gs_gauss);
	gauss_draw_stop ();
}

/*********************************************************************
  Color classification of img. Write median() smoothed result
  (mask size: size*2+1) to d.
  pix_cnt=0,1,2 -> calculate average over 1,4,5 pixels
*********************************************************************/
static BOOL gsimple_classify (iwImage *img, uchar *d, int pix_cnt, int size, int thresh)
{
	static BOOL last_init_gsimple = FALSE;
	uchar *y=img->data[0], *u=img->data[1], *v=img->data[2], *dpos, *buffer;
	int width = img->width, height = img->height;
	int count = width*height, index;
	BOOL use_lookup = TRUE;

	dpos = buffer = iw_img_get_buffer(count);

	if (opt_values.init_gsimple) {
		gsimple_init_interactive (img->data, width, height);
		last_init_gsimple = TRUE;
		use_lookup = FALSE;
	} else if (last_init_gsimple) {
		calc_lookup (&gs_gauss);
		last_init_gsimple = FALSE;
	}
	if (opt_values.calc_lookup) {
		gauss_draw_start (b_gauss);
		gauss_calc (&gs_gauss);
		gauss_draw_dist (&gs_gauss);
		gauss_draw_stop ();
		calc_lookup (&gs_gauss);
		opt_values.calc_lookup = FALSE;
	}
	if (opt_values.save != 0) {
		if (opt_values.save == 1)
			gsimple_write (GSIMPLE_FILE);
		else
			gsimple_read (GSIMPLE_FILE);
		opt_values.save = 0;
	}
	if (opt_values.class_mode != 0) {
		if (opt_values.class_mode != 1 && gs_state == GSIMPLE_UPDATE)
			iw_output_status (STATUS_UPDATE" "IW_OUTPUT_STATUS_CANCEL);
		set_update_mode (opt_values.class_mode==1, -1);
		opt_values.class_mode = 0;
	}

	if (size == 0)
		dpos = d;

	if (pix_cnt==0) {
		if (use_lookup) {
			while (count--) {
				index = ((*y++ >>1) << 14) | ((*u++ >>1) << 7) | (*v++ >>1);
				*dpos++ = gs_lookup[index];
			}
		} else {
			while (count--)
				*dpos++ = pixelclassify (&gs_gauss, *y++, *u++, *v++);
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
				if (use_lookup) {
					index = ((r >>1) << 14) | ((g >>1) << 7) | (b >>1);
					*dpos++ = gs_lookup[index];
				} else
					*dpos++ = pixelclassify (&gs_gauss, r,g,b);
				y++; u++; v++;
			}
			y+=2; u+=2; v+=2; dpos+=2;

		}
	}
	if (size>0)
		iw_img_median (buffer,d,width,height,size);

	iw_img_release_buffer();

	return TRUE;
}

static BOOL is_background (uchar **s, uchar **b, int of)
{
	/*
	int k = 0.333 * (abs(s[0][of]-b[0][of]) + abs(s[1][of]-b[1][of]) + abs(s[2][of]-b[2][of]));
	*/
	int k = 0.5 * (abs(s[1][of]-b[1][of]) + abs(s[2][of]-b[2][of]));
	return (k <= opt_values.diff_back);
}

static uchar **back_fgimg, *back_bgimg[3];
static int back_width;

/*********************************************************************
  Fill function to remove any background pixels staring at offset
  (One border pixel of the region labeld with label in ireg). Not
  recursive to save stack space.
*********************************************************************/
static void background_check (int *ireg, int label, int offset)
{
	int *stack, sptr, ssize;

	ssize = 1000;
	sptr = 0;
	stack = malloc (sizeof(int)*ssize);
	stack[sptr++] = offset;
	while (sptr) {
		offset = stack[--sptr];

		if (*(ireg+offset) == label &&
			is_background (back_fgimg, back_bgimg, offset)) {
			*(ireg+offset) = -1;

			if (sptr+4+1 >= ssize) {
				ssize *= 2;
				stack = realloc (stack, sizeof(int)*ssize);
			}
			stack[sptr++] = offset-1;
			stack[sptr++] = offset+1;
			stack[sptr++] = offset-back_width;
			stack[sptr++] = offset+back_width;
		}
	}
	free (stack);
}

/*********************************************************************
  Calculate and return threshold such that opt_values.threshold % of
  all training pixels (marked in hist_ireg by gsimple_update_do() with
  G_MAXINT) get classified correctly.
*********************************************************************/
static int gsimple_get_thresh (iwImage *img, int hist_cnt,
							   uchar *hist_img, int *hist_ireg)
{
	int hist[256];
	int count = img->width * img->height, anzvec, index;
	int x, sum, thresh = 0;

	memset (hist, 0, sizeof(int)*256);
	anzvec = 0;

	while (hist_cnt--) {
		for (x=0; x < count; x++) {
			if (*hist_ireg == G_MAXINT) {
				index = ((*hist_img >>1) << 14)
					| ((*(hist_img+count) >>1) << 7)
					| (*(hist_img+count*2) >>1);
				hist[gs_lookup[index]]++;
				anzvec++;
			}
			hist_img++;
			hist_ireg++;
		}
		hist_img += 2*count;
	}

	sum = 0;
	for (x=255; x>=0; x--) {
		sum += hist[x];
		if (100*sum/anzvec >= opt_values.threshold) {
			thresh = x;
			break;
		}
	}
	iw_debug (2, "Threshold to get %d%% correctly: %d",
			  opt_values.threshold, thresh);
	return thresh;
}

/*********************************************************************
  Calculate new distribution by collecting all pixels from the biggest
  regions of all hist_cnt images and removing pixels which are similar
  to the background image from hist_back.
*********************************************************************/
static void gsimple_update_do (iwImage *img, int hist_cnt,
							   uchar *hist_back, uchar *hist_img,
							   uchar *hist_ireg, uchar *hist_reg)
{
	t_Pixel rg;
	uchar *s[3];
	int w = img->width, h = img->height;
	int i, max, maxcnt, cnt;
	int x, y, of, reg_x, reg_y, reg_w, reg_h, reg_index;
	int *reg;
	uchar *buffer;
	gint32 *ireg;

	buffer = iw_img_get_buffer (w*h + sizeof(gint32)*w*h+sizeof(gint32));
	ireg = (gint32*)((size_t)(buffer + w*h + sizeof(gint32)-1) & -sizeof(gint32));
	gauss_clear (&gs_gauss);
	gauss_draw_clear();

	back_width = w;
	back_fgimg = s;
	back_bgimg[0] = hist_back;
	back_bgimg[1] = hist_back+w*h;
	back_bgimg[2] = hist_back+2*w*h;

	while (hist_cnt--) {

		s[0] = hist_img;
		s[1] = hist_img+w*h;
		s[2] = hist_img+2*w*h;

		reg = (int*)hist_reg;
		max = 0;
		maxcnt = -1;
		for (i=0; i<3; i++) {
			reg_x = *reg++;
			reg_y = *reg++;
			reg_w = *reg++;
			reg_h = *reg++;
			reg_index = *reg++;

			if (reg_x >= 0) {
				memcpy (ireg, hist_ireg, w*h*sizeof(gint32));
				iw_gsimple_shrink (ireg, w, h, reg_x, reg_y, reg_w, reg_h,
								   reg_index, opt_values.reg_shrink, background_check);

				cnt = 0;
				of = reg_y*w + reg_x;
				for (y=reg_h; y>0; y--) {
					for (x=reg_w; x>0; x--) {
						if (*((int*)ireg+of) == reg_index) {
							iw_skin_yuv2rg (*(s[0]+of), *(s[1]+of), *(s[2]+of), &rg);
						
							if (iw_skin_pixelIsSkin (skin_options, &rg))
								cnt++;
						}
						of++;
					}
					of += w - reg_w;
				}
				if (cnt > maxcnt) {
					maxcnt = cnt;
					max = i;
				}
				iw_debug (5, "%d: %d %d %d %d %d -> cnt %d\n",
						  hist_cnt, reg_x, reg_y, reg_w, reg_h, reg_index, cnt);
			}
		}
		reg = (int*)(hist_reg + sizeof(int)*5*max);
		reg_x = *reg++;
		reg_y = *reg++;
		reg_w = *reg++;
		reg_h = *reg++;
		reg_index = *reg++;

		if (b_region->window) memcpy (buffer, s[0], w*h);
		of = 0;
		for (x=w*h; x>0; x--) {
			if (*((int*)hist_ireg+of) == reg_index)
				*(buffer+of) = 0;
			of++;
		}

		iw_gsimple_shrink ((gint32*)hist_ireg, w, h, reg_x, reg_y, reg_w, reg_h,
						   reg_index, opt_values.reg_shrink, background_check);

		of = reg_y*w + reg_x;
		for (y=reg_h; y>0; y--) {
			for (x=reg_w; x>0; x--) {
				if (*((int*)hist_ireg+of) == reg_index) {
					iw_skin_yuv2rg (*(s[0]+of), *(s[1]+of), *(s[2]+of), &rg);

					if (iw_skin_pixelIsSkin (skin_options, &rg)) {
						gauss_add_pixel (&gs_gauss, rg);
						gauss_draw_pixel (rg, 255);
						*(buffer+of) = 255;
						*((int*)hist_ireg+of) = G_MAXINT;
					} else
						gauss_draw_pixel (rg, 127);
				}
				of++;
			}
			of += w - reg_w;
		}

		gauss_draw_unlock ();
		prev_render (b_region, &buffer, w, h, IW_YUV);
		prev_draw_buffer (b_region);
		if (opt_values.update_wait) {
			/* Single UpdateRegion display */
			while (opt_values.update_cont == 0)
				iw_usleep (100);
			if (opt_values.update_cont == 1)
				opt_values.update_cont = 0;
		}
		gauss_draw_lock ();

		hist_img += 3*w*h;
		hist_ireg += sizeof(int)*w*h;
		hist_reg += sizeof(int)*5*3;
	}
	iw_img_release_buffer();

	opt_values.update_cont = 0;
}

static t_gauss gs_fill_gauss;

/*********************************************************************
  Test if the pixel with offset 'offset' in s belongs to a hand
  region. If so add this pixel to a new gauss distribution.
*********************************************************************/
/*
static BOOL gsimple_fill_cb (iwImage *s, uchar *d, int offset, ppmColor col, void *data)
{
	t_Pixel rg;
	uchar *y = s->data[0]+offset;
	uchar *u = s->data[1]+offset;
	uchar *v = s->data[2]+offset;

	iw_skin_yuv2rg (*y, *u, *v, &rg);

	if (!iw_skin_pixelIsSkin (skin_options, &rg)) {
		gauss_draw_pixel (rg, 127);
		return FALSE;
	}

	if ((abs(*u - col.g) + abs(*v - col.b)) < (int)data) {
		gauss_add_pixel (&gs_fill_gauss, rg);
		gauss_draw_pixel (rg, 255);
		return TRUE;
	} else
		return FALSE;
}
*/

/*
static void gsimple_polygon_cb (iwImage *img, uchar *y, uchar *u, uchar *v,
								int cnt, void *data)
{
	t_Pixel rg;

	while (cnt--) {
		iw_skin_yuv2rg (*y++, *u++, *v++, &rg);

		gauss_add_pixel (&gs_fill_gauss, rg);
		gauss_draw_pixel (rg, 255);
	}
}
*/

/*********************************************************************
  Calculate new distribution by collecting all pixels from the
  biggest region (which was calculated by multiplying the motion and
  the color channel) over some frames (specified in the gui) and
  tacking a generated background image into account.
*********************************************************************/
static void gsimple_update (grabImageData *gimg, int *ireg,
							iwRegion *regions, int numregs)
{
	static unsigned long last_time = 0;
	static int iterations = 0;
	static uchar *hist_back, *hist_img, *hist_ireg, *hist_reg = NULL;
	static int hist_max, hist_cnt;

	iwImage *img = &gimg->img;
	unsigned long cur_time;
	int max, max2, max3, i, w = img->width, h = img->height;
	uchar *buffer;
	iwRegion *reg;

	if (gs_state != GSIMPLE_UPDATE) iterations = 0;
	if (iterations > 0) iterations++;

	if (gs_state != GSIMPLE_UPDATE || numregs <= 0) return;

	cur_time = gimg->time.tv_sec*1000 + gimg->time.tv_usec/1000;
	gauss_draw_start (b_gauss);

	/* Initialise the distribution if a new 'round' should be started */
	if (cur_time-last_time > 700 ||
		iterations > (int)(opt_values.iterations*1.5+0.5) ||
		iterations == 0) {

		gauss_clear (&gs_fill_gauss);
		iterations = 1;

		gauss_draw_clear();

		if (hist_reg) free (hist_reg);
		hist_max = opt_values.iterations;
		hist_cnt = 0;
		hist_reg = calloc (hist_max*img->planes*w*h+
							sizeof(int)*hist_max*w*h+
							(img->planes+1)*w*h+
							hist_max*5*3*sizeof(int), 1);
		hist_ireg = hist_reg + hist_max*5*3*sizeof(int);
		hist_img = hist_ireg + hist_max*sizeof(int)*w*h;
		hist_back = hist_img + hist_max*img->planes*w*h;
	}
	iw_debug (5,"dtime: %lu iterations: %d pixCount: %d",
			  cur_time-last_time, iterations, gs_fill_gauss.count);

	if (hist_cnt < hist_max) {
		int d, k, k2, count = w*h;
		uchar *y,*u,*v,*cnt, *m_ptr[3], *y0,*u0,*v0, *y1,*u1,*v1, *y2,*u2,*v2;
		uchar *buffer = iw_img_get_buffer (w*h), *b_pos = buffer;

		hist_cnt++;

		/* Save image and the region segmentation for later update of the
		   gauss distribution. */
		for (d=0; d<img->planes; d++)
			memcpy (hist_img+(hist_cnt-1)*img->planes*count+d*count, img->data[d], count);
		memcpy (hist_ireg+(hist_cnt-1)*sizeof(int)*count, ireg, sizeof(int)*count);

		/* Update the background image by adding pixels which were not
		   in the last two difference images. */
		if (hist_cnt > 2) {
			gauss_draw_unlock();

			y0 = hist_img + (hist_cnt-1)*img->planes*count;
			u0 = y0 + count;
			v0 = y0 + 2*count;
			y1 = hist_img + (hist_cnt-2)*img->planes*count;
			u1 = y1 + count;
			v1 = y1 + 2*count;
			y2 = hist_img + (hist_cnt-3)*img->planes*count;
			u2 = y2 + count;
			v2 = y2 + 2*count;
			y = hist_back;
			u = y+count;
			v = y+2*count;

			cnt = y+3*count;
			while (count--) {
				/*
				k = 0.333 * (abs((*y1)-(*y0)) + abs((*u1)-(*u0)) + abs((*v1)-(*v0)));
				k2 = 0.333 * (abs((*y2)-(*y1)) + abs((*u2)-(*u1)) + abs((*v2)-(*v1)));
				*/
				k = 0.5 * (abs((*u1)-(*u0)) + abs((*v1)-(*v0)));
				k2 = 0.5 * (abs((*u2)-(*u1)) + abs((*v2)-(*v1)));
				if (k <= opt_values.diff_thresh && k2 <= opt_values.diff_thresh) {
					*y = (*y * (*cnt) + *y0) / (*cnt+1);
					*u = (*u * (*cnt) + *u0) / (*cnt+1);
					*v = (*v * (*cnt) + *v0) / (*cnt+1);
					(*cnt)++;
					*b_pos++ = 0;
				} else
					*b_pos++ = 255;

				y++; u++; v++; cnt++;
				y0++; u0++; v0++;
				y1++; u1++; v1++;
				y2++; u2++; v2++;
			}

			m_ptr[0] = hist_back;
			m_ptr[1] = hist_back+w*h;
			m_ptr[2] = hist_back+2*w*h;
			prev_render (b_background, m_ptr, w, h, IW_YUV);
			prev_draw_buffer (b_background);

			prev_render (b_diff, &buffer, w, h, IW_YUV);
			prev_draw_buffer (b_diff);

			gauss_draw_lock();
		}
		iw_img_release_buffer();
	}

	/* Find biggest region */
	max = 0;
	for (i=1; i<numregs; i++)
		if (regions[i].r.pixelanzahl > regions[max].r.pixelanzahl)
			max = i;
	max2 = -1;
	for (i=0; i<numregs; i++)
		if (i != max &&
			(max2 == -1 || regions[i].r.pixelanzahl > regions[max2].r.pixelanzahl))
			max2 = i;
	max3 = -1;
	for (i=0; i<numregs; i++)
		if (i != max && i != max2 &&
			(max3 == -1 || regions[i].r.pixelanzahl > regions[max3].r.pixelanzahl))
			max3 = i;
	reg = &regions[max];

	buffer = iw_img_get_buffer (w*h);
	if (b_region->window) memcpy (buffer, img->data[0], w*h);

	/* Collect pixels for the new distribution */
	{
		t_Pixel rg;
		uchar **s, *d;
		int x, y, of, reg_x, reg_y, reg_w, reg_h;
		int *pos;

		iw_reg_boundingbox (&reg->r.polygon, &reg_x,&reg_y,&reg_w,&reg_h);
		reg_w = reg_w-reg_x+1;
		reg_h = reg_h-reg_y+1;

		iw_gsimple_shrink (ireg, w, h, reg_x, reg_y, reg_w, reg_h,
						   reg->labindex, opt_values.reg_shrink, NULL);

		if (hist_cnt <= hist_max) {
			int x, y, w, h, idx;

			pos = (int*)(hist_reg+(hist_cnt-1)*5*3*sizeof(int));
			*pos++ = reg_x;
			*pos++ = reg_y;
			*pos++ = reg_w;
			*pos++ = reg_h;
			*pos++ = reg->labindex;
			if (numregs > 1) {
				iw_reg_boundingbox (&regions[max2].r.polygon,
									&x, &y, &w, &h);
				idx = regions[max2].labindex;
			} else
				x = y = w = h = idx = -1;
			*pos++ = x;
			*pos++ = y;
			*pos++ = w-x+1;
			*pos++ = h-y+1;
			*pos++ = idx;
			if (numregs > 2) {
				iw_reg_boundingbox (&regions[max3].r.polygon,
									&x, &y, &w, &h);
				idx = regions[max3].labindex;
			} else
				x = y = w = h = idx = -1;
			*pos++ = x;
			*pos++ = y;
			*pos++ = w-x+1;
			*pos++ = h-y+1;
			*pos++ = idx;
		}
		s = img->data;
		d = buffer;
		of = reg_y*w + reg_x;
		for (y=reg_h; y>0; y--) {
			for (x=reg_w; x>0; x--) {
				if (*(ireg+of) == reg->labindex) {
					iw_skin_yuv2rg (*(s[0]+of), *(s[1]+of), *(s[2]+of), &rg);

					if (iw_skin_pixelIsSkin (skin_options, &rg)) {
						gauss_add_pixel (&gs_fill_gauss, rg);
						gauss_draw_pixel (rg, 255);
						*(d+of) = 255;
					} else
						gauss_draw_pixel (rg, 127);
				}
				of++;
			}
			of += w - reg_w;
		}
	}
	/* Two alternative tries on getting useful pixels for the new distribution
	{
		ppmColor col;
		col.r = reg->r.echtfarbe.x;
		col.g = reg->r.echtfarbe.y;
		col.b = reg->r.echtfarbe.z;

		iw_gsimple_fill (img, buffer, reg->r.schwerpunkt.x, reg->r.schwerpunkt.y,
						 col, gsimple_fill_cb, (void*)opt_values.fill_thresh);
	} */
	/*
	iw_img_fillPoly_raw (img->data, w, h, img->planes,
						 &reg->r.polygon, FALSE, gsimple_polygon_cb, NULL);
	*/

	gauss_draw_unlock();
	prev_render (b_region, &buffer, w, h, IW_YUV);
	prev_draw_buffer (b_region);
	gauss_draw_lock();
	iw_img_release_buffer();

	/* Calculate the new distribution */
	if (gs_fill_gauss.count > opt_values.fill_minsize &&
		iterations >= opt_values.iterations) {

		int thresh = 60;

		if (opt_values.background)
			gsimple_update_do (img, hist_cnt,
							   hist_back, hist_img, hist_ireg, hist_reg);
		else
			gs_gauss = gs_fill_gauss;

		gauss_calc (&gs_gauss);
		calc_lookup (&gs_gauss);
		gauss_draw_dist (&gs_gauss);
		/* prev_buffer_unlock() must be called before calling set_update_mode(),
		   otherwise a deadlock may happen */
		gauss_draw_stop ();

		if (opt_values.background)
			thresh = gsimple_get_thresh (img, hist_cnt, hist_img, (int*)hist_ireg);

		set_update_mode (FALSE, thresh);
		iw_output_status (STATUS_UPDATE" "IW_OUTPUT_STATUS_DONE);
		iw_debug (3,"New distribution with %d pixels after %d iterations",
				  gs_gauss.count, iterations);
		iterations = 0;
	} else if (cur_time-last_time < 200) {
		gauss_draw_stop ();
		/* Last iteration was quite fast,
		   slow it down to get reasonable difference images */
		iw_debug (3, "Fast iteration, slowing down by %ld ms ...",
				  200-(cur_time-last_time)+10);
		iw_usleep (200-(cur_time-last_time)+10);
	} else
		gauss_draw_stop ();

	if (iterations == 0) {
		free (hist_reg);
		hist_reg = NULL;
		hist_cnt = 0;
	}
	last_time = cur_time;
}

/*********************************************************************
  Free the resources allocated during bpro_???().
*********************************************************************/
static void gsimple_cleanup (plugDefinition *plug) {}

static void help (plugDefinition *plug)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Perform color classification with a gauss function.\n"
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
static void gsimple_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
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
	plug_function_register (plug, "classify", (plugFunc)gsimple_classify);
	plug_function_register (plug, "update", (plugFunc)gsimple_update);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int gsimple_init_options (plugDefinition *plug)
{
	int page;

	opt_values.init_gsimple = FALSE;
	opt_values.calc_lookup = FALSE;
	opt_values.var_scale = 1.5;
	opt_values.var_scale_kov = 1.5;
	opt_values.var_add = 0;
	opt_values.threshold = 95;
	opt_values.fill_minsize = 100;
	opt_values.iterations = 10;
	opt_values.reg_shrink = 2;
	opt_values.background = 1;
	opt_values.diff_thresh = 8;
	opt_values.diff_back = 8;
	opt_values.save = 0;
	opt_values.class_mode = 0;
	opt_values.update_wait = FALSE;
	opt_values.update_cont = 0;
	opt_values.rect_x = -1;
	opt_values.rect_y = -1;

	page = opts_page_append ("GaussSimple");
	opts_toggle_create (page, "Init GaussSimple", "Show some skin to the camera!",
						&opt_values.init_gsimple);
	opts_button_create (page, "Recalculate Lookup",
						"Calculating lookup table of gaussian classifier using current "
						"settings of var-scale, skin locus, and white point",
						&opt_values.calc_lookup);
	opts_float_create (page, "VarScaling", "variance scaling factor",
					   &opt_values.var_scale, 0.1, 200);
	opts_float_create (page, "VarScalKov", "covariance scaling factor",
					   &opt_values.var_scale_kov, -20, 20);
	opts_float_create (page, "VarAdd", "variance scaling addend",
					   &opt_values.var_add, -100, 100);
	opts_entscale_create (page, "Threshold",
						  "How much of the training material should be classified correctly?",
						  &opt_values.threshold, 0, 100);

	opts_button_create (page, "UpdateClass|DoClass",
						"Set mode to update and read the lookup table from file "
						GSIMPLE_FILE_UPDATE"|"
						"Set mode to classification(don't try to change the distribution)",
						&opt_values.class_mode);
	opts_toggle_create (page, "During Update: Wait for button",
						"Wait for NextRegDisp,Continue buttons during "
						"calculation of a new distribution",
						&opt_values.update_wait);
	opts_button_create (page, "NextRegDisp|Continue", NULL, &opt_values.update_cont);

	page = opts_page_append ("GaussSimple2");

	opts_entscale_create (page, "MinPixCount",
						  "Number of pixels necessary to recalculate a new distribution",
						  &opt_values.fill_minsize, 0, 2000);
	opts_entscale_create (page, "Iterations",
						  "Number of iterations (grabbed images) necessary"
						  " to recalculate a new distribution",
						  &opt_values.iterations, 1, 100);
	opts_entscale_create (page, "RegShrink",
						  "Number of pixels a difference region is shrunk before"
						  " adding the pixels to the new distribution",
						  &opt_values.reg_shrink, 0, 20);

	opts_toggle_create (page, "Background separation",
						"Remove pixels which are similar to the background",
						&opt_values.background);
	opts_entscale_create (page, "DiffThresh",
						  "Threshold for difference image for background image calculation",
						  &opt_values.diff_thresh, 0, 30);
	opts_entscale_create (page, "DiffBack",
						  "Min. difference with background image before pixels"
						  " are added to the new distribution",
						  &opt_values.diff_back, 0, 30);

	opts_button_create (page, "Save Lookup|Load Lookup",
						"Save gauss values and lookup table in "GSIMPLE_FILE"|"
						"Load gauss values and lookup table from "GSIMPLE_FILE,
						&opt_values.save);
	skin_options = iw_skin_init (page);

	b_region = prev_new_window (plug_name(plug, ".region"), -1, -1, TRUE, FALSE);
	b_gauss = prev_new_window (plug_name(plug, ".distribution"), -1, -1, FALSE, FALSE);
	b_background = prev_new_window (plug_name(plug, ".mean image"), -1, -1, FALSE, FALSE);
	b_diff = prev_new_window (plug_name(plug, ".backDiff image"), -1, -1, TRUE, FALSE);

	prev_signal_connect (grab_get_inputBuf(), PREV_BUTTON_PRESS|PREV_BUTTON_MOTION,
						 cb_button_event, NULL);

	gsimple_read (GSIMPLE_FILE);

	return page;
}

plugDefinition plug_gauss_simple = {
	"GSimple",
	PLUG_ABI_VERSION,
	gsimple_init,
	gsimple_init_options,
	gsimple_cleanup,
	NULL
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_gsimple_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_gauss_simple;
}
