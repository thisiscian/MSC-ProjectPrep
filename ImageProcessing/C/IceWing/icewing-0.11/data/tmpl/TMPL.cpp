/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*********************************************************************
  A small iceWing plugin as a starting point for own plugins.
  More information about iceWing can be found in the documentation
  icewing.pdf and in the installed header files.
*********************************************************************/

#include <stdio.h>

#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "TMPLlower.h"

namespace ICEWING
{

/*********************************************************************
  Free all resources that have been dynamically allocated while using
  this plugin.
*********************************************************************/
TMPL::~TMPL()
{
}

/*********************************************************************
  Show help message and exit.
*********************************************************************/
void TMPL::help()
{
	fprintf (stderr,"\n%s plugin for %s, (c) 2005-2009 by AUTHORS\n"
			 "Plugin template (replace with a short docu).\n"
			 "\n"
			 "Usage of the %s plugin: no options (yet)\n",
			 name, ICEWING_NAME, name);
	gui_exit (1);
}

/*********************************************************************
  Initialize the plugin.
  'para'    : command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
void TMPL::Init (grabParameter *para, int argc, char **argv)
{
	void *arg;
	char ch;
	int nr = 0;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, "-H:H");
		switch (ch) {
			case 'H':
			case '\0':
				help();
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help();
		}
	}

	/* imageRGB instead of image for images in RGB color space.
	   Different identifier if results from other plugins instead
	   of grab should be used. */
	plug_observ_data (this, "image");
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
int TMPL::InitOptions()
{
	int p;

	/* Init plugin parameter. */
	value = 5;

	/* Init/Display gui widgets. */
	p = plug_add_default_page (this, NULL, (plugPageFlags)0);

	opts_entscale_create (p, "Value", "Tooltip for the demo slider",
						  &value, 1, 100);

	/* Float slider example
	   opts_float_create (p, "Float value", "Tooltip for the float slider",
	                      &float_value, 1.5, 3.5);

	   Toggle button example
	   opts_toggle_create (p, "Toggle", "Tooltip for the toggle button",
                           &bool_value);
	*/

	/* Add a window to display data.
	   Parameter to prev_new_window():
		   name  : Unique title for the window
		   -1, -1: Use default window width/height
		   FALSE : Color images can be displayed
		   FALSE : Do not show the window immediately
	*/
	b_image = prev_new_window (plug_name(this, ".Result"),
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
bool TMPL::Process (char *ident, plugData *data)
{
	iwImage *img = &((grabImageData*)data->data)->img;
	int w = img->width, h = img->height;

	iw_debug (3, "Plugin '%s' called with data '%s'.",
			  name, ident);

	/* Process the input data.
	   ... */

	/* Display the processed image. */
	prev_render (b_image, img->data, w, h, IW_YUV);

	/* Render some (quite senseless) vector graphics. */
	prevDataRect r[] = {
		{IW_RGB, 0,255,0, 80,80,150,140},
		{IW_RGB, 0,0,255, 120,90,190,150}};

	prev_render_frects (b_image, r, 2, 0, w, h);
	prev_render_text (b_image, 0, w, h, 230, 30,
					  "<rotate=90 fg=\"255 0 0\">Value:</> %d\n"
					  "Second line",
					  value);

	/* Display the data on the screen. */
	prev_draw_buffer (b_image);

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

	/** Create a new plugin class instance. Note that you can instanciate
		any class that is derived from type ICEWING::Plugin. */
	ICEWING::Plugin* newPlugin =
		new ICEWING::TMPL (g_strdup_printf("TMPL%d", instCount));

	return newPlugin;
}
