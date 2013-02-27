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
#include <stdio.h>
#include "tools/tools.h"
#include "gui/Ggui.h"
#include "main/image.h"
#include "main/plugin.h"
#include "skinclass/sclas_image.h"

#define LOOKUP		"lookup.dat"		/* Default Color-Classifier-Lookup-File */

static plugDefinition plug_polynom;

static sclasLookup lookup = {NULL, NULL, NULL, FALSE, FALSE, -1};

/*********************************************************************
  Color classification of img. Write median() smoothed result
  (mask size: size*2+1) to dest.
  pix_cnt=0,1,2 -> calculate average over 1,4,5 pixels
*********************************************************************/
static BOOL poly_classify (iwImage *img, uchar *dest,
						   int pix_cnt, int size, int thresh)
{
	return iw_sclas_classify (img, dest, size, pix_cnt, &lookup);
}

/*********************************************************************
  Updating the BackProjection classifier taking into account the
  classified regions.
*********************************************************************/
static void poly_update (iwImage *img, int *ireg, iwRegion *regions, int numregs)
{
}

/*********************************************************************
  Free the resources allocated during poly_???().
*********************************************************************/
static void poly_cleanup (plugDefinition *plug)
{
}

static void help (plugDefinition *plug)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Perform color classification with a lookup table.\n"
			 "Needs the skinclass plugin.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-l lookup | -lc lookup]\n"
			 "-l       name of lookup table for color classification\n"
			 "         If the table is not found, it is searched in the\n"
			 "         plugins datadir '%s'.\n"
			 "         default: %s\n"
			 "-lc      lookup table for color classification, contains confidence mappings\n",
			 plug->name, ICEWING_NAME, plug->name,
			 lookup.datadir, LOOKUP);
	gui_exit (1);
}

/*********************************************************************
  Initialise the alternative classification.
  'para': command line parameter
  argc, argv: plugin specific options
*********************************************************************/
#define ARG_TEMPLATE "-L:Lr -LC:Cr -H:H"
static void poly_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
{
	void *arg;
	char ch;
	int nr = 0;

	lookup.name = LOOKUP;
	lookup.datadir = plug_get_datadir (plug);
	lookup.confidence = FALSE;
	lookup.twoclass = TRUE;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'L':				/* -l */
				lookup.name = (char*)arg;
				lookup.confidence = FALSE;
				lookup.twoclass = TRUE;
				break;
			case 'C':				/* -lc */
				lookup.name = (char*)arg;
				lookup.confidence = TRUE;
				lookup.twoclass = FALSE;
				break;
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	plug_function_register (plug, "classify", (plugFunc)poly_classify);
	plug_function_register (plug, "update", (plugFunc)poly_update);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int poly_init_options (plugDefinition *plug)
{
	return 0;
}

static plugDefinition plug_polynom = {
	"Polynom",
	PLUG_ABI_VERSION,
	poly_init,
	poly_init_options,
	poly_cleanup,
	NULL
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_poly_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_polynom;
}
