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

#ifndef iw_trajectory_H
#define iw_trajectory_H

#include <dacs.h>

typedef struct Position_t {
	float x;
	float y;
} Position_t;

typedef struct Trajectory_t {
	int n_pptRelPos;					/* Number of ... */
	Position_t **pptRelPos;				/* Positionen, aus denen Trajectory besteht */
	int n_pptAbsPos;					/* Number of ... */
	Position_t **pptAbsPos;				/* Positionen, aus denen Trajectory besteht */
} Trajectory_t;

#ifdef __cplusplus
extern "C" {
#endif

void trajectory_destroy (Trajectory_t *traject);
Trajectory_t *trajectory_alloc (int len);
NDRstatus_t ndr_Trajectory (NDRS *ndrs, Trajectory_t **obj);

#ifdef __cplusplus
}
#endif

#endif /* iw_trajectory_H */
