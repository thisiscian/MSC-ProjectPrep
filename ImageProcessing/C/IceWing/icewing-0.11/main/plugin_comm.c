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
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include "plugin_i.h"
#include "plugin_comm.h"
#include "gui/Goptions.h"

/*
  MainLoop processes currently ID1:
    plug_data_set(ID2)
        -> ID2 not in observer       : append at the end
        -> ID2 before ID1 in observer: move to the end
    plug_data_destroy(ID2)
        -> observer->data_count--
*/

	/* List of all plugin's observing one 'ident' */
typedef struct plugObservPlugins {
	plugPlugin *plug;
	int priority;
	struct plugObservPlugins *next;
} plugObservPlugins;

	/* List of all observer */
typedef struct plugObservList {
	char *ident;
	int data_count;				/* Number of data's set with _data_set() */
	int rank;					/* Smaller -> nearer to the start of the list */
	BOOL new;					/* New data since last restart of the mainloop ? */
	BOOL reorder;
	plugObservPlugins *plugs;
	struct plugObservList *next;
} plugObservList;

	/* List of all data's with one 'ident' set with plug_data_set() */
typedef struct plugDataList {
	plugData data;
	BOOL new;					/* Added since last restart of the mainloop ? */
	plugObservList *observer;	/* Observer with the same ident */
	int ref_count;
	BOOL floating;				/* TRUE: deletion is delayed until the end
                                         of the current mainloop run */
	plugDataDestroyFunc destroy;
	struct plugDataList *next;
} plugDataList;

	/* List of all registered functions with one 'ident' */
typedef struct plugDataFuncList {
	plugDataFunc func;
	plugPlugin *plug;
	struct plugDataFuncList *next;
} plugDataFuncList;

typedef struct plugMainFuncEntry {	/* Infos for a function called from the main loop */
	plugMainFunc func;
	void *data;					/* Data passed to the function */
	BOOL before;				/* Call before oder after the main_iteration */
} plugMainFuncEntry;

	/* All data stored with plug_data_set() */
static GHashTable *p_data = NULL;
static pthread_mutex_t p_data_mutex = PTHREAD_MUTEX_INITIALIZER;

	/* All functions registered with plug_function_register() */
static GHashTable *p_func = NULL;
static pthread_mutex_t p_func_mutex = PTHREAD_MUTEX_INITIALIZER;

	/* List of all observer */
static plugObservList *observer = NULL;
	/* Hash table for fast ident-based access of the observer-list */
static GHashTable *observer_hash = NULL;
	/* Current position in the main loop */
static plugObservList *observer_main = NULL;
static pthread_mutex_t p_observer_mutex = PTHREAD_MUTEX_INITIALIZER;

	/* Set with plug_main_register_before/after(),
	   called at start of main loop/before deleting any floating data */
static GSList *p_mainfunc = NULL;
static pthread_mutex_t p_mainfunc_mutex = PTHREAD_MUTEX_INITIALIZER;

static plugLogFunc p_logfunc = NULL;
static void *p_logdata = NULL;

#define PLUGLOG	if (p_logfunc) p_logfunc

/*********************************************************************
  Set the logging function. If !=NULL it is called from different
  plug-functions with data as the first argument.
*********************************************************************/
void plug_log_register (plugLogFunc func, void *data)
{
	p_logfunc = func;
	p_logdata = data;
}

/*********************************************************************
  Create a new observer, init 'observer' and 'observer_hash', and
  append it after ob_prev (if != NULL).
*********************************************************************/
static plugObservList *observer_new (const char *ident, plugObservList *ob_prev)
{
	plugObservList *observ = calloc (1, sizeof(plugObservList));

	observ->ident = strdup (ident);
	if (!observer)
		observer = observ;
	if (ob_prev) {
		ob_prev->next = observ;
		observ->rank = ob_prev->rank+1;
	}
	if (!observer_hash)
		observer_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (observer_hash, observ->ident, observ);

	return observ;
}

/*********************************************************************
  Remove the entry 'observ' from the observer list.
*********************************************************************/
static void observer_clear_one (plugObservList *observ)
{
	plugObservList *ob_prev = NULL;

	if (observ->data_count != 0 || observ->plugs != NULL)
		return;

	if (observ != observer)
		for (ob_prev = observer; ob_prev->next != observ;
			 ob_prev = ob_prev->next) /* empty */;

	if (observ == observer_main)
		observer_main = ob_prev;

	if (ob_prev)
		ob_prev->next = observ->next;
	else
		observer = observ->next;
	g_hash_table_remove (observer_hash, observ->ident);
	if (observ->ident)
		free (observ->ident);
	free (observ);
}

/*********************************************************************
  Remove all 'empty' entries from the observer list.
*********************************************************************/
static void observer_clear (void)
{
	plugObservList *observ, *ob_prev, *ob_cur;

	ob_prev = NULL;
	observ = observer;
	while (observ) {
		if (observ->data_count == 0 && observ->plugs == NULL) {
			ob_cur = observ;
			observ = observ->next;

			if (ob_cur == observer_main)
				observer_main = ob_prev;

			if (ob_prev)
				ob_prev->next = ob_cur->next;
			else
				observer = ob_cur->next;
			g_hash_table_remove (observer_hash, ob_cur->ident);
			if (ob_cur->ident)
				free (ob_cur->ident);
			free (ob_cur);
		} else {
			ob_prev = observ;
			observ = observ->next;
		}
	}
}

/*********************************************************************
  Free a plugDataList instance.
*********************************************************************/
static void plug_data_free (plugDataList *ldata)
{
	ldata->observer->data_count--;
	if (ldata->destroy)
		ldata->destroy (((plugData*)ldata)->data);
	free (ldata->data.ident);
	free (ldata);
}

/*********************************************************************
  Test if ref_count of ldata is below 1 and remove ldata if so.
*********************************************************************/
static void plug_data_test_destroy (plugDataList *ldata)
{
	plugDataList *list;

	if (ldata->ref_count > 0) return;

	iw_assert (p_data!=NULL, "No hashtable in plug_data_test_destroy()");

	list = g_hash_table_lookup (p_data, ldata->data.ident);

	if (list == ldata) {
		/* First entry in list of datas with identical ident's
		   -> update hash table entry */
		g_hash_table_remove (p_data, ldata->data.ident);
		if (ldata->next)
			g_hash_table_insert (p_data, ldata->data.ident, ldata->next);
	} else {
		while (list && list->next != ldata)
			list = list->next;
		if (list)
			list->next = ldata->next;
	}

	plug_data_free (ldata);
}

/*********************************************************************
  Callback for g_hash_table_foreach_remove(p_data, ...).
  Remove all data entries from the list value with a refcount of 0.
  list: A list of all entries which must be readded to p_data.
*********************************************************************/
static BOOL cb_plug_data_clear (gpointer key, gpointer value, gpointer list)
{
	BOOL delete = FALSE;
	plugDataList *ldata, *pos  = value, *first = NULL, *prev;

	prev = NULL;
	while (pos) {
		ldata = pos;
		pos = pos->next;

		if (ldata->ref_count <= 0) {
			if (ldata == value)
				delete = TRUE;
			if (prev)
				prev->next = ldata->next;
			plug_data_free (ldata);
		} else {
			ldata->new = FALSE;
			if (!first)
				first = ldata;
			prev = ldata;
		}
	}
	if (delete && first) {
		GSList **l = list;
		*l = g_slist_prepend (*l, first);
	}
	return delete;
}
static BOOL cb_plug_data_clear_all (gpointer key, gpointer value, gpointer list)
{
	plugDataList *ldata, *pos  = value;

	while (pos) {
		ldata = pos;
		pos = pos->next;
		plug_data_free (ldata);
	}
	return TRUE;
}

/*********************************************************************
  Remove all data entries (all==TRUE) or all data entries with a
  refcount of 0 (all==FALSE).
*********************************************************************/
static void plug_data_clear (BOOL all)
{
	GSList *list = NULL, *pos;
	plugObservList *observ;

	if (p_data) {
		if (all)
			g_hash_table_foreach_remove (p_data, cb_plug_data_clear_all, NULL);
		else
			g_hash_table_foreach_remove (p_data, cb_plug_data_clear, &list);

		/* Re-add all list's where at least the first
		   entry of the list was freed */
		for (pos = list; pos; pos = pos->next) {
			plugDataList *ldata = pos->data;
			g_hash_table_insert (p_data, ldata->data.ident, ldata);
		}
		g_slist_free (list);
	}

	pthread_mutex_lock (&p_observer_mutex);
	for (observ = observer; observ; observ = observ->next)
		observ->new = FALSE;
	pthread_mutex_unlock (&p_observer_mutex);
}

/*********************************************************************
  Call func at the start of the main loop or at the end before
  cleaning any floating data. An id is returned for unregistering the
  function. If no data is passed to ..._register_...(), the id is
  passed to func instead of the data.
*********************************************************************/
static void* plug_main_register (plugMainFunc func, void *data, BOOL before)
{
	plugMainFuncEntry *entry = malloc (sizeof(plugMainFuncEntry));
	entry->func = func;
	entry->data = data;
	entry->before = before;

	pthread_mutex_lock (&p_mainfunc_mutex);

	p_mainfunc = g_slist_prepend (p_mainfunc, entry);

	pthread_mutex_unlock (&p_mainfunc_mutex);
	return entry;
}
void* plug_main_register_before (plugMainFunc func, void *data)
{
	return plug_main_register (func, data, TRUE);
}
void* plug_main_register_after (plugMainFunc func, void *data)
{
	return plug_main_register (func, data, FALSE);
}

/* If a function should be unregistered which is currently
   called from plug_main_iterate(), delay its deletion and
   delete in plug_main_iterate(). */
static plugMainFuncEntry *p_mf_current = NULL;
static BOOL p_mf_current_deleted = FALSE;

void plug_main_unregister (void *id)
{
	iw_assert (p_mainfunc!=NULL, "No hashtable in plug_main_unregister()");
	iw_assert (id!=NULL, "No entry with id %p in plug_main_unregister()", id);

	pthread_mutex_lock (&p_mainfunc_mutex);

	if (p_mf_current == id) {
		p_mf_current_deleted = TRUE;
	} else {
		p_mainfunc = g_slist_remove (p_mainfunc, id);
		free (id);
	}
	pthread_mutex_unlock (&p_mainfunc_mutex);
}

/*********************************************************************
  Iterate through all registered functions and call all registered
  with _before() or _after().
*********************************************************************/
static void plug_main_iterate (BOOL before)
{
	GSList *pos;
	plugMainFuncEntry *entry;

	pthread_mutex_lock (&p_mainfunc_mutex);

	pos = p_mainfunc;
	while (pos) {
		entry = pos->data;
		if (entry->before == before && entry->func) {
			/* (un)register may be called from func, so unlock */
			p_mf_current = entry;
			pthread_mutex_unlock (&p_mainfunc_mutex);
			if (entry->data)
				entry->func (entry->data);
			else
				entry->func (entry);
			pthread_mutex_lock (&p_mainfunc_mutex);
			p_mf_current = NULL;
		}
		/* Current entry unregistered? */
		if (p_mf_current_deleted) {
			p_mf_current_deleted = FALSE;
			pos = pos->next;
			p_mainfunc = g_slist_remove (p_mainfunc, entry);
			free (entry);
		} else
			pos = pos->next;
	}

	pthread_mutex_unlock (&p_mainfunc_mutex);
}

/*********************************************************************
  Try to reorder all plugins observing one ident such that the
  dependencies between them are fulfilled (according to the
  plug->plugins entries).
*********************************************************************/
static void observer_reorder (plugObservList *observer)
{
	int o_cnt, iter;
	char *names, *name;
	plugObservPlugins *oplug, *op_prev, *op_p, *op;
	BOOL reordered;

	if (!observer->reorder) return;

	o_cnt = 0;
	oplug = observer->plugs;
	while (oplug) {
		o_cnt++;
		oplug = oplug->next;
	}

	iter = 0;
	op_prev = NULL;
	oplug = observer->plugs;
	while (oplug) {
		reordered = FALSE;
		if ((names = plug_plugins_get_names (oplug->plug->plugins,
											 observer->ident))) {
			/* Move all plugins depending on oplug before oplug. */
			op_p = oplug;
			op = oplug->next;
			while (op) {
				if ((name = strstr (names, op->plug->def->name))) {
					if (*(name-1) == ' ' &&
						*(name+strlen(op->plug->def->name)) == ' ') {
						op_p->next = op->next;
						op->next = oplug;
						if (op_prev)
							op_prev->next = op;
						else
							observer->plugs = op;
						oplug = op;
						op = op_p;
						reordered = TRUE;
					}
				}
				op_p = op;
				op = op->next;
			}
			free (names);
		}
		iter++;
		if (iter > o_cnt*o_cnt) {
			iw_warning ("Unable to resolve dependencies for ident '%s'",
						observer->ident);
			return;
		}
		if (!reordered) {
			op_prev = oplug;
			oplug = oplug->next;
		}
	}

	observer->reorder = FALSE;
}

/*********************************************************************
  Process the observer list and call func for each observer.
*********************************************************************/
void plug_main_iteration (void)
{
	plugObservPlugins *plug, *opl_next;
	plugData *data;
	BOOL cont = TRUE;
	char *ident;

	PLUGLOG (p_logdata, "--------------- mainloop start ---------------\n");

	plug_main_iterate (TRUE);

	pthread_mutex_lock (&p_observer_mutex);

	observer_main = observer;
	while (observer_main && cont) {
		if (observer_main->new) {
			observer_reorder (observer_main);
			plug = observer_main->plugs;
			ident = observer_main->ident;
			data = plug_data_get_new (ident, NULL);
			if (data) {
				while (observer_main && plug && cont) {
					if (!observer_main->data_count) break;
					opl_next = plug->next;

					pthread_mutex_unlock (&p_observer_mutex);
					PLUGLOG (p_logdata, "%s.process (%s)\n",
							 plug->plug->def->name, ident);
					cont = plug_process (plug->plug, ident, data);
					pthread_mutex_lock (&p_observer_mutex);

					plug = opl_next;
				}
			}
			plug_data_unget (data);
		}
		if (observer_main)
			observer_main = observer_main->next;
		else
			observer_main = observer;
	}
	observer_main = NULL;
	observer_clear();

	pthread_mutex_unlock (&p_observer_mutex);

	plug_main_iterate (FALSE);

	pthread_mutex_lock (&p_data_mutex);
	plug_data_clear (FALSE);
	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Store refcounted data under the ID ident. plug is the plugin from
  where this function is called.
  destroy is called if the refcount drops to 0.
*********************************************************************/
void plug_data_set (plugDefinition *plug, const char *ident, void *data,
					plugDataDestroyFunc destroy)
{
	plugDataList *list, *new;

	iw_assert (ident!=NULL, "No ident given in plug_data_set()");
	iw_assert (plug!=NULL, "No plugin given in plug_data_set()");

	PLUGLOG (p_logdata, "data_set (%s %s %p)\n", ident, plug->name, data);

	plug_check_name (ident);

	new = calloc (1, sizeof(plugDataList));
	new->data.plug = plug;
	new->data.ident = strdup (ident);
	new->data.data = data;
	new->new = TRUE;
	new->floating = TRUE;
	new->destroy = destroy;

	pthread_mutex_lock (&p_observer_mutex);
	if (observer_hash)
		new->observer = g_hash_table_lookup (observer_hash, new->data.ident);

	if (!new->observer) {
		/* No oberver for 'ident'?
		   -> Add an empty entry to the observer list */
		plugObservList *last = observer;
		while (last && last->next)
			last = last->next;
		new->observer = observer_new (new->data.ident, last);
	} else if (observer_main) {
		/* 'ident' before currently observer_main in observer list?
		   -> Move to end of observer list.
		      FIXME: What if already at the end of the list? */
		if (new->observer->rank < observer_main->rank) {
			plugObservList *observ = observer, *ob_prev = NULL, *last;
			while (observ != new->observer) {
				ob_prev = observ;
				observ = observ->next;
			}
			if (ob_prev)
				ob_prev->next = observ->next;
			else if (observer->next)
				observer = observer->next;
			last = observer_main;
			while (last->next)
				last = last->next;
			last->next = observ;
			observ->next = NULL;
			observ->rank = last->rank+1;
		}
	}
	new->observer->data_count++;
	new->observer->new = TRUE;
	pthread_mutex_unlock (&p_observer_mutex);

	pthread_mutex_lock (&p_data_mutex);
	if (!p_data)
		p_data = g_hash_table_new (g_str_hash, g_str_equal);

	list = g_hash_table_lookup (p_data, new->data.ident);
	if (!list) {
		g_hash_table_insert (p_data, new->data.ident, new);
	} else {
		while (list->next) list = list->next;
		list->next = new;
	}
	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  By default, even if refcount of data drops to 0, it is deleted not
  until the end of the current mainloop run. This changes it to delete
  data immediately if refcount drops to 0. If unsure, don't use it.
*********************************************************************/
void plug_data_sink (plugData *data)
{
	plugDataList *ldata = (plugDataList *)data;

	pthread_mutex_lock (&p_data_mutex);

	ldata->floating = FALSE;
	plug_data_test_destroy (ldata);

	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Retrieve all datas stored under the ID 'ident' which plugin names
  are equal to an entry in plug_names (space separated list).
  The refcount of 'data' is increased.
  data     : If != NULL the data stored after 'data' under 'ident'
             is returned, otherwise the first entry
  onlynew  : Get only data which was added after the last restart
             of the mainloop.
*********************************************************************/
void plug_data_get_list (const char *ident, plugData *data, BOOL onlynew,
						 const char *plug_names, plugData ***datas,
						 int *d_len, int *d_cnt)
{
	plugDataList *ldata = NULL;
	const char *pnames_end = NULL;
	const char *pname, *pos;
	int pname_len;

	iw_assert (ident!=NULL, "No ident given in plug_data_get()");

	pthread_mutex_lock (&p_data_mutex);

	if (data) {
		ldata = ((plugDataList *)data)->next;
	} else if (p_data) {
		ldata = g_hash_table_lookup (p_data, ident);
	}

	if (plug_names)
		pnames_end = plug_names+strlen(plug_names);

	while (ldata) {
		if (!onlynew || ldata->new) {
			if (plug_names) {
				/* Get data from specified plugins. */
				pname = ldata->data.plug->name;
				pname_len = strlen(pname);

				if ((pos = strstr (plug_names, pname)) &&
					(pos == plug_names || *(pos-1) == ' ') &&
					(pos + pname_len == pnames_end || *(pos + pname_len) == ' ')) {
					ldata->ref_count++;
					ARRAY_RESIZE (*datas, *d_len, *d_cnt);
					(*datas)[(*d_cnt)++] = (plugData*)ldata;
				}
			} else {
				/* Get all data. */
				ldata->ref_count++;
				ARRAY_RESIZE (*datas, *d_len, *d_cnt);
				(*datas)[(*d_cnt)++] = (plugData*)ldata;
			}
		}
		ldata = ldata->next;
	}

	pthread_mutex_unlock (&p_data_mutex);

	PLUGLOG (p_logdata, "data_get_list (%s) = #%d\n",
			 ident, *d_cnt);
}

/*********************************************************************
  Retrieve data stored under the ID 'ident'.
  The refcount of 'data' is increased.
  data     : If != NULL the data stored after 'data' under 'ident'
             is returned, otherwise the first data element with an
             ID of 'ident.
  onlynew  : Get only data which was added after the last restart of
             the mainloop.
  plug_name: Get data stored by a plugin by this name.
*********************************************************************/
plugData* plug_data_get_full (const char *ident, plugData *data,
							  BOOL onlynew, const char *plug_name)
{
	plugDataList *ldata = NULL;

	iw_assert (ident!=NULL, "No ident given in plug_data_get()");

	pthread_mutex_lock (&p_data_mutex);

	if (data) {
		ldata = ((plugDataList *)data)->next;
	} else if (p_data) {
		ldata = g_hash_table_lookup (p_data, ident);
	}
	while (ldata &&
		   ((onlynew && !ldata->new) ||
			(plug_name && strcmp (plug_name, ldata->data.plug->name))))
		ldata = ldata->next;

	if (ldata)
		ldata->ref_count++;

	pthread_mutex_unlock (&p_data_mutex);

	PLUGLOG (p_logdata, "data_get (%s) = %s %p\n", ident,
			 ldata ? ldata->data.plug->name : "NULL",
			 ldata ? ldata->data.data : 0);

	return (plugData*)ldata;
}
plugData* plug_data_get_new (const char *ident, plugData *data)
{
	return plug_data_get_full (ident, data, TRUE, NULL);
}
plugData* plug_data_get (const char *ident, plugData *data)
{
	return plug_data_get_full (ident, data, FALSE, NULL);
}

/*********************************************************************
  Increase the refcount of 'data'. plug_data_unget() must be called
  to drop the refcount.
*********************************************************************/
void plug_data_ref (plugData *data)
{
	if (!data) return;

	PLUGLOG (p_logdata, "data_ref (%s %s %p)\n",
			 data->ident, data->plug->name, data->data);

	pthread_mutex_lock (&p_data_mutex);
	((plugDataList *)data)->ref_count++;
	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Drop refcount of data.
*********************************************************************/
void plug_data_unget (plugData *data)
{
	plugDataList *ldata = (plugDataList *)data;

	if (!ldata) return;

	PLUGLOG (p_logdata, "data_unget (%s %s %p)\n",
			 data->ident, data->plug->name, data->data);

	pthread_mutex_lock (&p_data_mutex);

	ldata->ref_count--;
	if (!ldata->floating)
		plug_data_test_destroy (ldata);

	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Drop refcount of all stored data to 0 and call the destroy functions.
*********************************************************************/
void plug_data_cleanup (void)
{
	pthread_mutex_lock (&p_data_mutex);
	plug_data_clear (TRUE);
	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Returns TRUE if there is a plugin which observes 'ident'.
*********************************************************************/
BOOL plug_data_is_observed (const char *ident)
{
	BOOL ret = FALSE;

	pthread_mutex_lock (&p_observer_mutex);

	if (observer_hash)
		ret = g_hash_table_lookup (observer_hash, ident) != NULL ? TRUE : FALSE;

	pthread_mutex_unlock (&p_observer_mutex);

	return ret;
}

/*********************************************************************
  Mark the observer list of 'ident' for reordering. Done before the
  next main loop iteration.
*********************************************************************/
void plug_observer_reorder (const char *ident)
{
	plugObservList *observ;

	pthread_mutex_lock (&p_observer_mutex);

	if (observer_hash &&
		(observ = g_hash_table_lookup (observer_hash, ident))) {
		observ->reorder = TRUE;
	}

	pthread_mutex_unlock (&p_observer_mutex);
}

/*********************************************************************
  Add an observer for data with the ID 'ident'. plug is the plugin
  from where this function is called. The plugin's proces() function
  will be called if data with the ID 'ident' is available.
  ident == "id"       : Add the observer.
  ident == "id()"     : Add the observer. The proces() function is
        called one after another for all available data elements
        (see plug_add_default_page()).
  ident == "id(plugs)": Add the observer. The proces() function is
        additionally called for data elements from the plugins
        "plugs" (see plug_add_default_page()).
*********************************************************************/
void plug_observ_data_priority (plugDefinition *plug, const char *ident, int pri)
{
	plugObservList *observ = NULL;
	plugObservPlugins *oplug, *opl_prev = NULL;
	const char *plugref;
	char id[PLUG_NAME_LEN];

	iw_assert (ident!=NULL, "No ident given in plug_observ_data()");
	iw_assert (plug!=NULL, "No plugin given in plug_observ_data()");

	PLUGLOG (p_logdata, "observ_data (%s %s)\n", plug->name, ident);

	plug_getcheck_name (ident, id, &plugref);
	if (plugref) {
		/* Prevent that values from default config files overwrite these dependencies */
		char *title = g_strconcat ("variable.", plug->name, "Plugins", NULL);
		opts_defvalue_remove (title);
		g_free (title);

		plugins_add_names (plug_get_by_def(plug), id, plugref);
	}

	pthread_mutex_lock (&p_observer_mutex);

	if (observer_hash)
		observ = g_hash_table_lookup (observer_hash, id);

	/* Add new entry (plugObservPlugins and perhaps plugObservList) */

	if (observ) {
		/* Check if plug is already an observer of id */
		oplug = observ->plugs;
		opl_prev = NULL;
		while (oplug) {
			if (!strcmp (oplug->plug->def->name, plug->name)) {
				if (plugref)
					observ->reorder = TRUE;
				pthread_mutex_unlock (&p_observer_mutex);
				return;
			}
			opl_prev = oplug;
			oplug = oplug->next;
		}
		/* No, add it below */
	} else {
		plugObservList *last = observer;
		while (last && last->next) last = last->next;
		observ = observer_new (id, last);
	}
	observ->reorder = TRUE;

	/* Create new observer entry entry */
	oplug = calloc (1, sizeof(plugObservPlugins));
	oplug->plug = plug_get_by_def (plug);
	if (pri < PLUG_PRIORITY_MIN || pri > PLUG_PRIORITY_MAX) {
		if (oplug->plug->append)
			pri = PLUG_PRIORITY_MIN+1;
		else
			pri = PLUG_PRIORITY_MAX;
	}
	oplug->priority = pri;

	/* Insert oplug accrording to pri in observ->plugs list */
	if (opl_prev) {
		if (pri == PLUG_PRIORITY_MAX || observ->plugs->priority < pri) {
			oplug->next = observ->plugs;
			observ->plugs = oplug;
		} else if (pri == PLUG_PRIORITY_MIN || opl_prev->priority >= pri) {
			opl_prev->next = oplug;
		} else {
			plugObservPlugins *pos = observ->plugs;
			while (pos->priority >= pri &&
				   pos->next && pos->next->priority >= pri)
				pos = pos->next;
			oplug->next = pos->next;;
			pos->next = oplug;
		}
	} else
		observ->plugs = oplug;

	pthread_mutex_unlock (&p_observer_mutex);
}

void plug_observ_data (plugDefinition *plug, const char *ident)
{
	plug_observ_data_priority (plug, ident, PLUG_PRIORITY_DEFAULT);
}

/*********************************************************************
  Remove an observer set with plug_observ_data().
  ident == "id"       : Remove the observer.
  ident == "id()"     : Remove any dependent plugins in the plugin
        names string (see plug_add_default_page()) and the observer.
  ident == "id(plugs)": Remove the specified plugins in the plugin
        names string (see plug_add_default_page()) and the observer,
        if no dependent plugins in the plugin names string are left.
*********************************************************************/
void plug_observ_data_remove (plugDefinition *plug, const char *ident)
{
	plugObservList *observ;
	plugObservPlugins *oplug, *opl_prev;
	const char *plugref;
	char id[PLUG_NAME_LEN];
	BOOL remall;

	PLUGLOG (p_logdata, "observ_data_remove (%s %s)\n", plug->name, ident);

	if (!observer) return;

	plug_getcheck_name (ident, id, &plugref);
	if (plugref)
		remall = plugins_remove_names (plug_get_by_def(plug), id, plugref);
	else
		remall = TRUE;
	if (!remall) return;

	pthread_mutex_lock (&p_observer_mutex);

	observ = g_hash_table_lookup (observer_hash, id);
	if (observ) {
		oplug = observ->plugs;
		opl_prev = NULL;
		while (oplug) {
			if (!strcmp (oplug->plug->def->name, plug->name)) {
				if (opl_prev)
					opl_prev->next = oplug->next;
				else
					observ->plugs = oplug->next;
				free (oplug);
				break;
			}
			opl_prev = oplug;
			oplug = oplug->next;
		}
		observer_clear_one (observ);
	}

	pthread_mutex_unlock (&p_observer_mutex);
}

/*********************************************************************
  Register a function under the ID 'ident'. plug is the plugin from
  where this function is called.
*********************************************************************/
void plug_function_register (plugDefinition *plug, const char *ident, plugFunc func)
{
	plugDataFuncList *list, *new;

	iw_assert (ident!=NULL, "No ident given in plug_function_register()");
	iw_assert (plug!=NULL, "No plugin given in plug_function_register()");

	PLUGLOG (p_logdata, "function_register (%s %s %p)\n", ident, plug->name, func);

	new = malloc (sizeof(plugDataFuncList));
	new->func.plug = plug;
	new->func.ident = strdup (ident);
	new->func.func = func;
	new->plug = plug_get_by_def (plug);
	new->next = NULL;

	pthread_mutex_lock (&p_func_mutex);

	if (!p_func)
		p_func = g_hash_table_new (g_str_hash, g_str_equal);

	list = g_hash_table_lookup (p_func, new->func.ident);
	if (!list) {
		g_hash_table_insert (p_func, new->func.ident, new);
	} else {
		while (list->next) list = list->next;
		list->next = new;
	}
	pthread_mutex_unlock (&p_func_mutex);
}

/*********************************************************************
  Unregister a function registered with plug_function_register().
*********************************************************************/
void plug_function_unregister (plugDefinition *plug, const char *ident)
{
	plugDataFuncList *list, *lfunc;

	PLUGLOG (p_logdata, "function_register (%s %s)\n", ident, plug->name);

	if (!p_func) return;
	pthread_mutex_lock (&p_func_mutex);

	list = g_hash_table_lookup (p_func, ident);
	if (!strcmp (list->func.plug->name, plug->name)) {
		lfunc = list;
		g_hash_table_remove (p_func, ident);
		if (list->next)
			g_hash_table_insert (p_func, list->next->func.ident, list->next);
	} else {
		while (list && list->next &&
			   !strcmp (list->next->func.plug->name, plug->name))
			list = list->next;
		if (list) {
			lfunc = list->next;
			if (lfunc)
				list->next = lfunc->next;
		} else
			lfunc = NULL;
	}

	pthread_mutex_unlock (&p_func_mutex);

	if (lfunc) free (lfunc);
}

/*********************************************************************
  Get function stored under the ID 'ident'. If 'func'!=NULL the
  function stored after 'func' under 'ident' is returned.
  plug_name: Get function stored by a plugin by this name.
*********************************************************************/
plugDataFunc* plug_function_get_full (const char *ident, plugDataFunc *func,
									  const char *plug_name)
{
	plugDataFuncList *lfunc = NULL;

	iw_assert (ident!=NULL, "No ident given in plug_function_get()");

	pthread_mutex_lock (&p_func_mutex);
	if (func) {
		lfunc = ((plugDataFuncList *)func)->next;
	} else if (p_func) {
		lfunc = g_hash_table_lookup (p_func, ident);
	}
	while (lfunc &&
		   (!lfunc->plug->enabled ||
			(plug_name && strcmp (plug_name, lfunc->func.plug->name))))
		lfunc = lfunc->next;
	pthread_mutex_unlock (&p_func_mutex);

	PLUGLOG (p_logdata, "function_get (%s) = %s %p\n", ident,
			 lfunc ? lfunc->func.plug->name : "NULL",
			 lfunc ? lfunc->func.func : 0);

	return (plugDataFunc*)lfunc;
}
plugDataFunc* plug_function_get (const char *ident, plugDataFunc *func)
{
	return plug_function_get_full (ident, func, NULL);
}

/* Parameter for the hash table callbacks in the plug_info_() functions */
typedef struct infoData {
	void *func;
	void *data;
} infoData;

static void cb_plug_info_data (gpointer key, gpointer value, void *data)
{
	plugDataList *list = value;
	infoData *idata = data;
	plugInfoData info;

	info.name = list->data.ident;
	info.active = FALSE;
	info.ref_count = -1;
	info.floating = FALSE;
	info.ident = TRUE;
	((plugInfoDataFunc)idata->func) (&info, idata->data);

	while (list) {
		info.name = list->data.plug->name;
		info.active = list->new;
		info.ref_count = list->ref_count;
		info.floating = list->floating;
		info.ident = FALSE;
		((plugInfoDataFunc)idata->func) (&info, idata->data);
		list = list->next;
	}

}

/*********************************************************************
  Call for all registered data elements the function 'func' with data
  information and 'data' as arguments.
*********************************************************************/
void plug_info_data (plugInfoDataFunc func, void *data)
{
	infoData idata;

	pthread_mutex_lock (&p_data_mutex);

	if (p_data) {
		idata.func = (void*)func;
		idata.data = data;
		g_hash_table_foreach (p_data, cb_plug_info_data, &idata);
	}

	pthread_mutex_unlock (&p_data_mutex);
}

/*********************************************************************
  Call for all registered observer the function 'func' with observer
  information and 'data' as arguments.
*********************************************************************/
void plug_info_observer (plugInfoObserverFunc func, void *data)
{
	plugInfoObserver info;
	plugObservList *observ;
	plugObservPlugins *oplug;

	pthread_mutex_lock (&p_observer_mutex);
	observ = observer;
	while (observ) {
		info.name = observ->ident;
		info.active = observ->new;
		info.data_count = observ->data_count;
		info.ident = TRUE;
		func (&info, data);

		oplug = observ->plugs;
		while (oplug) {
			info.name = oplug->plug->def->name;
			info.active = FALSE;
			info.data_count = -1;
			info.ident = FALSE;
			func (&info, data);
			oplug = oplug->next;
		}
		observ = observ->next;
	}
	pthread_mutex_unlock (&p_observer_mutex);
}

static void cb_plug_info_func (gpointer key, gpointer value, void *data)
{
	plugDataFuncList *list = value;
	infoData *idata = data;
	plugInfoFunc info;

	info.name = list->func.ident;
	info.ident = TRUE;
	((plugInfoFuncFunc)idata->func) (&info, idata->data);

	while (list) {
		info.name = list->func.plug->name;
		info.ident = FALSE;
		((plugInfoFuncFunc)idata->func) (&info, idata->data);
		list = list->next;
	}

}

/*********************************************************************
  Call for all registered functions the function 'func' with function
  information and 'data' as arguments.
*********************************************************************/
void plug_info_func (plugInfoFuncFunc func, void *data)
{
	infoData idata;
	idata.func = (void*)func;
	idata.data = data;

	pthread_mutex_lock (&p_func_mutex);

	if (p_func)
		g_hash_table_foreach (p_func, cb_plug_info_func, &idata);

	pthread_mutex_unlock (&p_func_mutex);
}
