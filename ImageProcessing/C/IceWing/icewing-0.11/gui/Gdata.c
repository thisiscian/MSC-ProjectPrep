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

#include "config.h"
#include <gtk/gtk.h>
#include "tools/tools.h"
#include "Gdata.h"

typedef struct guiEntry {
	char *key;
	void *data;
	guiDataDestroyFunc destroy;
} guiEntry;

static gboolean cb_data_free (gpointer key, gpointer value, gpointer user_data)
{
	guiEntry *entry = value;

	if (entry->destroy) entry->destroy (entry->data);
	g_free (entry->key);
	g_free (entry);
	return TRUE;
}

/*********************************************************************
  Free 'data' and all associated objdata.
*********************************************************************/
void gui_data_free (guiData data)
{
	if (!data) return;

	g_hash_table_foreach_remove (data, cb_data_free, NULL);
	g_hash_table_destroy (data);
}

/*********************************************************************
  Free the data and the list entry associated with 'key'.
*********************************************************************/
void gui_data_remove (guiData data, const char *key)
{
	guiEntry *entry;

	if (!data) return;

	entry = g_hash_table_lookup (data, key);
	if (entry) {
		g_hash_table_remove (data, key);
		if (entry->destroy) entry->destroy (entry->data);
		g_free (entry->key);
		g_free (entry);
	}
}

/*********************************************************************
  Get the data associated with "key", return the address of the
  stored data.
*********************************************************************/
void **gui_data_get (guiData data, const char *key)
{
	guiEntry *entry;

	if (!data) return NULL;

	entry = g_hash_table_lookup (data, key);
	if (entry)
		return &entry->data;
	else
		return NULL;
}

/*********************************************************************
  Append 'objdata' to 'data'. The data is indexed by 'key'. If there
  is already data associated with 'key' the new data will replace it.
  'destroy' is called when the data is removed.
  Return the address of the stored 'objdata'.
*********************************************************************/
void** gui_data_set (guiData *data, const char *key, void *objdata,
					 guiDataDestroyFunc destroy)
{
	guiEntry *entry = NULL;
	GHashTable *hash;

	iw_assert (data!=NULL, "No data given in gui_data_set()");

	hash = *data;
	if (!hash) {
		*data = hash = g_hash_table_new (g_str_hash, g_str_equal);
	} else {
		entry = g_hash_table_lookup (hash, key);
		if (entry && entry->destroy)
			entry->destroy (entry->data);
	}
	if (!entry) {
		entry = malloc (sizeof(guiEntry));
		entry->key = g_strdup (key);
		g_hash_table_insert (hash, entry->key, entry);
	}
	entry->destroy = destroy;
	entry->data = objdata;

	return &entry->data;
}
