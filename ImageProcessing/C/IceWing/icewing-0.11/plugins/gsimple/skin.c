/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Jannik Fritsch and Frank Loemker
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
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gui/Goptions.h"
#include "skin.h"

/* Whitepoint 0.33 +/- 0.06 is excluded from skin region */
#define WHITE_MIN 0.327
#define WHITE_MAX 0.339

/*********************************************************************
 Convert YUV-Pixel to rg-Pixel
*********************************************************************/
void iw_skin_yuv2rg (uchar y, uchar u, uchar v, t_Pixel *rg)
{
	int rgb;
	double Y, U, V;
	int r, g, b;

	Y = 1.164*(double)(y - 16);
	U = (double)(u - 128);
	V = (double)(v - 128);

	r = (int)(Y+1.597*V+0.5);
	g = (int)(Y-0.392*U-0.816*V+0.5);
	b = (int)(Y+2.018*U+0.5);

	r = r < 0 ? 0 : (r > 255 ? 255: r);
	g = g < 0 ? 0 : (g > 255 ? 255: g);
	b = b < 0 ? 0 : (b > 255 ? 255: b);

	rgb = r+g+b;
	if (rgb > 0) {
		rg->r = (r * SKIN_SCALE) / rgb;
		rg->g = (g * SKIN_SCALE) / rgb;
	} else {
		rg->r = 0;
		rg->g = 0;
	}
}

/*********************************************************************
  Return TRUE if rg-value is a white pixel.
*********************************************************************/
static BOOL iw_skin_pixelIsWhite (skinOptions *opts, t_Pixel *rg)
{
	if (opts->excludeWhite &&
		rg->g >= WHITE_MIN*SKIN_SCALE && rg->g <= WHITE_MAX*SKIN_SCALE &&
		rg->r >= WHITE_MIN*SKIN_SCALE && rg->r <= WHITE_MAX*SKIN_SCALE )
		return TRUE;
	else
		return FALSE;
}

/*********************************************************************
  Return upper and lower g-value of skinlocus.
*********************************************************************/
static void iw_skin_functionSkinlocus (int type, double r, double *gup, double *gdown)
{
	static struct {
		double A_up, b_up, c_up, A_down, b_down, c_down;
	} para[] = {
		/* Own parameters */
		{ -4.1000, 3.2000, -0.2650, -0.5200, 0.4000, 0.2000},
		/* Paper parameters */
		{ -1.8423, 1.5294, 0.0422, -0.7279, 0.6066, 0.1766},
		/* NEW Own parameters */
		{ -5.05, 3.71, -0.32, -0.65, 0.05, 0.36},
		/* V9-Sony Firewire-Kameras */
		{ -4.1, 3.41, -0.34, -0.62, 0.08, 0.33}
	};

	type--;

	if (type < 0 || type > 3) {
		fprintf (stderr, "unknown skin locus %d\n", type);
		return;
	}

	/* Upper parabel */
	*gup = r*r*para[type].A_up / SKIN_SCALE
		+ r*para[type].b_up
		+ para[type].c_up * SKIN_SCALE;

	/* Lower parabel */
	*gdown = r*r*para[type].A_down / SKIN_SCALE
		+ r*para[type].b_down
		+ para[type].c_down * SKIN_SCALE;
}

/*********************************************************************
  Return TRUE if rg-value is in Skinlocus.
*********************************************************************/
static BOOL iw_skin_pixelInSkinlocus (skinOptions *opts, t_Pixel *rg)
{
	double gup = 1, gdown = 0;

	if (opts->useLocus == 0)
		return TRUE;

	iw_skin_functionSkinlocus (opts->useLocus, rg->r, &gup, &gdown);

	/* Check: is pixel in paper skinlocus */
	if (rg->g > gdown && rg->g < gup)
		return TRUE;
	else
		return FALSE;
}

/*********************************************************************
  Return true if rg-value is white pixel and in skinlocus.
*********************************************************************/
BOOL iw_skin_pixelIsSkin (skinOptions *opts, t_Pixel *rg)
{
	if (iw_skin_pixelInSkinlocus (opts, rg) &&
		!iw_skin_pixelIsWhite (opts, rg))
		return TRUE;
	else
		return FALSE;
}

/*********************************************************************
  Add GUI elements for skin locus management to page.
*********************************************************************/
skinOptions* iw_skin_init (int page)
{
	static char *skinlocus[] = {
		"none", "trained", "paper", "NEW", "V9", NULL
	};
	skinOptions *opts;

	opts = iw_malloc0 (sizeof(skinOptions), "ptSkinInitOptions");

	opts->excludeWhite = TRUE;
	opts->useLocus = 3;
	opts_toggle_create (page, "Exclude white pixels",
						"do not use white pixel in skinlocus",
						&(opts->excludeWhite));
	opts_radio_create (page, "skinlocus:",
					   "use skinlocus to restrict possible skin colors",
					   skinlocus, &(opts->useLocus));

	return opts;
}
