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
#include <string.h>

#include "main/output.h"
#include "gui/Goptions.h"
#include "track_tools.h"
#include "track_simple.h"

/* #[ Special data struct handling functions : */

static void tracker_simple_special_init (tracker_t *trk)
{
	trk->special = NULL;		/* Nothing special yet */
}

static void tracker_simple_special_free (tracker_t *trk)
{
	trk->special = NULL;
}
/* #]  : */

/* #[ Simple tracking: */

/*********************************************************************
  Predict new positions for all regions by simply using old position.
*********************************************************************/
static void track_simple_predict (tracker_t *trk, opt_values_t *opt_values,
								  track_global_t *global)
{
	global->dt = global->cur_time - trk->judge->start_zeit;
	trk->judge->start_zeit = global->cur_time;

	if (global->dt > MAX_TIME_DIFF) {
		iw_debug (2, "Removing Tracker due to time limit exception");
		tracker_remove (trk);
	} else {

		/* Bereinige Ausreisser */
		if (global->dt < global->dt_min)
			global->dt = global->dt_min;

		tracker_getNumCycles (trk, global);

		/* Alte vorherige Position als Praediktion*/
		trk->pos->predict.x = trk->pos->tracker.x;
		trk->pos->predict.y = trk->pos->tracker.y;

		/* Sichern des aktuellen Wertes fuer naechsten Schritt */
		trk->pos->previous.x = trk->pos->tracker.x;
		trk->pos->previous.y = trk->pos->tracker.y;
	}
}

/*********************************************************************
  Update positions and trajectories for simple tracker.
*********************************************************************/
static void track_simple_correct (tracker_t *trk, opt_values_t *opt_values,
								  track_global_t *global)
{
	trk->pos->tracker.x = trk->pos->region_hypo.x;
	trk->pos->tracker.y = trk->pos->region_hypo.y;

	/* 2. und jeder weitere Durchlauf: */
	if (trk->status == Run) {

		/* Messwerte fuer Tracker eintragen */
		trk->pos->predict.x = 2*trk->pos->tracker.x - trk->pos->previous.x;
		trk->pos->predict.y = 2*trk->pos->tracker.y - trk->pos->previous.y;

		/* Korrigierter Wert ist */
		trk->pos->correct.x = trk->pos->tracker.x;
		trk->pos->correct.y = trk->pos->tracker.y;

		/* Messpunkt in Trajektorie eintragen */
		traject_update (trk->pos->tracker.x, trk->pos->tracker.y,
						trk->work->num_cycles, &trk->traject->tracker.r);

		traject_update (trk->pos->correct.x,trk->pos->correct.y,
						trk->work->num_cycles,
						& trk->traject->correct.r);

	/* 1. Durchlauf, Tracker initialisieren */
	} else if (trk->status == Prepare) {

		/* Da erster Durchlauf, kein dt! */
		global->dt = global->dt_min;
		trk->judge->start_zeit = global->cur_time;

		/* Handtrajektorie initialisieren */
		traject_init (trk->pos->tracker.x, trk->pos->tracker.y,
					  &trk->traject->tracker.r);
		traject_init (trk->pos->tracker.x, trk->pos->tracker.y,
					  &trk->traject->correct.r);
		trk->pos->previous.x = trk->pos->tracker.x;
		trk->pos->previous.y = trk->pos->tracker.y;
		trk->pos->correct.x = trk->pos->tracker.x;
		trk->pos->correct.y = trk->pos->tracker.y;
		trk->pos->predict.x = trk->pos->tracker.x;
		trk->pos->predict.y = trk->pos->tracker.y;
		trk->status = Run;

	} else /* if (trk->status == Init) */ {
		iw_warning ("Not a valid tracking status for simple correct!");
	}
}

static void track_simple_traject_calc (tracker_t *trk, int i, float *vals, int window_len)
{
	int norm = trk->work->num_cycles * window_len;

	/* Absolute Position als Merkmal */
	vals[0] = trk->pos->correct.x;
	vals[1] = trk->pos->correct.y;

	/* Geschwindigkeit als Merkmal */
	vals[2] = (trk->pos->correct.x - trk->pos->previous.x)/norm;
	vals[3] = (trk->pos->correct.y - trk->pos->previous.y)/norm;
}

/* #]  : */

/* #[ GUI : */

/*********************************************************************
  Initialise the user interface for the simple tracker.
  Return: Number of the option page.
*********************************************************************/
int track_simple_init_options (opt_values_t *opt_values, track_imp_t *imp)
{
	imp->special_init = tracker_simple_special_init;
	imp->special_free = tracker_simple_special_free;
	imp->predict = track_simple_predict;
	imp->correct = track_simple_correct;
	imp->traject_calc = track_simple_traject_calc;


	return -1;
}

/* #]  : */
