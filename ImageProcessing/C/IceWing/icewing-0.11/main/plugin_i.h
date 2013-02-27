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

#ifndef iw_plugin_i_H
#define iw_plugin_i_H

#include "plugin.h"

#define ARRAY_RESIZE(array, len, cnt) \
	if ((cnt) >= (len)) { \
		(len) += 10; \
		(array) = realloc ((array), sizeof(void*)*(len)); \
	}

#define PLUG_NAME_LEN		100
#define PLUG_PLUGINS_LEN	256
#define PLUG_BUFFER_LEN		256

typedef struct _plugPlugin {
	plugDefinition *def;
	struct plugLibList *lib;
	BOOL append;		/* Default priority for observed data, from plug_get_info() */
	BOOL enabled;		/* Call process()?, see plug_add_default_page() */
	BOOL init_called : 1;
	BOOL init_options_called : 1;
	BOOL cleanup_called : 1;

	int timer;						/* -1 or result of iw_time_add() */
	int argc;						/* Command line arguments for the plugin */
	char **argv;
	plugPageFlags flags;			/* See plug_add_default_page() */
	char *defname;					/* Name of default page */
	char plugins[PLUG_PLUGINS_LEN];	/* See plug_add_default_page() */
	GtkWidget *p_entry;				/* Entry widget for plugins */

	char buffer[PLUG_BUFFER_LEN];	/* For plug_name() */
} _plugPlugin;

typedef struct plugInfoPlug {	/* Infos for one plugin */
	const char *name;	/* Plugin lib or plugin instance */
	BOOL enabled;
	plugPlugin *plug;	/* NULL -> pluginLib | !=NULL -> pluginInstance */
} plugInfoPlug;

typedef struct plugInfoData {	/* Infos for one data element */
	const char *name;	/* Ident or plugin name */
	BOOL active;		/* New since last restart of the mainloop? */
	int ref_count;
	BOOL floating;
	BOOL ident;			/* Ident or plugin which has stored data under ident? */
} plugInfoData;

typedef struct plugInfoObserver {	/* Infos for one observer */
	const char *name;	/* Ident or plugin name */
	BOOL active;		/* New data under ident stored since last mainloop restart? */
	int data_count;		/* Number of data items stored under ident */
	BOOL ident;			/* Ident or plugin which observes ident? */
} plugInfoObserver;

typedef struct plugInfoFunc {	/* Infos for one function */
	const char *name;	/* Ident or plugin name */
	BOOL ident;			/* Ident or plugin which has stored a function? */
} plugInfoFunc;

typedef void (*plugInfoPlugFunc) (plugInfoPlug *info, void *data);
typedef void (*plugInfoDataFunc) (plugInfoData *info, void *data);
typedef void (*plugInfoObserverFunc) (plugInfoObserver *info, void *data);
typedef void (*plugInfoFuncFunc) (plugInfoFunc *info, void *data);

typedef void (*plugMainFunc) (void *data);
typedef void (*plugLogFunc) (void *data, char* str, ...);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Set the logging function. If !=NULL it is called from different
  plug-functions with data as the first argument.
*********************************************************************/
void plug_log_register (plugLogFunc func, void *data);

/*********************************************************************
  Call for all registered plugins/data/observer/functions the
  function 'func' with information and 'data' as arguments.
*********************************************************************/
void plug_info_plugs (plugInfoPlugFunc func, void *data);
void plug_info_data (plugInfoDataFunc func, void *data);
void plug_info_observer (plugInfoObserverFunc func, void *data);
void plug_info_func (plugInfoFuncFunc func, void *data);

/*********************************************************************
  Mark the observer list of 'ident' for reordering. Done before the
  next main loop iteration.
*********************************************************************/
void plug_observer_reorder (const char *ident);

/*********************************************************************
  Retrieve all datas stored under the ID 'ident' which plugin names
  are equal to an entry in plug_names (space separated list).
  The refcount of 'data' is increased.
  data     : If != NULL the data stored after 'data' under 'ident'
             is returned, otherwise the first entry
  new      : Get only data which was added after the last restart
             of the mainloop.
*********************************************************************/
void plug_data_get_list (const char *ident, plugData *data, BOOL onlynew,
						 const char *plug_names, plugData ***datas,
						 int *d_len, int *d_cnt);

/*********************************************************************
  Drop refcount of all stored data to 0 and call the destroy functions.
*********************************************************************/
void plug_data_cleanup (void);

/*********************************************************************
  Call func at the start of the main loop or at the end before
  cleaning any floating data. An id is returned for unregistering the
  function. If no data is passed to ..._register_...(), the id is
  passed to func instead of the data.
*********************************************************************/
void* plug_main_register_before (plugMainFunc func, void *data);
void* plug_main_register_after (plugMainFunc func, void *data);
void plug_main_unregister (void *id);

/*********************************************************************
  Process the observer list and call func for each observer.
*********************************************************************/
void plug_main_iteration (void);

/*
 * Functions from plugin.c
 */

/*********************************************************************
  Check if 'name' is a valid name/identifier.
*********************************************************************/
void plug_check_name (const char *name);

/*********************************************************************
  Parse name accord to "ident (plugs)". Copy ident to the already
  allocated "ident", set "plugs" to the plugins start (first non space
  in the '()'). Check if "ident" and plugs are valid identifiers.
*********************************************************************/
void plug_getcheck_name (const char *name, char *ident, const char **plugs);

/*********************************************************************
  Return the first occurrence of the symbol 'symbol' in the main
  program or any loaded plugin library.
*********************************************************************/
void *plug_get_symbol (const char *symbol);

/*********************************************************************
  Return a newly alloced string with all plugins from 'plugs', which
  depend on ident.
*********************************************************************/
char* plug_plugins_get_names (char *plugs, char *ident);

/*********************************************************************
  Remove an ident with plugin dependencies from plug->plugins.
  If deps==')' remove all dependencies. Return if all dependencies
  for ident were removed.
*********************************************************************/
BOOL plugins_remove_names (plugPlugin *plug, const char *ident, const char *deps);

/*********************************************************************
  Add an ident with plugin dependencies to plug->plugins.
*********************************************************************/
void plugins_add_names (plugPlugin *plug, const char *ident, const char *deps);

/*********************************************************************
  Return the plugin corresponding to the plugin definition.
*********************************************************************/
plugPlugin *plug_get_by_def (const plugDefinition *plug);

/*********************************************************************
  Load plugin from Library 'file' and prepend/append it to the
  (internal) list of the plugins. If 'global', make symbols of the
  plugin globally available.
  Returns the newly created plugin or NULL on error.
*********************************************************************/
plugPlugin* plug_load (char *file, BOOL global);

/*********************************************************************
  Prepend all known plugins to the (internal) list of the plugin libs.
*********************************************************************/
void plug_load_intern (void);

/*********************************************************************
  Set plugin specific arguments.
*********************************************************************/
void plug_init_prepare (plugPlugin *plug, int argc, char **argv);

/*********************************************************************
  Init a timer for measuring the plugin process() call.
*********************************************************************/
void plug_init_timer (plugPlugin *plug);

/*********************************************************************
  Init timer for measuring all plugin process() calls.
*********************************************************************/
void plug_init_timer_all (void);

/*
 * Call functions from the plugin plug if the plugin is enabled.
 */

/*********************************************************************
  Call the function from plugPlugin for all plugins (if the plugin is
  enabled).
*********************************************************************/
void plug_init_all (grabParameter *para);
int plug_init_options_all (void);
void plug_cleanup_all (void);

/*********************************************************************
  Call the plugin process()-function if the plugin is enabled.
*********************************************************************/
BOOL plug_process (plugPlugin *plug, char *ident, plugData *data);

#ifdef __cplusplus
}
#endif

#endif /* iw_plugin_i_H */
