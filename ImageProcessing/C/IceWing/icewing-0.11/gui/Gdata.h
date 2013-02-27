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

#ifndef iw_Gdata_H
#define iw_Gdata_H

#include <stdlib.h>

typedef void *guiData;

typedef void (*guiDataDestroyFunc) (void *objdata);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Free 'data' and all associated objdata.
*********************************************************************/
void gui_data_free (guiData data);

/*********************************************************************
  Free the data and the list entry associated with 'key'.
*********************************************************************/
void gui_data_remove (guiData data, const char *key);

/*********************************************************************
  Get the data associated with "key", return the address of the
  stored data.
*********************************************************************/
void **gui_data_get (guiData data, const char *key);

/*********************************************************************
  Append 'objdata' to 'data'. The data is indexed by 'key'. If there
  is already data associated with 'key' the new data will replace it.
  'destroy' is called when the data is removed.
  Return the address of the stored 'objdata'.
*********************************************************************/
void** gui_data_set (guiData *data, const char *key, void *objdata,
					 guiDataDestroyFunc destroy);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gdata_H */
