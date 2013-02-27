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

#ifndef iw_handtrack_H
#define iw_handtrack_H

#include "main/sfb_iw.h"
#include "main/region.h"
#include "main/plugin.h"

/*********************************************************************
  Return TRUE if tracking filter has good judgement.
*********************************************************************/
BOOL track_is_valid (float judgement);

/*********************************************************************
  Clear all kalman filters.
*********************************************************************/
void track_clearAll (void);

/*********************************************************************
  Do a kalman tracking loop and output the reslts on the stream
  opened by track_init().
*********************************************************************/
void track_do_tracking (iwRegion *regions, int numregs, grabImageData *gimg);

/*********************************************************************
  Receive region hypotheses from the stream <source_region> and track
  them. This function does not return.
*********************************************************************/
void track_receive_regions (void);

#endif /* iw_handtrack_H */
