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

#include "config.h"
#include <stdlib.h>
#include <string.h>

#include "main/output.h"
#include "gui/Goptions.h"
#include "matrix.h"
#include "track_tools.h"
#include "track_kalman.h"

#ifdef KOHLER
/* Umrechnung von q in m/s^2 */
#define RAUSCHEN_Q			(1.0/(1000*1000)) /* Nach millisekunden */
#else
#define RAUSCHEN_Q			(0.0000001)
#endif

/* Special kalman data structure */
typedef struct {
	kalman_t       *kalman;            /* Der Kalman-Filter */
	double         adaptive_q;         /* Adaptives Modellrauschen */
	double         adaptive_r;         /* Adaptives Messrauschen (noch nicht benutzt) */
	matrix_t       *kal_state_prev;    /* Alter kalman-zustand */
	matrix_t       *kal_state_new;     /* Neuer kalman-zustand */
} tracker_kalman_t;

/* #[ Special data struct handling functions : */

/*********************************************************************
  Allocate and init special kalman data struct.
*********************************************************************/
static void tracker_kalman_special_init (tracker_t *trk)
{
	tracker_kalman_t *kalman;

	trk->special = (void*) calloc (1, sizeof(tracker_kalman_t));
	kalman = (tracker_kalman_t*)trk->special;

	kalman->kalman = kalman_create();
	kalman->adaptive_q = 0.0;
	kalman->adaptive_r = 0.0;

#ifdef KOHLER
	kalman->kal_state_prev = iw_create_matrix(4,1);
	kalman->kal_state_new = iw_create_matrix(4,1);
#else
	kalman->kal_state_prev = iw_create_matrix(6,1);
	kalman->kal_state_new = iw_create_matrix(6,1);
#endif
}

/*********************************************************************
  Free mem allocated by tracker_kalman_special_init.
*********************************************************************/
static void tracker_kalman_special_free (tracker_t *trk)
{
	iw_free_matrix ( ((tracker_kalman_t*)trk->special)->kal_state_prev);
	iw_free_matrix ( ((tracker_kalman_t*)trk->special)->kal_state_new);
	kalman_free ( ((tracker_kalman_t*)trk->special)->kalman);

	free(trk->special);
	trk->special = NULL;
}

/* #]  : */

  /* #[ Kalman tracking: */

/*********************************************************************
  Predict new positions for all regions.
*********************************************************************/
static void track_kalman_predict (tracker_t *trk, opt_values_t *opt_values,
								  track_global_t *global)
{
	global->dt = global->cur_time - trk->judge->start_zeit;
	trk->judge->start_zeit = global->cur_time;

	if (global->dt > MAX_TIME_DIFF) {
		iw_debug (2, "Removing Tracker due to time limit exception");
		tracker_remove (trk);
	} else {
		tracker_kalman_t *kalman = (tracker_kalman_t*)trk->special;

		/* Bereinige Ausreisser */
		if (global->dt < global->dt_min)
			global->dt = global->dt_min;

		tracker_getNumCycles (trk, global);

		kalman->adaptive_q = (double)opt_values->rauschen_q * RAUSCHEN_Q;
		kalman->adaptive_r = (double)opt_values->rauschen_r;

		kalman_change_Q_and_R ( kalman->kalman,
								kalman->adaptive_q, kalman->adaptive_r);

		/* Berechne unkorrigierte (Messwerte gehen nicht ein) Kalman Praediktion */
		kalman_setPeriode ( kalman->kalman, (double)global->dt,
							(double)kalman->adaptive_q);

		kalman_timeUpdate ( kalman->kalman);

		trk->pos->correct.x = kalman->kalman->x_predict->elem[0][0];
		trk->pos->correct.y = kalman->kalman->x_predict->elem[1][0];

		/* Visualisiere Kalman Praediktion */
		trk->pos->predict.x = kalman->kalman->x_predict->elem[0][0];
		trk->pos->predict.y = kalman->kalman->x_predict->elem[1][0];

		traject_update ( kalman->kalman->x_predict->elem[0][0],
						 kalman->kalman->x_predict->elem[1][0],
						 trk->work->num_cycles, &trk->traject->predict.r);
	}
}

/*********************************************************************
  Inintialize or update positions and variables of kalman filters.
*********************************************************************/
static void track_kalman_correct (tracker_t *trk, opt_values_t *opt_values,
								  track_global_t *global)
{
	double neu_vx, neu_vy;
	tracker_kalman_t *kalman = (tracker_kalman_t*)trk->special;

	iw_assign_matrix (kalman->kal_state_new, kalman->kal_state_prev);

	trk->pos->tracker.x = trk->pos->region_hypo.x;
	trk->pos->tracker.y = trk->pos->region_hypo.y;

	/* 3. und jeder weitere Durchlauf: */
	if (trk->status == Run) {

		kalman->adaptive_q = (double)opt_values->rauschen_q * RAUSCHEN_Q;
		kalman->adaptive_r = (double)opt_values->rauschen_r;
		kalman_change_Q_and_R ( kalman->kalman,
								kalman->adaptive_q, kalman->adaptive_r);

		/* Messwerte fuer Kalman-Filter eintragen */
		kalman->kalman->z->elem[0][0] = trk->pos->tracker.x;
		kalman->kalman->z->elem[1][0] = trk->pos->tracker.y;

		trk->pos->previous.x = trk->pos->tracker.x;
		trk->pos->previous.y = trk->pos->tracker.y;

		/* Messpunkt in Handtrajektorie eintragen */
		traject_update (trk->pos->tracker.x, trk->pos->tracker.y,
						trk->work->num_cycles, &trk->traject->tracker.r);

		/* Korrigiere die Praediktion anhand der Messwerte */
		kalman_measurementUpdate(kalman->kalman);

		iw_assign_matrix (kalman->kalman->x_correct, kalman->kal_state_new);
		trk->pos->correct.x = kalman->kalman->x_correct->elem[0][0];
		trk->pos->correct.y = kalman->kalman->x_correct->elem[1][0];

		/* Zustand des Kalman-Filters in die Kalman-Trajektorie eintragen */
		traject_update (trk->pos->correct.x,trk->pos->correct.y,
						trk->work->num_cycles,
						& trk->traject->correct.r);

		/* 2. Durchlauf, Kalman-Filter initialisieren */
	} else if (trk->status == Init) {

		iw_debug (3, "Initialize kalman (Reg %d)", trk->work->best_match_region);

		global->dt = global->cur_time - trk->judge->start_zeit;
		trk->judge->start_zeit = global->cur_time;

		if (global->dt > MAX_TIME_DIFF) {
			iw_debug (2,"Removing Kalman due to time limit exception");
			tracker_remove(trk);

		} else {
			/* Bereinige Ausreisser */
			if (global->dt < global->dt_min)
				global->dt = global->dt_min;

			tracker_getNumCycles (trk, global);
			neu_vx = (trk->pos->tracker.x - trk->pos->previous.x) / global->dt;
			neu_vy = (trk->pos->tracker.y - trk->pos->previous.y) / global->dt;

			/* Kalman-Filter initialisieren */
			kalman->adaptive_q = (double)opt_values->rauschen_q * RAUSCHEN_Q;
			kalman->adaptive_r = (double)opt_values->rauschen_r;
			kalman_init_handtrack ( kalman->kalman,
									trk->pos->tracker.x, trk->pos->tracker.y,
									neu_vx, neu_vy, 0.0, 0.0,
									kalman->adaptive_q, kalman->adaptive_r,
									100.0, global->dt);
			kalman_setPeriode ( kalman->kalman, (double)global->dt,
								(double)kalman->adaptive_q);

			kalman->kalman->z->elem[0][0] = trk->pos->tracker.x;
			kalman->kalman->z->elem[1][0] = trk->pos->tracker.y;
			kalman_measurementUpdate ( kalman->kalman);

			/* Trajektorien updaten */
			traject_update (trk->pos->tracker.x, trk->pos->tracker.y,
							trk->work->num_cycles, &trk->traject->tracker.r);
			traject_update (trk->pos->tracker.x, trk->pos->tracker.y,
							trk->work->num_cycles, &trk->traject->correct.r);
			traject_update (trk->pos->tracker.x, trk->pos->tracker.y,
							trk->work->num_cycles, &trk->traject->predict.r);

			/* Praediktion init */
			trk->pos->previous.x = trk->pos->tracker.x;
			trk->pos->previous.y = trk->pos->tracker.y;
			trk->pos->correct.x = trk->pos->tracker.x;
			trk->pos->correct.y = trk->pos->tracker.y;
			trk->status = Run;
		} /* (global->dt > MAX_TIME_DIFF) */

	} else if (trk->status == Prepare) {

		/* 1. Durchlauf, Kalman noch nicht initialisieren, stattdessen Position
		   merken, um im naechsten Durchlauf den Kalman-Filter mit der
		   Anfangsgeschwindigkeit initialisieren zu koennen */

		trk->pos->previous.x = trk->pos->tracker.x;
		trk->pos->previous.y = trk->pos->tracker.y;
		trk->pos->correct.x = trk->pos->tracker.x;
		trk->pos->correct.y = trk->pos->tracker.y;

		/* Trajektorien initialisieren */
		traject_init (trk->pos->tracker.x, trk->pos->tracker.y,
					  &trk->traject->tracker.r);
		traject_init (trk->pos->tracker.x, trk->pos->tracker.y,
					  &trk->traject->correct.r);
		traject_init (trk->pos->tracker.x, trk->pos->tracker.y,
					  &trk->traject->predict.r);

		trk->status = Init;
		trk->judge->start_zeit = global->cur_time;
	}
}

static void track_kalman_traject_calc (tracker_t *trk, int i, float *vals, int window_len)
{
	tracker_kalman_t *kal = (tracker_kalman_t*)trk->special;
	float interpolation = (double)i/trk->work->num_cycles;

	/* Absolute Position als Merkmal */
	vals[0] = kal->kal_state_prev->elem[0][0] +
		(kal->kal_state_new->elem[0][0] - kal->kal_state_prev->elem[0][0])*interpolation;
	vals[1] = kal->kal_state_prev->elem[1][0] +
		(kal->kal_state_new->elem[1][0] - kal->kal_state_prev->elem[1][0])*interpolation;

	/* Geschwindigkeit als Merkmal */
	vals[2] = kal->kal_state_prev->elem[2][0] +
		(kal->kal_state_new->elem[2][0] - kal->kal_state_prev->elem[2][0])*interpolation;
	vals[3] = kal->kal_state_prev->elem[3][0] +
		(kal->kal_state_new->elem[3][0] - kal->kal_state_prev->elem[3][0])*interpolation;
}

/* #]  : */

  /* #[ GUI : */

/*********************************************************************
  Initialise the user interface for the kalmn tracker.
  Return: Number of the option page.
*********************************************************************/
int kalman_init_options (opt_values_t *opt_values, track_imp_t *imp)
{
	int p;
	char *deriv_kind[] = {"None", "Back", "For", "Mid", "SingularVec3", "SingularVec5",
						  "7", "9", "11", "13", "15", "17", NULL};

	opt_values->kalmantrajekt = TRUE;
	opt_values->kalmanpredict = FALSE;
	opt_values->rauschen_q = 1;
	opt_values->rauschen_r = 50;
	opt_values->deriv_kind = 0;
	opt_values->clear_do = FALSE;

	p = opts_page_append ("Kalman");

	opts_entscale_create (p,"Model noise",NULL,&opt_values->rauschen_q,1,100);
	opts_entscale_create (p,"Measurement noise",NULL,&opt_values->rauschen_r,0,5000);
	opts_toggle_create (p,"Kalman traject. (green/yellow)",NULL,&opt_values->kalmantrajekt);
	opts_toggle_create (p,"Kalman prediction (red)",NULL,&opt_values->kalmanpredict);
	opts_option_create (p,"DerivKind:",NULL,deriv_kind,&opt_values->deriv_kind);
	opts_button_create (p,"Clear All",NULL,&opt_values->clear_do);

	imp->special_init = tracker_kalman_special_init;
	imp->special_free = tracker_kalman_special_free;
	imp->predict = track_kalman_predict;
	imp->correct = track_kalman_correct;
	imp->traject_calc = track_kalman_traject_calc;

	return p;
}

/* #]  : */
