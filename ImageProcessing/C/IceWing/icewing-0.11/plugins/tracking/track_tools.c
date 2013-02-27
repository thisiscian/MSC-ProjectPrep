/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Markus Wienecke, Joachim Schmidt, Jannik Fritsch, and Frank Loemker
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

/*********************************************************************
  This file contains functions used in
  handtrack.c, track_simple.c and track_kalman.c
*********************************************************************/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "track_tools.h"
#include "main/grab.h"

		/* Max length of trajectory (older points are removed) */
#define MAXTRACKLEN			500

/* #[ Math tools : */

/*********************************************************************
  Euclidian norm.
*********************************************************************/
static float _dist (Punkt_t p1, Punkt_t p2)
{
	return sqrt((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y));
}

/*********************************************************************
  Return a (ax+by+c = 0) of the best fitting line for the next
  cnt points from p and norm it such that b=1.
  If the points are all identical (-> no line can be calculated)
  FALSE is returned.

  http://www.coe.uncc.edu/~shbui/btry053/lsq_minimax/lsq_minimax.htm
  http://www.cs.ut.ee/~toomas_l/linalg/lin2/node14.html#SECTION00013300000000000000
  http://www.stanford.edu/class/cs205/notes/book/node18.html

  A = (Xi-x,Yi-y);
  [U,S,V]=svd(A,0);
  #  eigenvalue : det (A - lamda I) = 0                      T
  #  S: singular values = Singulaerwerte = sqrt(eigenvalues(A A)
  #                                                  T
  #  V: singular vectors of (A) = eigenvectors of  (A A)
  if (S(1,1) > S(2,2)) {
	  a=V(1,1);
	  b=V(2,1);
  } else {
	  a=V(1,2);
	  b=V(2,2);
  };

  for i=-4:1:4 {
	  X(i+5)=Xb+i*a;
	  Y(i+5)=Yb+i*b;
  }
*********************************************************************/
static BOOL singularVec (Punkt_t **p, int cnt, float *a, float *b)
{
	int i;
	float x=0, y=0, xys=0, xxs=0, yys=0;
	float root, ev1, ev2;

	/*               T    (xxs xys)               _       _
	  Calculate A'= A A = (xys yys) with A = (x - x , y - y)
	*/
	for (i=0; i<cnt; i++) {
		x += p[i]->x;
		y += p[i]->y;
	}
	x = x/cnt; y = y/cnt;

	for (i=0; i<cnt; i++) {
		xys += (p[i]->x-x)*(p[i]->y-y);
		xxs += (p[i]->x-x)*(p[i]->x-x);
		yys += (p[i]->y-y)*(p[i]->y-y);
	}
	if (!xys) return FALSE;

	/* Calculate eigenvalues of A' */
	root = sqrtf((xxs+yys)*(xxs+yys)/4+xys*xys-xxs*yys);
	ev1 = (xxs+yys)/2 + root;
	ev2 = (xxs+yys)/2 - root;

	/* Calculate eigenvector of the bigger eigenvalue
	   (its one singular vector for A, singular value of A = sqrt(eigenvalue( A' )))
	*/
	if (ev1 > ev2)
		x = (ev1-yys) / xys; /* eigenvector = (x,1) */
	else
		x = (ev2-yys) / xys; /* eigenvector = (x,1) */

	/* Scale the singular vector
	   - first to 1 (dividing by sqrt(x*x+1*1) and
	   - second to the length of the part */
	root = sqrtf((p[cnt-1]->x - p[0]->x)*(p[cnt-1]->x - p[0]->x) +
				 (p[cnt-1]->y - p[0]->y)*(p[cnt-1]->y - p[0]->y)) /
		sqrtf(x*x+1);

	*a = x*root / (cnt-1);
	*b = root / (cnt-1);
	return TRUE;
}

/*********************************************************************
  Berechnung der neuen Massenschwerpunkte, wenn zwei Tracker eine
  gemeinsame Region tracken, die eigentlich 2 verschmolzene Haende darstellt
*********************************************************************/
static int newCenterOfMass (Punkt_t *center1, Punkt_t *center2, Region_t *reg)
{
	int i, count1, count2;
	float b, n1, n2, m1, m2, x, y;
	Punkt_t new1, new2;

	m1 = 0.5 * (center1->x + center2->x);
	m2 = 0.5 * (center1->y + center2->y);

	if ((center1->x == center2->x) && (center1->y == center2->y)) {
		n1 = 1.0;
		n2 = 0.0;
		b = reg->schwerpunkt.x;
	} else  {
		n1 = center2->x - center1->x;
		n2 = center2->y - center1->y;
		b = n1*m1 + n2*m2;
	}

	count1 = 0;
	count2 = 0;
	new1.x = 0.0; new1.y = 0.0;
	new2.x = 0.0; new2.y = 0.0;

	for (i=0; i<reg->polygon.n_punkte; i++) {
		x = reg->polygon.punkt[i]->x;
		y = reg->polygon.punkt[i]->y;
		if ((n1*x + n2*y) < b) {
			new1.x += x;
			new1.y += y;
			count1++;
		} else {
			new2.x += x;
			new2.y += y;
			count2++;
		}
	}

	if (count1 == 0 || count2 == 0) {
		iw_debug (1, "can not calculate NewCenterofMass");
		return 0;
	} else {
		center1->x = new1.x / count1;
		center1->y = new1.y / count1;
		center2->x = new2.x / count2;
		center2->y = new2.y / count2;
		return 1;
	}
}

/* #]  : */

/* #[ Tracker : */

/*********************************************************************
  Init region.
*********************************************************************/
static void regRegion_init (iwRegion *reg)
{
	/* Set things which are != 0 */
	reg->id = -1;
	reg->r.pixelanzahl = 1;		/* To mark that the region is not deleted */
	reg->r.echtfarbe.modell = FARB_MODELL_UNDEF;
	reg->r.echtfarbe.x = reg->r.echtfarbe.y = reg->r.echtfarbe.z = 0;
}

/*********************************************************************
  Alloc and init tracker_t type.
*********************************************************************/
tracker_t* tracker_create (void)
{
	/* Alloc tracker_t struct */
	tracker_t* trk = (tracker_t*) calloc (1, sizeof(tracker_t));

	/* Alloc internal structs */
	trk->traject = (tracker_traject_t*)  calloc (1, sizeof(tracker_traject_t));
	trk->work    = (tracker_work_t*)     calloc (1, sizeof(tracker_work_t));
	trk->pos     = (tracker_position_t*) calloc (1, sizeof(tracker_position_t));
	trk->judge   = (tracker_judge_t*)    calloc (1, sizeof(tracker_judge_t));

	/* Init trajectories */
	regRegion_init (&trk->traject->tracker);
	regRegion_init (&trk->traject->predict);
	regRegion_init (&trk->traject->correct);

	trk->traject->tracker.r.farbe = rot;
	trk->traject->tracker.r.farbe2 = rot;
	trk->traject->predict.r.farbe = blau;
	trk->traject->predict.r.farbe2 = blau;
	trk->traject->correct.r.farbe = gruen;
	trk->traject->correct.r.farbe2 = gruen;

	/* Init work data */
	trk->work->region_zugeordnet = FALSE;
	trk->work->virtuell = FALSE;
	trk->work->best_match_region = -1;
	trk->work->num_cycles = 0;
	trk->work->remainder = 0.0;

	/* Init judge data */
	trk->judge->start_zeit = 0;
	trk->judge->init_zeit = 0;
	trk->judge->init_pos.x = 0;
	trk->judge->init_pos.y = 0;
	trk->judge->max_dist = 0.0;
	trk->judge->judgement = 0.0;

	/* Init pos data */
	trk->pos->tracker.x = 0;
	trk->pos->tracker.y = 0;
	trk->pos->tracker_2.x = 0;
	trk->pos->tracker_2.y = 0;
	trk->pos->previous.x = 0;
	trk->pos->previous.y = 0;
	trk->pos->correct.x = 0;
	trk->pos->correct.y = 0;
	trk->pos->predict.x = 0;
	trk->pos->predict.y = 0;
	trk->pos->region_hypo.x = 0;
	trk->pos->region_hypo.y = 0;

	trk->status = Prepare;
	trk->special = NULL;

	return(trk);
}

/*********************************************************************
  Remove tracker from HandTrack struct.
*********************************************************************/
void tracker_remove (tracker_t *trk)
{
	trk->status = Prepare;
	trk->work->region_zugeordnet = FALSE;
	trk->work->best_match_region = -1;
	trk->work->remainder = 0.0;
	trk->work->virtuell = FALSE;
	trk->judge->judgement = 0.0;
	trk->judge->motion = 0;
	trk->judge->init_zeit = 0;
	trk->judge->max_dist = 0.0;
	tracker_traject_free(trk);
	trk->traject->correct.r.farbe = gruen;
	trk->traject->correct.r.farbe2 = gruen;
}

/*********************************************************************
  Clear trajectory and points.
*********************************************************************/
static void region_clear (Region_t *reg)
{
	int i;

	if (reg->polygon.n_punkte > 0) {
		for (i=0; i<reg->polygon.n_punkte; i++)
			free (reg->polygon.punkt[i]);
		free (reg->polygon.punkt);

		reg->polygon.n_punkte = 0;
	}
}

/*********************************************************************
  Free tracker_t type.
*********************************************************************/
void tracker_free (tracker_t *trk)
{
	/* Free trajectories */
	region_clear (&trk->traject->tracker.r);
	region_clear (&trk->traject->predict.r);
	region_clear (&trk->traject->correct.r);

	/* Free other data */
	free (trk->traject);
	free (trk->work);
	free (trk->pos);
	free (trk->judge);

	/* Free special data stored in struct, should
	   be done before as tracking type is no longer */
	if (trk->special)
		free (trk->special);

	free (trk);
}

/*********************************************************************
  Only free trajectories, reset colors and predict point.
*********************************************************************/
void tracker_traject_free (tracker_t *trk)
{
	region_clear (&trk->traject->tracker.r);
	region_clear (&trk->traject->correct.r);
	region_clear (&trk->traject->predict.r);
	trk->traject->tracker.r.farbe = rot;
	trk->traject->tracker.r.farbe2 = rot;
	trk->traject->predict.r.farbe = blau;
	trk->traject->predict.r.farbe2 = blau;
	trk->traject->correct.r.farbe = gruen;
	trk->traject->correct.r.farbe2 = gruen;
	trk->pos->predict.x = 0;
	trk->pos->predict.y = 0;
}

/*********************************************************************
  Ermittlung aus der vergangenen Zeit zwischen 2 Messpunkten,
  wie oft dt_min (=33, 40, 66ms) vergangen ist, vermeidet Rundungsfehler
  ht->remainder speichert uebrige Nachkommastellen
*********************************************************************/
void tracker_getNumCycles (tracker_t *trk, track_global_t *global)
{
	float cnt = trk->work->remainder + (float)global->dt / global->dt_min;
	trk->work->num_cycles = rintf (cnt);
	trk->work->remainder = cnt - trk->work->num_cycles;
}

/*********************************************************************
  Clear all tracker
*********************************************************************/
void tracker_clear_all (tracker_t **trk)
{
	int i;
	for (i=0; i<MAXKALMAN; i++)
		tracker_remove(trk[i]);
	iw_debug (4, "Deleted all tracker");
}

/* #]  : */

/* #[ Handtrack : */

/*********************************************************************
  Initialisie tracker.
*********************************************************************/
void tracker_all_init (tracker_t **trk, iwRegion *regions,
					   int num_regions, opt_values_t *opt_values,
					   track_global_t *global)
{
	int i,j;
	BOOL tracker_zugeordnet, region_zugeordnet;

	/* Initialisiere tracker, beachte dabei:
	   - Region darf nicht einem anderen Filter zugeordnet sein
	   - Bewertung der Region muss ueber opt_values->region_judgement/init_bewertung liegen */

	for (i=0; i<num_regions; i++) {
		/* Soll diese Region getrackt werden? */
		if (regions[i].r.pixelanzahl > 0 &&
			regions[i].judgement >= opt_values->region_judgement) {

			/* Wird diese Region bereits getrackt? */
			region_zugeordnet = FALSE;
			for (j=0; j<MAXKALMAN; j++) {
				if (i == trk[j]->work->best_match_region) {
					region_zugeordnet = TRUE;
					break;
				}
			}

			/* Region ist noch nicht getrackt */
			if (!region_zugeordnet) {
				tracker_zugeordnet = FALSE;

				/* Suche "freien" Tracker */
				for (j=0; j<MAXKALMAN; j++) {
					if (! trk[j]->work->region_zugeordnet) {
						iw_debug (2,"Tracker %d is assigned to region %d", j, i);
						tracker_zugeordnet = TRUE;
						trk[j]->work->region_zugeordnet = TRUE;
						trk[j]->work->best_match_region = i;
						trk[j]->status = Prepare;
						trk[j]->pos->region_hypo.x = regions[i].r.schwerpunkt.x;
						trk[j]->pos->region_hypo.y = regions[i].r.schwerpunkt.y;
						trk[j]->pos->previous.x = trk[j]->pos->region_hypo.x;
						trk[j]->pos->previous.y = trk[j]->pos->region_hypo.y;
						trk[j]->pos->correct.x = trk[j]->pos->region_hypo.x;
						trk[j]->pos->correct.y = trk[j]->pos->region_hypo.y;
						trk[j]->judge->init_zeit = global->cur_time;
						trk[j]->judge->init_pos.x = trk[j]->pos->region_hypo.x;
						trk[j]->judge->init_pos.y = trk[j]->pos->region_hypo.y;
						trk[j]->judge->max_dist = 0.0;
						break;
					}
				}
				if (!tracker_zugeordnet)
					iw_warning ("No free tracker for a region");
			}
		}
	}
}

/*********************************************************************
  Zuordnung der Regionen zu den Trackern.
*********************************************************************/
void tracker_all_mapRegions (tracker_t **trk, iwRegion *regions,
							 int num_regions, opt_values_t *opt_values,
							 track_global_t *global)
{
	Punkt_t center1, center2;
	float dist;
	int j, k;

	for (j=0; j<MAXKALMAN; j++)
		trk[j]->work->best_match_region = -1;

	/* Zuordnung der Regionen zu den Trackern. Falls einem Tracker noch keine
	   Region zugeordnet ist, wird er mit einer Region im gueltigen Bereich
	   initialisiert */

	tracker_all_findBestMatchRegion (trk, regions, num_regions, opt_values);
	tracker_all_init (trk, regions, num_regions, opt_values, global);

	for (j=0; j<MAXKALMAN; j++) {
		if (trk[j]->work->region_zugeordnet) {

			/* Trage Parameter in die Trajektorien ein */
			dist = _dist (trk[j]->pos->previous, trk[j]->judge->init_pos);
			if (dist > trk[j]->judge->max_dist) trk[j]->judge->max_dist = dist;

			trk[j]->traject->correct.r.umfang =
				trk[j]->traject->tracker.r.umfang =
				trk[j]->traject->predict.r.umfang =
				global->cur_time - trk[j]->judge->init_zeit;

			trk[j]->traject->correct.r.compactness =
				trk[j]->traject->tracker.r.compactness =
				trk[j]->traject->predict.r.compactness =
				trk[j]->judge->max_dist;

			trk[j]->traject->correct.id =
				trk[j]->traject->tracker.id =
				trk[j]->traject->predict.id =
				regions[trk[j]->work->best_match_region].id =
				j;

			regions[trk[j]->work->best_match_region].alter =
				trk[j]->traject->correct.alter =
				trk[j]->traject->tracker.alter =
				trk[j]->traject->predict.alter =
				rintf((float)(global->cur_time - trk[j]->judge->init_zeit) / global->dt_min);

			for (k=0; k<MAXKALMAN; k++) {

				/* Falls 2 Tracker auf die gleiche Region zeigen, und
				   einer davon nur eine sehr kurze Zeit existiert bzw. einen
				   sehr kurzen Weg zurueckgelegt hat, wird davon ausgegangen, dass
				   die Region zuvor zerfallen ist, und ein Tracker wird geloescht. */

				if (trk[k]->work->region_zugeordnet &&
					k != j &&
					trk[j]->work->best_match_region == trk[k]->work->best_match_region) {

					/* Loesche tracker k, falls nicht gueltig */
					if (trk[k]->judge->judgement <= opt_values->rating) {

						iw_debug (3, "Overlapping: Deleted tracker %d", k);
						tracker_remove (trk[k]);

					} else if (opt_values->delete_merged) {

						/* Loesche aelteren tracker, falls in GUI eingestellt */
						if (trk[k]->judge->init_zeit < trk[j]->judge->init_zeit) {
							iw_debug (3, "Overlapping: Deleting older kalman %d", k);
							tracker_remove (trk[k]);
						} else if (trk[j]->judge->init_zeit < trk[k]->judge->init_zeit) {
							iw_debug (3, "Overlapping: Deleteing older tracker %d", j);
							tracker_remove (trk[j]);
						}
					}

					/* Berechne Schwerpunkte neu, falls beide Tracker weiterhin aktiv */
					if (trk[k]->work->region_zugeordnet) {
						iw_debug (3, "Both trackers %d, %d still active", j, k);
						trk[k]->work->virtuell = TRUE;
						trk[j]->work->virtuell = TRUE;
						trk[k]->pos->tracker_2.x = regions[trk[k]->work->best_match_region].r.schwerpunkt.x;
						trk[k]->pos->tracker_2.y = regions[trk[k]->work->best_match_region].r.schwerpunkt.y;
						trk[j]->pos->tracker_2.x = regions[trk[j]->work->best_match_region].r.schwerpunkt.x;
						trk[j]->pos->tracker_2.y = regions[trk[j]->work->best_match_region].r.schwerpunkt.y;

						center1.x = trk[k]->pos->previous.x;
						center1.y = trk[k]->pos->previous.y;
						center2.x = trk[j]->pos->previous.x;
						center2.y = trk[j]->pos->previous.y;

						if (newCenterOfMass (&center1, &center2,
											 &regions[trk[k]->work->best_match_region].r)) {
							int s1 = j, s2 = k;

							if ( ( center1.x < center2.x &&
								   trk[k]->pos->previous.x < trk[j]->pos->previous.x ) ||
								 ( center1.x > center2.x &&
								   trk[k]->pos->previous.x > trk[j]->pos->previous.x ) ) {
								s1 = k;
								s2 = j;
							}

							trk[s1]->pos->region_hypo.x = center1.x;
							trk[s1]->pos->region_hypo.y = center1.y;
							trk[s2]->pos->region_hypo.x = center2.x;
							trk[s2]->pos->region_hypo.y = center2.y;
						} /* newCenterOfMass */
					} /* trk[k]->work->region_zugeordnet */

				} else { /* Wenn 2 Tracker nicht auf dieselbe Region zeigen */

					trk[k]->work->virtuell = FALSE;
					trk[j]->work->virtuell = FALSE;

				}
			} /* for (k=0; k<MAXKALMAN; k++) */
		} /* if (trk[j]->work->region_zugeordnet) */
	} /* for (j=0; j<MAXKALMAN; j++) */
}

/*********************************************************************
  Suche die naechstliegende Region.
*********************************************************************/
void tracker_all_findBestMatchRegion (tracker_t **trk, iwRegion *regions,
									  int num_regions, opt_values_t *opt_values)
{
	int i,j;
	float min_dist, dist;
	float max_dist = (float)opt_values->max_tol_dist; /* Maximal tolerierte Entfernung */

	for (j=0; j<MAXKALMAN; j++) {

		/* Ist der tracker aktiv? */
		if (trk[j]->work->region_zugeordnet) {

			/* Suche die naheliegendste Region */
			min_dist = 10000.0;
			for (i=0; i<num_regions; i++) {
				if (!regions[i].r.pixelanzahl) continue;
				dist = _dist(regions[i].r.schwerpunkt, trk[j]->pos->correct);
				if (dist < min_dist) {
					min_dist = dist;
					trk[j]->work->best_match_region = i;
					trk[j]->pos->region_hypo.x = regions[i].r.schwerpunkt.x;
					trk[j]->pos->region_hypo.y = regions[i].r.schwerpunkt.y;
				}
			}

			/* Ist die Region zu weit weg und der tracker virtuell? */
			if (min_dist >= max_dist && trk[j]->work->virtuell) {

				/* Suche die naheliegendste Region
				   bezueglich des virtuellen Schwerpunktes */
				min_dist = 10000.0;
				for (i=0; i<num_regions; i++) {
					if (!regions[i].r.pixelanzahl) continue;
					dist = _dist (regions[i].r.schwerpunkt, trk[j]->pos->tracker_2);
					if (dist < min_dist) {
						min_dist = dist;
						trk[j]->work->best_match_region = i;
						trk[j]->pos->region_hypo.x = regions[i].r.schwerpunkt.x;
						trk[j]->pos->region_hypo.y = regions[i].r.schwerpunkt.y;
					}
				}
			}
			if (min_dist < max_dist) {
				trk[j]->work->region_zugeordnet = TRUE;
			} else {
				iw_debug (3, "Deleted tracker %d min_dist: %f", j, min_dist);
				tracker_remove (trk[j]);
			}
		}
	}
}

/*********************************************************************
  Bewertung der Tracker bezueglich der Zeit, die sie existieren und
  der Distanz, die sie zurueckgelegt haben.
*********************************************************************/
void tracker_all_judge (tracker_t **trk, iwRegion *regions, int numregs,
						opt_values_t *opt_values, unsigned int cur_time)
{
#define NORM	(1/(tanh(1)*tanh(1)))
	int i, j, cnt;
	float dist, time;
	int min_zeit = opt_values->time_init;
	float min_weg = (float)opt_values->dist_init;

	for (i=0; i<MAXKALMAN; i++) {

		if (trk[i]->work->region_zugeordnet && trk[i]->status == Run) {

			dist = trk[i]->judge->max_dist;
			time = cur_time - trk[i]->judge->init_zeit;

			if (min_zeit == 0 || min_weg == 0) {
				trk[i]->judge->judgement = 2.0;
			} else {
				/* Distance more important than time */
				trk[i]->judge->judgement = NORM * tanh(time/(2*min_zeit)) * tanh(dist/min_weg);
			}
			/* Or perhaps:
			   trk[i]->judge->judgement = (time > min_zeit && dist > min_weg); */

			/* Update motion judgement */
			cnt = trk[i]->traject->tracker.r.polygon.n_punkte - trk[i]->work->num_cycles - 2;
			if (cnt<-1) cnt = -1;
			for (j=0; j<trk[i]->work->num_cycles; j++) {
				cnt++;
				if (cnt > 100) cnt = 100;
				trk[i]->judge->motion =
					(trk[i]->judge->motion*cnt + regions[ trk[i]->work->best_match_region ].judgement) /
					(cnt+1);
			}

			if(opt_values->black_white) {
				if (trk[i]->judge->judgement > opt_values->rating) {
					trk[i]->traject->correct.r.farbe = blau;
					trk[i]->traject->correct.r.farbe2 = blau;
				} else {
					trk[i]->traject->correct.r.farbe = hintergrund;
					trk[i]->traject->correct.r.farbe2 = hintergrund;
				}
			} else {
				if (trk[i]->judge->judgement > opt_values->rating) {
					trk[i]->traject->correct.r.farbe = gelb;
					trk[i]->traject->correct.r.farbe2 = gelb;
				} else {
					trk[i]->traject->correct.r.farbe = gruen;
					trk[i]->traject->correct.r.farbe2 = gruen;
				}
			}

			regions[ trk[i]->work->best_match_region ].judge_kalman =
				trk[i]->traject->tracker.judge_kalman =
				trk[i]->traject->predict.judge_kalman =
				trk[i]->traject->correct.judge_kalman =
				trk[i]->judge->judgement;

			regions[ trk[i]->work->best_match_region ].judge_motion =
				trk[i]->traject->tracker.judge_motion =
				trk[i]->traject->predict.judge_motion =
				trk[i]->traject->correct.judge_motion =
				trk[i]->judge->motion;

			trk[i]->traject->tracker.judgement =
				trk[i]->traject->predict.judgement =
				trk[i]->traject->correct.judgement =
				regions[ trk[i]->work->best_match_region ].judgement;
		}
	}
}

/* #]  : */

/* #[ Trajectories : */

/*********************************************************************
  Allocate and init trajectory.
*********************************************************************/
void traject_init (float x, float y, Region_t *reg)
{
	region_clear (reg);

	reg->polygon.n_punkte = 2;
	reg->polygon.punkt = (Punkt_t**)malloc(sizeof(Punkt_t*) * 2);
	reg->polygon.punkt[0] = (Punkt_t*)malloc(sizeof(Punkt_t));
	reg->polygon.punkt[0]->x = x;
	reg->polygon.punkt[0]->y = y;
	reg->polygon.punkt[1] = (Punkt_t*)malloc(sizeof(Punkt_t));
	reg->polygon.punkt[1]->x = x;
	reg->polygon.punkt[1]->y = y;
	reg->schwerpunkt.x = x;
	reg->schwerpunkt.y = y+30;
}

/*********************************************************************
  Update trajectory with new points,
  remove old points, interpolate
*********************************************************************/
void traject_update (float x, float y, int cnt, Region_t *reg)
{
	int i, j, npunkt;
	float oldx, oldy;

	npunkt = reg->polygon.n_punkte;
	oldx = reg->polygon.punkt[npunkt-1]->x;
	oldy = reg->polygon.punkt[npunkt-1]->y;

	/* Allocate new points if less than MAXTRACKLEN */
	i = cnt;
	if (npunkt+i > MAXTRACKLEN) i = MAXTRACKLEN - npunkt;
	if (i > 0) {
		npunkt += i;
		reg->polygon.n_punkte += i;
		reg->polygon.punkt = (Punkt_t**)realloc(reg->polygon.punkt,
												(sizeof(Punkt_t*) * npunkt));
		for (j=npunkt-i; j<npunkt; j++)
			reg->polygon.punkt[j] = (Punkt_t*)malloc(sizeof(Punkt_t));
	}
	/* Remove old points if number of points gets above MAXTRACKLEN */
	if (i != cnt) {
		i = cnt-i;
		for (j=i; j<npunkt; j++) {
			reg->polygon.punkt[j-i]->x = reg->polygon.punkt[j]->x;
			reg->polygon.punkt[j-i]->y = reg->polygon.punkt[j]->y;
		}
	}
	/* Enter interpolated points */
	i = (cnt>MAXTRACKLEN) ? (cnt-MAXTRACKLEN) : 0;
	for (; i<cnt; i++) {
		reg->polygon.punkt[npunkt-cnt+i]->x = oldx + (x - oldx)*(i+1)/cnt;
		reg->polygon.punkt[npunkt-cnt+i]->y = oldy + (y - oldy)*(i+1)/cnt;
	}
	reg->schwerpunkt.x = x;
	reg->schwerpunkt.y = y+30;
}

/* #]  : */

/* Datastruct */
void traject_store (visTraject_t *traject, float *vals)
{
	int cnt = traject->cnt;
	traject->traject->pptAbsPos[cnt]->x = vals[0];
	traject->traject->pptAbsPos[cnt]->y = vals[1];
	traject->traject->pptRelPos[cnt]->x = vals[2];
	traject->traject->pptRelPos[cnt]->y = vals[3];
	traject->cnt++;
}

/* #[ GUI tools : */

/*********************************************************************
  Create string containing all data which should be given out via
  DACS and is not inside the normal RegionHyp_t struct.
*********************************************************************/
char *track_getDataString (tracker_t *trk, char *prefix, track_global_t *global)
{
	static char buf[100];
	sprintf (buf, "%s HT_MOTION: %f HT_TIME: %d HT_DIST: %f",
			 prefix, trk->judge->motion,
			 (int)rintf((float)(global->cur_time - trk->judge->init_zeit) / global->dt_min),
			 trk->judge->max_dist);
	return buf;
}

void track_show_derivative (track_visual_t *visual, Polygon_t *p, int deriv_kind)
{
	int i, cnt, x=0, y=0;
	float a, b;
	prevDataLine lines[MAXTRACKLEN];

	cnt = 0;
	for (i=1; i<p->n_punkte-1; i++) {
		switch (deriv_kind) {
			case 1:
				x = (int)(p->punkt[i]->x - p->punkt[i-1]->x);
				y = (int)(p->punkt[i]->y - p->punkt[i-1]->y);
				break;
			case 2:
				x = (int)(p->punkt[i+1]->x - p->punkt[i]->x);
				y = (int)(p->punkt[i+1]->y - p->punkt[i]->y);
				break;
			case 3:
				x = (int)((p->punkt[i+1]->x - p->punkt[i-1]->x) / 2);
				y = (int)((p->punkt[i+1]->y - p->punkt[i-1]->y) / 2);
				break;
			case 4:		/*  3 */
			case 5:		/*  5 */
			case 6:		/*  7 */
			case 7:		/*  9 */
			case 8:		/* 11 */
			case 9:		/* 13 */
			case 10:	/* 15 */
			case 11:	/* 17 */
			{
				x = deriv_kind - 3;
				if (i >= x && i+x < p->n_punkte
					&& singularVec (&p->punkt[i-x], x*2+1, &a, &b)) {
					x = (int)(a);
					y = (int)(b);
				} else {
					x = 0;
					y = 0;
				}
				break;
			}
		}
		lines[cnt].ctab = IW_INDEX;
		lines[cnt].r = weiss;
		lines[cnt].x1 = (int)p->punkt[i]->x;
		lines[cnt].y1 = (int)p->punkt[i]->y;
		lines[cnt].x2 = (int)p->punkt[i]->x + x;
		lines[cnt].y2 = (int)p->punkt[i]->y + y;
		cnt++;
	}

	prev_render_lines (visual->b_region, lines, cnt, 0,
					   visual->width, visual->height);
}

/* #]  : */
