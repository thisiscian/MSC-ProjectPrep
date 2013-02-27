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

#ifndef iw_plugin_comm_H
#define iw_plugin_comm_H

#include "plugin.h"

/* Use default priority values */
#define PLUG_PRIORITY_DEFAULT	(-1)
/* Always at end of list */
#define PLUG_PRIORITY_MIN		0
/* Always at start of list */
#define PLUG_PRIORITY_MAX		1000

/* Called if data stored with plug_data_set() should be released */
typedef void (*plugDataDestroyFunc) (void *data);
typedef void (*plugFunc) ();

/* Stored data, see e.g. plug_data_get(). */
typedef struct plugData {
	plugDefinition *plug;	/*  Plugin which stored the data */
	char *ident;			/*  ID under which the data was stored */
	void *data;				/*  The actual data */
} plugData;

/* Stored function, see e.g. plug_function_get() */
typedef struct plugDataFunc {
	plugDefinition *plug;	/*  Plugin which stored the function */
	char *ident;			/*  ID under which the function was stored */
	plugFunc func;			/*  The actual function */
} plugDataFunc;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Data interface:
 *   Store/Retrieve any data. Allows to exchange data between plugins.
 */

/*********************************************************************
  Store refcounted data under the ID ident. plug is the plugin from
  where this function is called.
  destroy is called if the refcount drops to 0.
*********************************************************************/
void plug_data_set (plugDefinition *plug, const char *ident, void *data,
					plugDataDestroyFunc destroy);

/*********************************************************************
  By default, even if refcount of data drops to 0, it is deleted not
  until the end of the current mainloop run. This changes it to delete
  data immediately if refcount drops to 0. If unsure, don't use it.
*********************************************************************/
void plug_data_sink (plugData *data);

/*********************************************************************
  Retrieve data stored under the ID 'ident'.
  The refcount of 'data' is increased.
  data     : If != NULL the data stored after 'data' under 'ident'
             is returned, otherwise the first data element with an
             ID of 'ident.
  _new() / onlynew: Get only data which was added after the
             last restart of the mainloop.
  plug_name: Get data stored by a plugin by this name.
*********************************************************************/
plugData* plug_data_get (const char *ident, plugData *data);
plugData* plug_data_get_new (const char *ident, plugData *data);
plugData* plug_data_get_full (const char *ident, plugData *data,
							  BOOL onlynew, const char *plug_name);

/*********************************************************************
  Increase the refcount of 'data'. plug_data_unget() must be called
  to drop the refcount.
*********************************************************************/
void plug_data_ref (plugData *data);

/*********************************************************************
  Drop refcount of data.
*********************************************************************/
void plug_data_unget (plugData *data);

/*********************************************************************
  Returns TRUE if there is a plugin which observes 'ident'.
*********************************************************************/
BOOL plug_data_is_observed (const char *ident);

/*
 * Observer interface:
 *   Install/Remove observer on data set with plug_data_set().
 *   The observer are called from the main loop if new data with the
 *   correct ident is stored via plug_data_set().
 *
 * Two points to remember:
 *    1. Only old Data (-> data set before the current mainloop run
 *       started) with ID exists after restarting the mainloop:
 *         Observer for ID are NOT called.
 *    2. MainLoop processes currently ID1 and new data with ID2 != ID1 is set:
 *         Call all ID2-observer before restarting the mainloop,
 *         if the new data is NOT deleted in the meantime
 */

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
void plug_observ_data (plugDefinition *plug, const char *ident);
void plug_observ_data_priority (plugDefinition *plug, const char *ident, int pri);

/*********************************************************************
  Remove an observer set with plug_observ_data().
  ident == "id"       : Remove the observer.
  ident == "id()"     : Remove the observer and any dependent plugins
        in the plugin names string (see plug_add_default_page()).
  ident == "id(plugs)": Remove the specified plugins from the plugin
        names string (see plug_add_default_page()) and the observer,
        if no dependent plugins in the plugin names string are left.
*********************************************************************/
void plug_observ_data_remove (plugDefinition *plug, const char *ident);

/*
 * Function interface:
 *   Store / Retrieve pointer to functions. Allows to call functions
 *   of other plugins. Please use the observer interface instead.
 */

/*********************************************************************
  Register a function under the ID 'ident'. plug is the plugin from
  where this function is called.
*********************************************************************/
void plug_function_register (plugDefinition *plug, const char *ident,
							 plugFunc func);

/*********************************************************************
  Unregister a function registered with plug_function_register().
*********************************************************************/
void plug_function_unregister (plugDefinition *plug, const char *ident);

/*********************************************************************
  Get function stored under the ID 'ident'. If 'func'!=NULL the
  function stored after 'func' under 'ident' is returned.

  plug_name: Get function stored by a plugin by this name.
*********************************************************************/
plugDataFunc* plug_function_get (const char *ident, plugDataFunc *func);
plugDataFunc* plug_function_get_full (const char *ident, plugDataFunc *func,
									  const char *plug_name);

#ifdef __cplusplus
}
#endif

#endif /* iw_plugin_comm_H */
