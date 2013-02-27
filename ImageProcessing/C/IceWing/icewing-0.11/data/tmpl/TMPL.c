/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*********************************************************************
  A small iceWing plugin as a starting point for own plugins.
  More information about iceWing can be found in the documentation
  icewing.pdf and in the installed header files.
*********************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "gui/Grender.h"
#include "main/plugin.h"

/* All gui parameter of one plugin instance */
typedef struct {
	int value;
} tmplValues;

/* All parameter of one plugin instance */
typedef struct tmplPlugin {
	plugDefinition def;		/* iceWing base class, a plugin */
	tmplValues values;
	prevBuffer *b_image;
} tmplPlugin;

/*********************************************************************
  Free the resources allocated during tmpl_xxx().
*********************************************************************/
static void tmpl_cleanup (plugDefinition *plug)
{
}

/*********************************************************************
  Show help message and exit.
*********************************************************************/
static void help (tmplPlugin *plug)
{
	fprintf (stderr,"\n%s plugin for %s, (c) 2005-2009 by AUTHORS\n"
			 "Plugin template (replace with a short docu).\n"
			 "\n"
			 "Usage of the %s plugin: no options (yet)\n",
			 plug->def.name, ICEWING_NAME, plug->def.name);
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para'    : command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
static void tmpl_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	tmplPlugin *plug = (tmplPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, "-H:H");
		switch (ch) {
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}

	/* imageRGB instead of image for images in RGB color space.
	   Different identifier if results from other plugins instead
	   of grab should be used. */
	plug_observ_data (plug_d, "image");
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int tmpl_init_options (plugDefinition *plug_d)
{
	tmplPlugin *plug = (tmplPlugin *)plug_d;
	int p;

	/* Init plugin parameter. */
	plug->values.value = 5;

	/* Init/Display gui widgets. */
	p = plug_add_default_page (plug_d, NULL, (plugPageFlags)0);

	opts_entscale_create (p, "Value", "Tooltip for the demo slider",
						  &plug->values.value, 1, 100);

	/* Float slider example
	   opts_float_create (p, "Float value", "Tooltip for the float slider",
	                      &plug->values.float_value, 1.5, 3.5);

	   Toggle button example
	   opts_toggle_create (p, "Toggle", "Tooltip for the toggle button",
	                       &plug->values.bool_value);
	*/

	/* Add a window to display data.
	   Parameter to prev_new_window():
		   name  : Unique title for the window
		   -1, -1: Use default window width/height
		   FALSE : Color images can be displayed
		   FALSE : Do not show the window immediately
	*/
	plug->b_image = prev_new_window (plug_name(plug_d, ".Result"),
									 -1, -1, FALSE, FALSE);

	return p;
}

/*********************************************************************
  Process the data that was registered for observation.
  ident : The id passed to plug_observ_data(), describes 'data'.
  data  : The currently available data,
          result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL tmpl_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	tmplPlugin *plug = (tmplPlugin *)plug_d;
	iwImage *img = &((grabImageData*)data->data)->img;
	int w = img->width, h = img->height;

	iw_debug (3, "Plugin '%s' called with data '%s'.",
			  plug->def.name, ident);

	/* Process the input data.
	   ... */

	/* Display the processed image. */
	prev_render (plug->b_image, img->data, w, h, IW_YUV);

	/* Render some (quite senseless) vector graphics. */
	{
		prevDataRect r[] = {
			{IW_RGB, 0,255,0, 80,80,150,140},
			{IW_RGB, 0,0,255, 120,90,190,150}};

		prev_render_frects (plug->b_image, r, 2, 0, w, h);
		prev_render_text (plug->b_image, 0, w, h, 230, 30,
						  "<rotate=90 fg=\"255 0 0\">Value:</> %d\n"
						  "Second line",
						  plug->values.value);
	}
	/* Display the data on the screen. */
	prev_draw_buffer (plug->b_image);

	return TRUE;
}

static plugDefinition plug_tmpl = {
	"TMPL%d",
	PLUG_ABI_VERSION,
	tmpl_init,
	tmpl_init_options,
	tmpl_cleanup,
	tmpl_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *plug_get_info (int instCount, BOOL *append)
{
	tmplPlugin *plug = (tmplPlugin*)calloc (1, sizeof(tmplPlugin));

	plug->def = plug_tmpl;
	plug->def.name = g_strdup_printf (plug_tmpl.name, instCount);

	*append = TRUE;
	return (plugDefinition*)plug;
}
