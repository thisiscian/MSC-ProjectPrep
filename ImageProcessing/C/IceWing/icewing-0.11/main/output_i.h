/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/* PRIVATE HEADER */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Frank Loemker, Applied Computer Science, Faculty of Technology,
 * Bielefeld University, Germany
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

#ifndef iw_output_i_H
#define iw_output_i_H

#include "tools/tools.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Get address of a flag which indicates if anything should be given
  out.
*********************************************************************/
BOOL *iw_output_get_output (void);

/*********************************************************************
  Set name for DACS registration.
*********************************************************************/
void iw_output_set_name (char *name);

/*********************************************************************
  Close any DACS connections.
*********************************************************************/
void iw_output_cleanup (void);

/*********************************************************************
  Init streams and function server for output of regions and images.
*********************************************************************/
void iw_output_init (int output);

#ifdef __cplusplus
}
#endif

#endif /* iw_output_i_H */
