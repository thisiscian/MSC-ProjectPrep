/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/**
  Datei:   calc_region.c
  Vorlage: Andrea Weide
  Autor:   Franz Kummert
  Datum:   10.02.1998

  Zweck: Bestimmung der Kontur von Regionen und Berechnung
		verschiedener Formparameter der Regionen.
**/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "region.h"
#include "tools/tools.h"
#include "plugin_comm.h"

#define MAX_DIST_KONTUR			1.0
#define LEN_PUNKTFELD			750000  /* Mehr Konturpunkte pro Bild kann es nicht geben */
#define MAXLOOP					50000	/* Laengere Konturen sollte es nicht geben */
#define VERZERR_FAKTOR			1.333
#define MAX_REGIONS				3000	/* Max. erlaubte Anzahl von regionen */

struct _regCalcData {
	int minPixelCount;
	iwImage *color;
	uchar **orig_img;
	uchar *confimg;
	iwRegThinning thin_mode;
	float thin_maxdist;
	iwRegMode mode;
};

typedef struct {
	int pixelcount;			/* Anzahl der Pixel der Region */
	int summe_conf;			/* Summe der Pixel im ConfidenceMapped Bild */
	int summe_x;			/* x-Koordinaten zur Schwerpunktberechnung */
	int summe_y;			/* y-Koordinaten zur Schwerpunktberechnung */
	int umfang;				/* Anzahl der Konturpixel der Region */
	int reg_in_line;	    /* Region schon aufgetreten */
	Punkt_t punkt;			/* Anfangspunkt der Region */
	Punkt_t schwerpunkt;	/* Schwerpunkt der Region */
	Polygon_t *polygon;		/* Polygon der Region */
	int color;				/* Erste Farbe der Region */
	int y,u,v;				/* Mittlere Farbe der Region */
	int *merge;				/* Die Regionen, die direkt benachbart sind */
	float m02;				/* Moment 02 der Region */
	float m20;				/* Moment 20 der Region */
	float m11;				/* Moment 11 der Region */
	int finalindex;			/* Endgueltiger Regionenindex */
} REGION_INFO;

typedef struct {
	int aktindex;
	int polindex;
	Polygon_t *polygons;
	Punkt_t *pktarray;
	Punkt_t **pktbar;
} PUNKTFELD;

typedef struct {
	int anzahl;
	int *liste;
} PUNKT_LISTE;

static float calcdist (Punkt_t p1, Punkt_t p2)
{
	return((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y));
}

static float laenge_rand_polygon (Region_t * r)
{
	Polygon_t *p;
	int i;
	float px, py, laenge = 0.0;

	p = &r->polygon;

	for (i = 1; i < p->n_punkte; i++) {
		px = p->punkt[i - 1]->x - p->punkt[i]->x;
		py = p->punkt[i - 1]->y - p->punkt[i]->y;
		laenge += (float) sqrt ((double) px * px + py * py);
	}
	return (laenge);
}

static float radius_polygon (Region_t * r)
{
	float a, b, c, vorzeichen_test, Q, Pa, Pb, Pc,radius,radiusn;
	double D;
	Polygon_t *p;
	Punkt_t S;
	int i = 0, j;

	radius = 0.0;
	/* Gleichung der Hauptachse (ax + by + c = 0) */

	a = (float) -sin ((double) r->hauptachse.winkel);
	b = (float) cos ((double) r->hauptachse.winkel);
	c = -(r->schwerpunkt.x * a + r->schwerpunkt.y * b);

	p = &r->polygon;

	while (i < p->n_punkte - 1){
		vorzeichen_test = p->punkt[i]->x * a + p->punkt[i]->y * b + c;
		if (vorzeichen_test == 0.0) {
			radiusn =  ((float) sqrt ( (double) ((p->punkt[i]->x - r->schwerpunkt.x) *
												 (p->punkt[i]->x - r->schwerpunkt.x) +
												 (p->punkt[i]->y - r->schwerpunkt.y) *
												 (p->punkt[i]->y - r->schwerpunkt.y))));
			if (radiusn > radius) radius = radiusn;
		} else {
			Q = (vorzeichen_test < 0) ? -1.0 : 1.0;
			do {
				i++;
				vorzeichen_test = p->punkt[i]->x * a + p->punkt[i]->y * b + c;
			} while ((p->n_punkte > (i+1)) && vorzeichen_test * Q > 0);

			if (vorzeichen_test * Q > 0)
				continue;

			if (i==p->n_punkte)
				j = 0;
			else
				j = i;
			Pa = p->punkt[j]->y - p->punkt[i - 1]->y;
			Pb = p->punkt[i - 1]->x - p->punkt[j]->x;
			D = a * Pb - b * Pa;
			if (D == 0) continue;	/* Beide Geraden sind parallel */
			Pc = p->punkt[j]->x * p->punkt[i - 1]->y - p->punkt[i - 1]->x *
				p->punkt[j]->y;
			S.x = (b * Pc - c * Pb) / D;
			S.y = (c * Pa - a * Pc) / D;

			radiusn =  ((float) sqrt ((double) ((S.x - r->schwerpunkt.x) *
												(S.x - r->schwerpunkt.x) + (S.y - r->schwerpunkt.y)
												* (S.y - r->schwerpunkt.y))));
			if (radiusn > radius) radius = radiusn;
		}
		i++;
	}
	return(radius);
}

/* Berechnet die senkrechte Projektion des Punktes punkt auf
   die Gerade mit Gleichung ax+by+c=0 */
static Punkt_t projection_point_line (float a, float b, float c, Punkt_t *punkt)
{
	Punkt_t point;

	if (a != 0) point.x = (punkt->y * b * (-1.0) - c ) / a;
	else point.x = punkt->x;
	if (b != 0) point.y = (punkt->x * a * (-1.0) - c ) / b;
	else point.y = punkt->y;

	return(point);
}

static void trage_kontur_ein (gint32 *image, Polygon_t polygon, int label,
							  PUNKT_LISTE *punkte, int xlen)
{
	Punkt_t hpunkt, spunkt;
	float a1, b1, c1;
	float d1, d2, d3;
	float incx, incy;
	int firstx, firsty, secondx, secondy;
	int i;

	for (i = 0; i < (polygon.n_punkte-1); i++) {
		/* Geradengleichung fuer Gerade:
		   PolygonPunkt[i], PolygonPunkt[i+1] */
		firstx = (int) (polygon.punkt[i]->x+0.5);
		firsty = (int) (polygon.punkt[i]->y+0.5);
		secondx = (int) (polygon.punkt[i+1]->x+0.5);
		secondy = (int) (polygon.punkt[i+1]->y+0.5);
		a1 = firsty - secondy;
		b1 = secondx - firstx;
		c1 = (-1.) * firsty * b1 - firstx * a1;

		if (firstx <= secondx) incx = 1.;
		else incx = (-1.);
		if (firsty <= secondy) incy = 1.;
		else incy = (-1.);

		/* Markiere solange bis zweiter Konturpunkt erreicht */
		spunkt.x = firstx;
		spunkt.y = firsty;
		image[((int) (spunkt.y)) * xlen + ((int) (spunkt.x))] = label;
		punkte->liste[punkte->anzahl] =
			((int) (spunkt.y)) * xlen + ((int) (spunkt.x));
		punkte->anzahl += 1;

		while ((int) (spunkt.x+0.5) != secondx ||
			   (int) (spunkt.y+0.5) != secondy) {
			hpunkt.x = spunkt.x;
			hpunkt.y = spunkt.y + incy;
			d1 = calcdist(hpunkt, projection_point_line(a1, b1, c1, &hpunkt));
			hpunkt.x = spunkt.x + incx;
			hpunkt.y = spunkt.y + incy;
			d2 = calcdist(hpunkt, projection_point_line(a1, b1, c1, &hpunkt));
			hpunkt.x = spunkt.x + incx;
			hpunkt.y = spunkt.y;
			d3 = calcdist(hpunkt, projection_point_line(a1, b1, c1, &hpunkt));
			if (d1 < d2 && d1 < d3) spunkt.y += incy;
			else if (d2 < d1 && d2 < d3) {
				spunkt.x += incx;
				spunkt.y += incy;
			}
			else spunkt.x += incx;

			image[((int) (spunkt.y+0.5)) * xlen +
				 ((int) (spunkt.x+0.5))] = label;
			punkte->liste[punkte->anzahl] =
				((int) (spunkt.y+0.5)) * xlen +
				((int) (spunkt.x+0.5));
			punkte->anzahl += 1;
		}
	}
}

static int region_contains_point (Region_t *region, Punkt_t *point)
{
	int i;
	int zaehl = 0;
	float D1;
	float a1, b1, c1;
	float a2, b2, c2;
	float xs, ys;

	if (region->polygon.punkt == NULL)
		return(FALSE);

	/* Berechne Schnittpunkt der Geraden durch die beiden Punkte
	   (-1,-1), (point->x,point->y) mit allen Konturliniengeraden:
	   Falls eine ungerade Anzahl von Schnittpunkten ==>
	     Punkt liegt innerhalb der Region
	   Falls eine gerade Anzahl von Schnittpunkten ==>
	     Punkt liegt ausserhalb der Region */

	/* Geradengleichung fuer Gerade: (-1,-1), (point->x,point->y)
	   aufstellen */
	a1 = ((-1.) - point->y);
	b1 = (point->x - (-1.));
	c1 = b1 + a1;

	for (i = 0; i < region->polygon.n_punkte-1; i++) {
		/* Geradengleichung fuer Gerade:
		   PolygonPunkt[i], PolygonPunkt[i+1] */
		a2 = region->polygon.punkt[i]->y -
			region->polygon.punkt[i+1]->y;
		b2 = region->polygon.punkt[i+1]->x -
			region->polygon.punkt[i]->x;
		c2 = (-1.) * region->polygon.punkt[i]->y * b2 -
			region->polygon.punkt[i]->x * a2;

		/* Schnittpunkt der beiden Geraden berechnen */
		D1 = a1 * b2 - b1 * a2;
		if (D1 == 0) continue; /* beide Geraden sind parallel */
		xs = (b1 * c2 - c1 * b2) / D1;
		ys = (c1 * a2 - a1 * c2) / D1;

		/* Test, ob Schnittpunkt auf Konturelement und auf
		   Geradenstueck (-1,-1),(point->x,point->y) */
		/* ... falls Schnittpunkt genau auf zweitem Konturpunkt
		   --> diesen nicht zweimal zaehlen */
		if (xs == region->polygon.punkt[i+1]->x &&
			ys == region->polygon.punkt[i+1]->y) continue;

		if (!((-1.) < xs && xs <= point->x)) continue;

		if (region->polygon.punkt[i]->x <
			region->polygon.punkt[i+1]->x) {
			if (region->polygon.punkt[i]->x <= xs &&
				xs <= region->polygon.punkt[i+1]->x)
				++zaehl;
		}
		else if (region->polygon.punkt[i+1]->x <
				 region->polygon.punkt[i]->x) {
			if (region->polygon.punkt[i+1]->x <= xs &&
				xs <= region->polygon.punkt[i]->x)
				++zaehl;
		}
		else if (region->polygon.punkt[i]->y <
				 region->polygon.punkt[i+1]->y) {
			if (region->polygon.punkt[i]->y <= ys &&
				ys <= region->polygon.punkt[i+1]->y)
				++zaehl;
		}
		else if (region->polygon.punkt[i+1]->y <
				 region->polygon.punkt[i]->y) {
			if (region->polygon.punkt[i+1]->y <= ys &&
				ys <= region->polygon.punkt[i]->y)
				++zaehl;
		}
	}

	/* Falls gerade Anzahl von Schnittpunkten ==>
	   Punkt liegt ausserhalb */
	if ((zaehl / 2) * 2 == zaehl) return (FALSE);
	else return (TRUE);
}

/*** In Punktsuche werden die Konturen immer Stueckweise durch Geraden
     approximiert. Ist ein Punkt der Kontur weniger weit von der Geraden
     entfernt als eine bestimmte Distanz wird er weggelassen. Ist er weiter
     von der Geraden entfernt, wird Punktsuche zwei Mal neu aufgerufen.
***/
static void punktsuche_dist (int pa, int pe, Polygon_t *oldpolygon,
							 Polygon_t *newpolygon, PUNKTFELD *punktfeld, float maxdist)
{
	int ptest, pw;
	float dw, delta_x, delta_y, a, sinus, cosinus, dist;

	/* Falls erster und letzter Punkt gemeinsam --> Sonderbehandlung,
	   da sich hier keine Gerade definieren laesst */
	if (oldpolygon->punkt[pa]->x == oldpolygon->punkt[pe]->x &&
		oldpolygon->punkt[pa]->y == oldpolygon->punkt[pe]->y) {
		/* Den entferntesten Punkt von diesem Punkt suchen */
		pw = pa + 1;
		dw = 0;
		for (ptest = pa + 1; ptest < pe; ptest++) {
			dist = calcdist(*(oldpolygon->punkt[ptest]),
							*(oldpolygon->punkt[pa]));
			if (dist > dw) {
				pw = ptest;
				dw = dist;
			}
		}
		dw = sqrtf(dw);
	}
	else {
		delta_x = oldpolygon->punkt[pe]->x - oldpolygon->punkt[pa]->x;
		delta_y = oldpolygon->punkt[pa]->y - oldpolygon->punkt[pe]->y;
		a = sqrtf(delta_x * delta_x + delta_y * delta_y);
		sinus = delta_x / a;
		cosinus = delta_y / a;

		/* Entferntesten Punkt zur Geraden zwischen pa und pe suchen */
		pw = pa + 1;
		dw = 0;
		for (ptest = pa + 1; ptest < pe; ptest++) {
			dist = (oldpolygon->punkt[ptest]->x -
					oldpolygon->punkt[pa]->x) * cosinus +
				(oldpolygon->punkt[ptest]->y -
				 oldpolygon->punkt[pa]->y) * sinus;

			if (fabs(dist) > dw) {
				pw = ptest;
				dw = fabs(dist);
			}
		}
	}

	/* Test, kann Punkt in Kontur eingetragen werden */
	if (dw > maxdist) {
		punktsuche_dist (pa, pw, oldpolygon, newpolygon, punktfeld, maxdist);
		punktsuche_dist (pw, pe, oldpolygon, newpolygon, punktfeld, maxdist);
	}
	else {
		/* Neuen Konturpunkt eintragen */
		newpolygon->punkt[newpolygon->n_punkte] = oldpolygon->punkt[pe];
		newpolygon->n_punkte++;
		++(punktfeld->aktindex);
	}
}

static void punktsuche (int pa, int pe, Polygon_t *oldpolygon,
						Polygon_t *newpolygon, PUNKTFELD *punktfeld, iwRegCalcData *data)
{
	int i, dist;

	switch (data->thin_mode) {
		case IW_REG_THIN_OFF:
			for (i=pa+1; i<=pe; i++) {
				newpolygon->punkt[newpolygon->n_punkte] = oldpolygon->punkt[i];
				newpolygon->n_punkte++;
				++(punktfeld->aktindex);
			}
			break;
		case IW_REG_THIN_ABS:
			dist = data->thin_maxdist;
			for (i=pa+1; i<=pe; i++) {
				if (!dist || i % dist == 0) {
					newpolygon->punkt[newpolygon->n_punkte] = oldpolygon->punkt[i];
					newpolygon->n_punkte++;
					++(punktfeld->aktindex);
				}
			}
			if (dist && pe % dist != 0) {
				newpolygon->punkt[newpolygon->n_punkte] = oldpolygon->punkt[pe];
				newpolygon->n_punkte++;
				++(punktfeld->aktindex);
			}
			break;
		case IW_REG_THIN_DIST:
			punktsuche_dist (pa, pe, oldpolygon, newpolygon, punktfeld, data->thin_maxdist);
			break;
	}
}

/* Polygonzug fuer gelabeltes Bild berechnen. Hierbei werden folgende
	Annahmen gemacht:
	- der Label von spunkt gehoert zur Region
	- die Region besteht aus mehr als einem Pixel
	- das Label-Bild `image' ist gross genug, so dass kein Konturpunkt
	  am Rand liegt ==> es kann auf zeitaufwendigen Test, ob Bildrand,
	  verzichtet werden
*/
static Polygon_t *berechne_kontur (gint32 *image, int xlen, int ylen, Punkt_t spunkt,
								   int label, PUNKTFELD *punktfeld)
{
	Polygon_t *polygon;
	Punkt_t *punkt;
	int akt_x, akt_y;
	int richtung;
	int loopcount = 0;

	/* Liste der x- und y- offsets, durch die der als naechstes zu
	   kontrollierende Punkt bestimmt wird */
	static int xoffset[9][2] = {
		{ 0, 0}, { 1, 1}, {-1, 0}, {-1, 0}, {-1,-1},
		{ 1, 1}, { 1, 0}, { 1, 0}, {-1,-1}};
	static int yoffset[9][2] = {
		{ 0, 0}, {-1, 0}, {-1,-1}, { 1, 1}, {-1, 0},
		{ 1, 0}, {-1,-1}, { 1, 1}, { 1, 0}};
	/* Suchrichtungen, die als naechstes verwendet werden,
	   wenn ein Konturpunkt gefunden wurde. */
	static int aendere_richtung[9][2] = {
		{0, 0}, {2, 7}, {8, 1}, {4, 5}, {6, 3},
		{3, 6}, {5, 4}, {1, 8}, {7, 2}};

	/* Speicher fuer neues Polygon und Polygonpunkte allozieren */
	polygon = punktfeld->polygons + punktfeld->polindex;
	++(punktfeld->polindex);

	/* Anfangspunkt in Kontur eintragen und aktuellen Punkt merken */
	polygon->punkt = punktfeld->pktbar + punktfeld->aktindex;
	polygon->n_punkte = 0;
	punkt = punktfeld->pktarray + punktfeld->aktindex;
	++(punktfeld->aktindex);
	punkt->x = spunkt.x;
	punkt->y = spunkt.y;
	polygon->punkt[polygon->n_punkte++] = punkt;
	akt_x = (int) (spunkt.x+0.5);
	akt_y = (int) (spunkt.y+0.5);

	/* Startrichtung ist immer 4 */
	richtung = 4;

	/* Schleife, bis geschlossene Kontur gefunden */
	do {
		/* Kontrolle des naechsten Punktes, der in Frage kommt */

		if (image[(akt_x + xoffset[richtung][0]) +
				 (akt_y + yoffset[richtung][0]) * xlen] == label) {

			/* Punkt wird in Kontur uebernommen */
			akt_x += xoffset[richtung][0];
			akt_y += yoffset[richtung][0];

			punkt = punktfeld->pktarray + punktfeld->aktindex;
			++(punktfeld->aktindex);
			punkt->x = akt_x;
			punkt->y = akt_y;
			polygon->punkt[polygon->n_punkte++] = punkt;

			/* Neue Suchrichtung */
			richtung = aendere_richtung[richtung][0];

			if (akt_x == polygon->punkt[0]->x &&
				akt_y == polygon->punkt[0]->y) {
				/* Test auf   12
							 3 4
				   1 ist Startpunkt und wird von 3 mit Richtung 6 (danach 5) erreicht,
				   ohne dass 2 und 4 in Polygon mit aufgenommen wurden.
				*/
				if (richtung != 5 ||
					(image[akt_x+1 + akt_y * xlen] != label &&
					 image[akt_x+1 + (akt_y + 1) * xlen] != label))
					loopcount = MAXLOOP;
			}
		}
		/* Kontrolle des naechsten Punktes, der in Frage kommt */
		else if (image[(akt_x + xoffset[richtung][1]) +
					  (akt_y + yoffset[richtung][1]) * xlen] == label) {

			/* Punkt wird in Kontur uebernommen */
			akt_x += xoffset[richtung][1];
			akt_y += yoffset[richtung][1];

			punkt = punktfeld->pktarray + punktfeld->aktindex;
			++(punktfeld->aktindex);
			punkt->x = akt_x;
			punkt->y = akt_y;
			polygon->punkt[polygon->n_punkte++] = punkt;

			/* In diesem Fall bleibt die Suchrichtung gleich!! */

			/* Startpunkt erreicht ? */
			if (akt_x == polygon->punkt[0]->x && akt_y == polygon->punkt[0]->y)
				loopcount = MAXLOOP;
		}
		/* Wenn die beiden kontrollierten Punkte keine Konturpunkte sind
		   --> Suchrichtung aendern */
		else richtung = aendere_richtung[richtung][1];

		++loopcount;
		/* Solange Konturpunkte suchen, bis Anfangspunkt erneut gefunden
		   oder im Fehlerfalle, falls loopcount ueberschritten */
	} while (loopcount < MAXLOOP);

	/* Erster und letzter Konturpunkt sind identisch */
	polygon->punkt[polygon->n_punkte-1] = polygon->punkt[0];

	return polygon;
}

/* Polygonzug fuer gelabeltes Bild berechnen. Hierbei werden folgende
	Annahmen gemacht:
	- der Label von spunkt gehoert zur Region
	- die Region besteht aus mehr als einem Pixel
	- das Label-Bild `image' ist gross genug, so dass kein Konturpunkt
	  am Rand liegt ==> es kann auf zeitaufwendigen Test, ob Bildrand,
	  verzichtet werden
*/
static Polygon_t *berechne_einschluss_kontur (gint32 *image, int xlen, int ylen, Punkt_t spunkt,
											  PUNKTFELD *punktfeld, gint32 **einschlfeld)
{
	Polygon_t *polygon;
	Punkt_t *punkt;
	int akt_x, akt_y;
	int richtung;
	int loopcount = 0;

	/* Liste der x- und y- offsets, durch die der als naechstes zu
	   kontrollierende Punkt bestimmt wird */
	static int xoffset[9][2] = {
		{ 0, 0}, { 1, 1}, {-1, 0}, {-1, 0}, {-1,-1},
		{ 1, 1}, { 1, 0}, { 1, 0}, {-1,-1}};
	static int yoffset[9][2] = {
		{ 0, 0}, {-1, 0}, {-1,-1}, { 1, 1}, {-1, 0},
		{ 1, 0}, {-1,-1}, { 1, 1}, { 1, 0}};
	/* Suchrichtungen, die als naechstes verwendet werden,
	   wenn ein Konturpunkt gefunden wurde. */
	static int aendere_richtung[9][2] = {
		{0, 0}, {2, 7}, {8, 1}, {4, 5}, {6, 3},
		{3, 6}, {5, 4}, {1, 8}, {7, 2}};

	/* Speicher fuer neues Polygon und Polygonpunkte allozieren */
	polygon = punktfeld->polygons + punktfeld->polindex;
	++(punktfeld->polindex);

	akt_x = (int) (spunkt.x+0.5);
	akt_y = (int) (spunkt.y+0.5);
	while (image[akt_x + (akt_y-1) * xlen] > 0) {
		einschlfeld[1][image[akt_x + akt_y * xlen]] = 0;
		akt_y--;
	}
	if (einschlfeld[1][image[akt_x + akt_y * xlen]] == 0)
		return NULL;

	/* Anfangspunkt in Kontur eintragen und aktuellen Punkt merken */
	polygon->punkt = punktfeld->pktbar + punktfeld->aktindex;

	polygon->n_punkte = 0;
	punkt = punktfeld->pktarray + punktfeld->aktindex;
	++(punktfeld->aktindex);
	punkt->x = (float)akt_x;
	punkt->y = (float)akt_y;
	polygon->punkt[polygon->n_punkte++] = punkt;

	/* Startrichtung ist immer 4 */
	richtung = 4;

	/* Schleife, bis geschlossene Kontur gefunden */
	do {
		/* Kontrolle des naechsten Punktes, der in Frage kommt */

		if (image[(akt_x + xoffset[richtung][0]) +
				 (akt_y + yoffset[richtung][0]) * xlen] > 0) {
			/* Punkt wird in Kontur uebernommen */
			akt_x += xoffset[richtung][0];
			akt_y += yoffset[richtung][0];

			/* Merken, welche Regionen bearbeitet wurden */
			einschlfeld[1][image[akt_x + akt_y * xlen]] = 0;

			punkt = punktfeld->pktarray + punktfeld->aktindex;
			++(punktfeld->aktindex);
			punkt->x = akt_x;
			punkt->y = akt_y;
			polygon->punkt[polygon->n_punkte++] = punkt;

			/* Neue Suchrichtung */
			richtung = aendere_richtung[richtung][0];

			if (akt_x == polygon->punkt[0]->x &&
				akt_y == polygon->punkt[0]->y) {
				/* Test auf   12
							 3 4
				   1 ist Startpunkt und wird von 3 mit Richtung 6 (danach 5) erreicht,
				   ohne dass 2 und 4 in Polygon mit aufgenommen wurden.
				*/
				if (richtung != 5 ||
					(image[akt_x+1 + akt_y * xlen] == 0 &&
					 image[akt_x+1 + (akt_y + 1) * xlen] == 0))
					loopcount = MAXLOOP;
			}
		}
		/* Kontrolle des naechsten Punktes, der in Frage kommt */
		else if (image[(akt_x + xoffset[richtung][1]) +
					  (akt_y + yoffset[richtung][1]) * xlen] > 0) {
			/* Punkt wird in Kontur uebernommen */
			akt_x += xoffset[richtung][1];
			akt_y += yoffset[richtung][1];

			/* Merken, welche Regionen bearbeitet wurden */
			einschlfeld[1][image[akt_x + akt_y * xlen]] = 0;

			punkt = punktfeld->pktarray + punktfeld->aktindex;
			++(punktfeld->aktindex);
			punkt->x = akt_x;
			punkt->y = akt_y;
			polygon->punkt[polygon->n_punkte++] = punkt;

			/* In diesem Fall bleibt die Suchrichtung gleich!! */

			/* Startpunkt erreicht ? */
			if (akt_x == polygon->punkt[0]->x && akt_y == polygon->punkt[0]->y)
				loopcount = MAXLOOP;
		}
		/* Wenn die beiden kontrollierten Punkte keine Konturpunkte sind
		   --> Suchrichtung aendern */
		else richtung = aendere_richtung[richtung][1];

		++loopcount;
		/* Solange Konturpunkte suchen, bis Anfangspunkt erneut gefunden
		   oder im Fehlerfalle, falls loopcount ueberschritten */
	} while (loopcount < MAXLOOP);
	/* Erster und letzter Konturpunkt sind identisch */
	polygon->punkt[polygon->n_punkte-1] = polygon->punkt[0];

	return polygon;
}

/*********************************************************************
  Calculate regions from the image "image".
  xlen, ylen: Size of image
  image     : Region labeld image
  num_reg   : in  : Number of labeld regions
              out : Number of calculated regions
  data      : Additional settings for the region calculation.
*********************************************************************/
iwRegion *iw_reg_calc_data (int xlen, int ylen, gint32 *image,
							int *num_reg, iwRegCalcData *data)
{
	int i, k, l, m;
	int reg_count;
	int *imagpntr;
	float x;
	Punkt_t spunkt;
	Polygon_t *einschlpolygon, *duennpolygon;
	REGION_INFO *infopntr;
	static int xlen_old = 0, ylen_old = 0;
	static gint32 **einschlfeld = NULL;
	static gint32 *einschlimage = NULL;
	static REGION_INFO *region_info = NULL;
	static Polygon_t *newpolygon = NULL;
	static iwRegion *regionen = NULL;
	static PUNKTFELD *punktfeld = NULL;
	static PUNKT_LISTE punkte = {0, NULL};
	static int len_region_info = 0,		/* Groesse von region_info, ... ==
										   max Anzahl Regionen */
			   len_region_big = 0;		/* Groesse von regionen, ... ==
										   max anz Regionen mit pixelcount > minPixelCount */
	uchar *y_img, *u_img, *v_img;

	if (*num_reg > MAX_REGIONS) {
		iw_debug (0,"Number of labeld regions: %d\n\t\t-> too many for iw_reg_calc",*num_reg);
		*num_reg = 0;
		return NULL;
	}

	/* Speicher fuer die Polygone, die Konturleisten und die
	   Konturpunkte allozieren */
	if (punktfeld == NULL) {
		punktfeld = (PUNKTFELD *)
			iw_malloc0 (sizeof(PUNKTFELD), "punktfeld in iw_reg_calc");

		punktfeld->pktarray =
			(Punkt_t *) iw_malloc0 (LEN_PUNKTFELD * sizeof(Punkt_t),
									"punktfeld->pktarray in iw_reg_calc");

		punktfeld->pktbar =
			(Punkt_t **) iw_malloc0 (LEN_PUNKTFELD * sizeof(Punkt_t *),
									 "punktfeld->pktbar in iw_reg_calc");
	}
	punktfeld->aktindex = 0;
	punktfeld->polindex = 0;

	if (newpolygon == NULL)
		newpolygon = (Polygon_t *) iw_malloc0(sizeof(Polygon_t),"newpolygon in iw_reg_calc");

	/* Statischen Speicher allozieren */
	if (region_info == NULL) {
		/* Speicher fuer die Regionenverwaltung allozieren */
		region_info =
			(REGION_INFO *) iw_malloc0 ((*num_reg+1)*sizeof(REGION_INFO),
										"region_info in iw_reg_calc");
		/* Kleiner Hack, damit Feldindex (-1) moeglich */
		region_info += 1;

		/* Speicher fuer die Nachbarschaft von Regionen allozieren */
		for (i=(-1); i<(*num_reg); ++i) {
			region_info[i].merge =
				(int *) iw_malloc0 ((*num_reg+1) * sizeof(int),
									"region_info[i].merge in iw_reg_calc");

			/* Kleiner Hack, damit Feldindex (-1) moeglich */
			region_info[i].merge += 1;
		}

		/* Speicher fuer die Einschlussfelder allozieren */
		einschlfeld = (gint32 **) iw_malloc0 (2 * sizeof(gint32 *),"einschlfeld in iw_reg_calc");

		einschlfeld[0] = (gint32 *) iw_malloc0 (*num_reg * sizeof(gint32),
												"einschlfeld[0] in iw_reg_calc");

		einschlfeld[1] = (gint32 *) iw_malloc0 (*num_reg * sizeof(gint32),
												"einschlfeld[1] in iw_reg_calc");

		len_region_info = *num_reg;
	}

	if (xlen != xlen_old || ylen != ylen_old) {
		/* Bild fuer Konturberechnung der Einschluesse allozieren */
		einschlimage = (gint32 *) realloc (einschlimage, xlen * ylen * sizeof(gint32));
		if (einschlimage == NULL)
			iw_error ("Cannot alloc %ld Bytes for einschlimage",
					  xlen * ylen * (long)sizeof(gint32));
		memset (einschlimage, 0, xlen * ylen * sizeof(gint32));

		/* Liste der zu loeschenden Konturpunkte allozieren */
		punkte.liste = (int*) realloc (punkte.liste, xlen * ylen * sizeof(int));
		if (!punkte.liste)
			iw_error ("Cannot alloc %d Bytes for punkte.liste", xlen * ylen);

		xlen_old = xlen; ylen_old = ylen;
	}
	punkte.anzahl = 0;

	/* Felder reallozieren, falls mehr Regionen */
	if (*num_reg > len_region_info) {
		region_info--;
		region_info = (REGION_INFO *) realloc (region_info,
											   (*num_reg + 1) * sizeof(REGION_INFO));
		if (region_info == NULL)
			iw_error ("Cannot realloc %ld Bytes for region_info",
					  (*num_reg + 1) * (long)sizeof(REGION_INFO));

		/* Kleiner Hack, damit Feldindex (-1) moeglich */
		region_info += 1;

		for (i=(-1); i<len_region_info; ++i) {
			region_info[i].merge = (int *) realloc (--region_info[i].merge,
													(*num_reg+1) * sizeof(int));
			if (region_info[i].merge == NULL)
				iw_error ("Cannot realloc %ld Bytes for region_info[i].merge",
						  (*num_reg+1) * (long)sizeof(int));

			/* Kleiner Hack, damit Feldindex (-1) moeglich */
			region_info[i].merge += 1;
		}
		for (i=len_region_info; i<(*num_reg); ++i) {
			region_info[i].merge =
				(int *) iw_malloc0 ((*num_reg+1) * sizeof(int),
									"region_info[i].merge in iw_reg_calc");

			/* Kleiner Hack, damit Feldindex (-1) moeglich */
			region_info[i].merge += 1;
		}

		/* Speicher fuer die Einschlussfelder reallozieren */
		einschlfeld[0] = (gint32 *) realloc(einschlfeld[0],
											*num_reg * sizeof(gint32));
		if (einschlfeld[0] == NULL)
			iw_error ("Cannot alloc %ld Bytes for einschlfeld[0]", *num_reg * (long)sizeof(gint32));

		einschlfeld[1] = (gint32 *) realloc(einschlfeld[1],
											*num_reg * sizeof(gint32));
		if (einschlfeld[1] == NULL)
			iw_error ("Cannot alloc %ld Bytes for einschlfeld[1]", *num_reg * (long)sizeof(gint32));

		len_region_info = *num_reg;
	}

	/* Initialisierung der Regioneninfos */
	for (i=-1; i<*num_reg; ++i) {
		region_info[i].pixelcount = 0;
		region_info[i].summe_conf = 0;
		region_info[i].summe_x = 0;
		region_info[i].summe_y = 0;
		region_info[i].reg_in_line = 0;
		region_info[i].umfang = 0;
		region_info[i].m02 = 0.;
		region_info[i].m20 = 0.;
		region_info[i].m11 = 0.;
		region_info[i].y = 0;
		region_info[i].u = 0;
		region_info[i].v = 0;
		for (k=-1; k<*num_reg; ++k) region_info[i].merge[k] = 0;
		region_info[i].finalindex = -1;
	}

	/* Bild zeilenweise ablaufen um die Anfangspunkte fuer
	   die Konturverfolgung einsammeln.
	   Ausserdem wird die Farbe jeder Region vermerkt
	   Zusaetzlich werden die benachbarten Regionen und die Laenge
	   deren gemeinsamer Grenze bestimmt
	   Bem.: fuer die Berechnung der benachbarten Regionen wird hierbei
	   die letzte und die erste Spalte als benachbart betrachtet.
	   Dies ist jedoch unerheblich, da diese Pixel die Farbe
	   undefiniert besitzen */
	reg_count = 0;
	imagpntr = image+xlen+1;	/* Beginne bei Pixel (1,1) */
	if (data->orig_img) {
		y_img = data->orig_img[0]+xlen+1;
		u_img = data->orig_img[1]+xlen+1;
		v_img = data->orig_img[2]+xlen+1;
	} else {
		y_img = NULL;
		u_img = NULL;
		v_img = NULL;
	}
	for (i=1; i<ylen-1; ++i) {
		for (k=1; k<xlen-1; ++k, imagpntr++) {
			/* Geht am schnellsten */
			infopntr = &(region_info[*imagpntr]);
			/* Wenn Region noch nicht aufgetreten, dann ... */
			if (infopntr->reg_in_line == 0) {
				infopntr->reg_in_line = 1;
				if (data->color) {
					switch (data->color->type) {
						case IW_8U:
							infopntr->color = data->color->data[0][i*xlen+k];
							break;
						case IW_16U:
							infopntr->color = ((guint16*)(data->color->data[0]))[i*xlen+k];
							break;
						case IW_32S:
							infopntr->color = ((gint32*)(data->color->data[0]))[i*xlen+k];
							break;
						default:
							iw_error ("Unsupported image type %d", data->color->type);
					}
				} else
					infopntr->color = 1;
				infopntr->punkt.x = k;
				infopntr->punkt.y = i;
			}
			if (!(data->mode & IW_REG_NO_ZERO) || infopntr->color) {
				/* "Hintergrund"-Regionen nur fuer Einschluesse bestimmen */
				if (data->confimg) infopntr->summe_conf += data->confimg[i*xlen+k];
				(infopntr->pixelcount)++;
				if (infopntr->pixelcount == data->minPixelCount)
					reg_count++;
				(infopntr->summe_x) += k;
				(infopntr->summe_y) += i;
				if (y_img) {
					(infopntr->y) += *y_img;
					(infopntr->u) += *u_img;
					(infopntr->v) += *v_img;
				}
				(*(infopntr->merge + *(imagpntr-xlen-1)))++;
				(*(infopntr->merge + *(imagpntr-xlen)))++;
				(*(infopntr->merge + *(imagpntr-xlen+1)))++;
				(*(infopntr->merge + *(imagpntr+1)))++;
			}
			if (y_img) {
				y_img++; u_img++; v_img++;
			}
		}
		imagpntr+=2;
		if (y_img) {
			y_img+=2; u_img+=2; v_img+=2;
		}
	}

	if (reg_count > len_region_big) {
		punktfeld->polygons = (Polygon_t *) realloc (punktfeld->polygons,
													 4 * reg_count * sizeof (Polygon_t));
		if (punktfeld->polygons == NULL)
			iw_error ("Cannot realloc %ld Bytes for punktfeld->polygons",
					  4 * reg_count * (long)sizeof(Polygon_t));

		regionen = (iwRegion *) realloc (regionen,reg_count * sizeof (iwRegion));
		if (regionen == NULL)
			iw_error ("Cannot realloc %ld Bytes for regionen",
					  reg_count * (long)sizeof(iwRegion));

		memset (regionen, 0, reg_count * sizeof (iwRegion));
		for (i=0; i<len_region_big; i++) {
			regionen[i].r.einschluss =
				(Polygon_t **) realloc (regionen[i].r.einschluss,
										sizeof(Polygon_t *) * reg_count);
			if (regionen[i].r.einschluss == NULL)
				iw_error ("Cannot realloc %ld Bytes for regionen[i].r.einschluss",
						  (long)sizeof(Polygon_t *) * reg_count);
		}
		for (i=len_region_big; i<reg_count; ++i)
			regionen[i].r.einschluss =
				(Polygon_t **) iw_malloc0 (sizeof(Polygon_t *) * reg_count,
										   "regionen[i].r.einschluss in iw_reg_calc");

		len_region_big = reg_count;
	}

	/**************************
		Konturverfolgung
	**************************/
	for (i=0; i<(*num_reg); ++i) {
		if (region_info[i].pixelcount < data->minPixelCount) {
			region_info[i].pixelcount = 0;
			continue;
		}
		/* Kontur bestimmen */
		region_info[i].polygon = berechne_kontur(image, xlen, ylen,
												 region_info[i].punkt, i, punktfeld);
		region_info[i].umfang = region_info[i].polygon->n_punkte;
	}


	/* Schwerpunkte der Regionen berechnen */
	for (i=0; i<(*num_reg); ++i) {
		if (region_info[i].pixelcount >= data->minPixelCount) {
			region_info[i].schwerpunkt.x =
				((float) region_info[i].summe_x) /
				((float) region_info[i].pixelcount);
			region_info[i].schwerpunkt.y =
				((float) region_info[i].summe_y) /
				((float) region_info[i].pixelcount);
		} else {
			region_info[i].schwerpunkt.x = 0.;
			region_info[i].schwerpunkt.y = 0.;
		}
	}

	/* Momente berechnen */
	for (i=0, imagpntr = image; i<ylen; ++i) {
		for (k=0; k<xlen; ++k, ++imagpntr) {
			if (*imagpntr < 0) continue;
			infopntr = &(region_info[*imagpntr]);
			infopntr->m02 += (i - infopntr->schwerpunkt.y) *
				(i - infopntr->schwerpunkt.y);
			infopntr->m20 += (k - infopntr->schwerpunkt.x) *
				(k - infopntr->schwerpunkt.x);
			infopntr->m11 += (k - infopntr->schwerpunkt.x) *
				(i - infopntr->schwerpunkt.y);
		}
	}

	/* Kontur ausduennen und in neue Region eintragen */
	for (i=0, reg_count=0; i<(*num_reg); ++i) {
		if (region_info[i].pixelcount >= data->minPixelCount &&
			i == image[((int) (region_info[i].punkt.y+0.5))*xlen +
					  ((int) (region_info[i].punkt.x+0.5))]) {
			newpolygon->punkt = punktfeld->pktbar + punktfeld->aktindex;

			/* Ersten Punkt eintragen */
			newpolygon->punkt[0] = region_info[i].polygon->punkt[0];
			newpolygon->n_punkte = 1;
			++(punktfeld->aktindex);
			punktsuche (0, region_info[i].polygon->n_punkte-1,
						region_info[i].polygon, newpolygon, punktfeld, data);
			regionen[reg_count].r.n_match = 0;
			regionen[reg_count].r.match = NULL;
			regionen[reg_count].r.polygon.n_punkte = newpolygon->n_punkte;
			regionen[reg_count].r.polygon.punkt = newpolygon->punkt;
			regionen[reg_count].r.umfang = region_info[i].umfang;
			regionen[reg_count].r.pixelanzahl = region_info[i].pixelcount;
			regionen[reg_count].r.farbe = region_info[i].color;
			regionen[reg_count].r.farbe2 = 0;
			regionen[reg_count].r.schwerpunkt.x = region_info[i].schwerpunkt.x;
			regionen[reg_count].r.schwerpunkt.y = region_info[i].schwerpunkt.y;

			/* Momente gemaess Kameraneigung korrigieren */
			region_info[i].m02 = region_info[i].m02 * VERZERR_FAKTOR * VERZERR_FAKTOR;
			region_info[i].m11 = region_info[i].m11 * VERZERR_FAKTOR;
			if ((region_info[i].m20 - region_info[i].m02) == 0)
				regionen[reg_count].r.hauptachse.winkel = 0.0;
			else
				regionen[reg_count].r.hauptachse.winkel =
					(float) (0.5 * atan2 ((double) (2 * region_info[i].m11),
										  (double) (region_info[i].m20 - region_info[i].m02)));
			regionen[reg_count].r.hauptachse.radius = radius_polygon (&(regionen[reg_count].r));

			/* Exzentrizitaet */
			regionen[reg_count].r.exzentrizitaet =
				((region_info[i].m20 -
				  region_info[i].m02) * (region_info[i].m20 -
										 region_info[i].m02) + 4 * region_info[i].m11 *
				 region_info[i].m11) / ((region_info[i].m20 +
										 region_info[i].m02) * (region_info[i].m20 +
																region_info[i].m02));

			/* Compactness */
			x = laenge_rand_polygon (&(regionen[reg_count].r));
			regionen[reg_count].r.compactness = 16.0 *
				(float) region_info[i].pixelcount / (x * x);

			/* Mittlerer ConfidenceWert */
			if (data->confimg)
				regionen[reg_count].avgConf =
					region_info[i].summe_conf / region_info[i].pixelcount;
			else
				regionen[reg_count].avgConf = 127;
			regionen[reg_count].judgement = 0.5;
			regionen[reg_count].judge_kalman = 0;
			regionen[reg_count].judge_motion = 0;
			regionen[reg_count].labindex = i;
			regionen[reg_count].id = -1;
			regionen[reg_count].alter = 0;

			/* Endgueltigen Regionenindex merken */
			region_info[i].finalindex = reg_count;

			/* Einschluesse initialisieren */
			regionen[reg_count].r.n_einschluss = 0;

			/* Echtfarbe auf undefiniert setzen */
			regionen[reg_count].r.echtfarbe.modell = FARB_MODELL_YUV;
			if (data->orig_img) {
				regionen[reg_count].r.echtfarbe.x = region_info[i].y/region_info[i].pixelcount;
				regionen[reg_count].r.echtfarbe.y = region_info[i].u/region_info[i].pixelcount;
				regionen[reg_count].r.echtfarbe.z = region_info[i].v/region_info[i].pixelcount;
			} else {
				regionen[reg_count].r.echtfarbe.x = 255;
				regionen[reg_count].r.echtfarbe.y = 127;
				regionen[reg_count].r.echtfarbe.z = 127;
			}
			++reg_count;
		}
	}

	/* Regioneneinschluesse berechnen */
	for (i=0; (data->mode & IW_REG_INCLUSION) && i<(*num_reg); i++) {
		if (region_info[i].finalindex < 0) continue;

		/* Einschlussfelder initialisieren */
		for (k = 0; k < (*num_reg); ++k) {
			einschlfeld[0][k] = 0; /* k-te Region ist potent. Einschluss */
			einschlfeld[1][k] = 0; /* k-te Region ist eigenstaendiger
									  Einschluss */
		}

		for (k=0; k < (*num_reg); ++k) {
			if (i == k  || region_info[k].finalindex < 0) continue;

			/* Test, ob Regionen benachbart */
			if (region_info[i].merge[k] > 0 ||
				region_info[k].merge[i] > 0) {
				/* Test, ob k-te Region in der i-ten enthalten */
				if (region_contains_point (&(regionen[region_info[i].finalindex].r),
										   regionen[region_info[k].finalindex
										   ].r.polygon.punkt[0])) {
					trage_kontur_ein (einschlimage, regionen[region_info[k].finalindex].r.polygon,
									  k, &punkte, xlen);
					einschlfeld[0][k] = 1;
					einschlfeld[1][k] = 1;
				}
			}
		}

		l = 0;
		/* for-Schleife geht gut, da die Regionen von links oben nach
		   rechts unten geordnet sind, d.h. der oberste Konturpunkt
		   der k-ten Region gehoert mit Sicherheit auch zur Kontur,
		   falls noch weitere Einschluss-Regionen direkt benachbart sind */
		for (k = 0; k < (*num_reg); ++k) {
			if (einschlfeld[0][k] > 0 && einschlfeld[1][k] > 0) {
				/* Bestimme obersten Punkt des Polygons ==
				   erster Punkt des Polygons */
				spunkt.x = regionen[region_info[k].finalindex].r.polygon.punkt[0]->x;
				spunkt.y = regionen[region_info[k].finalindex].r.polygon.punkt[0]->y;

				/* Hierbei wird bestimmt, welche anderen Polygone
				   mit verwendet werden,
				   d.h. einschlfeld[1][k] = 0 */
				einschlpolygon =
					berechne_einschluss_kontur(einschlimage, xlen, ylen, spunkt,
											   punktfeld, einschlfeld);
				if (einschlpolygon) {
					/* Kontur ausduennen -> neues Polygon allozieren */
					duennpolygon = punktfeld->polygons + punktfeld->polindex;
					++(punktfeld->polindex);
					duennpolygon->punkt = punktfeld->pktbar + punktfeld->aktindex;
					++(punktfeld->aktindex);
					duennpolygon->punkt[0] = einschlpolygon->punkt[0];
					duennpolygon->n_punkte = 1;

				 /* {
						static prevBuffer *b_reg = NULL, *b_img = NULL;
						iwRegion rr;
						if (!b_reg) {
							b_reg = prev_new_window ("einschlpolygon",xlen,ylen,FALSE,TRUE);
							b_img = prev_new_window ("einschlimage",xlen,ylen,TRUE,TRUE);
						}

						memset (&rr,0,sizeof(iwRegion));
						rr.r.echtfarbe.modell = FARB_MODELL_UNDEF;
						rr.r.farbe = rot;
						rr.r.polygon = *einschlpolygon;
						rr.r.pixelanzahl = 100;
						rr.r.n_einschluss = 0;
						rr.r.schwerpunkt.x = einschlpolygon->punkt[0]->x;
						rr.r.schwerpunkt.y = einschlpolygon->punkt[0]->y;
						prev_render_regions (b_reg, &rr, 1, RENDER_CLEAR, xlen, ylen);
						prev_draw_buffer (b_reg);
						prev_render_int (b_img, einschlimage, xlen, ylen, 60);
						prev_draw_buffer (b_img);
						gdk_threads_enter ();
						gdk_flush();
						gdk_threads_leave ();
						printf ("npunkte: %d\n", einschlpolygon->n_punkte);
					} */

					punktsuche (0, einschlpolygon->n_punkte-1,
								einschlpolygon, duennpolygon, punktfeld, data);

					/* Ausgeduennte Kontur als Einschluss eintragen */
					regionen[region_info[i].finalindex].r.einschluss[l++] =
						duennpolygon;
				}
			}
		}
		regionen[region_info[i].finalindex].r.n_einschluss = l;

		/* Bild auf `0' zuruecksetzen */
		for (m=0; m<punkte.anzahl; ++m) einschlimage[punkte.liste[m]] = 0;
	}

	{
		static plugDataFunc *func = (plugDataFunc*)1;

		if (func == (plugDataFunc*)1)
			func = plug_function_get (IW_REG_DATA_IDENT, NULL);
		if (func) {
			for (i=0; i<reg_count; i++)
				((iwRegDataFunc)func->func) (func->plug, &(regionen[i]),
											 IW_REG_DATA_SET);
		}
	}
	*num_reg = reg_count;
	return regionen;
}
iwRegion *iw_reg_calc (int xlen, int ylen, uchar *color,
						gint32 *image, uchar **orig_img, uchar *confimg,
						int *num_reg, int doEinschluss, int minPixelCount)
{
	iwRegCalcData *data = iw_reg_data_create();
	iwImage cimg;
	uchar *planeptr[1];
	iwRegion *regs;

	iw_img_init (&cimg);
	cimg.data = planeptr;
	cimg.data[0] = color;
	cimg.width = xlen;
	cimg.height = ylen;
	cimg.planes = 1;

	iw_reg_data_set_minregion (data, minPixelCount);
	iw_reg_data_set_images (data, &cimg, orig_img, confimg);
	if (doEinschluss)
		iw_reg_data_set_mode (data, IW_REG_INCLUSION);
	else
		iw_reg_data_set_mode (data, IW_REG_NO_ZERO);
	regs = iw_reg_calc_data (xlen, ylen, image, num_reg, data);

	iw_reg_data_free (data);

	return regs;
}
iwRegion *iw_reg_calc_img (int xlen, int ylen, iwImage *color,
						   gint32 *image, uchar **orig_img, uchar *confimg,
						   int *num_reg, iwRegMode mode, int minPixelCount)
{
	iwRegCalcData *data = iw_reg_data_create();
	iwRegion *regs;

	iw_reg_data_set_minregion (data, minPixelCount);
	iw_reg_data_set_images (data, color, orig_img, confimg);
	iw_reg_data_set_mode (data, mode);
	regs = iw_reg_calc_data (xlen, ylen, image, num_reg, data);

	iw_reg_data_free (data);
	return regs;
}

/*********************************************************************
  Maintain the struct holding settings for the region calculation.
  The different settings are:
  color   : Classified image (supported: 1 plane, IW_8U, IW_16U, and
            IW_32S), used to get a color index
  orig_img: Original color image, used to get the average region color
  confimg : Confidence mapped image from the color classifier.
			!= NULL: regionen->avgConf = average confidence value of
                                         the region
  minPixelCount: Minimal size of calculated regions
  iwRegMode|iwRegThinning: See the flags above.
  maxdist : Distance value for the modes IW_REG_THIN_ABS and
            IW_REG_THIN_DIST.
*********************************************************************/
iwRegCalcData *iw_reg_data_create (void)
{
	iwRegCalcData *data = calloc (sizeof(iwRegCalcData), 1);
	data->thin_mode = IW_REG_THIN_DIST;
	data->thin_maxdist = MAX_DIST_KONTUR;
	return data;
}

void iw_reg_data_free (iwRegCalcData *data)
{
	if (data) free (data);
}

void iw_reg_data_set_minregion (iwRegCalcData *data, int minPixelCount)
{
	data->minPixelCount = minPixelCount;
}

void iw_reg_data_set_images (iwRegCalcData *data,
						  iwImage *color, uchar **orig_img, uchar *confimg)
{
	data->color = color;
	data->orig_img = orig_img;
	data->confimg = confimg;
}

void iw_reg_data_set_mode (iwRegCalcData *data, iwRegMode mode)
{
	data->mode = mode;
}

void iw_reg_data_set_thinning (iwRegCalcData *data, iwRegThinning mode, float maxdist)
{
	data->thin_mode = mode;
	data->thin_maxdist = maxdist;
}
