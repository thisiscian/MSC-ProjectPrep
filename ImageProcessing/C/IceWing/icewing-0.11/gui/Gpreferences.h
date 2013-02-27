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

#ifndef iw_Gpreferences_H
#define iw_Gpreferences_H

#include <gtk/gtk.h>
#include "Gimage.h"

#define PREF_TREE_NONE			0
#define PREF_TREE_IMAGES		1
#define PREF_TREE_CATEGORIES	2
#define PREF_TREE_BOTH			3

typedef struct {
	iwImgFormat format;			/* Format for saving of images */
	char fname[PATH_MAX];		/* File name for image saving */
	float framerate;			/* Frame rate for avi files */
	int quality;
	gboolean save_full;			/* Save full window ? */
	gboolean show_message;		/* Show a message after saving of an image ? */
	gboolean reset_cnt;			/* Reset file name counter ? */
	gboolean save_atexit;		/* Save session file at program exit? */
	gboolean save_pan;			/* Save pan/zoom values in session file? */
	int use_tree;				/* Tree/list for 'Categories' */
	int use_scrolled;			/* 'Categories' with scrollbars? */
	char pdfviewer[PATH_MAX];	/* Add widgets for render options automaically ? */
} prevPrefs;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Return pointer to global variables. Used in the preferences gui.
*********************************************************************/
prevPrefs* iw_prefs_get (void);

/*********************************************************************
  Open the prefs window.
*********************************************************************/
void iw_prefs_open (void);

/*********************************************************************
  Init the prefs window (so that all opts_..._widgets() are
  initialized, but do not show it.
*********************************************************************/
void iw_prefs_init (void);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gpreferences_H */
