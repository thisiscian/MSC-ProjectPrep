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
 * Contains the source code implementing the ICEWing::Plugin interface.
 */

#include <stdio.h>
#include "min_cxx.h"

namespace ICEWING
{

/**
 *  Free all resources that have been dynamically allocated while using this plugin.
 */
MinPlugin::~MinPlugin()
{
	printf ("PluginCleanup() called...\n");
}

/**
 *  Initialize the plugin.
 *
 *  @param para Command line parameters for main program.
 *  @param argc Plugin specific command line parameter count.
 *  @param argv Plugin specific command line parameters.
 */
void MinPlugin::Init (grabParameter *para, int argc, char **argv)
{
	int i;

	printf ("PluginInit() of %s called with %d argument(s)\n",
			name, argc);
	for (i=0; i<argc; i++)
		printf ("    %d: %s\n", i, argv[i]);

	/** Register the plugin for data of type "image" */
	plug_observ_data (this, "image");
}

/**
 *  Initialize the user interface for the plugin.
 *
 *  @return Number of the created option page.
 */
int MinPlugin::InitOptions()
{
	printf ("PluginInitOptions() called...\n");
	return -1;
}

/**
 *  Process the data that was registered for observation.
 *
 *  @param  ident  String describing the data type (registered data).
 *  @param  data   The currently available data
 *
 *  @return Set to FALSE in order to cancel the execution of the further
 *          plugins. Set to TRUE otherwise.
 */
bool MinPlugin::Process (char *ident, plugData *data)
{
	grabImageData *gimg = (grabImageData*) data->data;

	printf ("Grabbed image:  id: %d, size: %d x %d, downsampling: %f x %f\n"
			"\t\tgrabbing time: %ld sec %ld msec\n",
			gimg->img_number, gimg->img.width, gimg->img.height,
			gimg->downw, gimg->downh,
			(long)gimg->time.tv_sec, (long)gimg->time.tv_usec);

	/** Let further plugins be processed. */
	return TRUE;
}

} // namespace ICEWING

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
extern "C" plugDefinition* plug_get_info (int instCount, bool* append)
{
	*append = TRUE;

	/** Create a new plugin class instance. Note that you can instanciate any class
	    that is derived of type ICEWING::Plugin. */
	ICEWING::Plugin* newPlugin =
		new ICEWING::MinPlugin (g_strdup_printf("c++Min%d", instCount));

	return newPlugin;
}
