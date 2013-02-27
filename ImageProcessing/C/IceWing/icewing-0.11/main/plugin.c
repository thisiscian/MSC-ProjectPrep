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
#include <stdio.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>
#include <ctype.h>

#include "gui/Gtools.h"
#include "gui/Ggui_i.h"
#include "gui/Goptions_i.h"
#include "plugin_i.h"
#include "plugin.h"

#ifndef RTLD_GLOBAL
#  define RTLD_GLOBAL	0
#endif
#ifndef RTLD_LAZY
#  define RTLD_LAZY		1
#endif

#define OPT_ENABLE		"Perform plugin processing"

/* For plug.plugins: Call process() only for first data element */
#define PLUGINS_FIRST	'\1'

typedef struct plugList {		/* List of all plugins */
	plugPlugin plug;
	struct plugList *next;
} plugList;

typedef struct plugLibList {	/* List of all opened libraries */
	plugGetInfoFunc func;		/*   Pointer to plug_get_info() */
	char *file;					/*   File name of library */
	char *path;					/*   Path to the DATADIR for the library */
	void *handle;				/*   Handle returned by dlopen() */
	int call_cnt;				/*   Number of times plug_get_info() was called */
	plugPlugin **plugs;			/*   All plugins instantiated from file */
	struct plugLibList *next;
} plugLibList;

typedef struct plugDefinitionPriv {
	char *name;
	int abi_version;
	void (*init) (struct plugDefinition *plug, grabParameter *para, int argc, char **argv);
	int (*init_options) (struct plugDefinition *plug);
	void (*cleanup) (struct plugDefinition *plug);
	BOOL (*process) (struct plugDefinition *plug, char *id, struct plugData *data);
	plugPlugin *plug;
} plugDefinitionPriv;

/* Automatically add a timer for every new plugin instance? */
static BOOL timer_add = FALSE;

static plugList *plugins = NULL;
static plugLibList *plugins_lib = NULL;

typedef plugDefinition* (_plugGetInfoFunc) (int call_cnt, BOOL *append);
extern _plugGetInfoFunc iw_grab_get_info;
extern _plugGetInfoFunc iw_iclas_get_info;
extern _plugGetInfoFunc iw_track_get_info;
extern _plugGetInfoFunc iw_bpro_get_info;
extern _plugGetInfoFunc iw_gsimple_get_info;
extern _plugGetInfoFunc iw_poly_get_info;
extern _plugGetInfoFunc iw_sclas_get_info;
extern _plugGetInfoFunc iw_record_get_info;

typedef struct plugEntryIntern {		/* Infos about plugin's compiled in */
	plugGetInfoFunc func;
	char *file;
} plugEntryIntern;
static plugEntryIntern plugin_entries[] = {
	{&iw_grab_get_info, "grab"},
	{&iw_iclas_get_info, "imgclass"},
	{&iw_track_get_info, "tracking"},
	{&iw_bpro_get_info, "backpro"},
	{&iw_gsimple_get_info, "gsimple"},
	{&iw_poly_get_info, "polynom"},
	{&iw_sclas_get_info, "skinclass"},
	{&iw_record_get_info, "record"},
	{NULL, NULL}
};

/*********************************************************************
  If the library file was already loaded return a pointer to it.
  Otherwise add it to the library list and return the new entry.
*********************************************************************/
static plugLibList *plug_lib_add (char *file)
{
	plugLibList *pos = plugins_lib;
	int i;

	while (pos) {
		if (!strcmp (file, pos->file))
			return pos;
		for (i=0; i<pos->call_cnt; i++)
			if (!strcasecmp (file, pos->plugs[i]->def->name))
				return pos;
		pos = pos->next;
	}
	pos = calloc (1, sizeof(plugLibList));
	pos->file = strdup (file);
	pos->next = plugins_lib;
	plugins_lib = pos;

	return pos;
}

/*********************************************************************
  Test if any already loaded library handle is identical to
  (*new)->handle. If so, free it and return TRUE and a pointer to the
  old one in *new.
*********************************************************************/
static BOOL plug_lib_already_loaded (plugLibList **new)
{
	plugLibList *pos = plugins_lib, *prev;

	while (pos) {
		if (pos != *new && pos->handle && pos->handle == (*new)->handle) {

			prev = plugins_lib;
			while (prev && prev->next != *new) prev = prev->next;
			if (prev)
				prev->next = (*new)->next;
			else
				plugins_lib = (*new)->next;

			if ((*new)->file) free ((*new)->file);
			if ((*new)->path) free ((*new)->path);
			free (*new);
			*new = pos;

			return TRUE;
		}
		pos = pos->next;
	}
	return FALSE;
}

/*********************************************************************
  Check if 'name' is a valid name/identifier.
*********************************************************************/
void plug_check_name (const char *name)
{
	const char *pos;

	if (!name)
		iw_error ("No name/identifier given");

	for (pos = name; *pos; pos++) {
		if (isspace((int)*pos) || *pos == '(' || *pos == ')' ||
			*pos == '"' || *pos == '\'') {
			iw_error ("Name/identifier '%s' contains an illegal character '%c'",
					  name, *pos);
		}
	}
	if (pos-name >= PLUG_NAME_LEN)
		iw_error ("Name/identifier '%s' longer than %d",
				  name, PLUG_NAME_LEN-1);
}

/*********************************************************************
  Parse name accord to "ident (plugs)". Copy ident to the already
  allocated "ident", set "plugs" to the plugins start (first non space
  in the '()'). Check if "ident" and plugs are valid identifiers.
*********************************************************************/
void plug_getcheck_name (const char *name, char *ident, const char **plugs)
{
	const char *pos;
	int len;

	if (!name)
		iw_error ("No name/identifier given");

	/* Get and check identifier */
	for (pos = name; *pos && !isspace((int)*pos) && *pos != '('; pos++) {
		if (*pos == ')' || *pos == '"' || *pos == '\'') {
			iw_error ("Identifier '%s' contains an illegal character '%c'",
					  name, *pos);
		} else if (pos-name >= PLUG_NAME_LEN-1)
			iw_error ("Name/identifier '%s' longer than %d",
					  name, PLUG_NAME_LEN-1);
		*ident++ = *pos;
	}
	*ident = '\0';
	while (*pos == ' ')
		pos++;
	if (*pos == '(') {
		pos++;
		while (*pos == ' ')
			pos++;
		*plugs = pos;

		/* Check all plugins */
		len = 0;
		while (*pos && *pos != ')') {
			if (*pos == ' ') {
				do {
					pos++;
				} while (*pos == ' ');
				len = 0;
			}
			if (*pos == '(' || *pos == '"' || *pos == '\'' || isspace((int)*pos)) {
				iw_error ("Names '%s' contain an illegal character '%c'",
						  *plugs, *pos);
			} else if (len >= PLUG_NAME_LEN-1) {
				iw_error ("Name in '%s' longer than %d",
						  *plugs, PLUG_NAME_LEN-1);
			} else if (*pos) {
				len++;
				pos++;
			}
		}
	} else if (*pos) {
		iw_error ("Identifier '%s' contains an illegal character '%c'",
				  name, *pos);
	} else
		*plugs = NULL;
}

/*********************************************************************
  Prepend/Append plugin plug to the (internal) list of the plugins.
  Return the newly allocated plugin.
*********************************************************************/
static plugPlugin *plug_register_do (plugLibList *lib, plugDefinition *def, BOOL append)
{
	plugList *new = plugins;

	if (!def) return NULL;

	plug_check_name (def->name);
	if (def->abi_version != PLUG_ABI_VERSION)
		iw_error ("\n\tIncompatible ABI versions in plugin '%s' (%d) and in "ICEWING_NAME_D" (%d).\n"
				  "\tPlease recompile the plugin",
				  def->name, def->abi_version, PLUG_ABI_VERSION);

	while (new) {
		if (!strcasecmp (def->name, new->plug.def->name)) {
			iw_warning ("Tried to register plugin '%s' two times",
						def->name);
			return NULL;
		}
		new = new->next;
	}
	new = calloc (1, sizeof(plugList));
	new->plug.def = def;
	new->plug.lib = lib;
	new->plug.append = append;
	new->plug.enabled = TRUE;
	new->plug.init_called = FALSE;
	new->plug.init_options_called = FALSE;
	new->plug.cleanup_called = FALSE;
	new->plug.timer = -1;
	new->plug.flags = PLUG_PAGE_NOPLUG | PLUG_PAGE_NODISABLE;
	new->plug.plugins[0] = PLUGINS_FIRST;
	new->plug.plugins[1] = '\0';
	gui_strlcpy (new->plug.buffer, def->name, PLUG_BUFFER_LEN);

	if (append && plugins) {
		plugList *pos = plugins;
		while (pos->next) pos = pos->next;
		pos->next = new;
	} else {
		new->next = plugins;
		plugins = new;
	}

	((plugDefinitionPriv*)def)->plug = &new->plug;
	lib->call_cnt++;
	lib->plugs = realloc (lib->plugs, sizeof(plugPlugin*) * lib->call_cnt);
	lib->plugs[lib->call_cnt-1] = &new->plug;

	if (timer_add)
		plug_init_timer (&new->plug);

	return &new->plug;
}

static void cb_plug_register_init (void *id)
{
	/* Only plugins which are not initalised are called
	   by plug_init_all() -> only the new one(s) are called */
	plug_init_all (NULL);
	plug_init_options_all();
	plug_main_unregister (id);
}

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
					BOOL append, int argc, char **argv)
{
	plugPlugin *p = ((plugDefinitionPriv*)plug)->plug;
	plugPlugin *p_new;

	if ((p_new = plug_register_do (p->lib, plug_new, append))) {
		/* Prepare deferred plugin-init (to init it from the main thread) */
		plug_init_prepare (p_new, argc, argv);
		plug_main_register_before (cb_plug_register_init, NULL);
		return TRUE;
	} else {
		return FALSE;
	}
}

/*********************************************************************
  Prepend all known plugins to the (internal) list of the plugin libs.
*********************************************************************/
void plug_load_intern (void)
{
	plugEntryIntern *entry = plugin_entries;
	while (entry->file) {
		plugLibList *lib = plug_lib_add (entry->file);
		lib->func = *entry->func;
		entry++;
	}
}

static char *plug_get_homePath()
{
	static char path[PATH_MAX] = "";
	if (!path[0])
		gui_convert_prg_rc (path, "plugins", TRUE);
	return path;
}

typedef enum {
	DL_DATADIR = 1 << 0,	/* Use DATADIR? */
	DL_SHOWERR = 1 << 1		/* Show every error message? */
} dlOptions;

static void plug_dlerror (char *name, BOOL showerr)
{
	char *err = dlerror();
	if (err) {
		static char enoent[100] = "";
		if (!enoent[0]) {
			strncpy (enoent, strerror(ENOENT), sizeof(enoent));
			enoent[sizeof(enoent)-1] = '\0';
		}
		/* Bad hack to suppress the very common ENOENT */
		if (showerr || !strstr(err, enoent))
			iw_warning ("dlopen('%s') error:\n\t%s", name, err);
	}
}

/*********************************************************************
  Try loading a lib from path"/"file or path"/lib"file".so". Return
  in pathData the complete path to the datadir of lib (must be freed).
*********************************************************************/
static void *plug_dlopen (char *path, char *file, int flags,
						  dlOptions opts, char **pathData)
{
	int len = strlen(path), lenF = strlen(file);
	char *pos, *name = malloc (len+lenF+10);
	void *handle;

	*pathData = NULL;

	/* File relative? */
	if (file[0] != '/') {
		strcpy (name, path);
		if (name[len-1] != '/') {
			strcat (name, "/");
			len++;
		}
	} else {
		name[0] = '\0';
		len = 0;
	}
	strcat (name, file);

	handle = dlopen (name, flags);
	if (!handle) {
		plug_dlerror (name, FALSE);

		/* No sucess -> try adding "lib" / ".so" */
		pos = strrchr (file, '/');
		if (pos)
			pos++;
		else
			pos = file;
		len += pos-file;
		if (strncmp(pos, "lib", 3) || strncmp (&pos[strlen(pos)-3], ".so", 3)) {
			if (strncmp(pos, "lib", 3)) {
				strcpy (&name[len], "lib");
				strcat (name, pos);
			}
			if (strncmp(&pos[strlen(pos)-3], ".so", 3))
				strcat (name, ".so");
			handle = dlopen (name, flags);
		}
	}
	if (!handle) {
		plug_dlerror (name, opts & DL_SHOWERR);
		free (name);
	} else {
		iw_warning ("Success loading from '%s'", name);
		/* Get the datadir based on the libdir */
		if ((opts & DL_DATADIR) || strchr (file, '/')) {
			*pathData = strdup (DATADIR);
			free (name);
		} else {
			pos = strrchr (name, '/');
			if (pos) {
				*pos = '\0';
				pos = strrchr (name, '/');
			}
			if (pos) {
				/* xxx/plugins/lib -> xxx/data */
				*++pos = '\0';
				strcpy (pos, "data");
				*pathData = name;
			} else {
				*pathData = strdup (DATADIR);
				free (name);
			}
		}
	}

	return handle;
}

/*********************************************************************
  Load plugin from Library 'name' and prepend/append it to the
  (internal) list of the plugins. If 'global', make symbols of the
  plugin globally available.
  Returns the newly created plugin or NULL on error.
*********************************************************************/
plugPlugin *plug_load (char *file, BOOL global)
{
	plugDefinition *def;
	plugLibList *lib;
	BOOL append = TRUE;
	int flags = RTLD_LAZY;

	if (global) flags |= RTLD_GLOBAL;

	lib = plug_lib_add (file);
	if (!lib->func) {
		/* Lib not loaded -> try loading */
		if (file[0] != '/') {
			/* Try 'other' path variants only for relative file paths */
			lib->handle = plug_dlopen (".", file, flags, DL_DATADIR, &lib->path);
			if (!lib->handle) {
				char *env = getenv("ICEWING_PLUGIN_PATH");
				char path[PATH_MAX];
				int i, iPath = 0;

				for (i=0; env; i++) {
					if (env[i] == ':' || !env[i]) {
						if (iPath) {
							if (path[iPath-1] != '/') path[iPath++] = '/';
							strcpy (&path[iPath], "plugins");
							lib->handle = plug_dlopen (path, file, flags, 0, &lib->path);
						}
						iPath = 0;
						if (!env[i] || lib->handle) break;
					} else if (iPath < PATH_MAX-10)  {
						path[iPath++] = env[i];
					}
				}
			}
			if (!lib->handle)
				lib->handle = plug_dlopen (plug_get_homePath(), file, flags, 0, &lib->path);
		}
		if (!lib->handle)
			lib->handle = plug_dlopen (LIBDIR, file, flags, DL_DATADIR|DL_SHOWERR, &lib->path);
		if (!lib->handle)
			return FALSE;
		if (!plug_lib_already_loaded (&lib)) {
			lib->func = (plugGetInfoFunc)dlsym (lib->handle, "plug_get_info");
			if (!lib->func) {
				iw_warning ("dlsym('plug_get_info') error for %s:\n\t%s",
							file, dlerror());
				dlclose (lib->handle);
				return FALSE;
			}
		}
	}
	def = lib->func (lib->call_cnt+1, &append);
	return plug_register_do (lib, def, append);
}

/*********************************************************************
  Return the path where 'plug' can store it's data files.
*********************************************************************/
char *plug_get_datadir (plugDefinition *plug)
{
	plugPlugin *p = ((plugDefinitionPriv*)plug)->plug;
	return p->lib->path;
}

/*********************************************************************
  Return the first occurrence of the symbol 'symbol' in the main
  program or any loaded plugin library.
*********************************************************************/
void *plug_get_symbol (const char *symbol)
{
	static void *handle = NULL;
	plugLibList *lib = plugins_lib;
	void *func;

	if (!handle)
		handle = dlopen (NULL, RTLD_LAZY);
	if ((func = dlsym (handle, symbol)))
		return func;

	while (lib) {
		if ((func = dlsym (lib->handle, symbol)))
			return func;
		lib = lib->next;
	}
	return NULL;
}

/*********************************************************************
  Call for all registered plugins the function 'func' with plugin
  information and 'data' as arguments.
*********************************************************************/
void plug_info_plugs (plugInfoPlugFunc func, void *data)
{
	plugLibList *pos = plugins_lib;
	plugInfoPlug info;
	int i;

	while (pos) {
		info.name = pos->file;
		info.enabled = FALSE;
		info.plug = NULL;
		func (&info, data);
		for (i=0; i<pos->call_cnt; i++) {
			info.name = pos->plugs[i]->def->name;
			info.enabled = pos->plugs[i]->enabled;
			info.plug = pos->plugs[i];
			func (&info, data);
		}
		pos = pos->next;
	}
}

/*********************************************************************
  Return the plugin corresponding to the plugin definition.
*********************************************************************/
plugPlugin *plug_get_by_def (const plugDefinition *plug)
{
	return ((plugDefinitionPriv*)plug)->plug;
}

/*********************************************************************
  Return the plugin whose name matches 'name'.
*********************************************************************/
plugPlugin *plug_get_by_name (const char *name)
{
	plugList *plug_pos = plugins;

	while (plug_pos) {
		if (!strcasecmp(name, plug_pos->plug.def->name))
			return &plug_pos->plug;
		plug_pos = plug_pos->next;
	}
	return NULL;
}

/*********************************************************************
  Return the plugin definition whose name matches 'name'.
*********************************************************************/
plugDefinition *plug_def_get_by_name (const char *name)
{
	plugPlugin *plug = plug_get_by_name(name);
	if (plug)
		return plug->def;
	else
		return NULL;
}

/*********************************************************************
  Return a pointer to a per-plugin-instance string of the form
  'plugDef->name''suffix'. Can be used e.g. for preview windows and
  option pages.
*********************************************************************/
char *plug_name (plugDefinition *plug, const char *suffix)
{
	int n_len = strlen(plug->name);
	plugPlugin *p = ((plugDefinitionPriv*)plug)->plug;
	int b_free;

	b_free = PLUG_BUFFER_LEN - 1 - n_len;
	gui_strlcpy (&p->buffer[n_len], suffix, b_free);

	return p->buffer;
}

/*********************************************************************
  Init a timer for measuring the plugin process() call.
*********************************************************************/
void plug_init_timer (plugPlugin *plug)
{
	if (plug->timer < 0)
		plug->timer = iw_time_add (plug_name(plug->def, " process"));
}

/*********************************************************************
  Init timer for measuring all plugin process() calls.
*********************************************************************/
void plug_init_timer_all (void)
{
	plugList *plug_pos = plugins;

	timer_add = TRUE;
	while (plug_pos) {
		plug_init_timer (&plug_pos->plug);
		plug_pos = plug_pos->next;
	}
}

/*********************************************************************
  Set the plugin enabled state.
*********************************************************************/
void plug_set_enable (plugPlugin *plug, BOOL enabled)
{
	if (!plug) return;

	plug->enabled = enabled;
	if (!(plug->flags & PLUG_PAGE_NODISABLE)) {
		char *name = g_strconcat (plug->defname, ".", OPT_ENABLE, NULL);
		opts_value_set (name, GINT_TO_POINTER(plug->enabled));
		g_free (name);
	}
}

/*********************************************************************
  Set plugin specific arguments.
*********************************************************************/
void plug_init_prepare (plugPlugin *plug, int argc, char **argv)
{
	if (plug) {
		plug->argc = argc;
		plug->argv = argv;
	}
}

/*********************************************************************
  Call the function from plugPlugin for all plugins.
*********************************************************************/
void plug_init_all (grabParameter *para)
{
	static grabParameter *parameter = NULL;
	plugList *plug_pos;

	if (!parameter) {
		parameter = para;
		iw_assert (parameter!=NULL, "No parameter given in plug_init_all()");
	}

	plug_pos = plugins;
	while (plug_pos) {
		plugPlugin *plug = &plug_pos->plug;
		if (plug && !plug->init_called && plug->def->init) {
			plug->def->init (plug->def, parameter, plug->argc, plug->argv);
			plug->init_called = TRUE;
		}
		plug_pos = plug_pos->next;
	}
}
int plug_init_options_all (void)
{
	int p = -1;
	plugList *plug_pos = plugins;

	while (plug_pos) {
		if (!plug_pos->plug.init_options_called && plug_pos->plug.def->init_options) {
			p = plug_pos->plug.def->init_options(plug_pos->plug.def);
			plug_pos->plug.init_options_called = TRUE;
		}
		plug_pos = plug_pos->next;
	}
	return p;
}
void plug_cleanup_all (void)
{
	plugList *plug_pos = plugins;

	/* Free all stored data elements */
	plug_data_cleanup();

	while (plug_pos) {
		if (!plug_pos->plug.cleanup_called && plug_pos->plug.def->cleanup) {
			plug_pos->plug.def->cleanup(plug_pos->plug.def);
			plug_pos->plug.cleanup_called = TRUE;
		}
		plug_pos = plug_pos->next;
	}
}

/*********************************************************************
  Call the plugin process()-function if the plugin is enabled.
*********************************************************************/
BOOL plug_process (plugPlugin *plug, char *ident, plugData *data)
{
	BOOL cont = TRUE;
	if (plug && plug->enabled && plug->def->process) {

		char *names;

		if (*plug->plugins == PLUGINS_FIRST) {
			/* No plugins entry -> Call the plugin directly. */
			if (plug->timer >= 0)
				iw_time_start (plug->timer);
			cont = plug->def->process (plug->def, ident, data);
			if (plug->timer >= 0)
				iw_time_stop (plug->timer, FALSE);
		} else {
			/* Distribute new data according to the option page settings. */
			plugData **datas = NULL;
			int i, d_len = 0, d_cnt = 0;

			/* First collect all wanted data. Plugins can set data at
			   every time, so plug_data_get() can not be intermixed with
			   plugin processing calls. Otherwise these new data would be
			   processed as well. */

			if (*plug->plugins) {
				/* Get data from specified plugins. */
				if ((names = plug_plugins_get_names (plug->plugins, ident))) {
					plug_data_get_list (ident, NULL, TRUE, names,
										&datas, &d_len, &d_cnt);
					free (names);
				}
			} else {
				/* Get all data. Only if plug->plugins is completely empty. */
				plug_data_get_list (ident, NULL, TRUE, NULL,
									&datas, &d_len, &d_cnt);
			}

			/* Now call the plugin processing function. */
			for (i=0; i<d_cnt; i++) {
				if (cont && strcmp (plug->def->name, datas[i]->plug->name)) {
					iw_debug (4, "%s: Processing data from '%s'...",
							  plug->def->name, datas[i]->plug->name);
					if (plug->timer >= 0)
						iw_time_start (plug->timer);
					cont = plug->def->process (plug->def, ident, datas[i]);
					if (plug->timer >= 0)
						iw_time_stop (plug->timer, FALSE);
				}
				plug_data_unget (datas[i]);
			}
			if (datas) free (datas);
		}
	}
	return cont;
}

typedef struct pluginsData {
	int i_len, i_cnt;
	char **idents;
	int p_cnt;
	char **plugs;
	plugPlugin *plug;
	GtkWidget *menu;
} pluginsData;

/* Indices into pluginsData->idents and pluginsData->plugs */
typedef struct pluginsDataIdx {
	int ident;
	int plug;
} pluginsDataIdx;

static void plugins_data_free (pluginsData *data)
{
	int i;

	if (!data) return;

	if (data->idents) {
		for (i=0; i<data->i_cnt; i++)
			free (data->idents[i]);
		free (data->idents);
	}
	if (data->plugs) {
		for (i=0; i<data->p_cnt; i++)
			free (data->plugs[i]);
		free (data->plugs);
	}
	free (data);
}

static void plugins_idx_free (void *data)
{
	if (data) free (data);
}

static void plugins_show_value (GtkWidget *entry, char *str)
{
	if (entry) {
		gtk_entry_set_text (GTK_ENTRY(entry), str);
		gtk_editable_set_position (GTK_EDITABLE(entry), -1);
	}
}

/*********************************************************************
  Get next token from plugs and return if there was one.
*********************************************************************/
static BOOL plugins_get_token (char **plugs, char *token)
{
	char *p = *plugs;
	int i = 0;

	while (*p == ' ') p++;

	if (*p == '(' || *p == ')') {
		token[i++] = *p++;
	} else {
		while (*p && *p != '(' && *p != ')' && *p != ' ' && i<PLUG_NAME_LEN-1)
			token[i++] = *p++;
	}
	token[i] = '\0';
	*plugs = p;

	return i>0;
}

/*********************************************************************
  Callback for plug->plugins. Called if the string is loaded.
*********************************************************************/
static BOOL plugins_set_value (void *value, void *new_value, void *data)
{
	char *str = value;
	char *new_str = new_value;
	GtkWidget *entry = data;
	char token[PLUG_NAME_LEN];

	gui_strcpy_back (str, new_str, PLUG_PLUGINS_LEN);
	plugins_show_value (entry, str);

	while (plugins_get_token (&str, token)) {
		if (token[0] == '(') {
			while (*str && *str != ')') str++;
		} else {
			plug_observer_reorder (token);
		}
	}
	return FALSE;
}

/*********************************************************************
  Return start of plugin names from 'plugs', which depend on ident.
*********************************************************************/
static char *plugins_find_names (char *plugs, const char *ident)
{
	char *pos = plugs;
	int l = strlen (ident);

	while ((pos = strstr (pos, ident))) {
		if ((pos == plugs || *(pos-1) == ' ') &&
			(*(pos+l) == '(' || *(pos+l) == ' ')) {
			pos += l;
			while (*pos == ' ')
				pos++;
			if (*pos == '(') {
				pos++;
				return pos;
			}
		} else
			pos += l;
	}
	return NULL;
}

/*********************************************************************
  Return a newly alloced string with all plugins from 'plugs', which
  depend on ident.
*********************************************************************/
char *plug_plugins_get_names (char *plugs, char *ident)
{
	char *start, *names = NULL;
	int l;

	if ((start = plugins_find_names (plugs, ident))) {
		l = 0;
		while (*(start+l) && *(start+l) != ')')
			l++;
		if (l > 0) {
			names = malloc (l+3);
			memcpy (names+1, start, l);
			names[0] = ' ';
			names[l+1] = ' ';
			names[l+2] = '\0';
		}
		return names;
	}
	return NULL;
}

/*********************************************************************
  Append src to dst and increment len by the length of src (only if
  length(dst+src) < PLUG_PLUGINS_LEN).
*********************************************************************/
static void plugins_cat (char *dst, const char *src, int *len)
{
	int l_src = strlen(src);

	if (*len + l_src >= PLUG_PLUGINS_LEN) {
		*len = PLUG_PLUGINS_LEN;
		return;
	}
	strcat (*len+dst, src);
	*len += l_src;
}

/*********************************************************************
  Remove an ident with plugin dependencies from plug->plugins.
  If deps==')' remove all dependencies. Return if all dependencies
  for ident were removed.
*********************************************************************/
BOOL plugins_remove_names (plugPlugin *plug, const char *ident, const char *deps)
{
	char plugins[PLUG_PLUGINS_LEN];
	char *pstart, *pend, pendch;
	char *dpos, dident[PLUG_NAME_LEN];
	int l, plen;
	BOOL remall = TRUE;

	if (!*deps) return FALSE;

	strcpy (plugins, plug->plugins);
	printf ("rem: |%s| deps|%s|\n", plugins, deps);

	if ((pstart = plugins_find_names (plugins, ident))) {
		/* Ident in plugins. Remove all dependencies. */
		plen = strlen (plugins);
		for (pend = pstart; *pend && *pend != ')'; pend++) /* empty */;
		pendch = *pend;
		*pend = '\0';

		remall = (*deps == ')');

		while (*deps && *deps != ')') {
			for (l = 0; deps[l] && deps[l] != ' ' && deps[l] != ')'; l++)
				dident[l] = deps[l];
			dident[l] = '\0';
			deps += l;
			while (*deps == ' ')
				deps++;

			if ((dpos = strstr (pstart, dident)) &&
				(dpos == pstart || *(dpos-1) == ' ') &&
				(dpos+l == pend || *(dpos+l) == ' ') ) {
				/* Dependency in plugins -> Remove dependency dident */
				if (dpos != pstart) {
					memmove (dpos-1, dpos+l, plugins+plen-dpos-l+1);
					l++;
				} else if (dpos+l != pend) {
					memmove (dpos, dpos+l+1, plugins+plen-dpos-l);
					l++;
				} else {
					memmove (dpos, dpos+l, plugins+plen-dpos-l+1);
				}
				pend -= l;
				plen -= l;
			}
		}
		*pend = pendch;
		/* Remove ident if "ident()" was given or if no dependencies are left */
		if (remall || pstart == pend) {
			while (*pstart != '(') pstart--;
			while (*pstart == ' ') pstart--;
			while (pstart >= plugins && *pstart != ' ') pstart--;
			while (pstart >= plugins && *pstart == ' ') pstart--;
			pstart++;
			if (*pend) pend++;
			while (*pend == ' ') pend++;

			if (*pend)
				*pstart++ = ' ';
			memmove (pstart, pend, plugins+plen-pend+1);
			remall = TRUE;
		}

		gui_strcpy_back (plug->plugins, plugins, PLUG_PLUGINS_LEN);
		plugins_show_value (plug->p_entry, plug->plugins);
	}
	printf ("rem: |%s|\n", plugins);
	return remall;
}

/*********************************************************************
  Add an ident with plugin dependencies to plug->plugins.
*********************************************************************/
void plugins_add_names (plugPlugin *plug, const char *ident, const char *deps)
{
	char plugins[PLUG_PLUGINS_LEN];
	char *pstart, *pend, pendch;
	char *dpos, dident[PLUG_NAME_LEN];
	int l, plen;

	if (*plug->plugins == PLUGINS_FIRST)
		*plug->plugins = '\0';

	if (!*deps || *deps == ')')
		return;

	strcpy (plugins, plug->plugins);
	plen = strlen (plugins);
	printf ("add: |%s| deps|%s|\n", plugins, deps);

	if ((pstart = plugins_find_names (plugins, ident))) {
		/* Ident in plugins. Add any missing dependencies. */
		for (pend = pstart; *pend && *pend != ')'; pend++) /* empty */;
		pendch = *pend;
		*pend = '\0';
		while (*deps && *deps != ')') {
			for (l = 0; deps[l] && deps[l] != ' ' && deps[l] != ')'; l++)
				dident[l] = deps[l];
			dident[l] = '\0';
			if ( plen+l+1 < PLUG_PLUGINS_LEN &&
				 (!(dpos = strstr (pstart, dident)) ||
				  (dpos != pstart && *(dpos-1) != ' ') ||
				  (dpos+l != pend && *(dpos+l) != ' ')) ) {
				/* Dependency not yet in plugins and enough space in plugins.
				   -> Insert dependency dident */
				memmove (pend+l+1, pend, 1 + plugins+plen-pend);
				*pend = ' ';
				memcpy (pend+1, dident, l);
				pend += l+1;
				plen += l+1;
			}
			deps += l;
			while (*deps == ' ') deps++;
		}
		*pend = pendch;
	} else {
		/* Ident not yet in plugins, add it including all dependencies */
		if (*plugins)
			plugins_cat (plugins, " ", &plen);
		plugins_cat (plugins, ident, &plen);
		plugins_cat (plugins, "(", &plen);
		pstart = plugins+plen;
		while (*deps && *deps != ')') {
			for (l = 0; deps[l] && deps[l] != ' ' && deps[l] != ')'; l++)
				dident[l] = deps[l];
			dident[l] = '\0';
			deps += l;
			while (*deps == ' ') deps++;
			/* Remove double entries */
			if ( (!(dpos = strstr (pstart, dident)) ||
				  (dpos != pstart && *(dpos-1) != ' ') ||
				  (*(dpos+l) && *(dpos+l) != ' ')) ) {
				plugins_cat (plugins, dident, &plen);
				plugins_cat (plugins, " ", &plen);
			}
		}
		if (plugins[plen-1] == ' ')
			plugins[plen-1] = ')';
		else
			plugins_cat (plugins, ")", &plen);
	}
	gui_strcpy_back (plug->plugins, plugins, PLUG_PLUGINS_LEN);
	plugins_show_value (plug->p_entry, plug->plugins);
	printf ("add: |%s|\n", plugins);
}

/*********************************************************************
  Clean data->plug->plugins and add/remove a new entry with ident
  i_index and plugin name p_index to it.
*********************************************************************/
static void plugins_change_names (pluginsData *data, int i_index, int p_index, BOOL add)
{
	char *plugs_new = malloc(PLUG_PLUGINS_LEN);
	char *plugs_old = strdup(data->plug->plugins), *plugs;
	char token[PLUG_NAME_LEN], ident[PLUG_NAME_LEN];
	char **names = NULL;
	int i, n_len = 0, n_cnt, p_o_len;
	BOOL i_found, output, done = FALSE;

	*plugs_new = '\0';
	p_o_len = 0;
	plugs = plugs_old;
	while (plugins_get_token (&plugs, token)) {
		if (token[0] == '(') {
			/* Parse plugin names. */
			n_cnt = 0;
			i_found = !strcmp (ident, data->idents[i_index]);
			/* Remember all still valid plugin names */
			while (plugins_get_token (&plugs, token)) {
				if (token[0] == ')')
					break;
				for (i=0; i<data->p_cnt; i++) {
					if (!strcmp (token, data->plugs[i])) {
						BOOL found = i_found && !strcmp (token, data->plugs[p_index]);
						if (found)
							done = TRUE;
						if (strcmp (token, data->plug->def->name)
							&& ((found && add) || !found)) {
							ARRAY_RESIZE (names, n_len, n_cnt);
							names[n_cnt++] = strdup (token);
						}
						break;
					}
				}
			}
			output = TRUE;
			if (i_found && add && !done) {
				done = TRUE;
				ARRAY_RESIZE (names, n_len, n_cnt);
				names[n_cnt++] = strdup (data->plugs[p_index]);
			} else {
				/* Check if plugin still observes ident */
				for (i=0; i<data->i_cnt; i++)
					if (!strcmp (ident, data->idents[i]))
						break;
				if (i >= data->i_cnt)
					output = FALSE;
			}
			if (output && n_cnt > 0) {
				/* Output current ident and plugin names */
				if (*plugs_new)
					plugins_cat (plugs_new, " ", &p_o_len);
				plugins_cat (plugs_new, ident, &p_o_len);
				plugins_cat (plugs_new, "(", &p_o_len);
				for (i=0; i<n_cnt; i++) {
					plugins_cat (plugs_new, names[i], &p_o_len);
					if (i < n_cnt-1)
						plugins_cat (plugs_new, " ", &p_o_len);
				}
				plugins_cat (plugs_new, ")", &p_o_len);
			}
			for (i=0; i<n_cnt; i++)
				free (names[i]);
		} else {
			/* Remember current ident. */
			strcpy (ident, token);
		}
	}
	if (add && !done) {
		/* If the new ident was not already in data->plug->plugins,
		   add it now with the new plugin name. */
		if (*plugs_new)
			plugins_cat (plugs_new, " ", &p_o_len);
		plugins_cat (plugs_new, data->idents[i_index], &p_o_len);
		plugins_cat (plugs_new, "(", &p_o_len);
		plugins_cat (plugs_new, data->plugs[p_index], &p_o_len);
		plugins_cat (plugs_new, ")", &p_o_len);
	}
	gui_strcpy_back (data->plug->plugins, plugs_new, PLUG_PLUGINS_LEN);
	plugins_show_value (data->plug->p_entry, data->plug->plugins);

	if (names) free (names);
	free (plugs_old);
	free (plugs_new);

	plug_observer_reorder (data->idents[i_index]);
}

static void cb_menu_selectiondone (GtkWidget *widget, void *data_v)
{
	pluginsData *data = data_v;
	gtk_widget_destroy (data->menu);
	plugins_data_free (data);
}

static void cb_menu_activate (GtkWidget *widget, void *data_v)
{
	pluginsData *data = data_v;
	pluginsDataIdx *idx = gtk_object_get_data (GTK_OBJECT(widget), "index");

	plugins_change_names (data, idx->ident, idx->plug,
						  GTK_CHECK_MENU_ITEM(widget)->active);

	gtk_widget_destroy (data->menu);
	plugins_data_free (data);
}

static void cb_menu_clear (GtkWidget *widget, void *data_v)
{
	pluginsData *data = data_v;

	*data->plug->plugins = '\0';
	plugins_show_value (data->plug->p_entry, data->plug->plugins);

	gtk_widget_destroy (data->menu);
	plugins_data_free (data);
}

/*********************************************************************
  Add all registered plugins to one ident menu.
*********************************************************************/
static void plugins_add_menu (GtkWidget *menu, pluginsData *data, int ident)
{
	GtkWidget *item;
	pluginsDataIdx *idx;
	char *names, *name = NULL;
	int i;

	/* Get all up to now selected plugins */
	names = plug_plugins_get_names (data->plug->plugins, data->idents[ident]);

	for (i=0; i<data->p_cnt; i++) {
		if (strcmp (data->plugs[i], data->plug->def->name)) {
			item = gtk_check_menu_item_new_with_label (data->plugs[i]);
			if (names)
				name = strstr (names, data->plugs[i]);
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(item),
											name &&
											*(name-1) == ' ' &&
											*(name+strlen(data->plugs[i])) == ' ');
			gtk_menu_append (GTK_MENU(menu), item);
			gtk_signal_connect (GTK_OBJECT(item), "activate",
								GTK_SIGNAL_FUNC(cb_menu_activate), data);
			idx = malloc (sizeof(pluginsDataIdx));
			idx->ident = ident;
			idx->plug = i;
			gtk_object_set_data_full (GTK_OBJECT(item), "index", idx, plugins_idx_free);
		}
	}

	free (names);
}

/*********************************************************************
  Callback for plug_info_observer(): Get all idents observed by
  data->plug.
*********************************************************************/
static void cb_plugins_observer (plugInfoObserver *info, void *data)
{
	static const char *ident = NULL;
	pluginsData *observ = data;

	if (info->ident) {
		ident = info->name;
	} else if (!strcmp (info->name, observ->plug->def->name)) {
		ARRAY_RESIZE (observ->idents, observ->i_len, observ->i_cnt);
		observ->idents[observ->i_cnt++] = strdup(ident);
	}
}

/*********************************************************************
  Create context menu for the plugin selection widget.
*********************************************************************/
static void plugins_popup_menu (GdkEventButton *event, plugPlugin *plug)
{
	GtkWidget *menu, *item, *sub;
	pluginsData *data;
	plugList *pos;
	int i;

	data = malloc (sizeof(pluginsData));
	data->plug = plug;
	data->plugs = NULL;
	data->idents = NULL;
	data->i_len = data->i_cnt = 0;

	plug_info_observer (cb_plugins_observer, data);
	i = 0;
	for (pos = plugins; pos; pos = pos->next) i++;
	data->plugs = malloc (sizeof(char*)*i);
	i = 0;
	for (pos = plugins; pos; pos = pos->next)
		data->plugs[i++] = strdup(pos->plug.def->name);
	data->p_cnt = i;

	menu = gtk_menu_new();
	gtk_signal_connect (GTK_OBJECT(menu), "selection-done",
						GTK_SIGNAL_FUNC(cb_menu_selectiondone), data);
	data->menu = menu;

	if (data->i_cnt == 1) {
		item = gtk_menu_item_new_with_label (data->idents[0]);
		gtk_widget_set_sensitive (GTK_WIDGET(item), FALSE);
		gtk_menu_append (GTK_MENU(menu), item);
		gtk_menu_append (GTK_MENU(menu), gtk_menu_item_new());
		plugins_add_menu (menu, data, 0);
		gtk_menu_append (GTK_MENU(menu), gtk_menu_item_new());
	} else if (data->i_cnt > 1) {
		for (i=0; i<data->i_cnt; i++) {
			item = gtk_menu_item_new_with_label (data->idents[i]);

			sub = gtk_menu_new();
			plugins_add_menu (sub, data, i);

			gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), sub);
			gtk_menu_append (GTK_MENU(menu), item);
		}
	}
	item = gtk_menu_item_new_with_label ("Clear all");
	gtk_signal_connect (GTK_OBJECT(item), "activate",
						GTK_SIGNAL_FUNC(cb_menu_clear), data);
	gtk_menu_append (GTK_MENU(menu), item);

	gtk_widget_show_all (menu);
	if (event) {
		gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL,
						event->button, event->time);
	} else {
		gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL,
						0, gtk_get_current_event_time());
	}
}

static void cb_plugins_button (GtkWidget *widget, void *plug_v)
{
	plugins_popup_menu (NULL, plug_v);
}
static gboolean cb_plugins_entry (GtkWidget *widget, GdkEventButton *event,
								  void *plug_v)
{
	if (event && event->button == 3) {
		plugins_popup_menu (event, plug_v);
		gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "button_press_event");
		return TRUE;
	}
	return FALSE;
}

/*********************************************************************
  Create a new option page with the name 'plug->name" "suffix' and
  two widgets:
    1. Toggle to enable/disable the plugin processing call
       (only if PLUG_PAGE_NODISABLE is not set in flags).
    2. String for plugin names (only if PLUG_PAGE_NOPLUG is not set in
	   flags). The plugin gets called one after another for all data
	   from plugins which are entered here.
  Return the number of the newly created page.
*********************************************************************/
int plug_add_default_page (plugDefinition *plugDef, const char *suffix,
						   plugPageFlags flags)
{
#define TTIP	"List of all plugin names this plugin should handle, " \
				"separated by the idents this plugin observes. "\
				"The plugin gets called one after another for every entry."
	int page;
	plugPlugin *plug = ((plugDefinitionPriv*)plugDef)->plug;
	GtkWidget *box, *hbox, *label, *entry;

	if (!plug) return -1;

	plug->flags = flags;
	if (*plug->plugins == PLUGINS_FIRST)
		*plug->plugins = '\0';

	if (plug->defname)
		g_free (plug->defname);
	if (suffix && *suffix)
		plug->defname = g_strjoin (" ", plugDef->name, suffix, NULL);
	else
		plug->defname = g_strdup (plugDef->name);
	page = opts_page_append (plug->defname);

	if (!(flags & PLUG_PAGE_NODISABLE))
		opts_toggle_create (page, OPT_ENABLE,
							"Should the plugin process function be called?",
							&plug->enabled);

	if (!(flags & PLUG_PAGE_NOPLUG)) {
		gui_threads_enter();

		box = gtk_vbox_new (FALSE, 1);

		gtk_box_pack_start (GTK_BOX (opts_page_get__nl(page)->widget), box, FALSE, FALSE, 0);

		label = gtk_label_new ("Plugins to handle (empty: handle all):");
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new (FALSE, 2);
		gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

		entry = gtk_entry_new_with_max_length (PLUG_PLUGINS_LEN-1);
		plug->p_entry = entry;
		gtk_entry_set_editable (GTK_ENTRY(entry), FALSE);
		gui_tooltip_set (entry, TTIP);
		gtk_entry_set_text (GTK_ENTRY(entry), plug->plugins);
		gtk_editable_set_position (GTK_EDITABLE(entry), -1);
		gtk_signal_connect (GTK_OBJECT(entry), "button_press_event",
							GTK_SIGNAL_FUNC(cb_plugins_entry), plug);
		gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

		label = gtk_button_new_with_label (" ... ");
		gui_tooltip_set (label, TTIP);
		gtk_signal_connect (GTK_OBJECT(label), "clicked",
							GTK_SIGNAL_FUNC(cb_plugins_button), plug);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		gtk_object_set_user_data (GTK_OBJECT(label), entry);

		gtk_widget_show_all (box);

		opts_varstring_add (plug_name(plugDef, "Plugins"),
							plugins_set_value, entry, plug->plugins, PLUG_PLUGINS_LEN);
		gui_threads_leave();
	}

	return page;
}
