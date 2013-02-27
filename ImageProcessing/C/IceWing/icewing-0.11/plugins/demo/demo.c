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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tools/tools.h"
#include "gui/Ggui.h"
#include "gui/Goptions.h"
#include "gui/Gpreview.h"
#include "main/image.h"
#include "main/plugin.h"

#define STRING_LENGTH	100

typedef struct {					/* Rendering parameter */
	BOOL inv_y;
	BOOL inv_u;
	BOOL inv_v;
	BOOL vectors;
	int thickness;
	int lines_cnt;
	int anchor;
	int zoom;
	int rotate;

	int t_button;					/* Parameter of the widget test pages */
	char t_string[STRING_LENGTH];
	char t_string2[STRING_LENGTH];
	char t_file[STRING_LENGTH];
	int t_entscale;
	float t_float;
	BOOL t_toggle;
	int t_option;
	optsListData t_list;
	optsListData t_list2;
	int t_radio;
} demoValues;

/* Command line arguments */
typedef struct demoParameter {
	int y1;
	int y2;
} demoParameter;

/* All parameter of one plugin instance */
typedef struct demoPlugin {
	plugDefinition def;
	demoValues values;
	demoParameter para;
	prevBuffer *b_image;
	prevBuffer *b_text;
} demoPlugin;

/*********************************************************************
  Free the resources allocated during demo_???().
*********************************************************************/
static void demo_cleanup (plugDefinition *plug)
{
}

/*********************************************************************
  Show help message and exit.
*********************************************************************/
static void help (demoPlugin *plug)
{
	fprintf (stderr,"\n%s plugin for %s, (c) 1999-2009 by Frank Loemker\n"
			 "Shows some of the widget and render possibilities of iceWing.\n"
			 "\n"
			 "Usage of the %s plugin:\n"
			 "     [-ys start] [-ye end]\n"
			 "-ys      Start line (in %%)\n"
			 "-ye      End line (in %%)\n",
			 plug->def.name, ICEWING_NAME, plug->def.name);
	gui_exit (1);
}

/*********************************************************************
  Initialise the plugin.
  'para': command line parameter for main program
  argc, argv: plugin specific command line parameter
*********************************************************************/
#define ARG_TEMPLATE "-YS:Si -YE:Ei -H:H"
static void demo_init (plugDefinition *plug_d, grabParameter *para, int argc, char **argv)
{
	demoPlugin *plug = (demoPlugin *)plug_d;
	void *arg;
	char ch;
	int nr = 0;

	plug->para.y1 = 30;
	plug->para.y2 = 90;

	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'S':
				plug->para.y1 = (int)(long)arg;
				break;
			case 'E':
				plug->para.y2 = (int)(long)arg;
				break;
			case 'H':
			case '\0':
				help (plug);
			default:
				fprintf (stderr, "Unknown character %c!\n", ch);
				help (plug);
		}
	}
	plug_observ_data (plug_d, "image");
}

/*********************************************************************
  Callback function for the InputBufWindow to move the inverted area,
  i.e. save two y-values in the plugin struct.
*********************************************************************/
static void cb_mouse_event (prevBuffer *b, prevEvent signal, int x, int y, void *data)
{
	static int *y_change = NULL;
	demoPlugin *plug = data;

	switch (signal) {
		case PREV_BUTTON_PRESS:
			if (abs(y-plug->para.y1*(b->gc.height-1)/100) <
				abs(y-plug->para.y2*(b->gc.height-1)/100))
				y_change = &plug->para.y1;
			else
				y_change = &plug->para.y2;
		case PREV_BUTTON_MOTION:
			if (y_change)
				*y_change = y*100/(b->gc.height-1);
			break;
		case PREV_BUTTON_RELEASE:
			y_change = NULL;
			break;
		default:
			break;
	}
}

/*********************************************************************
  Callback function for the opts_buttoncb() function.
*********************************************************************/
static void cb_button_click (GtkWidget *widget, int number, void *data)
{
	printf ("Button %d pressed\n", number);
}

/*********************************************************************
  Initialise the user interface for the plugin.
  Return: Number of the last opened option page.
*********************************************************************/
static int demo_init_options (plugDefinition *plug_d)
{
	static char *anchor[] = {
		"Don't Render", "Top-Left", "Top-Right",
		"Bottom-Left", "Bottom-Right", "Center", NULL};
	static char *rotate[] = {"0", "90", "180", "270", NULL};

	static char *t_option[] = {"Option 1", "Option 2", "Last Option", NULL};
	static char *t_radio[] = {"Radio1", "Radio2", "Radio3", NULL};
	static char *t_list[] = {"List entry1", "List entry2", "Last entry", NULL};
	static char *t_list2[] = {
		">Menu 1",
		"Entry 1",
		">Submenu 1",
		"Subentry 1",
		"<<>Menu 2",
		"Entry 2",
		"Entry 3",
		"<Entry 4",
		NULL
	};
	demoPlugin *plug = (demoPlugin *)plug_d;
	int p;

	/* Init rendering parameter ... */
	plug->values.inv_y = TRUE;
	plug->values.inv_u = FALSE;
	plug->values.inv_v = FALSE;
	plug->values.vectors = TRUE;
	plug->values.anchor = 5;
	plug->values.zoom = 0;
	plug->values.rotate = 0;
	plug->values.thickness = 2;
	plug->values.lines_cnt = 20;

	/* ... then init render widgets and windows. */
	p = plug_add_default_page (plug_d, NULL, 0);

	opts_toggle_create (p, "Invert y channel", NULL, &plug->values.inv_y);
	opts_toggle_create (p, "Invert u channel", NULL, &plug->values.inv_u);
	opts_toggle_create (p, "Invert v channel", NULL, &plug->values.inv_v);
	opts_toggle_create (p, "Draw vector objects", NULL, &plug->values.vectors);
	opts_entscale_create (p, "Thickness", "Thickness of line drawings",
						  &plug->values.thickness, 0, 20);
	opts_entscale_create (p, "NumLines", "Number of displayed lines",
						  &plug->values.lines_cnt, 0, 100);

	opts_option_create (p, "Text anchor:", "Where to position the long text",
						anchor, &plug->values.anchor);
	opts_toggle_create (p, "Zoom text", "Zoom text according to the window zoom?",
						&plug->values.zoom);
	opts_radio_create (p, "Rotate text:", "Rotation angle of one paragraph of the text",
						rotate, &plug->values.rotate);

	plug->b_image = prev_new_window (plug_name(plug_d, ".image"), -1, -1, FALSE, FALSE);
	plug->b_text = prev_new_window (plug_name(plug_d, ".text"), -1, -1, FALSE, FALSE);

	prev_signal_connect (plug->b_image,
						 PREV_BUTTON_PRESS|PREV_BUTTON_MOTION|PREV_BUTTON_RELEASE,
						 cb_mouse_event, plug);

	/* Two pages which show all the different widgets ... */

	/* ... first init the parameter of the widget test pages ... */
	plug->values.t_button = 0;
	strcpy (plug->values.t_string, "a string");
	strcpy (plug->values.t_string2, "Updated on enter");
	strcpy (plug->values.t_file, "a file");
	plug->values.t_entscale = 2;
	plug->values.t_float = 3.14;
	plug->values.t_toggle = FALSE;
	plug->values.t_option = 0;
	plug->values.t_radio = 1;
	plug->values.t_list.entries = NULL;
	plug->values.t_list2.entries = NULL;

	/* ... then init the widget test pages themselves. */
	p = opts_page_append (plug_name(plug_d, " WidgetTest"));

	opts_separator_create (p, "Separator");
	opts_button_create (p, "Button1|Button2", "Two button widgets|ToolTip of button 2",
						&plug->values.t_button);
	opts_buttoncb_create (p, "ButtonCB 1|Button CB 2", "Two buttons with a callback|ToolTip 2",
						  cb_button_click, &plug);

	opts_string_create (p, "Enter some text", "A string widget",
						plug->values.t_string, STRING_LENGTH);
	opts_stringenter_create (p, "Enter some text and press enter", "A string widget, which reacts on enter/return",
							 plug->values.t_string2, STRING_LENGTH);
	opts_filesel_create (p, "Enter a file/dir name", "A filesel widget",
						 plug->values.t_file, STRING_LENGTH);
	opts_entscale_create (p, "EntScale", "A entry/scale widget",
						  &plug->values.t_entscale, -7, 42);
	opts_float_create (p, "Float", "A float widget",
					   &plug->values.t_float, -0.1, 13.4);
	opts_toggle_create (p, "Toggle", "A toggle widget",
						&plug->values.t_toggle);

	p = opts_page_append (plug_name(plug_d, " WidgetTest2"));

	opts_option_create (p, "Option:", "An option widget",
						t_option, &plug->values.t_option);
	opts_radio_create (p, "Radio:", "A radio widget",
					   t_radio, &plug->values.t_radio);
	opts_list_create (p, "ListSimple", "A list widget",
					  t_list, OPTS_SELECT|OPTS_REORDER, &plug->values.t_list);
	opts_list_create (p, "List",
					  "List widget with add/data options, right click for context menu",
					  t_list2, OPTS_SELECT|OPTS_REORDER|OPTS_ADD|OPTS_DATA,
					  &plug->values.t_list2);

	return p;
}

/*********************************************************************
  Demonstrate the use of text formating options by rendering a long
  string.
*********************************************************************/
void demo_render_text (demoPlugin *plug)
{
#define WIDTH 700
#define HEIGHT 500
	static char *anchor[]={"", "tl", "tr", "bl", "br", "c"};
	prevDataLine l[2] = {
		{IW_RGB, 255,0,0, WIDTH/2, 0, WIDTH/2, 2000},
		{IW_RGB, 255,0,0, 0, HEIGHT/2, 2000, HEIGHT/2}
	};

	prev_set_bg_color (plug->b_text, 100, 100, 100);
	prev_render_lines (plug->b_text, l, 2, RENDER_CLEAR, WIDTH, HEIGHT);

	if (plug->values.anchor > 0) {
		prev_render_text (plug->b_text, 0, WIDTH, HEIGHT, WIDTH/2, HEIGHT/2,
		  "<zoom=%d anchor=%s>"
		  "Values of the <font=big bg=\"0 0 0\"><shadow=1 bg=\"index:8\">test</> widgets</>:\n"
		  "\t<bg=\"-1 0 0\">Button\t</>: <fg=\"bw:255\" bg=\"0 0 0\">%d</>\n"
		  "\t<bg=\"0 -1 0\">String\t</>: <fg=\"bw:225\" bg=\"30 0 0\">%s</>\n"
		  "\t<bg=\"0 0 -1\">String2\t</>: <fg=\"bw:225\" bg=\"60 0 0\">%s</>\n"
		  "\t<bg=\"-1 0 0\">File\t</>: |<fg=\"bw:195\" bg=\"90 0 0\">%s</>|\n"
		  "\t<bg=\"0 -1 0\">EntScale</>: <fg=\"bw:165\" bg=\"120 0 0\">%d</>\n"
		  "\t<bg=\"0 0 -1\">Float\t</>: <fg=\"bw:135\" bg=\"150 0 0\">%f</>\n"
		  "\t<rotate=%d bg=\"-1 0 0\">Toggle\t</>: <fg=\"bw:105\" bg=\"180 0 0\">%d</>\n"
		  "\t<bg=\"0 -1 0\">Option\t</>: <fg=\"bw:75\" bg=\"210 0 0\">%d</>\n"
		  "\t<bg=\"0 0 -1\">Radio\t</>: <fg=\"bw:45\" bg=\"230 0 0\">%d</>\n"
		  "<adjust=center shadow=1>****************\n"
		  "***********\n"
		  "******\n"
		  "*",
		  plug->values.zoom,
		  anchor[plug->values.anchor],
		  plug->values.t_button,
		  plug->values.t_string,
		  plug->values.t_string2,
		  plug->values.t_file,
		  plug->values.t_entscale,
		  plug->values.t_float,
		  plug->values.rotate*90,
		  plug->values.t_toggle,
		  plug->values.t_option,
		  plug->values.t_radio);
	}
	prev_draw_buffer (plug->b_text);
}

/*********************************************************************
  Process the data that was registered for observation.
  ident: The id passed to plug_observ_data(), specifies what to do.
  data : Result of plug_data_get_new (ident, NULL).
  Return: Continue the execution of the remaining plugins?
*********************************************************************/
static BOOL demo_process (plugDefinition *plug_d, char *ident, plugData *data)
{
	iwImage *img = &((grabImageData*)data->data)->img;
	int cnt, start, end, w = img->width, h = img->height;
	uchar *ys, *us, *vs, *yd, *ud, *vd;
	uchar *d[3];
	demoPlugin *plug = (demoPlugin *)plug_d;
	iw_time_add_static (time_demo, "Img Demo");

	iw_time_start (time_demo);

	/* Show all widget values for the two widget test pages. */
	printf ("Values of the test widgets:\n"
			"\tButton\t: %d\n"
			"\tString\t: |%s|\n"
			"\tString2\t: |%s|\n"
			"\tFile\t: |%s|\n"
			"\tEntScale: %d\n"
			"\tFloat\t: %f\n"
			"\tToggle\t: %d\n"
			"\tOption\t: %d\n"
			"\tRadio\t: %d\n",
			plug->values.t_button,
			plug->values.t_string,
			plug->values.t_string2,
			plug->values.t_file,
			plug->values.t_entscale,
			plug->values.t_float,
			plug->values.t_toggle,
			plug->values.t_option,
			plug->values.t_radio);
	printf ("\tListSimple:");
	for (cnt=0; plug->values.t_list.entries[cnt].oldindex>=0; cnt++)
		printf (" %s%d",
				plug->values.t_list.entries[cnt].selected ? "*":" ",
				plug->values.t_list.entries[cnt].oldindex);
	printf ("\n\tList\t:");
	for (cnt=0; plug->values.t_list2.entries[cnt].oldindex>=0; cnt++)
		printf (" %s%d |%s|",
				plug->values.t_list2.entries[cnt].selected ? "*":" ",
				plug->values.t_list2.entries[cnt].oldindex,
				plug->values.t_list2.entries[cnt].data);
	printf ("\n");

	/* Copy and partly invert the source image. */
	d[0] = iw_img_get_buffer(3*w*h);
	d[1] = d[0]+w*h;
	d[2] = d[1]+w*h;

	ys = img->data[0];
	if (img->planes > 2) {
		us = img->data[1];
		vs = img->data[2];
	} else {
		us = vs = ys;
	}
	yd = d[0];
	ud = d[1];
	vd = d[2];

	start = w*h - plug->para.y1*h/100 * w;
	end = w*h - (plug->para.y2)*h/100 * w - 1;

	cnt = w*h;
	while (cnt--) {
		if (cnt<start && cnt>end && plug->values.inv_y)
			*yd = 255-*ys;
		else
			*yd = *ys;
		if (cnt<start && cnt>end && plug->values.inv_u)
			*ud = 255-*us;
		else
			*ud = *us;
		if (cnt<start && cnt>end && plug->values.inv_v)
			*vd = 255-*vs;
		else
			*vd = *vs;
		ys++; us++; vs++;
		yd++; ud++; vd++;
	}

	/* Display the copied image. */
	prev_render (plug->b_image, d, w, h, IW_YUV);

	/* Render some vector graphics. */
	prev_set_thickness (plug->b_image, plug->values.thickness);
	if (plug->values.vectors) {
		{
			prevDataPoint p[] = {
				{10,10},{100,10}, {50,100},{10,10}};
			prevDataPoint p2[] = {
				{110,10},{150,10},{140,50},{170,50},{160,10},{200,10}, {150,100},{110,10}};
			prevDataPoly poly = {IW_INDEX, weiss,0,0, 4,p};

			prev_render_polys (plug->b_image, &poly, 1, RENDER_THICK, w, h);
			poly.npts = sizeof(p2)/sizeof(prevDataPoint);
			poly.pts = p2;
			prev_render_fpolys (plug->b_image, &poly, 1, 0, w, h);
		} {
			prevDataEllipse e[] = {
				{IW_INDEX,  gruen, 0, 0,  50,50, 30,10,  30,10,270, TRUE},
				{IW_INDEX,   blau, 0, 0,  50,50, 30,10,  30,10,270,FALSE},
				{IW_INDEX,violett, 0, 0, 100,50, 30,10,  70, 0,360, TRUE},
				{IW_INDEX, orange, 0, 0, 150,50, 30,10, 130,50,290,FALSE}};
			prev_render_ellipses (plug->b_image, e, 4, RENDER_THICK, w, h);
		} {
			prevDataCircle c[] = {
				{IW_INDEX, weiss,0,0, 20,90, 10},
				{IW_INDEX, weiss,0,0, 40,90, 20}};
			prev_render_fcircles (plug->b_image, c, 2, 0, w, h);
			c[0].r = rot;
			c[1].r = schwarz;
			prev_render_circles (plug->b_image, c, 2, RENDER_THICK, w, h);
		} {
			prevDataRect r[] = {
				{IW_INDEX,      holz,0,0, 80,80,150,140},
				{IW_INDEX, elfenbein,0,0, 120,90,190,150}};
			prev_render_frects (plug->b_image, &r[0], 1, RENDER_THICK, w, h);
			prev_render_rects (plug->b_image, &r[1], 1, RENDER_THICK, w, h);
		} {
			prevDataText t = {IW_INDEX, rot,0,0, 200, 25,
							  "<rotate=90 bg=\"-1 -1 150\">A short</> sentence."};
			prev_render_texts (plug->b_image, &t, 1, 0, w, h);
		}
		if (plug->values.lines_cnt > 0) {
			prevDataLine *lines;

			end = plug->values.lines_cnt;
			lines = malloc (sizeof(prevDataLine)*end);
			for (cnt=0; cnt<end; cnt++) {
				lines[cnt].ctab = IW_INDEX;
				lines[cnt].r = weiss;
				lines[cnt].x1 = abs((int)(sin(plug->para.y1*G_PI/100) * w));
				lines[cnt].y1 = plug->para.y1*h/100;
				lines[cnt].x2 = w*(cnt+1)/end;
				lines[cnt].y2 = plug->para.y2*h/100;
			}
			prev_render_lines (plug->b_image, lines, cnt, RENDER_THICK, w, h);
			free (lines);
		}
		demo_render_text (plug);
	}
	prev_draw_buffer (plug->b_image);

	iw_time_stop (time_demo, FALSE);

	iw_img_release_buffer();

	/* "unclick" the button */
	plug->values.t_button = 0;

	return TRUE;
}

static plugDefinition plug_demo = {
	"Demo%d",
	PLUG_ABI_VERSION,
	demo_init,
	demo_init_options,
	demo_cleanup,
	demo_process
};

/*********************************************************************
  Called on load of the plugin. Returns the filled plugin definition
  structure and whether the plugin should be inserted at the start or
  at the end of the iceWing internal plugin list.
*********************************************************************/
plugDefinition *plug_get_info (int instCount, BOOL *append)
{
	demoPlugin *plug = calloc (1, sizeof(demoPlugin));

	plug->def = plug_demo;
	plug->def.name = g_strdup_printf (plug_demo.name, instCount);

	*append = TRUE;
	return (plugDefinition*)plug;
}
