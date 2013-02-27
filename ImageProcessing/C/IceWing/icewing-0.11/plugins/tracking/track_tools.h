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

#ifndef iw_track_tools_H
#define iw_track_tools_H

#include <math.h>
#include "gui/Grender.h"
#include "matrix.h"
#include "kalman.h"
#include "trajectory.h"
#include "main/grab.h"

/* #[ Defines : */

#define MAXKALMAN			20			/* Max number of kalmans */
#define MAX_TIME_DIFF		160000

#define VIS_TRAJECT_CNT		2

/* #]  : */

/* #[ Typedefs : */

typedef struct {
	iwRegion	  tracker;				/* Gemessene Trajektorie */
	iwRegion	  predict;				/* Unkorrigierte Praediktion */
	iwRegion	  correct;				/* Korrigierte Trajektorie */
} tracker_traject_t;

typedef struct {
	BOOL		  region_zugeordnet;	/* Ist dem Tracker eine Region zugeordnet */
	BOOL		  virtuell;				/* Verfolge virtuellen Schwerpunkt */
	int			  best_match_region;	/* Index der zugeordneten Handregion */
	int			  num_cycles;			/* Anzahl der 40ms Schritte zwischen den Messpunkten */
	float		  remainder;			/* Fuer Nachkommaanteil bei der Zeitkalkulation */
} tracker_work_t;

typedef struct {
	unsigned int  start_zeit;			/* Zeitpunkt der letzten Tracker-Praediktion */
	unsigned int  init_zeit;			/* Zeit bei Tracker-Initialisierung */
	Punkt_t		  init_pos;				/* Position bei Tracker-Initialisierung */
	float		  max_dist;				/* Max. Entfernung von init_pos */
	float		  judgement;			/* Bewertung aus vergangener Zeit und */
										/* Zurueckgelegter Distanz */
	float		  motion;				/* Judgement based on motion information */
} tracker_judge_t;

typedef struct {
	Punkt_t		  tracker;				/* Gemessene Trackerposition */
	Punkt_t		  tracker_2;			/* Gemessene Trackerposition fuer virtuellen Schwerpunkt? */
										/* 2 * noetig: siehe -> findBestMatchRegion */
	Punkt_t		  previous;				/* Vorige gemessene trackerposition */
	Punkt_t		  correct;				/* Kalman praedizierte / korrigierte Trackerposition */
	Punkt_t		  predict;				/* Kalman praedizierte, NICHT korrigierte Trackerposition*/
	Punkt_t		  region_hypo;			/* Angenommene Position der zugeordneten Trackerregion */
} tracker_position_t;

typedef enum {
	Prepare,
	Init,
	Run
} tracker_status_t;

typedef struct tracker_t {
	tracker_traject_t*	traject;
	tracker_work_t*		work;
	tracker_judge_t*	judge;
	tracker_position_t* pos;
	tracker_status_t	status;
	void*				special;		/* Holds special data e.g. *kalman */
} tracker_t;

typedef enum {
	None,
	Kalman,
	Simple
} track_type_t;

typedef struct {
	track_type_t track_type;			/* Type of tracking: None, Kalman, Simple */

	int outputNr;
	int rauschen_q;						/* Model noise */
	int rauschen_r;						/* Measurement noise */
	int max_tol_dist;					/* Max. tol. distance between two time stamps */
	int dist_init;						/* Distance from initial position */
	int time_init;						/* Time since initialization */
	BOOL delete_merged;					/* Remove older of two merged tracker? */
	BOOL handtrajekt;					/* Display hand trajectorie ? */
	BOOL kalmantrajekt;					/* Display Kalman trajectorie ? */
	BOOL kalmanpredict;					/* Display Kalman prediction ? */
	float region_judgement;				/* Min. rating of region before a tracker is initialised */
	float rating;						/* Min. rating to accept a tracker */
	BOOL clear_do;

	int deriv_kind;						/* Kind of derivation to display: */
										/* "None", "Back", "For", "Mid", "SingularVec3", "SingularVec5", */
										/* "7", "9", "11", "13", "15", "17" */
	int datapoints;
	int window_len;
	BOOL black_white;					/* Draw black on white background */

	int time_wait;						/* Waiting time in ms */
	BOOL cont;							/* time_wait == -1 -> wait on button click */
} opt_values_t;

typedef struct {
	unsigned int cur_time;				/* Current time */
	unsigned int dt;					/* Difference between two time steps */
	int dt_min;
	track_type_t track_type_last;		/* If tracking type is switched */
} track_global_t;

typedef struct {
	char *out_track;
	char *in_regions;
}  visParameter_t;

typedef struct {
	Trajectory_t *traject;	
	int length;							/* Allocated length of traject */
	int cnt;							/* Current number of data points in traject */
	char *stream;						/* Stream name for output */
} visTraject_t;

typedef struct {
	visTraject_t traject[VIS_TRAJECT_CNT];
	prevBuffer *b_region;
	int width, height;					/* Width/height of original image */
	visParameter_t parameter;			/* I/O fuer Plugin */
} track_visual_t;

typedef struct track_imp_t {
	void (*special_init) (tracker_t *trk);
	void (*special_free) (tracker_t *trk);
	void (*predict) (tracker_t *trk, opt_values_t *opt_values, track_global_t *global);
	void (*correct) (tracker_t *trk, opt_values_t *opt_values, track_global_t *global);
	void (*traject_calc) (tracker_t *trk, int i, float *vals, int window_len);
} track_imp_t;

/* #]  : */

#ifdef __cplusplus
extern "C" {
#endif

/* #[ Prototypes : */

/* Tracker_t */
tracker_t* tracker_create (void);
void tracker_remove(tracker_t *trk);
void tracker_free (tracker_t *trk);
void tracker_traject_free (tracker_t *trk);
void tracker_getNumCycles (tracker_t *trk, track_global_t *global);
void tracker_clear_all (tracker_t **trk);

/* Handtrack */
void tracker_all_mapRegions (tracker_t **trk, iwRegion *regions,
							 int num_regions, opt_values_t *opt_values,
							 track_global_t *global);
void tracker_all_findBestMatchRegion (tracker_t **trk, iwRegion *regions,
									  int num_regions, opt_values_t *opt_values);
void tracker_all_init (tracker_t **trk, iwRegion *regions,
					   int num_regions, opt_values_t *opt_values,
					   track_global_t *global);
void tracker_all_judge (tracker_t **trk, iwRegion *regions, int numregs,
						opt_values_t *opt_values, unsigned int cur_time);

/* Trajectories */
void traject_init (float x, float y, Region_t *reg);
void traject_update (float x, float y, int cnt, Region_t *reg);

/* Datastruct */
void traject_store (visTraject_t *traject, float *vals);

/* GUI */
char *track_getDataString (tracker_t *trk, char *prefix, track_global_t *global);
void track_show_derivative (track_visual_t *visual, Polygon_t *p, int deriv_kind);

/* #]  : */

#ifdef __cplusplus
}
#endif

#endif /* iw_track_tools_H */
