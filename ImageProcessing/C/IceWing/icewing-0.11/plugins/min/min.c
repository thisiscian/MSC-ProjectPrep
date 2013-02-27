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

#include <stdio.h>
#include "main/plugin.h"

/*********************************************************************
  Free the resources allocated during min_???().
*********************************************************************/
static void min_cleanup (plugDefinition *plug)
{
	printf ("min_cleanup() called...\n");
}

/*********************************************************************
  Initialise the plugin.
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
static void min_init (plugDefinition *plug, grabParameter *para, int argc, char **argv)
{
	int i;

	printf ("min_init() of %s called with %d argument(s)\n",
			plug->name, argc);
	for (i=0; i<argc; i++)
		printf ("    %d: %s\n", i, argv[i]);

	plug_observ_data (plug, "image");
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int min_init_options (plugDefinition *plug)
{
	printf ("min_init_options() called...\n");
	return -1;
}

/*********************************************************************
  Process the data that was registered for observation.
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL min_process (plugDefinition *plug, char *ident, plugData *data)
{
	grabImageData *gimg = (grabImageData*)data->data;

	printf ("Grabbed image:  id: %d, size: %d x %d, downsampling: %f x %f\n"
			"\t\tgrabbing time: %ld sec %ld msec\n",
			gimg->img_number, gimg->img.width, gimg->img.height,
			gimg->downw, gimg->downh,
			(long)gimg->time.tv_sec, (long)gimg->time.tv_usec);

	return TRUE;
}

static plugDefinition plug_min = {
	"Min",
	PLUG_ABI_VERSION,
	min_init,
	min_init_options,
	min_cleanup,
	min_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *plug_get_info (int instCount, BOOL *append)
{
	*append = TRUE;
	return &plug_min;
}
