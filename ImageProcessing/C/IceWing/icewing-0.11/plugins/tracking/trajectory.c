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

#include "trajectory.h"

#define __IFSTATUSSUCCESS         if (status != NDR_SUCCESS) return(status)

void trajectory_destroy (Trajectory_t *traject)
{
	int i;

	if (!traject) return;

	for (i=0; i<traject->n_pptRelPos; i++)
		dfree (traject->pptRelPos[i]);
	dfree (traject->pptRelPos);

	for (i=0; i<traject->n_pptAbsPos; i++)
		dfree (traject->pptAbsPos[i]);
	dfree (traject->pptAbsPos);

	dfree (traject);
}

static Position_t *position_create (void)
{
	Position_t *pos = (Position_t *) dalloc(sizeof(Position_t));
	pos->x = 0.0;
	pos->y = 0.0;
	return pos;
}

Trajectory_t *trajectory_alloc (int len)
{
	Trajectory_t *traject;
	int i;

	traject = (Trajectory_t *) dalloc (sizeof(Trajectory_t));

	traject->n_pptRelPos = len;
	traject->pptRelPos = (Position_t**) dzalloc (sizeof(Position_t*)*len);

	for (i=0; i<traject->n_pptRelPos; i++)
		traject->pptRelPos[i] = (Position_t*) position_create();

	traject->n_pptAbsPos = len;
	traject->pptAbsPos = (Position_t**) dzalloc (sizeof(Position_t*)*len);

	for (i=0; i<traject->n_pptAbsPos; i++)
		traject->pptAbsPos[i] = (Position_t*) position_create();

	return traject;
}

static NDRstatus_t ndr_Position (NDRS *ndrs, Position_t **obj)
{
	NDRstatus_t   status;

	status = ndr_open (ndrs, (void **)obj);
	if (status == NDR_EOS)
		return (NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter (ndrs, (void **)obj, sizeof(Position_t));
	__IFSTATUSSUCCESS;

	status = ndr_float (ndrs, (float *)&((*obj)->x));
	__IFSTATUSSUCCESS;

	status = ndr_pair (ndrs);
	__IFSTATUSSUCCESS;

	status = ndr_float (ndrs, (float *)&((*obj)->y));
	__IFSTATUSSUCCESS;

	status = ndr_nil (ndrs);
	__IFSTATUSSUCCESS;

	return (_ndr__leave (ndrs, (void **) obj));
}

NDRstatus_t ndr_Trajectory (NDRS *ndrs, Trajectory_t **obj)
{
	NDRstatus_t status;

	status = ndr_open (ndrs, (void **)obj);
	if (status == NDR_EOS)
		return (NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter (ndrs, (void **)obj, sizeof(Trajectory_t));
	__IFSTATUSSUCCESS;

	status = ndr_list (ndrs, &((*obj)->n_pptRelPos),
					   (NDRfunction_t *)ndr_Position, (void ***)(void*)&((*obj)->pptRelPos));
	__IFSTATUSSUCCESS;

	status = ndr_pair (ndrs);
	__IFSTATUSSUCCESS;

	status = ndr_list (ndrs, &((*obj)->n_pptAbsPos),
					   (NDRfunction_t *)ndr_Position, (void ***)(void*)&((*obj)->pptAbsPos));
	__IFSTATUSSUCCESS;

	status = ndr_nil (ndrs);
	__IFSTATUSSUCCESS;

	return (_ndr__leave (ndrs, (void **)obj));
}
