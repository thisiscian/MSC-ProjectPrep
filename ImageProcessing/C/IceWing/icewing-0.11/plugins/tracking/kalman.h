/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
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

#ifndef iw_kalman_H
#define iw_kalman_H

#include "matrix.h"

/* Nachfolgendes Define fuer Annahme konstanter Geschwindigkeit nach "Kohler", bei
   zusaetzlicher Annahme konstanter Beschleunigung dieses Define auf NO_KOHLER o.ae.! */
#define NoKOHLER

typedef struct kalman_t {
	matrix_t *x_predict;
	matrix_t *x_correct;
	matrix_t *P_predict;
	matrix_t *P_correct;
	matrix_t *K;
	matrix_t *z;
	matrix_t *A;
	matrix_t *A_trans;
	matrix_t *H;
	matrix_t *H_trans;
	matrix_t *Q;
	matrix_t *R;
	matrix_t *I;
} kalman_t;

kalman_t *kalman_create (void);

void kalman_free (kalman_t *kalman);

void kalman_init_handtrack(kalman_t *kalman,
						   double x, double y, double vx, double vy , double ax, double ay,
						   double q, double r, double P, double t);

void kalman_setPeriode(kalman_t *kalman, double t, double q);

int kalman_change_Q_and_R(kalman_t *kalman, double new_q, double new_r);

int kalman_measurementUpdate(kalman_t *kalman);

int kalman_timeUpdate(kalman_t *kalman);

#endif /* iw_kalman_H */
