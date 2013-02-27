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
#include "gui/Gimage.h"
#include "tools/tools.h"
#include "main/image.h"
#include "gsimple_image.h"

/* Variables for the img_fill function,
 *  global to reduce stack need of the recursive img_fill_helper()
 */
static iwImage *fill_s;		/* Source image */
static uchar *fill_d;			/* Destination image */
static iwColor fill_col;		/* Comparison color in img_cmp_pix() */
static int fill_y, fill_x;		/* Current location in image */
static iwGsimpleFillFunc fill_fkt;	/* !=NULL: Function to use instead of img_cmp_pix() */
								/*		   fill_data is passed to the function. */
								/* ==NULL: fill_data gives threshold for img_cmp_pix() */
static void *fill_data;

static BOOL img_cmp_pix (int offset)
{
	BOOL fill;

	/* Pixel already set -> return */
	if (*(fill_d+offset) == IW_COLMAX) return FALSE;

	if (fill_fkt) {
		fill = fill_fkt (fill_s, fill_d, offset, fill_col, fill_data);
	} else {
		if (fill_s->planes == 1) {
			fill = (*(fill_s->data[0]+offset) - fill_col.r) < GPOINTER_TO_INT(fill_data);
		} else {
			int y = abs(*(fill_s->data[0]+offset) - fill_col.r);
			int u = abs(*(fill_s->data[1]+offset) - fill_col.g);
			int v = abs(*(fill_s->data[2]+offset) - fill_col.b);

			/* Compare max difference of all channels with the threshold */
			int max = y;
			if (u > max) max = u;
			if (v > max) max = v;

			fill = max < GPOINTER_TO_INT(fill_data);

			/*
			fill = (u + v) < GPOINTER_TO_INT(fill_data);
			*/
		}
	}

	if (fill) {
		*(fill_d+offset) = IW_COLMAX;
		return TRUE;
	} else
		return FALSE;
}

static void img_fill_helper (int offset)
{
	int of, start, end;

	/* Get maximal points left and right from the current point */
	start = fill_x;
	of = offset;
	while (start>0 && img_cmp_pix (--of))
		start--;

	end = fill_x;
	of = offset;
	while (end<fill_s->width-1 && img_cmp_pix (++of))
		end++;

	of = offset-(fill_x-start);

	/* Test all points below and above the found line for possible continuations */
	for (; start<=end; start++) {
		if (fill_y > 0 && img_cmp_pix (of-fill_s->width)) {
			fill_y -= 1;
			fill_x = start;
			img_fill_helper (of-fill_s->width);
			fill_y += 1;
		}
		if (fill_y < fill_s->height-1 && img_cmp_pix (of+fill_s->width)) {
			fill_y += 1;
			fill_x = start;
			img_fill_helper (of+fill_s->width);
			fill_y -= 1;
		}
		of++;
	}
}

/*********************************************************************
  Fill in d (size: width x height) the region
    - which has in s a color which is not more differnt than (int)data
	  if (fkt==NULL) to (ycol,ucol,vcol) and
	- which starts at (xs , ys), (xs+-3 , ys), or (xs , ys+-3)
*********************************************************************/
void iw_gsimple_fill (iwImage *s, uchar *d, int xs, int ys,
					  iwColor col, iwGsimpleFillFunc fkt, void *data)
{
	int of = s->width*ys + xs;

	/* Set variables,
	   global to reduce stack need of the recursive img_fill_helper() */
	fill_s = s;
	fill_d = d;
	fill_col = col;
	fill_fkt = fkt;
	fill_data = data;
	fill_y = ys;
	fill_x = xs;

	/* Search for a start point which is below the threshold */
	if (img_cmp_pix (of)) {
	} else if (fill_x >= 3 && img_cmp_pix (of-3)) {
		fill_x -= 3;
		of -= 3;
	} else if (fill_x < fill_s->width-3 && img_cmp_pix (of+3)) {
		fill_x += 3;
		of += 3;
	} else if (fill_y >= 3 && img_cmp_pix (of-3*fill_s->width)) {
		fill_y -= 3;
		of += 3*fill_s->width;
	} else if (fill_y < fill_s->height-3 && img_cmp_pix (of+3*fill_s->width)) {
		fill_y += 3;
		of -= 3*fill_s->width;
	} else
		return;
	iw_img_col_debug ("img_fill", fill_s, fill_x, fill_y);
	img_fill_helper (of);
}

/*********************************************************************
  Call for every border pixel of the region with label 'label' the
  function fkt (img, label, pixel-offset) and shrink the region 'cnt'
  times by 1 pixel.
  img : region labeled image of size width x height.
  reg_x, reg_y, reg_w, reg_h: Part of the image which should be
							  searched for the region.
*********************************************************************/
void iw_gsimple_shrink (int *img, int width, int height, int reg_x, int reg_y,
						int reg_w, int reg_h, int label, int cnt,
						iwGsimpleShrinkFunc fkt)
{
#define MAXLOOP			50000	/* Maximal contour length */
	/* x- and y- offsets giving the next point to be checked */
	static int xoffset[9][2] = {
		{ 0, 0}, { 1, 1}, {-1, 0}, {-1, 0}, {-1,-1},
		{ 1, 1}, { 1, 0}, { 1, 0}, {-1,-1}};
	static int yoffset[9][2] = {
		{ 0, 0}, {-1, 0}, {-1,-1}, { 1, 1}, {-1, 0},
		{ 1, 0}, {-1,-1}, { 1, 1}, { 1, 0}};
	/* Search directions which are used next if a contour point was found */
	static int aendere_richtung[9][2] = {
		{0, 0}, {2, 7}, {8, 1}, {4, 5}, {6, 3},
		{3, 6}, {5, 4}, {1, 8}, {7, 2}};
	int x, y, start_x, start_y, richtung;
	int loopcount = 0;
	int *disk, *point, *points;

	if (cnt==0 && !fkt) return;

	points = point = iw_img_get_buffer (sizeof(int) * (MAXLOOP*2 + (cnt*2-1)*(cnt*2-1)));
	disk = points + MAXLOOP*2;

	start_x = reg_x;
	start_y = reg_y;

	/* Search for starting point of region */
	while (*(img + start_y*width+start_x) != label) {
		start_x++;
		if (start_x >= reg_x+reg_w) {
			start_x = reg_x;
			start_y++;
		}

		/* Check for end of image */
		if (start_y >= reg_y+reg_h) {
			iw_img_release_buffer();
			return;
		}
	}

	/* Start direction is always 4 */
	richtung = 4;
	x = start_x;
	y = start_y;

	/* Loop until the start point is reached again */
	do {
		/* Check next possible point */
		if (img[(x + xoffset[richtung][0]) +
			   (y + yoffset[richtung][0]) * width] == label) {

			x += xoffset[richtung][0];
			y += yoffset[richtung][0];

			/* Remember contour point */
			*point++ = x;
			*point++ = y;

			/* New search direction */
			richtung = aendere_richtung[richtung][0];

			if (x == start_x && y == start_y) {
				/* Test for   12
							 3 4
				   1 is start point and is reached from 3 with direction 6 (then 5).
				   2 and 4 are up to then not part of the polygon.
				*/
				if (richtung != 5 ||
					(img[x+1 + y * width] != label &&
					 img[x+1 + (y + 1) * width] != label))
					loopcount = MAXLOOP;
			}
		}
		/* Check next possible point */
		else if (img[(x + xoffset[richtung][1]) +
					(y + yoffset[richtung][1]) * width] == label) {

			x += xoffset[richtung][1];
			y += yoffset[richtung][1];

			/* Remember contour point */
			*point++ = x;
			*point++ = y;

			/* Here the search direction is NOT changed! */

			/* Start point reached? */
			if (x == start_x && y == start_y)
				loopcount = MAXLOOP;
		}
		/* If the two checked points are no contour points
		   -> change search direction */
		else richtung = aendere_richtung[richtung][1];

		loopcount++;
		/* Search until start point is reached again, or, if an error
		   occured, until to many loops */
	} while (loopcount < MAXLOOP);

	/* Remove a contour of thickness cnt from the image */
	loopcount = x = (point-points)/2;
	if (fkt) {
		point = points;
		while (x--) {
			fkt (img, label, *point+ *(point+1)*width);
			point += 2;
		}
	}
	if (cnt == 1) {
		while (loopcount--) {
			img[*points+ *(points+1)*width] = 0;
			points += 2;
		}
	} else if (cnt > 1) {
		int step = width-cnt*2+1;
		int *s, *d, *endimg;

		cnt--;
		s = disk;
		for (x=-cnt; x<=cnt; x++)
			for (y=-cnt; y<=cnt; y++)
				*s++ = (x*x+y*y <= cnt*cnt) ? 0 : -1;

		endimg = img+width*height;
		while (loopcount--) {
			s = disk;
			d = img + *points - cnt + (*(points+1) - cnt)*width;

			for (x=cnt*2+1; x>0; x--) {
				for (y=cnt*2+1; y>0; y--) {
					if (d >= img && d < endimg)
						*d &= *s;
					d++; s++;
				}
				d += step;
			}
			points += 2;
		}
	}

	iw_img_release_buffer();
#undef MAXLOOP
}
