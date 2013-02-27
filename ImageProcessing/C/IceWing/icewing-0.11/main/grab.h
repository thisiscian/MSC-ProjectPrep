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

#ifndef iw_grab_types_H
#define iw_grab_types_H

#include "gui/Gimage.h"
#include "gui/Gpreview.h"
#include "dacs.h"
#include "tools/tools.h"

#define IW_DACSNAME			"icewing"			/* Default DACS name */

/* Used as a source for the default preview window size */
#define IW_PAL_WIDTH			768
#define IW_PAL_HEIGHT			576

#define IW_OUTPUT_NONE			0
#define IW_OUTPUT_FUNCTION		(1<<0)
#define IW_OUTPUT_STREAM		(1<<1)
#define IW_OUTPUT_STATUS		(1<<2)

#define IW_GRAB_IDENT			"image"
#define IW_GRAB_RGB_IDENT		"imageRGB"

/* Type of the data the grabbing plugin is providing for other plugins
   under the identifiers "image" and "imageRGB" */
typedef struct grabImageData {
	iwImage img;			/* The image data */
	struct timeval time;	/* Time the image was grabbed */
	int img_number;			/* Consecutive number of the grabbed image */
	char *fname;			/* If image was read from a file, the name of the file */
	int frame_number;		/* Image from video file -> frame number, 0 otherwise */
	float downw, downh;		/* Down sampling, which was applied to the image */
	void *reserved1;
	void *reserved2;
} grabImageData;

typedef struct grabParameter {
	char *dname;			/* Name for DACS registration */
	int output;				/* What to output via DACS (see IW_OUTPUT_...) */
	int prev_width, prev_height;
	char **rcfile;			/* rc-files to load additionaly */
	char *session;			/* Session-file to load additionaly */
	int time_cnt;			/* How often time measurements are given out */
} grabParameter;

#endif /* iw_grab_types_H */

#ifndef IW_GRAB_TYPES
#ifndef iw_grab_protos_H
#define iw_grab_protos_H

#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  The iceWing mainloop creates one preview window, where e.g. the grab
  plugin displays the first image. Return a pointer to this window,
  so that other plugins can display additional data in this window.
*********************************************************************/
prevBuffer *grab_get_inputBuf (void);

/*********************************************************************
  Free img including all substructures.
*********************************************************************/
void grab_image_free (grabImageData *img);

/*********************************************************************
  Allocate a new image and init everything. -> Do NOT allocate any
  image data.
*********************************************************************/
grabImageData* grab_image_new (void);

/*********************************************************************
  Return a deep copy of img.
*********************************************************************/
grabImageData* grab_image_duplicate (const grabImageData *img);

/*********************************************************************
  Return the grabbed image with
    time!=NULL: a grab-time most similar to time
    time==NULL: number img_num (0: last grabbed image) if still in
                memory or NULL otherwise.
    img_num<=0: return a full size image, if available
  grab_get_image(): Use the first registered grabbing plugin.
*********************************************************************/
grabImageData* grab_get_image (int img_num, const struct timeval *time);
grabImageData* grab_get_image_from_plug (plugDefinition *plug,
										 int img_num, const struct timeval *time);

/*********************************************************************
  Indicate that the img obtained with grab_get_image() is not needed
  any longer.
  grab_release_image(): Use the first registered grabbing plugin.
*********************************************************************/
void grab_release_image (const grabImageData *img);
void grab_release_image_from_plug (plugDefinition *plug, const grabImageData *img);

#ifdef __cplusplus
}
#endif

#endif /* iw_grab_protos_H */
#endif /* IW_GRAB_TYPES */
