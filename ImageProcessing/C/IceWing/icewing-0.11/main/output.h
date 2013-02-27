/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

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

#ifndef iw_output_H
#define iw_output_H

#include <time.h>
#include "region.h"
#include "grab.h"

/* The typ used in iw_output_hypos() */
#define IW_GHYP_TITLE			"GestureHyp-Regions"

/* Status messages which should be send
   if something is completed or aborted */
#define IW_OUTPUT_STATUS_DONE	"DONE"
#define IW_OUTPUT_STATUS_CANCEL	"CANCEL"

extern DACSentry_t *iw_dacs_entry;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Output a SYNC signal on stream 'stream'.
*********************************************************************/
void iw_output_sync (const char *stream);

/*********************************************************************
  Output 'data' on stream 'stream'.
*********************************************************************/
void iw_output_stream (const char *stream, const void *data);

/*********************************************************************
  Output img as Bild_t on stream 'stream' (!=NULL) or
  DACSNAME_images ('stream'==NULL).
*********************************************************************/
void iw_output_image (const grabImageData *img, const char *stream);

/*********************************************************************
  Output regions as RegionHyp_t on stream 'stream' WITHOUT SYNC
  (data is entered in the Hypothesenkopf) and return the number of
  regions given out.
*********************************************************************/
int iw_output_regions (iwRegion *regions, int nregions,
					   struct timeval time, int img_num, const char *typ,
					   const char *stream);

/*********************************************************************
  Distribute regions as RegionHyp_t on stream 'stream' with time
  'time' and sequenz_nr 'img_num'.
*********************************************************************/
void iw_output_hypos (iwRegion *regions, int nregions,
					  struct timeval time, int img_num, const char *stream);

/*********************************************************************
  Output status message msg on stream DACSNAME_status.
*********************************************************************/
void iw_output_status (const char *msg);

/*********************************************************************
  Register a DACS stream as DACSNAME'suffix' and return a pointer
  to this newly allocated name.
*********************************************************************/
char *iw_output_register_stream (const char *suffix, NDRfunction_t *fkt);

/*********************************************************************
  Register a DACS function as DACSNAME'suffix'.
*********************************************************************/
void iw_output_register_function_with_data (const char *suffix, DACSfunction_t* fkt,
											NDRfunction_t *ndr_in, NDRfunction_t *ndr_out,
											void (*out_kill_fkt)(void*), void *data);
void iw_output_register_function (const char *suffix, DACSfunction_t* fkt,
								  NDRfunction_t *ndr_in, NDRfunction_t *ndr_out,
								  void (*out_kill_fkt)(void*));

/*********************************************************************
  Register with DACS. The other output_register_...() functions call
  this by themselves.
*********************************************************************/
void iw_output_register (void);

#ifdef __cplusplus
}
#endif

#endif /* iw_output_H */
