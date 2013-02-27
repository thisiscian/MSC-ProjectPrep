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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#include "main/image.h"
#include "sclas_image.h"

/*********************************************************************
  Calculate difference image of s1 and s2, threshold result with
  thresh, and write median smoothed result (mask size: size*2+1) to d.
  Return approximation for the max value in d:
		thresh >  0 -> 255
		thresh <= 0 -> MAX{abs(127-d[i][j])} (before median is used)
*********************************************************************/
uchar iw_sclas_difference (uchar *s1, uchar *s2, uchar *d, int width, int height,
						   int size, int thresh)
{
	int x, y;
	uchar *dpos, *buffer = iw_img_get_buffer (width*height), ret;

	if (size>0) dpos = buffer;
	else dpos = d;

	if (thresh > 0) {
		for (x=width*height; x>0; x--) {
			y = (*s1++)-(*s2++);
			*dpos++ = abs(y) > thresh ? IW_COLMAX:0;
		}
		ret = IW_COLMAX;
	} else {
		uchar h, min=127, max=127;

		for (x=width*height; x>0; x--) {
			h = ((*s1++)-(*s2++) + IW_COLCNT) / 2;
			if (h > max) max = h;
			if (h < min) min = h;
			*dpos++ = h;
		}
		ret = abs(127-max);
		if (127-min > ret) ret = 127-min;
	}
	if (size>0) {
		if (thresh>0)
			iw_img_medianBW (buffer, d, width, height, size);
		else
			iw_img_median (buffer, d, width, height, size);
	}
	iw_img_release_buffer();
	return ret;
}

/*********************************************************************
  Color classification of img by using a lookup table. Write median
  smoothed result (mask size: size*2+1) to d.
  pix_cnt=0,1,2 -> calculate average over 1,4,5 pixels
  Lookup table is loaded from look->name if it is NULL.
*********************************************************************/
BOOL iw_sclas_classify (iwImage *img, uchar *d, int size, int pix_cnt, sclasLookup *look)
{
	uchar *y=img->data[0], *u=img->data[1], *v=img->data[2], *dpos, *buffer;
	int width = img->width, height = img->height;
	int count = width*height, index;

	dpos = buffer = iw_img_get_buffer (width*height);

	if (look->lookup == NULL) {
		static int sizes[] = {2097152, 65536, 2097152*8, 0};
		struct stat stbuf;
		FILE *file;
		char name[PATH_MAX];
		int i = 0;

		strcpy (name, look->name);
		if (stat(name, &stbuf) != 0) {
			if (look->datadir)
				strcpy (name, look->datadir);
			if (name[strlen(name)-1] != '/') strcat (name, "/");
			strcat (name, look->name);
			if (stat(name, &stbuf) != 0) {
				iw_warning ("Unable to stat lookup table '%s'\n"
							"       or '%s'", name, look->name);
				iw_img_release_buffer();
				return FALSE;
			}
		}
		while (sizes[i] != stbuf.st_size) {
			if (sizes[i] <= 0)
				iw_error ("Lookup table '%s' has unknown size %ld",
						  name, (long)stbuf.st_size);
			i++;
		}
		look->feat_mode = i;
		look->lookup = iw_malloc0 (stbuf.st_size, "lookup table for color classification");
		if (!(file = fopen(name, "r"))) {
			iw_warning ("Unable to open lookup table '%s'", name);
			iw_img_release_buffer();
			return FALSE;
		}
		if (fread (look->lookup, 1, stbuf.st_size, file) != stbuf.st_size) {
			iw_warning ("Unable to read lookup table '%s'", name);
			iw_img_release_buffer();
			fclose (file);
			return FALSE;
		}
		fclose (file);
	}
	if (look->confidence) {
		if (size == 0)
			dpos = d;

		if (pix_cnt==0) {
			if (look->feat_mode == SCLAS_FEAT_ONE_YUV) {
				while (count--) {
					index = ((*y++ >>1) << 14) | ((*u++ >>1) << 7) | (*v++ >>1);
					*dpos++ = look->lookup[index];
				}
			} else if (look->feat_mode == SCLAS_FEAT_ONE_UV) {
				while (count--) {
					*dpos++ = look->lookup[(*u++ << 8) | *v++];
				}
			} else {
				count-=2; dpos++;
				while (count--) {
					index = ((*u++ >>2) << 18) | ((*v++ >>2) << 12);
					index |= ((*(u+1) >>2) << 6) | (*(v+1) >>2);
					*dpos++ = look->lookup[index];
				}
			}
		} else {
			int i, k, r, g, b;
			if (!size) iw_img_border (dpos, width, height, 1);
			y+=width+1; u+=width+1; v+=width+1; dpos+=width+1;
			if (look->feat_mode == SCLAS_FEAT_ONE_YUV) {
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
						index = ((r>>1) << 14) | ((g>>1) << 7) | (b>>1);
						*dpos++ = look->lookup[index];
						y++; u++; v++;
					}
					y+=2; u+=2; v+=2; dpos+=2;
				}
			} else if (look->feat_mode == SCLAS_FEAT_ONE_UV) {
				for (i=1; i<height-1; i++) {
					for (k=1; k<width-1; k++) {
						if (pix_cnt==2) {
							g = (*u + *(u-1) + *(u+1) + *(u-width) + *(u+width)) / 5;
							b = (*v + *(v-1) + *(v+1) + *(v-width) + *(v+width)) / 5;
						} else {
							g = (*(u-1) + *(u+1) + *(u-width) + *(u+width)) / 4;
							b = (*(v-1) + *(v+1) + *(v-width) + *(v+width)) / 4;
						}
						*dpos++ = look->lookup[(g << 8) | b];
						u++; v++;
					}
					u+=2; v+=2; dpos+=2;
				}

			} else {
				for (i=1; i<height-1; i++) {
					for (k=2; k<width-2; k++) {
						if (pix_cnt==2) {
							g = (*u + *(u-1) + *(u+1) + *(u-width) + *(u+width)) / 5;
							b = (*v + *(v-1) + *(v+1) + *(v-width) + *(v+width)) / 5;
						} else {
							g = (*(u-1) + *(u+1) + *(u-width) + *(u+width)) / 4;
							b = (*(v-1) + *(v+1) + *(v-width) + *(v+width)) / 4;
						}
						index = ((g>>2) << 18) | ((b>>2) << 12);
						u++; v++;
						if (pix_cnt==2) {
							g = (*(u+1) + *u + *(u+2) + *(u-width+1) + *(u+width+1)) / 5;
							b = (*(v+1) + *v + *(v+2) + *(v-width+1) + *(v+width+1)) / 5;
						} else {
							g = (*u + *(u+2) + *(u-width+1) + *(u+width+1)) / 4;
							b = (*v + *(v+2) + *(v-width+1) + *(v+width+1)) / 4;
						}
						index |= ((g>>2) << 6) | (b>>2);
						*dpos++ = look->lookup[index];
					}
					u+=4; v+=4; dpos+=4;
				}
			}
		}
		if (size>0)
			iw_img_median (buffer, d, width, height, size);
	} else {
		if (size == 0) dpos = d;
		if (pix_cnt==0) {
			if (look->feat_mode == SCLAS_FEAT_ONE_YUV) {
				if (look->twoclass)
					while (count--) {
						index = ((*y++ >>1) << 14) | ((*u++ >>1) << 7) | (*v++ >>1);
						*dpos++ = (1-look->lookup[index])*IW_COLMAX;
					}
				else
					while (count--) {
						index = ((*y++ >>1) << 14) | ((*u++ >>1) << 7) | (*v++ >>1);
						*dpos++ = look->lookup[index];
					}
			} else if (look->feat_mode == SCLAS_FEAT_ONE_UV) {
				if (look->twoclass) {
					while (count--) {
						*dpos++ = (1-look->lookup[(*u++ << 8) | *v++])*IW_COLMAX;
					}
				} else
					while (count--) {
						*dpos++ = look->lookup[(*u++ << 8) | *v++];
					}
			} else {
				count-=2; dpos++;
				if (look->twoclass)
					while (count--) {
						index = ((*u++ >>2) << 18) | ((*v++ >>2) << 12);
						index |= ((*(u+1) >>2) << 6) | (*(v+1) >>2);
						*dpos++ = (1-look->lookup[index])*IW_COLMAX;
					}
				else
					while (count--) {
						index = ((*u++ >>2) << 18) | ((*v++ >>2) << 12);
						index |= ((*(u+1) >>2) << 6) | (*(v+1) >>2);
						*dpos++ = look->lookup[index];
					}
			}
		} else {
			int i, k, r, g, b;
			if (!size) iw_img_border (dpos, width, height, 1);
			y+=width+1; u+=width+1; v+=width+1; dpos+=width+1;
			if (look->feat_mode == SCLAS_FEAT_ONE_YUV) {
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
						index = ((r>>1) << 14) | ((g>>1) << 7) | (b>>1);
						if (look->twoclass)
							*dpos++ = (1-look->lookup[index])*IW_COLMAX;
						else
							*dpos++ = look->lookup[index];
						y++; u++; v++;
					}
					y+=2; u+=2; v+=2; dpos+=2;
				}
			} else if (look->feat_mode == SCLAS_FEAT_ONE_UV) {
				for (i=1; i<height-1; i++) {
					for (k=1; k<width-1; k++) {
						if (pix_cnt==2) {
							g = (*u + *(u-1) + *(u+1) + *(u-width) + *(u+width)) / 5;
							b = (*v + *(v-1) + *(v+1) + *(v-width) + *(v+width)) / 5;
						} else {
							g = (*(u-1) + *(u+1) + *(u-width) + *(u+width)) / 4;
							b = (*(v-1) + *(v+1) + *(v-width) + *(v+width)) / 4;
						}
						if (look->twoclass)
							*dpos++ = (1-look->lookup[(g << 8) | b])*IW_COLMAX;
						else
							*dpos++ = look->lookup[(g << 8) | b];
						u++; v++;
					}
					u+=2; v+=2; dpos+=2;
				}
			} else {
				for (i=1; i<height-1; i++) {
					for (k=2; k<width-2; k++) {
						if (pix_cnt==2) {
							g = (*u + *(u-1) + *(u+1) + *(u-width) + *(u+width)) / 5;
							b = (*v + *(v-1) + *(v+1) + *(v-width) + *(v+width)) / 5;
						} else {
							g = (*(u-1) + *(u+1) + *(u-width) + *(u+width)) / 4;
							b = (*(v-1) + *(v+1) + *(v-width) + *(v+width)) / 4;
						}
						index = ((g>>2) << 18) | ((b>>2) << 12);
						u++; v++;
						if (pix_cnt==2) {
							g = (*(u+1) + *u + *(u+2) + *(u-width+1) + *(u+width+1)) / 5;
							b = (*(v+1) + *v + *(v+2) + *(v-width+1) + *(v+width+1)) / 5;
						} else {
							g = (*u + *(u+2) + *(u-width+1) + *(u+width+1)) / 4;
							b = (*v + *(v+2) + *(v-width+1) + *(v+width+1)) / 4;
						}
						index |= ((g>>2) << 6) | (b>>2);
						if (look->twoclass)
							*dpos++ = (1-look->lookup[index])*IW_COLMAX;
						else
							*dpos++ = look->lookup[index];
					}
					u+=4; v+=4; dpos+=4;
				}
			}
		}
		if (size>0) {
			if (look->twoclass)
				iw_img_medianBW (buffer, d, width, height, size);
			else
				iw_img_max (buffer, d, width, height, size);
		}
	}
	iw_img_release_buffer();
	return TRUE;
}

/*********************************************************************
  Combine difference image and color classification (size
  width x height) according to mode comb_mode. Threshold result with
  thresh, and write it to d.
*********************************************************************/
void iw_sclas_combine (uchar *diff, uchar max_diff, uchar *classif, uchar *d,
					   int width, int height, int thresh, int comb_mode)
{
	int count = width*height;

	if (thresh>0) {
		if (comb_mode == SCLAS_COMB_ADD) {
			if (max_diff == IW_COLMAX)
				while (count--)
					*d++ = ((*diff++ + *classif++)/2) > thresh ? IW_COLMAX:0;
			else
				while (count--)
					*d++ = ((abs(127-*diff++) + *classif++)/2) > thresh ? IW_COLMAX:0;
		} else {
			if (max_diff == IW_COLMAX)
				while (count--)
					*d++ = ((*diff++ * *classif++)/IW_COLMAX) > thresh ? IW_COLMAX:0;
			else {
				if (max_diff < 30) max_diff = 30;	/* Hardcoded lower bound */
				while (count--)
					*d++ = ((abs(127-*diff++) * *classif++)/max_diff) > thresh ? IW_COLMAX:0;
			}
		}
	} else {
		if (comb_mode == SCLAS_COMB_ADD) {
			if (max_diff == IW_COLMAX)
				while (count--)
					*d++ = (*diff++ + *classif++)/2;
			else
				while (count--)
					*d++ = (abs(127-*diff++) + *classif++)/2;
		} else {
			if (max_diff == IW_COLMAX)
				while (count--)
					*d++ = (*diff++ * *classif++)/IW_COLMAX;
			else {
				if (max_diff < 30) max_diff = 30;	/* Hardcoded lower bound */
				while (count--)
					*d++ = (abs(127-*diff++) * *classif++)/max_diff;
			}
		}
	}
}

/*********************************************************************
  Return the square of the min. distance of (x,y) to the polygon p.
*********************************************************************/
static int reg_calcdist (Polygon_t *p, int x, int y)
{
	int x1, y1, x2, y2, d, l2, min, i;
	float r;

	min = 9999999;
	x2 = p->punkt[0]->x;
	y2 = p->punkt[0]->y;
	for (i=1; i<p->n_punkte; i++) {
		x1 = x2; y1 = y2;
		x2 = p->punkt[i]->x;
		y2 = p->punkt[i]->y;

		l2 = (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1);

		r = (float)((y1-y)*(y1-y2)-(x1-x)*(x2-x1)) / l2;
		/* |r*sqrt(l2)| = Distance from (x1,y1) and
						  Schnittpunkt des Lots von (x,y) auf Linie mit Linie */
		if (r<0) {
			d = (x-x1)*(x-x1) + (y-y1)*(y-y1);
		} else if (r>1) {
			d = (x-x2)*(x-x2) + (y-y2)*(y-y2);
		} else {
			d = (y1-y)*(x2-x1)-(x1-x)*(y2-y1);
			d = (double)d*d / l2;	/* Prevent overrun by casting to double */
		}
		if (d<min) min = d;
	}
	return min;
}

/*********************************************************************
  Compare function for qsort to sort regions.
*********************************************************************/
static int region_cmp (iwRegion *small, iwRegion *big)
{
	if (small->r.pixelanzahl == 0) return 1;
	if (big->r.pixelanzahl == 0) return -1;
	if (small->judgement < big->judgement)
		return 1;
	else if (small->judgement > big->judgement)
		return -1;
	else
		return 0;
}

/*********************************************************************
  Judge regions based on avgConf and based on the diff regions regDiff
  (are attached to neighboring regions).
  image: region labeled image of size xsize x ysize.

  Regions with a judgement < bew_min -> pixelanzahl = 0
  max_reg_anz>0: Only the max_reg_anz regions with the best judgement
                 will have pixelanzahl > 0.
*********************************************************************/
void iw_sclas_judge_diff (iwRegion *regions, int nregions,
						  iwRegCOMinfo *regDiff, int nregDiff,
						  int xsize, int ysize, gint32 *image,
						  float bew_min, int max_reg_anz)
{
	int d, r, col, sum_in_region;
	BOOL done;
	static int *in_region = NULL, len_region = 0;

	if (len_region < nregions) {
		in_region = (int *) realloc (in_region, nregions * sizeof(int));
		if (in_region == NULL)
			iw_error ("Cannot alloc %ld Bytes for in_region", nregions * (long)sizeof(int));
		len_region = nregions;
	}

	memset (in_region, 0, nregions * sizeof(int));

	/* Try to attach diff regions to the color regions */
	sum_in_region = 0;
	for (d=0; d<nregDiff; d++) {
		if (regDiff[d].pixelcount > 0) {
			done = FALSE;
			col = image[regDiff[d].com_y*xsize+regDiff[d].com_x];

			for (r=0; !done && r<nregions; r++) {
				if (regions[r].labindex == col) {
					/* Center of mass of the diff region is inside a region */
					sum_in_region += regDiff[d].pixelcount;
					in_region[r] += regDiff[d].pixelcount;
					regDiff[d].color = gruen;
					regions[r].r.echtfarbe.modell = FARB_MODELL_UNDEF;
					regions[r].r.farbe = weiss;

					done = TRUE;
				}
			}
			if (!done) {
				/* Try to attach diff regions, which are outside of regions,
				   to the region with the smallest distance */
				int min = 9999999, r_min = -1, min2 = 9999999, dist;

				/* Get the two regions with the smallest distance */
				for (r=0; r<nregions; r++) {
					dist = reg_calcdist (&regions[r].r.polygon,
										 regDiff[d].com_x, regDiff[d].com_y);
					if (dist<min2) {
						if (dist<min) {
							min2 = min;
							min = dist; r_min = r;
						} else
							min2 = dist;
					}
				}
				min = sqrtf ((float)min);
				min2 = sqrtf ((float)min2);
				if ((min==0 || (float)min2/min > 1.8) && min < xsize/10) {
					regDiff[d].color = gruen;
					regions[r_min].r.echtfarbe.modell = FARB_MODELL_UNDEF;
					regions[r_min].r.farbe = weiss;
					sum_in_region += regDiff[d].pixelcount;
					in_region[r_min] += regDiff[d].pixelcount;
				}
			}
		}
	}
	/* Judge based on avgConf and diff regions attached to the region */
	for (r=0; r<nregions; r++) {
		float b;
		if (regions[r].r.pixelanzahl)
			b = regions[r].avgConf/255 +
				(float)in_region[r] / sqrtf((float)regions[r].r.pixelanzahl);
		else
			b = regions[r].avgConf/255;
		if (b < bew_min) {
			/* Judgement of region is too bad -> remove */
			regions[r].judgement = 0.0;
			regions[r].r.pixelanzahl = 0;
		} else
			regions[r].judgement = b;
	}

	qsort (regions, nregions, sizeof(iwRegion), (int(*)())region_cmp);

	if (max_reg_anz > 0)
		for (r=max_reg_anz; r<nregions; r++)
			regions[r].r.pixelanzahl = 0;
}

/*********************************************************************
  Judge the regions 'regions' based on avgConf.

  If judgement is < bew_min -> pixelanzahl = 0
  max_reg_anz>0: Only the max_reg_anz regions with the best judgement
                 will have pixelanzahl > 0.
*********************************************************************/
void iw_sclas_judge (iwRegion *regions, int nregions,
					 float bew_min, int max_reg_anz)
{
	int r;
	for (r=0; r<nregions; r++) {
		float b = regions[r].avgConf/255;
		if (b < bew_min) {
			/* Judgement of region is too bad -> remove */
			regions[r].judgement = 0.0;
			regions[r].r.pixelanzahl = 0;
		} else
			regions[r].judgement = b;
	}

	qsort (regions, nregions, sizeof(iwRegion), (int(*)())region_cmp);

	if (max_reg_anz > 0)
		for (r=max_reg_anz; r<nregions; r++)
			regions[r].r.pixelanzahl = 0;
}
