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

#ifndef iw_plugin_H
#define iw_plugin_H

#define IW_GRAB_TYPES
#include "grab.h"
#undef IW_GRAB_TYPES

/* plugDefinition->abi_version should be set to this value in
   the plug_get_info() factory function of a plugin. */
#define PLUG_ABI_VERSION		2

/* For C++ plugins, this is the provided version.
   This can also be used to test whether icemm support is available at all. */
#define ICEMM_VERSION			1

/* Customizing flags for plug_add_default_page() */
typedef enum {
	PLUG_PAGE_NOPLUG	= 1 << 0,
	PLUG_PAGE_NODISABLE	= 1 << 1
} plugPageFlags;

typedef struct _plugPlugin plugPlugin;
struct plugData;

/* Definition of a plugin instance. Must be returned by the
   plug_get_info() factory function of a plugin. */
typedef struct plugDefinition {
	const char *name;			/* Name of the instance, must be unique */
	int abi_version;			/* Always PLUG_ABI_VERSION */
	void (*init) (struct plugDefinition *plug, grabParameter *para, int argc, char **argv);
	int (*init_options) (struct plugDefinition *plug);
	void (*cleanup) (struct plugDefinition *plug);
	BOOL (*process) (struct plugDefinition *plug, char *id, struct plugData *data);
	void *reserved1;
	void *reserved2;
	void *reserved3;
} plugDefinition;

/* Called on load of the plugin. Returns the filled plugin definition
   structure and whether the plugin should be inserted at the start or
   at the end of the iceWing internal plugin list. */
typedef plugDefinition* (*plugGetInfoFunc) (int call_cnt, BOOL *append);

#include "plugin_comm.h"
#include "grab.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return the plugin whose name matches 'name'.
*********************************************************************/
plugPlugin *plug_get_by_name (const char *name);
plugDefinition *plug_def_get_by_name (const char *name);

/*********************************************************************
  Return the path where 'plug' can store it's data files.
*********************************************************************/
char *plug_get_datadir (plugDefinition *plug);

/*********************************************************************
  Return a pointer to a per-plugin-instance string of the form
  'plugDef->name''suffix'. Can be used e.g. for preview windows and
  option pages.
*********************************************************************/
char *plug_name (plugDefinition *plug, const char *suffix);

/*********************************************************************
  Set the plugin enabled state.
*********************************************************************/
void plug_set_enable (plugPlugin *plug, BOOL enabled);

/*********************************************************************
  Register a new plugin 'plug_new' with iceWing. The new plugin gets
  associated with the plugin 'plug', which should be the calling
  plugin. Initialization is scheduled before the next main loop run.
  append    : Should the new plugin be inserted at the start or at
              the end of the iceWing internal plugin list.
  argc, argv: Command line arguments for the new plugin.
  Return TRUE on sucess, FALSE otherwise.

  Attention: The function is not thread save. Should only be used for
             language bindings. No arguments are copied.
*********************************************************************/
BOOL plug_register (plugDefinition *plug, plugDefinition *plug_new,
					BOOL append, int argc, char **argv);

/*********************************************************************
  Create a new option page with the name 'plugDef->name" "suffix' and
  two widgets:
    1. Toggle to enable/disable the plugin processing call
       (only if PLUG_PAGE_NODISABLE is not set in flags).
    2. String for plugin names (only if PLUG_PAGE_NOPLUG is not set in
	   flags). The plugin gets called one after another for all data
	   from plugins which are entered here.
  Return the number of the newly created page.
*********************************************************************/
int plug_add_default_page (plugDefinition *plugDef, const char *suffix,
						   plugPageFlags flags);

#ifdef __cplusplus
}
#endif

#endif /* iw_plugin_H */
