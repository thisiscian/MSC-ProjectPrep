/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker and Dirk Stoessel
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

/** @file
 * Contains the definition of the iceWing callback functions. Their sole purpose
 * is to forward any iceWing function call to the appropriate C++ plugin instance
 * method. As such, there should be no need to change this code at all when
 * writing new plugins. */

#include "config.h"
#include "plugin_cxx.h"

namespace ICEWING
{

/***********************************************************************************
  Is invoked by iceWing in order to free all dynamically allocated plugin resources.
***********************************************************************************/
static void PluginCleanup (plugDefinition *plugDef)
{
	Plugin *plug = (Plugin *)plugDef;
	if (plug) delete (plug);
}

/***********************************************************************************
  Is invoked by iceWing in order to initialise a new plugin (passing any command
  line parameters).
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
***********************************************************************************/
static void PluginInit (plugDefinition *plugDef, grabParameter *para, int argc, char **argv)
{
	Plugin *plug = (Plugin *)plugDef;
	if (plug)
		plug->Init (para, argc, argv);
}

/***********************************************************************************
  Is invoked by iceWing in order to initialise the user interface of a new plugin.
  Return: Number of the last opened option page.
***********************************************************************************/
static int PluginInitOptions (plugDefinition *plugDef)
{
	Plugin *plug = (Plugin *)plugDef;

	if (plug)
		return plug->InitOptions();
	return -1;
}

/***********************************************************************************
  Is invoked by iceWing as part of the render command chain in order to process
  any grabbed / other data.
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
***********************************************************************************/
static BOOL PluginProcess (plugDefinition *plugDef, char *ident, plugData *data)
{
	Plugin *plug = (Plugin *)plugDef;

	if (plug)
		return plug->Process (ident, data);
	return TRUE;
}

Plugin::Plugin (char *_name, int _abi_version)
{
	name         = _name;
	abi_version  = _abi_version;
	init         = PluginInit;
	init_options = PluginInitOptions;
	cleanup      = PluginCleanup;
	process      = PluginProcess;
}

} /* namespace ICEWING */
