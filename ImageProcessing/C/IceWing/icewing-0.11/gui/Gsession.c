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
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "Ggui_i.h"
#include "Gdialog.h"
#include "Gpreferences.h"
#include "Gpreview_i.h"
#include "Gsession.h"
#include "Gtools.h"

#define ID_MAXLEN		10

typedef struct sesSession {
	char *title;
	int x, y, width, height;
	float zoom;
	int pan_x, pan_y;
	struct sesSession *next;
} sesSession;

typedef struct sesWindows {
	char *title;
	GtkWidget *window;
	prevBuffer *buf;
	struct sesWindows *next;
} sesWindows;

static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Session information loaded on startup from a file */
static sesSession *session = NULL;
/* List of all open Windows */
static sesWindows *windows;

static char session_name[PATH_MAX] = "";

/*********************************************************************
  Return name of the session file. Returned value is a pointer to a
  static variable.
*********************************************************************/
char *iw_session_get_name (void)
{
	if (!session_name[0])
		gui_convert_prg_rc (session_name, "session", TRUE);
	return session_name;
}

/*********************************************************************
  Set name of the session file.
*********************************************************************/
void iw_session_set_name (const char *name)
{
	gui_strlcpy (session_name, name, PATH_MAX);
}

/*********************************************************************
  Delete the session file and thus clear the session.
*********************************************************************/
void iw_session_clear (void)
{
	char *name = iw_session_get_name();
	unlink (name);
}

/*********************************************************************
  Save session information for all open windows to file
  `iw_session_get_name()`.
*********************************************************************/
void iw_session_save (void)
{
	sesWindows *w;
	int x, y, width, height;
	char *name = iw_session_get_name();
	FILE *file = NULL;
	gboolean save_pan = iw_prefs_get()->save_pan;

	pthread_mutex_lock (&session_mutex);
	gui_threads_enter();
	w = windows;
	while (w) {
		if (w->title && w->window) {
			if (!file) {
				if (!(file = fopen (name, "w"))) {
					gui_message_show__nl ("Error",
										  "Unable to open '%s' for saving the session!",
										  name);
					goto cleanup;
				}
				if (fprintf (file,"# Session information for %s V%s\n\n",
							 GUI_PRG_NAME, GUI_VERSION) <= 0) {
					gui_message_show__nl ("Error",
										  "Error saving session information in '%s'!",
										  name);
					goto cleanup;
				}
			}
			gdk_window_get_root_origin (w->window->window, &x, &y);
			gdk_window_get_size (w->window->window, &width, &height);
			if (save_pan && w->buf)
				fprintf (file,"\"%s\" = x %d y %d w %d h %d zoom %f dx %d dy %d\n",
						 w->title, x, y, width, height,
						 w->buf->zoom, w->buf->x, w->buf->y);
			else
				fprintf (file,"\"%s\" = x %d y %d w %d h %d\n",
						 w->title, x, y, width, height);
		}
		w = w->next;
	}
 cleanup:
	gui_threads_leave();
	pthread_mutex_unlock (&session_mutex);

	if (file) fclose (file);
}

static gboolean session_parse (char **str, char *id)
{
	char *pos = *str;
	int i = 0;

	if (!pos) return FALSE;

	while (isspace ((int)*pos)) pos++;

	while (*pos && i < ID_MAXLEN && !isspace ((int)*pos))
		id[i++] = *pos++;
	id[i] = '\0';
	*str = pos;

	return *pos && i < ID_MAXLEN;
}

/*********************************************************************
  Load new (not already loaded) session information from file
  `iw_session_get_name()`.
*********************************************************************/
gboolean iw_session_load (void)
{
	char *line = NULL, *id, *value;
	char *name = iw_session_get_name();
	FILE *file = NULL;
	int pos, line_len = 0;

	if (!(file = fopen (name, "r"))) return FALSE;

	pthread_mutex_lock (&session_mutex);
	while (gui_fgets (&line, &line_len, file)) {
		if (line[0] && line[strlen(line)-1]=='\n')
			line[strlen(line)-1] = '\0';
		if (gui_parse_line (line, &id, &value)) {
			sesSession *ses = session;
			while (ses) {
				if (!strcmp (id, ses->title)) break;
				ses = ses->next;
			}
			if (!ses) {
				char token[ID_MAXLEN+1];

				ses = g_malloc (sizeof(sesSession));
				ses->title = g_strdup (id);

				ses->x = ses->y = ses->width = ses->height = -999;
				ses->pan_x = ses->pan_y = -999;
				ses->zoom = -999;

				while (session_parse (&value, token)) {
					pos = 0;
					if (token[0] && !token[1]) {
						switch (token[0]) {
							case 'x':
								if (sscanf (value, "%d%n", &ses->x, &pos) < 1)
									value = NULL;
								break;
							case 'y':
								if (sscanf (value, "%d%n", &ses->y, &pos) < 1)
									value = NULL;
								break;
							case 'w':
								if (sscanf (value, "%d%n", &ses->width, &pos) < 1)
									value = NULL;
								break;
							case 'h':
								if (sscanf (value, "%d%n", &ses->height, &pos) < 1)
									value = NULL;
								break;
							default:
								value = NULL;
								break;
						}
					} else if (!strcmp (token, "zoom")) {
						if (sscanf (value, "%f%n", &ses->zoom, &pos) < 1)
							value = NULL;
					} else if (!strcmp (token, "dx")) {
						if (sscanf (value, "%d%n", &ses->pan_x, &pos) < 1)
							value = NULL;
					} else if (!strcmp (token, "dy")) {
						if (sscanf (value, "%d%n", &ses->pan_y, &pos) < 1)
							value = NULL;
					} else {
						value = NULL;
					}
					if (value)
						value += pos;
				}
				if (ses->x >= 0 || ses->y >= 0) {
					if (ses->x < 0)
						ses->x = 0;
					if (ses->y < 0)
						ses->y = 0;
				}
				if (ses->width > 0 || ses->height > 0) {
					if (ses->width <= 0)
						ses->width = prev_get_default_width();
					if (ses->height <= 0)
						ses->height = prev_get_default_height();
				}
				ses->next = session;
				session = ses;
			}
		}
	}
	if (line) g_free (line);
	if (file) fclose (file);
	pthread_mutex_unlock (&session_mutex);
	return TRUE;
}

/*********************************************************************
  PRIVATE: Apply the session entry for 'title', free the entry
  afterwards, and add window to the list of session managed windows.
  set_pos: Set position of window ?
  set_size: Set size of window ?
  Return: Session entry for 'title' found and applied?
*********************************************************************/
gboolean iw_session_set_prev (GtkWidget *window, prevBuffer *buf,
							  const char *title, gboolean set_pos, gboolean set_size)
{
	static int screen_width = 0, screen_height = 0;
	sesSession *ses, *old;
	sesWindows *win;

	pthread_mutex_lock (&session_mutex);

	/* Add window to thw list of session managed windows */
	win = windows;
	while (win) {
		if (!strcmp (title, win->title)) break;
		win = win->next;
	}
	if (!win) {
		win = g_malloc (sizeof(sesWindows));
		win->title = g_strdup (title);
		win->next = windows;
		windows = win;
	}
	win->window = window;
	win->buf = buf;

	if (screen_width == 0 || screen_height == 0) {
		screen_width  = gdk_screen_width ();
		screen_height = gdk_screen_height ();
	}

	/* Set position and size */
	old = NULL;
	ses = session;
	while (ses) {
		if (!strcmp (title, ses->title)) {
			if (set_pos
				&& ses->x >= -10 && ses->x + 32 < screen_width
				&& ses->y >= -10 && ses->y + 32 < screen_height) {
#if GTK_CHECK_VERSION(2,0,0)
				char geom[30];
				sprintf (geom, "+%d+%d", ses->x, ses->y);
				gtk_window_parse_geometry (GTK_WINDOW(window), geom);
#else
				gtk_widget_set_uposition (window, ses->x, ses->y);
#endif
			}

			if (set_size && ses->width > 0 && ses->height > 0)
				gtk_window_set_default_size (GTK_WINDOW(window),
											 ses->width, ses->height);

			if (buf && (ses->pan_x >= 0 || ses->pan_y >= 0 || ses->zoom >= -0.01))
				prev_pan_zoom__nl (buf, ses->pan_x, ses->pan_y, ses->zoom);

			if (old)
				old->next = ses->next;
			else
				session = ses->next;
			g_free (ses->title);
			g_free (ses);
			pthread_mutex_unlock (&session_mutex);
			return TRUE;
		}
		old = ses;
		ses = ses->next;
	}

	pthread_mutex_unlock (&session_mutex);
	return FALSE;
}

/*********************************************************************
  Apply the session entry for 'title', free the entry afterwards, and
  add window to the list of session managed windows.
  set_pos: Set position of window ?
  set_size: Set size of window ?
  Return: Session entry for 'title' found and applied?
*********************************************************************/
gboolean iw_session_set (GtkWidget *window, const char *title,
						 gboolean set_pos, gboolean set_size)
{
	return iw_session_set_prev (window, NULL, title, set_pos, set_size);
}

/*********************************************************************
  Return if a session entry with 'title' was loaded.
*********************************************************************/
gboolean iw_session_find (const char *title)
{
	sesSession *ses;
	gboolean ret = FALSE;

	pthread_mutex_lock (&session_mutex);
	ses = session;
	while (ses) {
		if (!strcmp (title, ses->title)) {
			ret = TRUE;
			break;
		}
		ses = ses->next;
	}
	pthread_mutex_unlock (&session_mutex);
	return ret;
}

/*********************************************************************
  Remove window with 'title' from the list of session managed windows,
  e.g. if window was closed.
*********************************************************************/
void iw_session_remove (const char *title)
{
	sesWindows *old = NULL, *win;

	pthread_mutex_lock (&session_mutex);
	win = windows;
	while (win) {
		if (!strcmp (title, win->title)) {
			if (old)
				old->next = win->next;
			else
				windows = win->next;
			g_free (win->title);
			g_free (win);
			break;
		}
		old = win;
		win = win->next;
	}
	pthread_mutex_unlock (&session_mutex);
}
