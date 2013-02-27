/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2012
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

#ifndef __STDC_VERSION__
#  define __STDC_VERSION__ 199409
#endif

#include "config.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "dacs.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>

#include "gui/Gtools.h"
#include "gui/Ggui_i.h"
#include "gui/Goptions.h"
#include "gui/Gsession.h"
#include "tools/tools.h"
#include "tools/fileset.h"
#include "tools/img_video.h"
#include "output_i.h"
#include "plugin.h"
#include "plugin_gui.h"
#include "plugin_i.h"
#include "mainloop.h"
#include "avvideo/AVOptions.h"
#include "wing.xpm"
#include <wchar.h>

char *ICEWING_NAME = "iceWing";
char *ICEWING_VERSION = "0.11";

/* Default size of the displayed images */
#define PREV_WIDTH	(IW_PAL_WIDTH/2)
#define PREV_HEIGHT	(IW_PAL_HEIGHT/2)

#define SYNC_LEVEL	0

/*********************************************************************
  Automaticly called on program exit (-> clean up).
*********************************************************************/
static void stop_it (void)
{
	plug_cleanup_all();
	iw_output_cleanup();
}

/*********************************************************************
  Automaticly called on SIGINT and -TERM (-> clean up).
*********************************************************************/
static void stop_it_sig (int sig)
{
	gui_try_exit (0);
}

static void help_err (char **argv)
{
	fprintf (stderr, "Use '%s -h' for help.\n\n", argv[0]);
	gui_exit (1);
}

static void version_disp (void)
{
	fprintf (stderr, "\n%s V%s (October 3 2012), (c) 1999-2012 by Frank Loemker\n",
			 ICEWING_NAME, ICEWING_VERSION);
}
static void version (void)
{
	version_disp();
	gui_exit (0);
}

/*********************************************************************
  Show information about plugins.
*********************************************************************/
static void cb_help_plugs (plugInfoPlug *info, void *data)
{
	if (!info->plug)
		fprintf (stderr, "  %s\n", info->name);
	else
		fprintf (stderr, "      %s (%s)\n",
				 info->name, info->enabled ? "enabled":"disabled");
}

static void help (void)
{
	version_disp();
	fprintf (stderr,
			 "Usage: %s [-n name] [-sg [inputDrv] [drvOptions] | -sd stream [synclev] |\n"
			 "                -sp <fileset>... | -sp1 <fileset>...] [-prop] [-c cnt] [-f [cnt]]\n"
			 "               [-r factor] [-stereo] [-bayer [method] [pattern]] [-crop x y w h]\n"
			 "               [-rot {0|90|180|270}] [-nyuv name] [-nrgb name] [-oi [interval]]\n"
			 "               [-of] [-os] [-p <width>x<height>] [-rc config-file|config-setting]\n"
			 "               [-ses session-file] [-l libs] [-lg libs] [-a plugin args] [-d plugins]\n"
			 "               [-iconic] [-t talklevel] [-time <cnt|plugins|all>...]\n"
			 "               [--help] [--version] [@file]\n",
			 ICEWING_NAME);
	fprintf (stderr,
			 "-n        name for DACS registration, default: %s\n"
			 "-sg       %s"
			 "-sd       use DACS stream with a specified sync level (default: %d) for input\n"
			 "-sp       use fileset for input, e.g. -sp image%%03d.ppm r image%%d.gif y image.jpg\n"
			 "          fileset = fileset | 'y' | 'r' | 'e' | 'E' | 'f' | 'F' | 'file'\n"
			 "          'y'|'r': following images are YUV|RGB-images, default: RGB\n"
			 "          'e'|'E': open files/check only extension to verify if files\n"
			 "                   are movie files, default: open files\n"
			 "          'f'|'F': FPS correct/single frames from movies, default: FPS\n"
			 "          'file' may contain %%d, %%t, and %%T\n"
			 "-sp1      use fileset for input and play it only once\n"
			 "-prop     do not create a GUI for changing grabber properties\n"
			 "-c        number of images queued in memory at the same time, default: 2\n"
			 "-f        grab full images, optional argument gives number of images\n"
			 "          to queue in memory at the same time, default: 1\n"
			 "-r        upsample factor needed to get to full sized images, default: 1\n"
			 "-stereo   expect interlaced gray stereo image and decompose it\n"
			 "-bayer    expect gray image with bayer pattern, use specified method and\n"
			 "          specified bayer pattern to decompose it, default: down RGGB;\n"
			 IW_IMG_BAYER_HELP
			 "              pattern: RGGB | BGGR | GRBG | GBRG\n"
			 "-crop     crop a rectangle from the input image\n"
			 "-rot      rotate the input image\n"
			 "-nyuv     specify the name of the id under which the images shall be stored,\n"
			 "          default: '"IW_GRAB_IDENT"'\n"
			 "-nrgb     specify the name of the id under which the RGB images shall be stored,\n"
			 "          default: '"IW_GRAB_RGB_IDENT"'\n"
			 "-oi       output every 'interval' image (default: 1) on stream <%s>_images\n"
			 "          and provide a function <%s>_setCrop(\"x1 y1 x2 y2\")\n"
			 "          to crop the streamed images\n"
			 "-of       provide a function <%s>_control(char) to control the gui via DACS,\n"
			 "          a function <%s>_getSettings(void) to get the current widget settings\n"
			 "          and a function <%s>_getImg(imgspec) to get the current image\n"
			 "-os       output some (currently very few) status informations on\n"
			 "          stream <%s>_status\n"
			 "-p        size of preview windows, default: %dx%d\n"
			 "-rc       if the argument contains a '=', set gui options according to the argument;\n"
			 "          otherwise, load the config-file additionaly to \"%s\"\n"
			 "-ses      load, use, and save the session-file instead of \"%s\"\n"
			 "-l        load plugins from libraries 'libs'\n"
			 "-lg       load plugins from libraries 'libs' and make its symbols glabally available\n"
			 "-a        arguments for plugin 'plugin',\n"
			 "          help of 'plugin' is shown with -a 'plugin' \"-h\"\n"
			 "-d        disable plugins, eg. -d \"backpro imgclass\"\n"
			 "-iconic   start main window iconified\n"
			 "-t        TalkLevel, output messages only if respective level < TalkLevel\n"
			 "          default: 5, used range of levels: 0..4\n"
			 "-time     how often timers are given out, default: all 50 mainloop runs;\n"
			 "          for which plugins process() execution time is measured;\n"
			 "          all: measure all plugin instances; eg. -time \"5 backpro imgclass\"\n"
			 "--help    display this help and exit\n"
			 "--version display version information and exit\n"
			 "@file     replace the argument '@file' with the content of file\n",
			 IW_DACSNAME, AVDriverHelp(), SYNC_LEVEL, IW_DACSNAME, IW_DACSNAME,
			 IW_DACSNAME, IW_DACSNAME, IW_DACSNAME, IW_DACSNAME,
			 PREV_WIDTH, PREV_HEIGHT,
			 opts_get_default_file(), iw_session_get_name());

	fprintf (stderr,
			 "\nCurrently loaded plugin libraries and registered plugins:\n");
	plug_info_plugs (cb_help_plugs, NULL);

	gui_exit (0);
}

static void plugin_init (char **argv, char *plug, char *args, grabParameter *para)
{
	char **p_argv;
	int p_argc;
	plugPlugin *plugin;

	if (!iw_load_args (args, 0, NULL, &p_argc, &p_argv))
		help_err (argv);

	if ((plugin = plug_get_by_name(plug))) {
		plug_init_prepare (plugin, p_argc, p_argv);
	} else {
		fprintf (stderr, "No plugin of name '%s' found, perhaps not registered?\n",
				 plug);
		help_err (argv);
	}
}

/*********************************************************************
  Append strings to str with ' ' in between, realloc str if needed.
  len is always strlen(str), max contains the size of the allocated
  memory for str.
*********************************************************************/
static void str_append (char **str, int *len, int *max, ...)
{
	va_list args;
	char *s;
	int l = 1;

	va_start (args, max);
	s = va_arg (args, char*);
	while (s) {
		l += strlen (s) + 3;
		s = va_arg (args, char*);
	}
	va_end (args);

	/* Extend str in bigger chunks */
	if (l < 2000) l = 2000;
	if (*max - *len < l) {
		*max = *len + l;
		*str = realloc (*str, *max);
	}
	va_start (args, max);
	s = va_arg (args, char*);
	while (s) {
		(*str)[*len] = ' '; (*len)++;
		(*str)[*len] = '"'; (*len)++;

		strcpy (&(*str)[*len], s);
		*len += strlen(s);

		(*str)[*len] = '"'; (*len)++;
		(*str)[*len] = '\0';

		s = va_arg (args, char*);
	}
	va_end (args);
}

/*********************************************************************
  Parse and initialise the arguments.
*********************************************************************/
#define ARG_TEMPLATE \
	"-N:dr -SG:1 -SD:2r -SP:3r -SP1:4r -PROP:p -NYUV:nr -NRGB:Nr -C:ci -F:fio -R:ri -STEREO:s -BAYER:b -CROP:C -ROT:Ji -O:Oc -OF:6 -OI:7io -OS:8 -P:Pr -RC:Rr -SES:Sr -ICONIC:I -T:Ti -A:Ar -D:Dr -L:lr -LG:Lr -H:H -HELP:H --HELP:H -VERSION:V --VERSION:V -TIME:tr"
static void init_args (int argc, char **argv, grabParameter *para)
{
	void *arg;
	char ch, *ch_arg, *name;
	int nr, rcfile_cnt;
	int *plug_num, plug_cnt, args_len, args_max;
	char *grab_args;

	para->dname = NULL;
	para->prev_width = PREV_WIDTH;
	para->prev_height = PREV_HEIGHT;
	para->output = IW_OUTPUT_NONE;
	rcfile_cnt = 0;
	para->rcfile = NULL;
	para->session = NULL;
	para->time_cnt = 50;

	/* Parse arguments */
	if (!iw_load_args (NULL, argc, argv, &argc, &argv))
		help_err(argv);

	plug_num = malloc (sizeof(int)*argc / 3);
	plug_cnt = 0;
	grab_args = NULL;
	args_len = args_max = 0;

	nr = 1;
	while (nr < argc) {
		ch = iw_parse_args (argc, argv, &nr, &arg, ARG_TEMPLATE);
		switch (ch) {
			case 'd':				/* -n */
				para->dname = (char*)arg;
				break;
			case '6':				/* -of */
				para->output |= IW_OUTPUT_FUNCTION;
				break;
			case '8':				/* -os */
				para->output |= IW_OUTPUT_STATUS;
				break;
			case 'O':				/* -o[fis] */
				ch_arg = (char*)arg;
				while (ch_arg && *ch_arg) {
					if (toupper((int)*ch_arg) == 'F') {
						para->output |= IW_OUTPUT_FUNCTION;
					} else if (toupper((int)*ch_arg) == 'I') {
						para->output |= IW_OUTPUT_STREAM;
					} else if (toupper((int)*ch_arg) == 'S') {
						para->output |= IW_OUTPUT_STATUS;
					} else {
						fprintf (stderr, "Only f, i, and s are allowed with option -o!\n");
						help_err (argv);
					}
					ch_arg++;
				}
				break;
			case 'P':
				if (sscanf ((char*)arg, "%dx%d",
							&para->prev_width, &para->prev_height) != 2) {
					fprintf (stderr,
							 "Argument format must be (width)x(height), e.g.: 120x100!\n");
					help_err (argv);
				}
				break;
			case 'T':
				iw_debug_set_level ((int)(long)arg);
				break;
			case 't': {				/* -time */
				plugPlugin *plug;
				char *ptrptr;

				for (name=strtok_r((char*)arg, " \t,;\n", &ptrptr);
					 name;
					 name = strtok_r(NULL, " \t,;\n", &ptrptr)) {
					if ((plug = plug_get_by_name(name))) {
						plug_init_timer (plug);
					} else if (!strcmp (name, "all")) {
						plug_init_timer_all();
					} else {
						char *end = NULL;
						para->time_cnt = strtol (name, &end, 10);
						if (end && !*end) {
							iw_time_set_enabled (para->time_cnt > 0);
						} else {
							fprintf (stderr, "Argument '%s' for -time must be "
									 "plugin names, 'all', or an integer!\n", name);
							help_err (argv);
						}
					}
				}
				break;
			}
			case 'R':				/* -rc */
				if (rcfile_cnt == 0)
					rcfile_cnt = 2;
				else
					rcfile_cnt++;
				para->rcfile = realloc (para->rcfile, sizeof(char*)*rcfile_cnt);
				para->rcfile[rcfile_cnt-2] = (char*)arg;
				para->rcfile[rcfile_cnt-1] = NULL;
				break;
			case 'S':				/* -ses */
				para->session = (char*)arg;
				break;
			case 'A':				/* -a */
				if (nr < argc) {
					plug_num[plug_cnt++] = nr-1;
					nr++;
				} else {
					fprintf (stderr, "Option '-a' needs two arguments!\n");
					help_err (argv);
				}
				break;
			case 'D': {				/* -d */
				plugPlugin *plug;
				char *ptrptr;

				for (name=strtok_r((char*)arg, " \t,;\n", &ptrptr);
					 name;
					 name = strtok_r(NULL, " \t,;\n", &ptrptr)) {
					if ((plug = plug_get_by_name(name))) {
						plug_set_enable (plug, FALSE);
					} else {
						fprintf (stderr, "No plugin matching '%s' registered!\n", name);
						help_err (argv);
					}
				}
				break;
			}
			case 'l':				/* -l */
			case 'L': {				/* -lg */
				char *ptrptr;
				for (name=strtok_r((char*)arg, " \t,;\n", &ptrptr);
					 name;
					 name = strtok_r(NULL, " \t,;\n", &ptrptr)) {
					if (!plug_load (name, ch == 'L')) {
						fprintf (stderr, "Unable to init plugin from '%s'!\n", name);
						help_err (argv);
					}
				}
				break;
			}
			case 'I':				/* -iconic */
				gui_set_iconified (TRUE);
				break;
			case 'H':				/* -h */
				help();
				break;
			case 'V':				/* --version */
				version();
				break;
			case '\0':
				help_err (argv);
				break;

				/* Argumnts for the grabbing plugin */
			case 'n':				/* -nyuv */
			case 'N':				/* -nrgb */
			case 'c':				/* -c */
			case 'r':				/* -r */
			case 'J':				/* -rot */
				str_append (&grab_args, &args_len, &args_max,
							argv[nr-2], argv[nr-1], NULL);
				break;
			case 'f':				/* -f */
			case '7':				/* -oi */
				if (arg != IW_ARG_NO_NUMBER)
					str_append (&grab_args, &args_len, &args_max,
								argv[nr-2], argv[nr-1], NULL);
				else
					str_append (&grab_args, &args_len, &args_max, argv[nr-1], NULL);
				break;
			case 'p':				/* -prop */
			case 's':				/* -stereo */
				str_append (&grab_args, &args_len, &args_max, argv[nr-1], NULL);
				break;
			case 'C': {				/* -crop */
				int i;
				str_append (&grab_args, &args_len, &args_max, argv[nr-1], NULL);
				for (i=0; i<4 && nr<argc; i++) {
					str_append (&grab_args, &args_len, &args_max, argv[nr], NULL);
					nr++;
				}
				break;
			}
			case '1':				/* -sg */
			case '2':				/* -sd */
			case '3':				/* -sp */
			case '4':				/* -sp1 */
			case 'b':				/* -bayer */
				if (ch == '1' || ch == 'b')
					str_append (&grab_args, &args_len, &args_max, argv[nr-1], NULL);
				else
					str_append (&grab_args, &args_len, &args_max,
								argv[nr-2], argv[nr-1], NULL);
				while (nr<argc && argv[nr][0]!='-') {
					str_append (&grab_args, &args_len, &args_max, argv[nr], NULL);
					nr++;
				}
				break;
			default:
				fprintf (stderr, "Unknown character %c!\n",ch);
				help_err (argv);
		}  /* case */
	}  /* while */

	/* Create the DACS name if it is not given as an argument */
	if (!para->dname)
		para->dname = strdup (IW_DACSNAME);
	iw_output_set_name (para->dname);

	for (nr=0; nr<plug_cnt; nr++) {
		if (!strcmp (argv[plug_num[nr]], "GrabImage1")) {
			char *args = g_strjoin (" ", argv[plug_num[nr]+1], grab_args, NULL);
			plugin_init (argv, argv[plug_num[nr]], args, para);
			g_free (args);
			free (grab_args);
			grab_args = NULL;
		} else
			plugin_init (argv, argv[plug_num[nr]], argv[plug_num[nr]+1], para);
	}
	free (plug_num);

	if (grab_args) {
		if (grab_args[0]) {
			if (!plug_get_by_name("GrabImage1"))
				plug_load ("grab", FALSE);
			plugin_init (argv, "GrabImage1", grab_args, para);
		}
		free (grab_args);
	}

	if (para->session) iw_session_set_name (para->session);
}

int main (int argc, char **argv)
{
	static grabParameter para;		/* Is used in exit proc, therefore static */
	pthread_t main_thread = (pthread_t) NULL;
#ifdef __alpha
	pthread_attr_t attr;
#endif

#ifdef RTLD_LAZY
	/* Normally XInitThreads() should not be necessary, as X is called at the
	   same time only from one thread. But starting with X11/XCB (perhaps, or
	   something undiscovered) XCB threading bugs sometimes occur. Perhaps
	   this hack helps. Or should we always call XInitThreads()? */
	void *handle = dlopen (NULL, RTLD_LAZY);
	if (handle) {
		if (dlsym (handle, "xcb_flush")) XInitThreads();
		dlclose (handle);
	}
#endif

	/* Try to prevent any changes of the default streams to wide character. */
	fwide (stdout, -1);
	fwide (stderr, -1);

	gtk_set_locale();
	g_thread_init (NULL);
	gdk_threads_init();
	gtk_init (&argc, &argv);

	plug_load_intern();
	gui_set_prgname (ICEWING_NAME, ICEWING_VERSION);
	gui_set_icon (wing_xpm);
	init_args (argc, argv, &para);

	atexit (stop_it);
	signal (SIGINT, stop_it_sig);
	signal (SIGTERM, stop_it_sig);

	plug_init_all (&para);

#ifdef IW_DEBUG
	if (iw_debug_get_level() > 5) gdk_rgb_set_verbose (TRUE);
#endif
	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	gdk_threads_enter();
	gui_init (para.rcfile, para.prev_width, para.prev_height);
	plug_gui_init();

#ifdef __alpha
	/* The OSF alpha stack seems to be very small (40k), enlarge it */
	if (pthread_attr_init (&attr))
		iw_error ("Unable to set stack size of thread");
	if (pthread_attr_setstacksize (&attr, MAX(PTHREAD_STACK_MIN, 100000)))
		iw_error ("Unable to set stack size of thread");
	pthread_create (&main_thread, &attr,
					(void*(*)(void*))iw_mainloop_start, &para);
#else
	pthread_create (&main_thread, NULL,
					(void*(*)(void*))iw_mainloop_start, &para);
#endif

	gtk_main();
	gdk_threads_leave();

	return 0;
}
