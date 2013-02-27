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
#include <string.h>
#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "main/output.h"
#include "main/image.h"
#include "tracking/handtrack.h"
#include "sclas_image.h"
#include "skinclass.h"

#define BUF_DIFF		0		/* Indices for buffer (intermediate images) */
#define BUF_CLASS_CONF	1
#define BUF_CLASSIFY	2
#define BUF_COMB		3
#define BUF_MOTION		4
#define BUF_TMP			5

static plugDefinition plug_skinclass;

typedef struct {
	int class_mode;			/* Do color classification ? */
	int class_avg;			/* Number of pixels for averaging */
	int class_med_size;		/* Size of median filter (radius) */
	int class_thresh;		/* Threshold value for confidence mapping */

	BOOL diff_do;			/* Calculate difference image ? */
	int diff_med_size;		/* Size of median filter (radius) */
	int diff_thresh;		/* Threshold value after diff. image calculation */
	int diff_source;		/* Y, U, or V */

	BOOL mhi_do;			/* Motion history image */
	int mhi_diff_thresh;	/* Threshold for initial difference image */
	int mhi_diff_src;		/* Source data for difference image calculation */
	float mhi_weight;		/* Portion of old MHI-Image compared to new part */
	int mhi_median;			/* Size of median filter */

	int comb_mode;			/* Modus for diff. image color classification combination */
	int comb_thresh;		/* Threshold value after combination */

	int reg_min;			/* Smallest region (of the hand regions) */
	int reg_diff_min;		/* Smallest region of the difference image */
	float reg_bew_min;		/* Smallest allowed judgement */
	int reg_bew_anz;		/* Provide n best judgest regions */
} sclasValues;

static sclasValues opt_values;
static char **class_mode_names;
		/* Buffer for intermediate images */
static uchar *buffer[BUF_TMP+1] = {NULL,NULL,NULL,NULL,NULL};

static struct sclasParameter {
	char *out_regions;
} parameter;

static prevBuffer *b_diff, *b_mhihist, *b_mhithresh, *b_colorseg, *b_combine, *b_rlab, *b_region;

/*********************************************************************
  Free the resources allocated during sclas_???().
*********************************************************************/
static void sclas_cleanup (plugDefinition *plug)
{
}

static void help (void)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Segment skin colored regions. Needs at least one other\n"
			 "plugin to perform color classification.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-o]\n"
			 "-o       output hand hypotheses on stream <%s>_gHypos\n",
			 plug_skinclass.name, ICEWING_NAME, plug_skinclass.name,
			 IW_DACSNAME);
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
#define ARG_TEMPLATE "-L:Lr -O:O -H:H"
static void sclas_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
{
	void *arg;
	char ch;
	int nr = 0;

	parameter.out_regions = NULL;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'O':
				parameter.out_regions =
					iw_output_register_stream ("_gHypos",
											   (NDRfunction_t*)ndr_RegionHyp);
				break;
			case 'H':
			case '\0':
				help();
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help();
		}
	}
	plug_observ_data (plug, "image");
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int sclas_init_options (plugDefinition *plug)
{
	static char *diff_sources[] = {"Y","U","V",NULL};
	static char *mhi_diff_sources[] = {"Y","U","V","U+V",NULL};
	static char *comb_mode[] = {"None","Add","Mul",NULL};
	static char *class_avg[] = {"1","4","5",NULL};

	int p, cnt;
	plugDataFunc *func;

	opt_values.diff_do = TRUE;
	opt_values.diff_med_size = 0;
	opt_values.diff_thresh = 6;
	opt_values.diff_source = 2;

	opt_values.mhi_do = 0;
	opt_values.mhi_diff_thresh = 12;
	opt_values.mhi_diff_src = 0;
	opt_values.mhi_weight = 0.7;
	opt_values.mhi_median = 2;


	cnt = 0;
	class_mode_names = malloc (sizeof(char*)*2);
	class_mode_names[cnt++] = strdup("None");
	
	/* Get list of classifier plugins */
	func = NULL;
	while ((func = plug_function_get (SCLAS_IDENT_CLASSIFY, func))) {
		class_mode_names = realloc (class_mode_names, sizeof(char*)*(cnt+2));
		class_mode_names[cnt++] = strdup (func->plug->name);
	}
	class_mode_names[cnt] = NULL;

	opt_values.class_mode = cnt>0 ? 1:0;
	opt_values.class_avg = 0;
	opt_values.class_med_size = 2;
	opt_values.class_thresh = 127;
	opt_values.comb_mode = SCLAS_COMB_NONE;
	opt_values.comb_thresh = 0;
	opt_values.reg_min = 100;
	opt_values.reg_diff_min = 100;
	opt_values.reg_bew_min = 0.1;
	opt_values.reg_bew_anz = 0;

	b_diff = prev_new_window (plug_name(plug, ".Differenz image"),-1,-1,TRUE,FALSE);
	prev_opts_append (b_diff, PREV_IMAGE, PREV_TEXT, -1);
	b_mhihist = prev_new_window (plug_name(plug, ".MHI history image"),-1,-1,TRUE,FALSE);
	b_mhithresh = prev_new_window (plug_name(plug, ".MHI threshold"),-1,-1,TRUE,FALSE);
	b_colorseg = prev_new_window (plug_name(plug, ".Color segmentation"),-1,-1,TRUE,FALSE);
	b_combine = prev_new_window (plug_name(plug, ".Combined Segment + Diff"),-1,-1,TRUE,FALSE);

	b_rlab = prev_new_window (plug_name(plug, ".Region lab"),-1,-1,TRUE,FALSE);
	b_region = prev_new_window (plug_name(plug, ".Recognized regions"),-1,-1,FALSE,FALSE);
	prev_opts_append (b_region, PREV_TEXT, PREV_REGION, PREV_COMINFO, -1);

	p = opts_page_append (OPT_SKIN_MOTION);

	opts_toggle_create (p,OPT_DIFF_DO,NULL,&opt_values.diff_do);
	opts_entscale_create (p,"Diff. Med size",
						  "Radius of median filter",&opt_values.diff_med_size,0,20);
	opts_entscale_create (p,OPT_DIFF_THRESH,NULL,&opt_values.diff_thresh,0,255);
	opts_radio_create (p,"Diff. Source:",NULL,diff_sources,&opt_values.diff_source);

	opts_toggle_create (p, "Motion History Image", NULL, &opt_values.mhi_do);
	opts_entscale_create (p, "MHI DiffThresh",
						  "Threshold for initial difference image",
						  &opt_values.mhi_diff_thresh, 0, 255);
	opts_radio_create (p,"MHI DiffSrc:",
					   "Source data for difference image calculation",
					   mhi_diff_sources, &opt_values.mhi_diff_src);
	opts_float_create (p, "MHI Weight",
					   "Portion of old MHI-Image compared to new part",
					   &opt_values.mhi_weight, 0, 1);
	opts_entscale_create (p,"MHI Med size", "Radius of median filter",
						  &opt_values.mhi_median, 0, 20);

	p = opts_page_append (OPT_SKIN_CLASS);

	opts_option_create (p, "ColSeg Mode:",
						"Color classifier to use",
						class_mode_names, &opt_values.class_mode);
	opts_radio_create (p, "Pixel Cnt:",
					   "Number of to pixels to average for the classifier feature",
					   class_avg, &opt_values.class_avg);
	opts_entscale_create (p, OPT_CLASS_MED_SIZE,
						  "Radius of median filter for color segmented image",
						  &opt_values.class_med_size, 0, 20);
	opts_entscale_create (p, OPT_CLASS_THRESH,
						  "Threshold for color segmented image",
						  &opt_values.class_thresh, 0, 255);

	opts_radio_create (p, OPT_COMB_MODE,
					   "Combine color segmentation and difference-/MHI-image",
					   comb_mode, &opt_values.comb_mode);
	opts_entscale_create (p, "Comb. Threshold",
						  "Threshold for the combined image",
						  &opt_values.comb_thresh, 0, 255);

	opts_entscale_create (p, "Min Region",
						  "Remove all smaller regions",
						  &opt_values.reg_min, 32, 5000);
	opts_entscale_create (p, "Min DiffReg",
						  "Remove all smaller difference regions",
						  &opt_values.reg_diff_min, 32, 5000);
	opts_float_create (p, "Min Rating",
					   "Remove all regions which are judged worse",
					   &opt_values.reg_bew_min, 0, 2);
	opts_entscale_create (p, "#Regions",
						  "Keep only the best n judged regions, 0: keep all",
						  &opt_values.reg_bew_anz, 0, 20);

	return p;
}

/*********************************************************************
  Calculate motion history image.
  src1, src2: Data of last two grabbed images.
  w, h      : Size of the images.
*********************************************************************/
static void motion_do (uchar **src1, uchar **src2, int w, int h)
{
	static int w_old = -1, h_old = -1;
	static uchar *buf;
	uchar *u, *u2, *v, *v2;
	uchar *d, *d2, val;
	int x;

	if (w_old != w || h_old != h) {
		if (buf) free (buf);
		buf = calloc (1, 2*w*h);
		w_old = w;
		h_old = h;
	}

	d = buf;
	if (opt_values.mhi_median > 0)
		d2 = buf+w*h;
	else
		d2 = buffer[BUF_MOTION];
	if (opt_values.diff_do && opt_values.diff_source == opt_values.mhi_diff_src &&
		opt_values.diff_thresh == opt_values.mhi_diff_thresh) {
		/* Use previously calculated difference image */
		u = buffer[BUF_DIFF];
		for (x=w*h; x>0; x--) {
			*d = (*d)*opt_values.mhi_weight + (*u++)*(1-opt_values.mhi_weight);
			*d2++ = *d > 0 ? 255:0;
			d++;
		}
	} else if (opt_values.mhi_diff_src <= 2) {
		/* Difference image between two planes */
		u = src1[opt_values.mhi_diff_src];
		u2 = src2[opt_values.mhi_diff_src];
		for (x=w*h; x>0; x--) {
			val = abs((*u++)-(*u2++)) > opt_values.mhi_diff_thresh ? 255:0;
			*d = (*d)*opt_values.mhi_weight + val*(1-opt_values.mhi_weight);
			*d2++ = *d > 0 ? 255:0;
			d++;
		}
	} else {
		/* U+V difference image */
		u = src1[1];
		v = src1[2];
		u2 = src2[1];
		v2 = src2[2];
		for (x=w*h; x>0; x--) {
			val = (abs((*(u)++)-(*(u2)++)) + abs((*(v)++)-(*(v2)++)))
				> opt_values.mhi_diff_thresh ? 255:0;
			*d = (*d)*opt_values.mhi_weight + (val)*(1-opt_values.mhi_weight);
			*d2++ = *d > 0 ? 255:0;
			d++;
		}
	}
	prev_render (b_mhihist, &buf, w, h, IW_YUV);
	prev_draw_buffer (b_mhihist);

	if (opt_values.mhi_median > 0) {
		u = buf+w*h;
		d = buffer[BUF_MOTION];
		iw_img_medianBW (u, d, w, h, opt_values.mhi_median);
	}
	prev_render (b_mhithresh, &buffer[BUF_MOTION], w, h, IW_YUV);
	prev_draw_buffer (b_mhithresh);
}
	
/*********************************************************************
  Bild s[actImg] (im YUV-Format) bearbeiten.
*********************************************************************/
static uchar* do_image (iwImage **s, int actImg, char *classifier)
{
	int x, w = s[actImg]->width, h = s[actImg]->height;
	uchar **planes = s[actImg]->data, *buf_classify;
	uchar *retbuf = NULL, max_diff = 0;

	iw_time_add_static2 (time_motion, "Motion", time_class, "Classifiy");

	iw_debug (4,"Image size: %dx%d", w, h);

	if ((opt_values.diff_do || opt_values.mhi_do) && actImg <= 0)
		return NULL;

	iw_time_start (time_motion);
	if (opt_values.diff_do) {
		max_diff = iw_sclas_difference (planes[opt_values.diff_source],
										s[actImg-1]->data[opt_values.diff_source],
										buffer[BUF_DIFF],
										w, h, opt_values.diff_med_size,opt_values.diff_thresh);
		prev_render (b_diff, &buffer[BUF_DIFF], w, h, IW_YUV);
		prev_draw_buffer (b_diff);

		if (s[actImg-1]->data[0])
			retbuf = buffer[BUF_DIFF];
	}
	if (opt_values.mhi_do)
		motion_do (planes, s[actImg-1]->data, w, h);
	iw_time_stop (time_motion, FALSE);

	iw_time_start (time_class);
	if (classifier) {
		BOOL ok;
		plugDataFunc *func =
			plug_function_get_full (SCLAS_IDENT_CLASSIFY, NULL, classifier);
		if (func) {
			ok = ((sclasClassifyFunc)func->func) (s[actImg], buffer[BUF_CLASS_CONF],
												  opt_values.class_avg,
												  opt_values.class_med_size,
												  opt_values.class_thresh);
			if (!ok) {
				iw_time_stop (time_class, FALSE);
				return NULL;
			}

			if (opt_values.class_thresh) {
				uchar *s = buffer[BUF_CLASS_CONF], *d = buffer[BUF_CLASSIFY];
				for (x=w*h; x>0; x--)
					*d++ = *s++ > opt_values.class_thresh ? 255:0;
				buf_classify = buffer[BUF_CLASSIFY];
			} else
				buf_classify = buffer[BUF_CLASS_CONF];

			prev_render (b_colorseg, &buf_classify, w, h, IW_YUV);
			prev_draw_buffer (b_colorseg);

			if ((opt_values.diff_do || opt_values.mhi_do) &&
				opt_values.comb_mode != SCLAS_COMB_NONE) {
				uchar *src;

				if (opt_values.mhi_do) {
					max_diff = 255;
					src = buffer[BUF_MOTION];
				} else
					src = buffer[BUF_DIFF];
				iw_sclas_combine (src, max_diff, buf_classify,
								  buffer[BUF_COMB], w, h, opt_values.comb_thresh,
								  opt_values.comb_mode);
				prev_render (b_combine, &buffer[BUF_COMB], w, h, IW_YUV);
				prev_draw_buffer (b_combine);
				retbuf = buffer[BUF_COMB];
			} else
				retbuf = buf_classify;
		}
	}
	iw_time_stop (time_class, FALSE);

	return retbuf;
}

static void get_regions (uchar *reg_img, grabImageData *gimg, char *classifier)
{
	iwRegion *regions = NULL;
	iwRegCOMinfo *COMinfo = NULL;
	static int *ireg = NULL, *iregdiff = NULL, ir_width=0, ir_height=0;
	int w = gimg->img.width, h = gimg->img.height, i, nregions;
	iw_time_add_static3 (time_region, "RegionCalc",
					  time_regrest, "RegionRest",
					  time_track, "Tracking");

	if (!reg_img) return;

	if (!ireg || ir_width!=w || ir_height!=h) {
		ir_width = w;
		ir_height = h;
		if (!(ireg = realloc (ireg, sizeof(int) * ir_width * ir_height)))
			iw_error ("Out of memory: ireg for regionlab");
		if (!(iregdiff = realloc (iregdiff, sizeof(int) * ir_width * ir_height)))
			iw_error ("Out of memory: iregdiff for regionlab");
	}

	iw_time_start (time_region);
	iw_img_border (reg_img, w, h, 1);
	nregions = iw_reg_label (w, h, reg_img, ireg);
	regions = iw_reg_calc (w, h, reg_img, ireg,
						   gimg->img.data, buffer[BUF_CLASS_CONF],
						   &nregions, FALSE, opt_values.reg_min);
	iw_time_stop (time_region, FALSE);

	prev_render_int (b_rlab, ireg, w, h, 60);
	prev_draw_buffer (b_rlab);

	iw_time_start (time_regrest);
	if (regions && nregions > 0) {
		int mode = RENDER_CLEAR;

		iw_debug (3, "Number of hand regions: %d", nregions);

		if (opt_values.diff_do && opt_values.diff_thresh>0) {
			uchar *s = buffer[BUF_DIFF];
			int nCOMinfo;

			iw_img_border (s, w, h, 1);

			COMinfo = iw_reg_label_with_calc (w, h, s, iregdiff,
											  &nCOMinfo, opt_values.reg_diff_min);
			iw_debug (3, "Number of diff regions: %d", nCOMinfo);

			if (COMinfo) {
				for (i=0; i<nCOMinfo; i++)
					COMinfo[i].color = rot;
				iw_sclas_judge_diff (regions, nregions, COMinfo, nCOMinfo,
									 w, h, ireg, opt_values.reg_bew_min,
									 opt_values.reg_bew_anz);

				prev_render_COMinfos (b_region, COMinfo, nCOMinfo, mode, w, h);
				mode &= ~RENDER_CLEAR;
			} else
				iw_sclas_judge (regions, nregions, opt_values.reg_bew_min,
								opt_values.reg_bew_anz);
		} else
			iw_sclas_judge (regions, nregions, opt_values.reg_bew_min,
							opt_values.reg_bew_anz);
		prev_render_regions (b_region, regions, nregions, mode, w, h);
		prev_draw_buffer (b_region);
	}
	iw_time_stop (time_regrest, FALSE);

	for (i=0; i<nregions; i++)
		regions[i].r.farbe = holz;

	iw_time_start (time_track);
	iw_reg_upsample (nregions, regions, gimg->downw, gimg->downh);
	track_do_tracking (regions, nregions, gimg);
	iw_reg_upsample (nregions, regions, 1.0/gimg->downw, 1.0/gimg->downh);
	iw_time_stop (time_track, FALSE);

	{
		static plugDataFunc *func = (plugDataFunc*)1;
		if (func == (plugDataFunc*)1)
			func = plug_function_get (SCLAS_IDENT_REGION, NULL);
		if (func)
			((sclasUpdateFunc)func->func) (&gimg->img, ireg, regions, nregions);
	}

	if (classifier) {
		/* Update the classifier with the help of the regions */
		plugDataFunc *func =
			plug_function_get_full (SCLAS_IDENT_UPDATE, NULL, classifier);
		if (func)
			((sclasUpdateFunc)func->func) (&gimg->img, ireg, regions, nregions);
	}
	if (parameter.out_regions) {
		if (regions)
			iw_reg_upsample (nregions, regions, gimg->downw, gimg->downh);
		iw_output_hypos (regions, nregions, gimg->time, gimg->img_number,
						 parameter.out_regions);
	}
}

static void check_buffer (int width, int height)
{
	static int buf_size = 0;

	if (!buffer[0] || width*height != buf_size) {
		int i;
		for (i=0; i<=BUF_TMP; i++) {
			if (!(buffer[i] = realloc (buffer[i],width*height)))
				iw_error ("Out of memory: image buffer of size %dx%d",
						  width, height);
			memset (buffer[i], 0, width*height);
		}
		buf_size = width*height;
	}
}

/*********************************************************************
  Process the grabbed image/other data.
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL sclas_process (plugDefinition *plug, char *ident, plugData *data)
{
	grabImageData *gimg = (grabImageData*)data->data;
	grabImageData *gimg_old;
	iwImage *imgs[2];
	int actImg = 0;

	gimg_old = grab_get_image (gimg->img_number-1, NULL);

	if (gimg_old) {
		imgs[0] = &gimg_old->img;
		actImg++;
	}
	imgs[actImg] = &gimg->img;

	if (actImg > 0 && (imgs[0]->width != imgs[1]->width ||
					   imgs[0]->height != imgs[1]->height ||
					   imgs[0]->planes != imgs[1]->planes)) {
		iw_warning ("Images differ in size or depth");
	} else {
		char *class_plug = NULL;
		uchar *reg_img;

		if (opt_values.class_mode > 0)
			class_plug = class_mode_names[opt_values.class_mode];

		check_buffer (imgs[0]->width, imgs[0]->height);
		gui_check_exit (FALSE);
		reg_img = do_image (imgs, actImg, class_plug);
		gui_check_exit (FALSE);
		get_regions (reg_img, gimg, class_plug);
	}

	if (gimg_old) grab_release_image (gimg_old);

	return TRUE;
}

static plugDefinition plug_skinclass = {
	"SkinClass",
	PLUG_ABI_VERSION,
	sclas_init,
	sclas_init_options,
	sclas_cleanup,
	sclas_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_sclas_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_skinclass;
}
