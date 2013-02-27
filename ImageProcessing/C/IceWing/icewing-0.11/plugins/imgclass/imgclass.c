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
#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "main/output.h"
#include "main/plugin.h"
#include "main/image.h"
#include "skinclass/sclas_image.h"

	/* Default Color-Classifier-Lookup-File */
#define LOOKUP		"imgclass.dat"

#define ICLAS_OFF		0
#define ICLAS_SEG		1
#define ICLAS_TRACE		2

static plugDefinition plug_imgclass;

typedef struct {
	int class_mode;		/* Table lookup based image segmentation? region tracing? */
	int class_avg;		/* Number of pixels used for averaging */
	int smooth_size;	/* Size of smoothing (~median) filter */
	int reg_min;		/* Smallest regions to be calculated */
	BOOL inclusion;		/* Calc inclusions? */
} iclasValues;

typedef struct iclasParameter {
	sclasLookup lookup;
	char *output;
} iclasParameter;

typedef struct iclasPlugin {		/* All parameter of one plugin instance */
	plugDefinition def;
	iclasValues values;
	iclasParameter para;
	prevBuffer *b_colorseg, *b_region;
} iclasPlugin;

/*********************************************************************
  Free the resources allocated during iclas_???().
*********************************************************************/
static void iclas_cleanup (plugDefinition *plug)
{
}

static void help (iclasPlugin *plug)
{
	fprintf (stderr,"\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Perform color classification of the image with the id 'image'\n"
			 "by using a lookup table\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-l lookup] [-o]\n"
			 "-l       name of lookup table for color classification\n"
			 "         If the table is not found, it is searched in the\n"
			 "         plugins datadir '%s'.\n"
			 "         default: %s\n"
			 "-o       output regions on stream <%s>_regions\n",
			 plug->def.name, ICEWING_NAME, plug->def.name,
			 plug->para.lookup.datadir, LOOKUP, IW_DACSNAME);
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
#define ARG_TEMPLATE "-L:Lr -O:O -H:H"
static void iclas_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	iclasPlugin *plug = (iclasPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;

	plug->para.lookup.lookup = NULL;
	plug->para.lookup.name = LOOKUP;
	plug->para.lookup.datadir = plug_get_datadir (plug_d);
	plug->para.lookup.confidence = FALSE;
	plug->para.lookup.twoclass = FALSE;
	plug->para.output = NULL;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'L':				/* -l */
				plug->para.lookup.name = (char*)arg;
				break;
			case 'O':
				plug->para.output = iw_output_register_stream ("_regions",
															   (NDRfunction_t*)ndr_RegionHyp);
				break;
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	plug_observ_data (plug_d, "image");
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int iclas_init_options (plugDefinition *plug_d)
{
	static char *class_mode[] = {"Off", "Segment", "Seg+Trace", NULL};
	static char *class_avg[] = {"1", "4", "5", NULL};
	iclasPlugin *plug = (iclasPlugin *)plug_d;
	int p;
	char name[30];

	plug->values.class_mode = ICLAS_OFF;
	plug->values.class_avg = 0;
	plug->values.smooth_size = 0;
	plug->values.reg_min = 100;
	plug->values.inclusion = FALSE;

	p = opts_page_append (plug->def.name);

	opts_radio_create (p, "Detection:",
					   "Perform table lookup based image segmentation and/or region tracing?"
					   " Only segmentation: result is stored under id 'segmentation'.",
					   class_mode, &plug->values.class_mode);
	opts_radio_create (p, "Pixel Cnt:",
					   "Number of to pixels to average for the classifier feature",
					   class_avg, &plug->values.class_avg);
	opts_entscale_create (p, "Smooth Size",
						  "Radius of filter (kind of median) for color segmented image",
						  &plug->values.smooth_size, 0, 20);
	opts_entscale_create (p, "Min Region", "Remove all smaller regions",
						  &plug->values.reg_min, 8, 5000);
	opts_toggle_create (p, "Calc Inclusions",
						"Determine if a region is inside another region",
						&plug->values.inclusion);

	sprintf (name, "%s color segmentation", plug->def.name);
	plug->b_colorseg = prev_new_window (name, -1, -1, FALSE, FALSE);

	sprintf (name, "%s recognized regions", plug->def.name);
	plug->b_region = prev_new_window (name, -1, -1, FALSE, FALSE);
	prev_opts_append (plug->b_region, PREV_TEXT, PREV_REGION, -1);

	if (!plug->para.lookup.name) opts_value_set ("Detection:", FALSE);

	return p;
}

static void segmentation_destroy (void *data)
{
	free (data);
}

/*********************************************************************
  Process the grabbed image/other data.
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL iclas_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	iclasPlugin *plug = (iclasPlugin *)plug_d;
	grabImageData *gimg = (grabImageData*)data->data;
	iwImage *img = &gimg->img;
	int w = img->width, h = img->height;
	int *ireg, nregions, i;
	uchar *d;
	iwRegion *regions = NULL;

	iw_time_add_static (time_class, "IClass segm");

	if (plug->values.class_mode == ICLAS_OFF) return TRUE;

	ireg = iw_img_get_buffer(sizeof(int)*w*h);
	d = malloc (w*h);

	iw_time_start (time_class);

	iw_sclas_classify (img, d, plug->values.smooth_size,
					   plug->values.class_avg, &plug->para.lookup);
	prev_render (plug->b_colorseg, &d, w, h, IW_INDEX);
	prev_draw_buffer (plug->b_colorseg);

	if (plug->values.class_mode == ICLAS_SEG) {
		plug_data_set (plug_d, "segmentation", d, segmentation_destroy);
	} else {
		iw_img_border (d, w, h, 1);
		nregions = iw_reg_label (w, h, d, ireg);
		regions = iw_reg_calc (w, h, d, ireg, img->data, NULL, &nregions,
							   plug->values.inclusion, plug->values.reg_min);
		if (regions) {
			iw_debug (3, "Number of second regions: %d", nregions);

			/* If doEinschluss == true: Regions with color 0 are calculated */
			for (i = 0; i < nregions; i++)
				if (!regions[i].r.farbe) regions[i].r.pixelanzahl = 0;

			prev_render_regions (plug->b_region, regions, nregions, RENDER_CLEAR, w, h);
			prev_draw_buffer (plug->b_region);
		}
		if (plug->para.output) {
			if (regions)
				iw_reg_upsample (nregions, regions, gimg->downw, gimg->downh);
			iw_output_hypos (regions, nregions, gimg->time, gimg->img_number,
							 plug->para.output);
		}
		free (d);
	}
	iw_img_release_buffer();
	iw_time_stop (time_class, FALSE);

	return TRUE;
}

static plugDefinition plug_imgclass = {
	"ImgClass%d",
	PLUG_ABI_VERSION,
	iclas_init,
	iclas_init_options,
	iclas_cleanup,
	iclas_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_iclas_get_info (int instCount, BOOL *append)
{
	iclasPlugin *plug = calloc (1, sizeof(iclasPlugin));

	plug->def = plug_imgclass;
	plug->def.name = g_strdup_printf (plug_imgclass.name, instCount);

	*append = TRUE;
	return (plugDefinition*)plug;
}
