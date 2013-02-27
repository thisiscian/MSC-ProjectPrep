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
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "tools/tools.h"
#include "region.h"

#define isequal(a,b)	(fabsf((a)-(b)) < 0.00001)

/**
* Zweck:	Regionenlabelling eines farbklassifizierten Eingabebildes
* Autor:	Gernot A. Fink
* Speed-Up      Franz Kummert
* Datum:	11.1.1996
**/

static gint32 *r_alias = NULL;
static gint32 last_new = -1, last_old = -1;

static int normalize (int r)
{
	while (r_alias[r] >= 0 && r_alias[r] < r)
		r = r_alias[r];

	return r;
}

static inline void alias (gint32 r_new, gint32 r_old)
{
	int r_al;

	if (r_new == r_old) return;

	if (r_new < r_old) {
		r_al = r_old; r_old = r_new; r_new = r_al;
	}

	if (last_new == r_new && last_old == r_old)
		return;
	else {
		last_new = r_new;
		last_old = r_old;
	}

	do {
		r_al = r_alias[r_new];
		if (r_old == r_al)
			return;
		if (r_old > r_al) {
			r_alias[r_new] = r_old;
			r_new = r_old;
			r_old = r_al;
		} else
			r_new = r_al;
	} while (r_old >= 0);
}

/*********************************************************************
  Regionenlabeling des Bildes color (Groesse xsize x ysize)
  durchfuehren und nach region schreiben.
  Falls minPixelCount>0:
	Pixelanzahl, Farbe und Schwerpunkt der Regionen berechnen.
	Pixelanzahl < minPixelCount: Pixelanzahl der Region = Null
*********************************************************************/
static iwRegCOMinfo *region_label_do (int xsize, int ysize, const uchar *color,
									  gint32 *region, int *nregions, int minPixelCount)
{
	static int len_COMinfo = 0;
	static iwRegCOMinfo *COMinfo = NULL;

	static int _xsize = -1;
	static int _ysize = -1;

	int rcount = 0;
	int found;
	int i, k;
	gint32 *regionpntr, *r;
	const uchar *pixelpntr;

	last_new = last_old = -1;

	/* Beim ersten Aufruf ... */
	if (!r_alias || xsize != _xsize || ysize != _ysize) {
		/* Einzel-Farbbild erzeugen */
		_xsize = xsize;
		_ysize = ysize;

		/* Regionen-Alias-Liste erzeugen ... */
		if (r_alias) r_alias--;			/* Index -1 erlauben */
		if (!(r_alias = realloc (r_alias, (1 + xsize * ysize) * sizeof(gint32))))
			iw_error ("Out of memory: r_alias in regionlab");

		/* ... und initialisieren */
		for (i = 1 + xsize * ysize, r = r_alias; i > 0; i--)
			*r++ = -1;
		r_alias++;
	}

	for (i=1; i<ysize-1; i++) {
		*(region+i*xsize) = 0;
		*(region+i*xsize+xsize-1) = 0;
	}
	memset (region, 0, sizeof(gint32)*xsize);
	memset (region+(ysize-1)*xsize, 0, sizeof(gint32)*xsize);

	/* Uebers Bild laufen und jeweils linken Nachbarn und die drei
	   oberhalb liegenden Pixel betrachten */
	/* Sonderbehandlung fuer erste Bildzeile */
	for (k=2, region[xsize+1] = rcount++; k<xsize-1; ++k) {
		if (color[k+xsize] == color[k-1+xsize])
			region[k+xsize] = region[k-1+xsize];
		else
			region[k+xsize] = rcount++;
	}

	for (i=2; i<ysize-1; ++i) {
		/* Sonderbehandlung fuer erstes Pixel der Zeile */
		pixelpntr = color+i*xsize + 1;
		regionpntr = region+i*xsize + 1;
		found = 0;
		if (*pixelpntr == *(pixelpntr-xsize)) {
			*regionpntr = *(regionpntr-xsize);
			found = 1;
		}
		if (*pixelpntr == *(pixelpntr-xsize+1)) {
			if (found)
				alias(*regionpntr, *(regionpntr-xsize+1));
			else {
				*regionpntr = *(regionpntr-xsize+1);
				found = 1;
			}
		}
		if (!found) *regionpntr = rcount++;

		/* Rest der Zeile */
		for (k=2, pixelpntr = color+i*xsize+k,
				 regionpntr = region+i*xsize+k; k<xsize-2;
			 ++k, pixelpntr++, regionpntr++) {
			found = 0;
			if (*pixelpntr == *(pixelpntr-1)) {
				*regionpntr = *(regionpntr-1);
				found = 1;
			}
			if (*pixelpntr == *(pixelpntr-xsize-1)) {
				if (found)
					alias(*regionpntr,
						  *(regionpntr-xsize-1));
				else {
					*regionpntr = *(regionpntr-xsize-1);
					found = 1;
				}
			}
			if (*pixelpntr == *(pixelpntr-xsize)) {
				if (found)
					alias(*regionpntr,
						  *(regionpntr-xsize));
				else {
					*regionpntr = *(regionpntr-xsize);
					found = 1;
				}
			}
			if (*pixelpntr == *(pixelpntr-xsize+1)) {
				if (found)
					alias(*regionpntr,
						  *(regionpntr-xsize+1));
				else {
					*regionpntr = *(regionpntr-xsize+1);
					found = 1;
				}
			}
			if (!found) *regionpntr = rcount++;
		}

		/* Sonderbehandlung fuer letztes Pixel der Zeile */
		pixelpntr = color+i*xsize+k;
		regionpntr = region+i*xsize+k;
		found = 0;
		if (*pixelpntr == *(pixelpntr-1)) {
			*regionpntr = *(regionpntr-1);
			found = 1;
		}
		if (*pixelpntr == *(pixelpntr-xsize-1)) {
			if (found)
				alias(*regionpntr, *(regionpntr-xsize-1));
			else {
				*regionpntr = *(regionpntr-xsize-1);
				found = 1;
			}
		}
		if (*pixelpntr == *(pixelpntr-xsize)) {
			if (found)
				alias(*regionpntr, *(regionpntr-xsize));
			else {
				*regionpntr = *(regionpntr-xsize);
				found = 1;
			}
		}
		if (!found) *regionpntr = rcount++;
	}

	/* Alle eingetragenen Regionen-Aliase normalisieren ... */
	for (i = 0, r = r_alias; i < rcount; i++, r++)
		*r = normalize(i);
	/* ... und fortlaufende Nummern vergeben */
	*nregions = 0;
	for (i = 0, r = r_alias; i < rcount; i++, r++)
		if (*r < i)
			*r = r_alias[*r];
		else
			*r = (*nregions)++;

	/* Regionen-Aliase aufloesen und COM und pixelcount bestimmen... */
	if (minPixelCount>0) {
		iwRegCOMinfo *infoptr;

		if (*nregions > len_COMinfo) {
			len_COMinfo = *nregions;
			COMinfo = (iwRegCOMinfo *) realloc (COMinfo, len_COMinfo * sizeof (iwRegCOMinfo));
			if (COMinfo == NULL)
				iw_error ("Cannot realloc %ld Bytes for COMinfo in regionlabel",
						  len_COMinfo * (long)sizeof(iwRegCOMinfo));
		}

		for (i=0; i<(*nregions); i++) {
			COMinfo[i].pixelcount = 0;
			COMinfo[i].color = -1;
			COMinfo[i].summe_x = 0;
			COMinfo[i].summe_y = 0;
		}

		r = region+xsize;
		for (i=1; i<ysize-1; i++) {
			r++;
			for (k=1; k<xsize-1; k++) {
				*r = r_alias[*r];

				infoptr = &COMinfo[*r];
				if (infoptr->color == -1) {
					infoptr->color = color[i*xsize+k];
				}
				if (infoptr->color>0) {
					(infoptr->pixelcount)++;
					infoptr->summe_x += k;
					infoptr->summe_y += i;
				}
				r++;
			}
			r ++;
		}

		for (i=0; i<(*nregions); i++) {
			if (COMinfo[i].pixelcount >= minPixelCount) {
				COMinfo[i].com_x = COMinfo[i].summe_x / COMinfo[i].pixelcount;
				COMinfo[i].com_y = COMinfo[i].summe_y / COMinfo[i].pixelcount;
			} else {
				COMinfo[i].com_x = 0;
				COMinfo[i].com_y = 0;
				COMinfo[i].pixelcount = 0;
			}
		}
	} else {
		for (i = xsize * ysize, r = region; i > 0; i--, r++)
			*r = r_alias[*r];
	}

	/* ... und Alias-Liste fuer naechsten Durchlauf initialisieren */
	for (i = rcount, r = r_alias; i > 0; i--)
		*r++ = -1;

	/* Label-Bild am Rand mit -1 besetzen, damit nachfolgende Module
	   keine spezielle Randbehandlung durchfuehren muessen */
	for (i=0, r=region; i<xsize; ++i, ++r)
		*r = -1;
	for (i=1; i<ysize; ++i) {
		region[i*xsize] = -1;
		region[i*xsize-1] = -1;
	}
	for (i=0, r=(region+(ysize-1)*xsize); i<xsize; ++i, ++r)
		*r = -1;

	return COMinfo;
}

/*********************************************************************
  Do a region labeling of the image color (size: xsize x ysize) and
  write the result to region.
  Return: Number of regions.
*********************************************************************/
int iw_reg_label (int xsize, int ysize, const uchar *color, gint32 *region)
{
	int nregions;
	region_label_do (xsize, ysize, color, region, &nregions, -1);
	return nregions;
}

/*********************************************************************
  Do a region labeling of the image color (size: xsize x ysize) and
  write the result to region. Calculate pixel count, color, and COM
  of the regions.
  pixel count < minPixelCount -> Pixel count of the region = 0
  Return value is a pointer to a static variable!
*********************************************************************/
iwRegCOMinfo *iw_reg_label_with_calc (int xsize, int ysize, const uchar *color,
									  gint32 *region, int *nregions, int minPixelCount)
{
	return region_label_do (xsize,ysize,color,region,nregions,minPixelCount);
}

/*********************************************************************
  Stretch region reg by scale pixels in all directions and
  restrict the region to a size of width x height.
*********************************************************************/
void iw_reg_stretch (int width, int height, iwRegion *reg, int scale)
{
	Polygon_t *pol;
	float dirx, diry, norm;
	float meanx, meany;
	int i;
	int xnew, ynew;

	meanx = reg->r.schwerpunkt.x;
	meany = reg->r.schwerpunkt.y;

	pol = &(reg->r.polygon);

	for (i=0; i < pol->n_punkte; i++) {
		dirx = pol->punkt[i]->x - meanx;
		diry = pol->punkt[i]->y - meany;
		norm = sqrt ((dirx*dirx) + (diry*diry));
		dirx = dirx / norm;
		diry = diry / norm;

		xnew =  (int)pol->punkt[i]->x + (int)(scale * dirx);
		if (xnew < 0) xnew = 0;
		if (xnew >= width) xnew = width-1;
		/*
		  printf ("dirx: %f diry: %f norm: %f %f->%d \n",
				  dirx, diry, norm, pol->punkt[i]->x, xnew);
		*/
		pol->punkt[i]->x = (float)xnew;

		ynew = (int)pol->punkt[i]->y + (int)(scale * diry);
		if (ynew < 0) ynew = 0;
		if (ynew >= height) ynew = height-1;

		pol->punkt[i]->y = (float)ynew;
	}
}

/*********************************************************************
  Return bounding box of polygon p.
*********************************************************************/
void iw_reg_boundingbox (const Polygon_t *p, int *x1, int *y1, int *x2, int *y2)
{
	int n;

	*x1 = *x2 = p->punkt[0]->x;
	*y1 = *y2 = p->punkt[0]->y;

	for (n=1; n<p->n_punkte-1; n++) {
		if (p->punkt[n]->x < *x1)
			*x1 = p->punkt[n]->x;
		else if (p->punkt[n]->x > *x2)
			*x2 = p->punkt[n]->x;
		if (p->punkt[n]->y < *y1)
			*y1 = p->punkt[n]->y;
		else if (p->punkt[n]->y > *y2)
			*y2 = p->punkt[n]->y;
	}
}

/*********************************************************************
  Upsample region by factor (upw,uph).
*********************************************************************/
static void region_upsample_reg (Region_t *region, float upw, float uph)
{
	int i,e;

	if (region->pixelanzahl > 0) {
		region->schwerpunkt.x *= upw;
		region->schwerpunkt.y *= uph;
		/* ATTENTION: the first and the last point are identical */
		for (i=0; i<region->polygon.n_punkte-1; i++) {
			region->polygon.punkt[i]->x *= upw;
			region->polygon.punkt[i]->y *= uph;
		}
		/* Check if first and last point are different */
		if (region->polygon.punkt[0] != region->polygon.punkt[i]) {
			region->polygon.punkt[i]->x *= upw;
			region->polygon.punkt[i]->y *= uph;
		}

		for (e=0; e<region->n_einschluss; e++) {
			for (i=0; i<region->einschluss[e]->n_punkte-1; i++) {
				region->einschluss[e]->punkt[i]->x *= upw;
				region->einschluss[e]->punkt[i]->y *= uph;
			}
			/* Check if first and last point are different */
			if (region->einschluss[e]->punkt[0] != region->einschluss[e]->punkt[i]) {
				region->einschluss[e]->punkt[i]->x *= upw;
				region->einschluss[e]->punkt[i]->y *= uph;
			}
		}
		region->pixelanzahl = 0.999 + region->pixelanzahl * upw * uph;

		region->umfang *= (upw+uph)/2;		/* Wrong if upw!=uph, but not possible */
	}										/* to reconstruct correctly */
}

/*********************************************************************
  Upsample regions by factor (upw,uph).
*********************************************************************/
void iw_reg_upsample (int nregions, iwRegion *regions, float upw, float uph)
{
	int n;

	if (isequal(upw,1.0) && isequal(uph,1.0)) return;

	for (n=0; n < nregions; n++, regions++)
		region_upsample_reg (&regions->r, upw, uph);
}

/*********************************************************************
  Free regions allocated with iw_reg_copy(). Attention: This works
  only correctly for regions allocated with iw_reg_copy().
*********************************************************************/
void iw_reg_free (int nregions, iwRegion *regions)
{
	int i;

	if (nregions<=0) return;

	free (regions->r.polygon.punkt[0]);
	free (regions->r.polygon.punkt);
	for (i=0; i<nregions; i++) {
		if (regions[i].r.n_einschluss > 0) {
			free (regions[i].r.einschluss[0]);
			free (regions[i].r.einschluss);
			free (regions);
			return;
		}
	}
	free (regions);
	return;
}

/*********************************************************************
  Return a copy of regions. Attention: Single regions can't be freed
  as all points and polygons are malloced en bloc.
*********************************************************************/
iwRegion *iw_reg_copy (int nregions, const iwRegion *regions)
{
	int i, j, k, npunkte, npolygone;
	Punkt_t *punkte, **pktbar, **pnew, **pold;
	Polygon_t *polygone = NULL, **polbar = NULL;
	iwRegion *new;

	if (nregions<=0) return NULL;

	new = iw_malloc0 (nregions*sizeof(iwRegion), "regRegions in iw_reg_copy");
	memcpy (new,regions,nregions*sizeof(iwRegion));

	npunkte = npolygone = 0;
	for (i=0; i<nregions; i++) {
		npunkte += regions[i].r.polygon.n_punkte;
		npolygone += regions[i].r.n_einschluss;
		for (j=0; j<regions[i].r.n_einschluss; j++)
			npunkte += regions[i].r.einschluss[j]->n_punkte;
	}

	punkte = iw_malloc0 (npunkte*sizeof(Punkt_t), "punkte in iw_reg_copy");
	pktbar = iw_malloc0 (npunkte*sizeof(Punkt_t*), "pktbar in iw_reg_copy");
	if (npolygone > 0) {
		polygone = iw_malloc0 (npolygone*sizeof(Polygon_t), "polygone in iw_reg_copy");
		polbar = iw_malloc0 (npolygone*sizeof(Polygon_t*), "polbar in iw_reg_copy");
	}

	npunkte = npolygone = 0;
	for (i=0; i<nregions; i++) {
		pnew = new[i].r.polygon.punkt = pktbar + npunkte;
		npunkte += regions[i].r.polygon.n_punkte;
		pold = regions[i].r.polygon.punkt;
		for (k=0; k<regions[i].r.polygon.n_punkte; k++) {
			pnew[k] = punkte++;
			pnew[k]->x = pold[k]->x;
			pnew[k]->y = pold[k]->y;
		}

		if (regions[i].r.n_einschluss > 0) {
			new[i].r.einschluss = polbar + npolygone;
			npolygone += regions[i].r.n_einschluss;

			for (j=0; j<regions[i].r.n_einschluss; j++) {
				new[i].r.einschluss[j] = polygone++;
				pnew = new[i].r.einschluss[j]->punkt = pktbar + npunkte;
				npunkte += regions[i].r.einschluss[j]->n_punkte;
				pold = regions[i].r.einschluss[j]->punkt;
				for (k=0; k<regions[i].r.einschluss[j]->n_punkte; k++) {
					pnew[k] = punkte++;
					pnew[k]->x = pold[k]->x;
					pnew[k]->y = pold[k]->y;
				}
			}
		}
	}
	return new;
}
