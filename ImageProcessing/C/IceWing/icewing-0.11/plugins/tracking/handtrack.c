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
#include <gtk/gtk.h>
#include <dacs.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include "handtrack.h"
#include "track_tools.h"
#include "trajectory.h"
#include "track_simple.h"
#include "track_kalman.h"
#include "main/output.h"
#include "main/mainloop.h"
#include "gui/Goptions.h"
#include "gui/Ggui.h"
#include "gui/Grender.h"

#define MODULE_ATTACH_RETRY	300
#define SYNC_LEVEL			1
#define MAXREGIONS			20

static plugDefinition plug_tracking;

static track_global_t global;             /* Lokale Variablen */
static track_visual_t visual;             /* Dto. fuer Visualisierung */
static tracker_t      **trk = NULL;       /* Die Tracker */
static opt_values_t   opt_values;         /* Optionen aus der GUI */
static track_imp_t    track_imp[3]={{NULL,NULL,NULL,NULL,NULL}};

/*********************************************************************
  Return TRUE if tracking filter has good judgement.
*********************************************************************/
BOOL track_is_valid (float judgement)
{
	if (judgement > opt_values.rating)
		return TRUE;

	return FALSE;
}

/*********************************************************************
  Called from the main loop for every pass one time.
*********************************************************************/
static void track_cb_main (plugDefinition *plug)
{
	if (opt_values.time_wait > 0) {
		iw_usleep (opt_values.time_wait*1000);
	} else if (opt_values.time_wait < 0) {
		while (!opt_values.cont && opt_values.time_wait < 0)
			iw_usleep (10000);
		opt_values.cont = FALSE;
	}
}

/*********************************************************************
  Clear all trackers.
*********************************************************************/
void track_clearAll (void)
{
	tracker_clear_all (trk);
}

/*********************************************************************
  Initialise the user interface for the tracking plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int track_init_options (plugDefinition *plug)
{
	int p;
	char *track_type[] = {"None", "Kalman", "Simple", NULL};
	char *tracker_output[] = {"best", "second", NULL};

	opt_values.handtrajekt = TRUE;
	opt_values.clear_do = FALSE;
	opt_values.rating = 1.0;
	opt_values.track_type = Simple;
	opt_values.max_tol_dist	 =	200;
	opt_values.dist_init  =	 50;
	opt_values.time_init  =	 320;
	opt_values.region_judgement = 0.0;
	opt_values.delete_merged = FALSE;

	opt_values.outputNr	 =	0;

	opt_values.datapoints = 15;
	opt_values.window_len = 40;
	opt_values.black_white = FALSE;

	visual.b_region = prev_new_window ("Tracking Trajectories", -1, -1, FALSE, FALSE);
	prev_opts_append (visual.b_region, PREV_TEXT, PREV_REGION, -1);

	p = opts_page_append ("Tracking");
	opts_option_create (p, "TrackingKind:", "Select kind of tracking to use",
						track_type, (int*)(void*)&opt_values.track_type);

	opts_radio_create (p, "Output Order", "Selection of tracker for output",
					   tracker_output, &opt_values.outputNr);
	opts_float_create (p, "Rating",
					   "filter judgment (Time+Dist) to be accepted",
					   &opt_values.rating, 0, 2);
	opts_entscale_create (p, "Max TolDist",
						  "max. tolerated distance between two time steps",
						  &opt_values.max_tol_dist, 50, 500);
	opts_entscale_create (p, "Dist Init",
						  "distance from initial position",
						  &opt_values.dist_init, 0, 500);
	opts_entscale_create (p, "Time Init",
						  "time since initialization",
						  &opt_values.time_init, 0, 4000);
	opts_float_create (p, "Min Rating",
					   "region judgement for initializing a new tracking filter",
					   &opt_values.region_judgement, 0, 2);
	opts_toggle_create (p, "Delete merged trackers",
						"Delete older tracker if two track the same region",
						&opt_values.delete_merged);

	opts_button_create (p, "Clear All", NULL, &opt_values.clear_do);
	opts_toggle_create (p, "Measured Hand trajectory (red | green/yellow)", NULL, &opt_values.handtrajekt);
	opts_toggle_create (p, "white background", NULL, &opt_values.black_white);

	opts_entscale_create (p, "Data Points",
						  "Size of data points array send over DACS",
						  &opt_values.datapoints, 1, 25);

	opts_entscale_create (p, "Sampling time",
						  "Time in ms for grabbing next image (30 Hz=33, 25Hz=40, 15Hz=66)",
						  &opt_values.window_len, 33, 67);

	kalman_init_options (&opt_values, &track_imp[Kalman]);
	track_simple_init_options(&opt_values, &track_imp[Simple]);

	if (visual.parameter.in_regions) {
		opt_values.time_wait = 0;
		opt_values.cont = FALSE;

		p = opts_page_append ("RegionRead");

		opts_entscale_create (p, "Wait Time",
							  "Time in ms to wait between region reads"
							  "  -1: wait until the button is pressed",
							  &opt_values.time_wait, -1, 2000);
		opts_button_create (p, "Read Reagion",
							"Read next region if 'Wait Time == -1'",
							&opt_values.cont);
	}

	return p;
}

static void help (void)
{
	fprintf (stderr, "\n%s plugin for %s, (c) 1999-2009 by various people\n"
			 "Track regions with a kalman filter.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-s stream][-o]\n"
			 "-s       read regions from the DACS regionHyp stream\n"
			 "-o       output tracking results on streams\n"
			 "         <%s>_track, <%s>_points, and <%s>_points2\n",
			 plug_tracking.name, ICEWING_NAME, plug_tracking.name,
			 IW_DACSNAME, IW_DACSNAME, IW_DACSNAME);
	gui_exit (1);
}

/*********************************************************************
  Initialise the tracking and register the output streams
  <dname>_track and <dname>_points and the input stream
  <source_region>.
*********************************************************************/
#define ARG_TEMPLATE "-S:Sr -O:O -H:H"
static void track_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
{
	struct timeval time;
	void *arg;
	char ch;
	int nr = 0, i;

	visual.parameter.out_track = NULL;
	visual.parameter.in_regions = NULL;
	for (i=0; i<VIS_TRAJECT_CNT; i++) {
		visual.traject[i].traject = NULL;
		visual.traject[i].stream = NULL;
	}

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'S':
				visual.parameter.in_regions = (char*)arg;
				break;
			case 'O':
				visual.parameter.out_track =
					iw_output_register_stream ("_track",
											   (NDRfunction_t*)ndr_RegionHyp);
				visual.traject[0].stream =
					iw_output_register_stream ("_points",
											   (NDRfunction_t*)ndr_Trajectory);
				visual.traject[1].stream =
					iw_output_register_stream ("_points2",
											   (NDRfunction_t*)ndr_Trajectory);
				break;
			case 'H':
			case '\0':
				help();
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help();
		}
	}

	gettimeofday (&time, NULL);
	global.cur_time = time.tv_sec*1000 + time.tv_usec/1000;
	global.track_type_last = None;
	global.dt_min = 40;

	/* Allokation des Speichers fuer Tracker, Trajektorien,... */
	trk = (tracker_t**)malloc(sizeof(tracker_t*)*MAXKALMAN);
	for (nr=0; nr<MAXKALMAN; nr++)
		trk[nr] = tracker_create();

	if (visual.parameter.in_regions) {
		plug_observ_data (plug, "start");
		plug_function_register (plug, "mainCallback", (plugFunc)track_cb_main);
	}
}

/*********************************************************************
  Free the resources allocated by track_init().
*********************************************************************/
static void track_cleanup (plugDefinition *plug)
{
	int i;

	for (i=0; i<VIS_TRAJECT_CNT; i++) {
		if (visual.traject[i].traject) {
			trajectory_destroy (visual.traject[i].traject);
			visual.traject[i].traject = NULL;
		}
	}

	if (trk) {
		for (i=0; i<MAXKALMAN; i++)
			tracker_free (trk[i]);
		free (trk);
		trk = NULL;
	}
}

/*********************************************************************
  Ausgabe der Tracker-Zustaende.
*********************************************************************/
static void tracker_output_traject (tracker_t **trk, opt_values_t *opt_values,
									track_visual_t *visual, int track_type)
{
	/*
	static BOOL vertauschen = FALSE;
	float max = -1, max2 = -1;
	*/

	int cycles = 1, num_tracker = 0;
	int i, j, index[2] = {-1,-1};
	visTraject_t *traject;
	float vals[4];

	if (!visual->traject[0].stream) return;

	for (i=0; i<MAXKALMAN; i++) {
		/* Ermittlung der beiden best-bewertetesten Tracker, die ausgegeben werden sollen
		if (track_is_valid(trk[i]->judge->judgement) && trk[i]->judge->judgement > max2) {
			if (trk[i]->judge->judgement > max) {
				max2 = max;
				index[1] = index[0];

				max = trk[i]->judge->judgement;
				index[0] = i;
			} else {
				max2 = trk[i]->judge->judgement;
				index[1] = i;
			}
			num_tracker++;
		}
		*/
		/* Ermittlung der beiden ersten guten Tracker, die ausgegeben werden sollen */
		if (track_is_valid(trk[i]->judge->judgement)) {
			if (index[0] == -1) {
				index[0] = i;
			} else if (index[1] == -1) {
				index[1] = i;
			}
			num_tracker++;
		}
	}

	iw_debug (2, "%d good trackers", num_tracker);

	/* Linker Tracker bekommt index[0] und wird dann zuerst ausgegeben */
	/* ggf. Tracker vertauschen */
	/*
	if (num_tracker < 2) vertauschen = FALSE;
	if (num_tracker == 2 && !vertauschen) {
		vertauschen = TRUE;
		if (trk[index[0]]->pos->correct.x > trk[index[1]]->pos->correct.x) {
			tracker_t *tausch = trk[index[0]];
			trk[index[0]] = trk[index[1]];
			trk[index[1]] = tausch;
		}
	}
	*/

	for (j=0; j<VIS_TRAJECT_CNT; j++) {
		traject = &visual->traject[j];
		if (traject->traject == NULL) {
			traject->length = opt_values->datapoints;
			traject->cnt = 0;

			iw_debug (2, "create Trajectorie with %d points", traject->length);
			traject->traject = trajectory_alloc (traject->length);
		}
	}

	if ((opt_values->outputNr == 1) && (num_tracker>1)) {
		i = index[0]; index[0] = index[1]; index[1] = i;
	}

	for (j=0; j<VIS_TRAJECT_CNT && j<num_tracker; j++) {
		iw_debug (2, "output tracker index %d",index[j]);

		traject = &visual->traject[j];
		cycles = trk[index[j]]->work->num_cycles;

		for (i=0; i<cycles; i++) {
			track_imp[track_type].traject_calc (trk[index[j]], i, vals, global.dt_min);
			traject_store (traject, vals);

			if (traject->cnt == traject->length) {
				iw_debug (2, "send Trajectorie points on stream");

				iw_output_stream (traject->stream, traject->traject);
				iw_output_sync (traject->stream);

				if (traject->length != opt_values->datapoints) {
					traject->length = opt_values->datapoints;

					trajectory_destroy (traject->traject);
					traject->traject = trajectory_alloc (traject->length);
				}
				traject->cnt = 0;
			}
		}
	}
}

/*********************************************************************
  Do a tracking loop and output the results on the stream
  opened by track_init().
*********************************************************************/
void track_do_tracking (iwRegion *regions, int numregs, grabImageData *gimg)
{
	int i, numTracker = 0;
	unsigned int last_time;
	int track_type = opt_values.track_type;

	global.dt_min = opt_values.window_len;
	visual.width = gimg->img.width*gimg->downw;
	visual.height = gimg->img.height*gimg->downh;

	/* Has user switched tracking types? */
	if (global.track_type_last != track_type) {
		iw_debug (2, "Switched tracking type.");
		/* Clear old data */
		for (i=0; i<MAXKALMAN; i++) {
			tracker_remove (trk[i]);
			trk[i]->status = Prepare;

			/* Free old special tracker data */
			if (track_imp[global.track_type_last].special_init)
				track_imp[global.track_type_last].special_init (trk[i]);

			/* Init special tracker data */
			if (track_imp[track_type].special_init)
				track_imp[track_type].special_init (trk[i]);
		}
	}

	global.track_type_last = track_type;

	if (opt_values.clear_do) {
		tracker_clear_all (trk);
		opt_values.clear_do = FALSE;
	}

	if (track_type == None) return;

	last_time = global.cur_time;
	global.cur_time = gimg->time.tv_sec*1000 + gimg->time.tv_usec/1000;

	if (trk == NULL)
		iw_error ("No tracker structure");

	/* Praediziere die vermutete Position */
	for (i=0; i<MAXKALMAN; i++)
		if (trk[i]->status == Run)
			track_imp[track_type].predict (trk[i], &opt_values, &global);

	/* Finde die guenstigste Zuordnung der Regionen zu den
	   Trackern auf Grund der praedizierten Positionen */
	tracker_all_mapRegions (trk, regions, numregs, &opt_values, &global);

	/* Falls fuer einen Tracker eine Zuordnung zustande kam,
	   wird der Schaetzwert der Position anhand der Messung korrigiert */
	numTracker = 0;
	for (i=0; i<MAXKALMAN; i++) {

		if (trk[i]->work->region_zugeordnet) {
			track_imp[track_type].correct (trk[i], &opt_values, &global);
		} else {
			trk[i]->status = Prepare;
			tracker_traject_free (trk[i]);
		}
		if (trk[i]->work->region_zugeordnet && trk[i]->status == Run)
			numTracker++;
		else if (trk[i]->work->best_match_region >= 0)
			regions[trk[i]->work->best_match_region].id = -1;
	}
	iw_debug (2, "Duration: %d, No. tracker: %d", global.cur_time - last_time, numTracker);

	tracker_all_judge (trk, regions, numregs, &opt_values, global.cur_time);

	/* Streams rausschicken */
	if (visual.parameter.out_track) {
		/*
		  'Special' values for the output:
		  regions, trajektorie_hand, trajektorie_predict, trajektorie_kalman:
		  judgement = regions[ht[j]->best_match_region].judgement
		  stabilitaet = ht[j]->judgement
		  id = j  [0..MAXKALMAN-1]
		  alter = rintf((cur_time - ht[j]->init_zeit) / global.dt_min);

		  trajektorie_hand, trajektorie_predict, trajektorie_kalman:
		  umfang = cur_time - ht[j]->init_zeit
		  compactness = ht[j]->max_dist
		*/
		iw_output_regions (regions, numregs, gimg->time, gimg->img_number,
						   IW_GHYP_TITLE, visual.parameter.out_track);

		for (i=0; i<MAXKALMAN; i++) {
			if (! trk[i]->work->region_zugeordnet || trk[i]->status!=Run)
				continue;
			if (opt_values.handtrajekt && trk[i]->traject->tracker.r.polygon.n_punkte > 0)
				iw_output_regions (&trk[i]->traject->tracker, 1, gimg->time,
								   gimg->img_number,
								   track_getDataString(trk[i], "hand", &global),
								   visual.parameter.out_track);

			if (opt_values.kalmanpredict && trk[i]->traject->predict.r.polygon.n_punkte > 0)
				iw_output_regions (&trk[i]->traject->predict, 1, gimg->time,
								   gimg->img_number,
								   track_getDataString(trk[i], "predict", &global),
								   visual.parameter.out_track);

			if (opt_values.kalmantrajekt && trk[i]->traject->correct.r.polygon.n_punkte > 0)
				iw_output_regions (&trk[i]->traject->correct, 1, gimg->time,
								   gimg->img_number,
								   track_getDataString(trk[i], "kalman", &global),
								   visual.parameter.out_track);
		}
		iw_output_sync (visual.parameter.out_track);
	}

	/* Trajektorien der Tracker-Zustaende rausschicken */
	tracker_output_traject (trk, &opt_values, &visual, track_type);

	if (visual.b_region->window) {

		int cnt = 0, lcnt = 0;
		prevData data[MAXKALMAN*4];
		prevDataLine lines[MAXKALMAN];

		for (i=0; i<MAXKALMAN; i++)
			if (trk[i]->work->region_zugeordnet && trk[i]->status == Run) {
				if (opt_values.handtrajekt && trk[i]->traject->tracker.r.polygon.n_punkte > 0) {
					data[cnt].type = PREV_REGION;
					data[cnt++].data = &trk[i]->traject->tracker;
				}
				if (opt_values.kalmanpredict && trk[i]->traject->predict.r.polygon.n_punkte > 0) {
					lines[lcnt].ctab = IW_INDEX;
					lines[lcnt].r = rot;
					lines[lcnt].x1 = (int)trk[i]->pos->predict.x-25;
					lines[lcnt].y1 = (int)trk[i]->pos->predict.y-25;
					lines[lcnt].x2 = (int)trk[i]->pos->predict.x+25;
					lines[lcnt].y2 = (int)trk[i]->pos->predict.y+25;
					data[cnt].type = PREV_RECT;
					data[cnt++].data = &lines[lcnt++];

					data[cnt].type = PREV_REGION;
					data[cnt++].data = &trk[i]->traject->predict;
				}
				if (opt_values.kalmantrajekt && trk[i]->traject->correct.r.polygon.n_punkte > 0) {
					data[cnt].type = PREV_REGION;
					data[cnt++].data = &trk[i]->traject->correct;
				}
			}
		/* White background for snapshots's? */
		if (opt_values.black_white)
			prev_set_bg_color (visual.b_region, 255, 255, 255);
		else
			prev_set_bg_color (visual.b_region, 0, 0, 0);

		prev_render_regions (visual.b_region, regions, numregs, RENDER_CLEAR,
							 visual.width, visual.height);
		prev_render_set (visual.b_region, data, cnt, 0,
						 visual.width, visual.height);

		if (opt_values.deriv_kind)
			for (i=0; i<MAXKALMAN; i++)
				if (trk[i]->work->region_zugeordnet && trk[i]->status == Run) {
					if (opt_values.handtrajekt && trk[i]->traject->tracker.r.polygon.n_punkte > 0)
						track_show_derivative (&visual, &trk[i]->traject->tracker.r.polygon,
											   opt_values.deriv_kind);
					if (opt_values.kalmantrajekt && trk[i]->traject->correct.r.polygon.n_punkte > 0)
						track_show_derivative (&visual, &trk[i]->traject->correct.r.polygon,
											   opt_values.deriv_kind);
				}
		prev_draw_buffer (visual.b_region);
	}
}

/*********************************************************************
  Receive region hypotheses from <parameter.in_regions> and track
  them. This function does not return.
*********************************************************************/
static BOOL track_process (plugDefinition *plug, char *ident, plugData *data)
{
	DACSstatus_t status;
	NDRstatus_t nstatus;
	DACSmessage_t *message;
	RegionHyp_t *region_hyp[MAXREGIONS];	/* Speicher fuer Regionen eines Zeitschritts */
	iwRegion regions[MAXREGIONS];
	int i, region_counter = 0;
	grabImageData gimg;

	memset (&gimg, 0, sizeof(grabImageData));
	gimg.downw = gimg.downh = 1;
	gimg.img.width = IW_PAL_WIDTH;
	gimg.img.height = IW_PAL_HEIGHT;

	iw_output_register();

	if (!visual.parameter.in_regions || !*visual.parameter.in_regions)
		iw_error ("No name for the region stream set");

	/* Order region stream */
	if (dacs_order_stream_with_retry (iw_dacs_entry, visual.parameter.in_regions,
									  NULL, 0, MODULE_ATTACH_RETRY) != D_OK)
		iw_error ("Can't attach to signal stream '%s'", visual.parameter.in_regions);

	iw_mainloop_cyclic();

	while (1) {
		status = _dacs_recv_message (iw_dacs_entry, visual.parameter.in_regions,
									 DACS_BLOCK, &message);
		switch (status) {
			case D_OK:
				if (region_counter < MAXREGIONS) {
					/* Daten dekodieren, damit DACS den Speicher fuer das einzulesende Element
					   alloziert, muss der Pointer hier genullt werden... */
					region_hyp[region_counter] = NULL;
					nstatus = ndr_RegionHyp (message->ndrs, &region_hyp[region_counter]);
					if (nstatus != NDR_SUCCESS) {
						iw_warning ("Error #%d decoding data", nstatus);
						break;
					}
					if (region_hyp[region_counter]!=NULL)
						region_counter++;
				}
				break;

			case D_SYNC:
				/* break; */
			case D_NO_DATA:
				/* Alle Regionen uebermittelt, Ende des Zeitschrittes */
				if (region_counter > 0) {
					gimg.time.tv_sec = region_hyp[0]->kopf.sec;
					gimg.time.tv_usec = region_hyp[0]->kopf.usec;
					for (i=0; i<region_counter; i++) {
						regions[i].r = *(region_hyp[i]->region);
						regions[i].judgement = region_hyp[i]->kopf.bewertung;
						regions[i].judge_kalman = 0;
						regions[i].judge_motion = 0;
						regions[i].labindex = 0;
						regions[i].avgConf = 0;
						regions[i].data = NULL;
						if (regions[i].r.echtfarbe.modell == FARB_MODELL_UNDEF) {
							regions[i].r.echtfarbe.x = 0.0;
							regions[i].r.echtfarbe.y = 0.0;
							regions[i].r.echtfarbe.z = 0.0;
						} else if (regions[i].r.echtfarbe.modell == FARB_MODELL_SW) {
							regions[i].r.echtfarbe.y = 0.0;
							regions[i].r.echtfarbe.z = 0.0;
						}
					}
					track_do_tracking (regions, region_counter, &gimg);

					for (i=0; i<region_counter; i++)
						ndr_free ((void **)&region_hyp[i], (NDRfunction_t*)ndr_RegionHyp);
					region_counter = 0;
				}
				iw_mainloop_cyclic();
				if (status == D_NO_DATA) {
					/* ... und Signalquelle (Handregionen) erneut oeffnen */
					if (dacs_order_stream_with_retry (iw_dacs_entry, visual.parameter.in_regions, NULL,
													  SYNC_LEVEL, MODULE_ATTACH_RETRY) != D_OK)
						iw_error ("Cannot re-attach to signal source '%s'",
								 visual.parameter.in_regions);
				}
				break;
			default:
				iw_warning ("Receiving %d from '%s'", status, visual.parameter.in_regions);
				break;
		}
		/* ... und Nachricht (insbes. NDR-Stream) freigeben */
		_dacs_destroy_message (message);
	}
	return TRUE;
}

/*********************************************************************
  Plugin definition.
*********************************************************************/
static plugDefinition plug_tracking = {
	"Tracking",
	PLUG_ABI_VERSION,
	track_init,
	track_init_options,
	track_cleanup,
	track_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *iw_track_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_tracking;
}
