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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "kalman.h"

kalman_t *kalman_create (void)
{
	int i,j;
	kalman_t *kalman;
	int      dim_x, dim_z;

#ifdef KOHLER
	dim_x = 4;
#else
	dim_x = 6;
#endif
	dim_z = 2;

	kalman = (kalman_t*) malloc(sizeof(kalman_t));
	kalman->x_predict = iw_create_matrix(dim_x,1);
	kalman->x_correct = iw_create_matrix(dim_x,1);
	kalman->P_predict = iw_create_matrix(dim_x,dim_x);
	kalman->P_correct = iw_create_matrix(dim_x,dim_x);
	kalman->K = iw_create_matrix(dim_x,dim_z);
	kalman->z = iw_create_matrix(dim_z,1);
	kalman->A = iw_create_matrix(dim_x,dim_x);
	kalman->A_trans = iw_create_matrix(dim_x,dim_x);
	kalman->H = iw_create_matrix(dim_z,dim_x);
	kalman->H_trans = iw_create_matrix(dim_x,dim_z);
	kalman->Q = iw_create_matrix(dim_x,dim_x);
	kalman->R = iw_create_matrix(dim_z,dim_z);
	kalman->I = iw_create_matrix(dim_x,dim_x);
	for (i=0;i < kalman->I->rows;i++)
		for (j=0;j < kalman->I->cols;j++)
			kalman->I->elem[i][j] = ((i==j) ? 1.0 : 0.0);

	return kalman;
}

void kalman_free (kalman_t *kalman)
{
	iw_free_matrix(kalman->x_predict);
	iw_free_matrix(kalman->x_correct);
	iw_free_matrix(kalman->P_predict);
	iw_free_matrix(kalman->P_correct);
	iw_free_matrix(kalman->K);
	iw_free_matrix(kalman->z);
	iw_free_matrix(kalman->A);
	iw_free_matrix(kalman->A_trans);
	iw_free_matrix(kalman->H);
	iw_free_matrix(kalman->H_trans);
	iw_free_matrix(kalman->Q);
	iw_free_matrix(kalman->R);
	iw_free_matrix(kalman->I);

	free(kalman);
	kalman = NULL;
}

void kalman_init_handtrack (kalman_t *kalman,
							double x, double y, double vx, double vy , double ax, double ay,
							double q, double r, double P, double t)
{
	register int i;
	register int j;
	
#ifdef KOHLER
	float a_2;
#endif

	kalman->x_predict->elem[0][0] = x;
	kalman->x_predict->elem[1][0] = y;
	kalman->x_predict->elem[2][0] = vx;
	kalman->x_predict->elem[3][0] = vy;
#ifndef KOHLER
	kalman->x_predict->elem[4][0] = ax;
	kalman->x_predict->elem[5][0] = ay;
#endif
	for(i=0; i < kalman->P_predict->rows; i++) {
		for(j=0; j < kalman->P_predict->cols; j++) {
			kalman->P_predict->elem[i][j] = ((i==j) ? P : 0.0);
		}
	}

	for(i=0; i < kalman->A->rows; i++) {
		for(j=0; j < kalman->A->cols; j++) {
			if (i==j)
				kalman->A->elem[i][j] = 1.0;
			else if (j==i+2)
				kalman->A->elem[i][j] = 1.0;
			else if (j==i+4)
				kalman->A->elem[i][j] = 0.5;
			else
				kalman->A->elem[i][j] = 0.0;
		}
	}
	iw_transpose_matrix(kalman->A, kalman->A_trans);

	/*nur die Position uebernehmen, alles weitere schaetzen....*/
	for(i=0; i < kalman->H->rows; i++) {
		for(j=0; j < kalman->H->cols; j++) {
			kalman->H->elem[i][j] = ((i==j) ? 1.0 : 0.0);
		}
	}

	iw_transpose_matrix(kalman->H, kalman->H_trans);

#ifdef KOHLER
	a_2 = q * q;
	kalman->Q->elem[0][0] = (a_2*t*t*t)/3;
	kalman->Q->elem[1][1] = (a_2*t*t*t)/3;
	kalman->Q->elem[2][2] = (a_2*t);
	kalman->Q->elem[3][3] = (a_2*t);

	kalman->Q->elem[0][2] = (a_2*t*t)/2;
	kalman->Q->elem[1][3] = (a_2*t*t)/2;
	kalman->Q->elem[2][0] = (a_2*t*t)/2;
	kalman->Q->elem[3][1] = (a_2*t*t)/2;
#else
	for (i=0; i < kalman->Q->rows; i++) {
		for (j=0; j < kalman->Q->cols; j++) {
			kalman->Q->elem[i][j] = ((i==j) ? q : 0.0);
		}
	}
#endif

	for (i=0; i < kalman->R->rows; i++) {
		for (j=0; j < kalman->R->cols; j++) {
			kalman->R->elem[i][j] = ((i==j) ? r : 0.0);
		}
	}
}


void kalman_setPeriode (kalman_t *kalman, double t,double q)
{
	register int i,j;
#ifdef KOHLER
	float a_2;
#endif

	for (i=0; i < kalman->A->rows; i++)
		for (j=0; j < kalman->A->cols; j++) {
			if (i==j)
				kalman->A->elem[i][j] = 1.0;
			else if (j==i+2)
				kalman->A->elem[i][j] = t;
			else if (j==i+4)
				kalman->A->elem[i][j] = 0.5*t*t;
			else
				kalman->A->elem[i][j] = 0.0;
		}
	iw_transpose_matrix(kalman->A, kalman->A_trans);
#ifdef KOHLER
	a_2 = q * q;

	kalman->Q->elem[0][0] = (a_2*t*t*t)/3;
	kalman->Q->elem[1][1] = (a_2*t*t*t)/3;
	kalman->Q->elem[2][2] = (a_2*t);
	kalman->Q->elem[3][3] = (a_2*t);

	kalman->Q->elem[0][2] = (a_2*t*t)/2;
	kalman->Q->elem[1][3] = (a_2*t*t)/2;
	kalman->Q->elem[2][0] = (a_2*t*t)/2;
	kalman->Q->elem[3][1] = (a_2*t*t)/2;
#endif
}

int kalman_change_Q_and_R(kalman_t *kalman, double new_q, double new_r)
{
	register int i, j;

#ifdef KOHLER
	float a_2;
	float t = 40.0;

	printf("Test2\n");
	if (kalman->Q->elem[0][2] > 0.0) {
	    t = (3/2) * (kalman->Q->elem[0][0]/kalman->Q->elem[0][2]);
	    printf("\n t: %f\n",t);
	}
	a_2 = new_q * new_q;

	kalman->Q->elem[0][0] = (a_2*t*t*t)/3;
	kalman->Q->elem[1][1] = (a_2*t*t*t)/3;
	kalman->Q->elem[2][2] = (a_2*t);
	kalman->Q->elem[3][3] = (a_2*t);

	kalman->Q->elem[0][2] = (a_2*t*t)/2;
	kalman->Q->elem[1][3] = (a_2*t*t)/2;
	kalman->Q->elem[2][0] = (a_2*t*t)/2;
	kalman->Q->elem[3][1] = (a_2*t*t)/2;
#else
	for (i=0; i < kalman->Q->rows; i++)
		for (j=0; j < kalman->Q->cols; j++)
			kalman->Q->elem[i][j] = (i==j) ? new_q : 0.0;
#endif

	for (i=0; i < kalman->R->rows; i++)
		for (j=0; j < kalman->R->cols; j++)
			kalman->R->elem[i][j] = (i==j) ? new_r : 0.0;

	return 0;
}

/*
execution of Kalman calculation:

       P + H'
K = --------------
    H * P * H' + R

x_correct = x + K * (z - H * x)

P_correct = (I - K * H) * P
*/
int kalman_measurementUpdate(kalman_t *kalman)
{
	matrix_t *erg1, *erg2, *erg3;

	erg1 = iw_multMatrix(kalman->H, kalman->P_predict);
	erg2 = iw_multMatrix(erg1, kalman->H_trans);
	iw_add_matrix(erg2, kalman->R, erg1);
	iw_invert_matrix(erg1, erg2);
	erg3 = iw_multMatrix(kalman->P_predict, kalman->H_trans);
	iw_mult_matrix(erg3, erg2, erg1);
	iw_assign_matrix(erg1, kalman->K);

	iw_mult_matrix(kalman->H, kalman->x_predict, erg1);
	iw_sub_matrix(kalman->z, erg1, erg2);
	iw_mult_matrix(kalman->K, erg2, erg1);
	iw_add_matrix(kalman->x_predict, erg1, erg2);
	iw_assign_matrix(erg2, kalman->x_correct);

	iw_mult_matrix(kalman->K, kalman->H, erg1);
	iw_sub_matrix(kalman->I, erg1, erg2);
	iw_mult_matrix(erg2, kalman->P_predict, erg1);
	iw_assign_matrix(erg1, kalman->P_correct);

	iw_free_matrix(erg1);
	iw_free_matrix(erg2);
	iw_free_matrix(erg3);

	return 0;
}

/*
x_predict = A * x

P_predict = A * P * A' + Q

*/

int kalman_timeUpdate(kalman_t *kalman)
{
	matrix_t *erg1, *erg2;

	erg1 = iw_multMatrix(kalman->A, kalman->x_correct);
	iw_assign_matrix(erg1, kalman->x_predict);

	iw_mult_matrix(kalman->A, kalman->P_correct, erg1);
	erg2 = iw_multMatrix(erg1, kalman->A_trans);
	iw_add_matrix(erg2, kalman->Q, erg1);
	iw_assign_matrix(erg1, kalman->P_predict);

	iw_free_matrix(erg1);
	iw_free_matrix(erg2);

	return 0;
}
