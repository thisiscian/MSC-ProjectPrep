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
#include <math.h>
#include "matrix.h"

matrix_t *iw_create_matrix (int rows, int cols)
{
	int i;

	matrix_t *m = (matrix_t*) malloc(sizeof(matrix_t));
	m->rows = rows;
	m->cols = cols;
	m->elem = (double**) malloc(sizeof(double*)*rows);
	for (i=0;i<rows;i++)
		m->elem[i] = (double*) malloc(sizeof(double)*cols);

	return m;
}

void iw_free_matrix (matrix_t *m)
{
	int i;

	for (i=0; i<m->rows; i++)
		free (m->elem[i]);
	free (m->elem);
	free (m);
	m = NULL;
}

void iw_changeDim_matrix (matrix_t *m, int rows, int cols)
{
	int i;

	for (i=0; i<m->rows; i++)
		free(m->elem[i]);
	free(m->elem);

	m->rows = rows;
	m->cols = cols;
	m->elem = (double**) malloc(sizeof(double*)*rows);
	for (i=0;i<rows;i++)
		m->elem[i] = (double*) malloc(sizeof(double)*cols);
}

void iw_init_matrix (matrix_t *m)
{
	int i,j ;

	for (i=0; i < m->rows; i++)
		for (j=0; j < m->cols; j++)
			m->elem[i][j] = 0.0;
}

matrix_t *iw_copyMatrix (matrix_t *m)
{
	int i, j;
	matrix_t *res;

	res = iw_create_matrix(m->rows,m->cols);

	for (i=0; i < m->rows; i++)
		for (j=0; j < m->cols; j++)
			res->elem[i][j] = m->elem[i][j];

	return res;
}

int iw_assign_matrix (matrix_t *m1, matrix_t *m2)
{
	int i, j;

	if ((m1->cols == m2->cols) && (m1->rows == m2->rows)) {
		for (i=0; i < m1->rows; i++)
			for (j=0; j < m1->cols; j++)
				m2->elem[i][j] = m1->elem[i][j];
		return 1;
	} else {
		printf ("Matrizen passen dimensionsmaessig nicht (assign_matrix) \n");
		return 0;
	}
}

matrix_t *iw_addMatrix (matrix_t *m1, matrix_t *m2)
{
	matrix_t *res = iw_create_matrix (m1->rows, m1->cols);

	if (iw_add_matrix (m1, m2, res)) {
		return res;
	} else {
		iw_free_matrix (res);
		return NULL;
	}
}

int iw_add_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res)
{
	int i, j;

	if (m1->cols != m2->cols || m1->rows != m2->rows) {
		printf("Matrizen passen dimensionsmaessig nicht (add_matrix)\n");
		return 0;
	} else {
		if (res->rows != m1->rows || res->cols != m1->cols)
			iw_changeDim_matrix (res, m1->rows, m1->cols);

		for (i=0; i < m1->rows; i++)
			for (j=0; j < m1->cols; j++)
				res->elem[i][j] = m1->elem[i][j] + m2->elem[i][j];
		return 1;
	}
}

matrix_t *iw_subMatrix (matrix_t *m1, matrix_t *m2)
{
	matrix_t *res = iw_create_matrix(m1->rows, m1->cols);

	if (iw_sub_matrix (m1, m2, res)) {
		return res;
	} else {
		iw_free_matrix (res);
		return NULL;
	}
}

int iw_sub_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res)
{
	int i, j;

	if (m1->cols != m2->cols || m1->rows != m2->rows) {
		printf ("Matrizen passen dimensionsmaessig nicht (sub_matrix)\n");
		return 0;
	} else {
		if (res->rows != m1->rows || res->cols != m1->cols)
			iw_changeDim_matrix(res, m1->rows, m1->cols);

		for (i=0; i < m1->rows; i++)
			for (j=0; j < m1->cols; j++)
				res->elem[i][j] = m1->elem[i][j] - m2->elem[i][j];
		return 1;
	}
}

matrix_t *iw_multMatrix (matrix_t *m1, matrix_t *m2)
{
	matrix_t *res = iw_create_matrix (m1->rows, m2->cols);

	if (iw_mult_matrix (m1, m2, res)) {
		return res;
	} else {
		iw_free_matrix (res);
		return NULL;
	}
}

int iw_mult_matrix (matrix_t *m1, matrix_t *m2, matrix_t *res)
{
	int i, j, k;

	if (m1->cols != m2->rows) {
		printf("Matrizen passen dimensionsmaessig nicht (mult_matrix)\n");
		return 0;
	} else {
		if (res->rows != m1->rows || res->cols != m2->cols)
			iw_changeDim_matrix (res, m1->rows, m2->cols);

		iw_init_matrix(res);
		for (i=0; i<m1->rows; i++) {
			for (j=0; j<m2->cols; j++) {
				for (k=0; k<m1->cols; k++) {
					res->elem[i][j] += m1->elem[i][k] * m2->elem[k][j];
				}
			}
		}
		return 1;
	}
}

matrix_t *iw_transposeMatrix (matrix_t *m)
{
	matrix_t *res = iw_create_matrix(m->cols, m->rows);
	iw_transpose_matrix (m, res);
	return res;
}

void iw_transpose_matrix (matrix_t *m, matrix_t *res)
{
	int i, j;

	if (res->cols != m->rows || res->rows != m->cols)
		iw_changeDim_matrix(res, m->cols, m->rows);

	for (i=0; i<m->rows; i++) {
		for (j=0; j<m->cols; j++) {
			res->elem[j][i] = m->elem[i][j];
		}
	}
}

void iw_print_matrix (matrix_t *m)
{
	int i, j;

	for (i=0; i < m->rows; i++) {
		for (j=0; j < m->cols; j++)
			printf("%6.3f ", m->elem[i][j]);
		printf("\n");
	}
	printf("\n");
}

/* Der Gauss-Jordan-Algorithmus aus Numerical Recipes */
matrix_t *iw_invertMatrix (matrix_t *m)
{
	matrix_t *a = iw_create_matrix (m->rows, m->rows);

	if (iw_invert_matrix (m, a)) {
		return a;
	} else {
		iw_free_matrix (a);
		return NULL;
	}
}

int iw_invert_matrix (matrix_t *m, matrix_t *a)
{
	int res = 0;
	int n = m->rows;
#define SWAP(a,b) {swap=(a); (a)=(b); (b)=swap;}
	int *indxc, *indxr, *ipiv;
	register int j;
	register int i;
	register int l;
	register int ll;
	int icol=0, irow=0, k;
	double big, dum, pivinv, swap;

	if (m->rows == m->cols) {
		indxc = malloc (sizeof (int) * (n + 1));
		indxr = malloc (sizeof (int) * (n + 1));
		ipiv = malloc (sizeof (int) * (n + 1));

		if (a->rows != n || a->cols != n)
			iw_changeDim_matrix(a,n,n);

		for (i=0; i<n; i++)
			for (j=0; j<n; j++)
				a->elem[i][j] = m->elem[i][j];

		for (j = 0; j < n; j++)
			ipiv[j] = 0;
		for (i = 0; i < n; i++) {
			big = 0.0;
			for (j = 0; j < n; j++)
				if (ipiv[j] != 1)
					for (k = 0; k < n; k++) {
						if (ipiv[k] == 0) {
							if (fabs (a->elem[j][k]) >= big) {
								big = fabs (a->elem[j][k]);
								irow = j;
								icol = k;
							}
						} else if (ipiv[k] > 1) {
							fprintf (stderr, "gaussj: Singular Matrix-1\n");
							goto cleanup;
						}
					}
			++(ipiv[icol]);
			if (irow != icol) {
				for (l = 0; l < n; l++)
					SWAP (a->elem[irow][l], a->elem[icol][l]);
			}
			indxr[i] = irow;
			indxc[i] = icol;
			if (a->elem[icol][icol] == 0.0) {
				fprintf (stderr, "gaussj: Singular Matrix-2\n");
				goto cleanup;
			}
			pivinv = 1.0 / a->elem[icol][icol];
			a->elem[icol][icol] = 1.0;
			for (l = 0; l < n; l++)
				a->elem[icol][l] *= pivinv;
			for (ll = 0; ll < n; ll++)
				if (ll != icol) {
					dum = a->elem[ll][icol];
					a->elem[ll][icol] = 0.0;
					for (l = 0; l < n; l++)
						a->elem[ll][l] -= a->elem[icol][l] * dum;
				}
		}
		for (l = n-1; l >= 0; l--) {
			if (indxr[l] != indxc[l])
				for (k = 0; k < n; k++)
					SWAP (a->elem[k][indxr[l]], a->elem[k][indxc[l]]);
		}
#undef SWAP
		res = 1;
	cleanup:
		free (ipiv);
		free (indxr);
		free (indxc);
	}
	return res;
}
