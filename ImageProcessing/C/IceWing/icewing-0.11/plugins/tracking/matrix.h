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

#ifndef iw_matrix_H
#define iw_matrix_H

typedef struct matrix_t {
	int     rows;
	int     cols;
	double **elem;
} matrix_t;

matrix_t *iw_create_matrix (int rows, int cols);

void iw_free_matrix (matrix_t *m);

void iw_changeDim_matrix (matrix_t *m, int rows, int cols);

void iw_init_matrix (matrix_t *m);

int iw_assign_matrix (matrix_t *m1, matrix_t *m2);

int iw_add_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res);

int iw_sub_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res);

int iw_mult_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res);

void iw_transpose_matrix (matrix_t *m, matrix_t *res);

void iw_print_matrix (matrix_t *m);

int iw_invert_matrix (matrix_t *m, matrix_t *a);

/* Funktionen wie oben, fuer das Ergebnis wird jedoch
   Speicher alloziert und Zeiger darauf zurueckgegeben */

matrix_t *iw_copyMatrix (matrix_t *m);

matrix_t *iw_addMatrix (matrix_t *m1, matrix_t *m2);

matrix_t *iw_subMatrix (matrix_t *m1, matrix_t *m2);

matrix_t *iw_multMatrix (matrix_t *m1, matrix_t *m2);

matrix_t *iw_transposeMatrix (matrix_t *m);

matrix_t *iw_invertMatrix (matrix_t *m);

#endif
