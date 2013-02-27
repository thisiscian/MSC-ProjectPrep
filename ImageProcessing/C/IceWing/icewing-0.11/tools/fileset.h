/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/* PRIVATE HEADER */

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

#ifndef iw_fileset_H
#define iw_fileset_H

#include <sys/time.h>

#include "gui/Gimage.h"
#include "tools/tools.h"

/* End Of Stream, no further image,
   next call to fset_get() returns the start image */
#define IW_IMG_STATUS_EOS	(IW_IMG_STATUS_MAX)

typedef struct _fsetList fsetList;

typedef struct fsetImage {
	iwImage *img;
	char *fname;		/* File name of the image, pointer to a buffer of size PATH_MAX */
	int frame_num;		/* Image from video file -> frame number, 0 otherwise */
	BOOL rgb;			/* Image is a RGB|YUV image */
	struct timeval time; /* If a timestemp is given for the current image, the timestemp,
							otherwise, if an animation is read, a frame timestep in ms */
	int fset_pos;		/* Current position in the fileset */
} fsetImage;

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Change the entry which will be returned by fset_get().
  relative==TRUE: change relative to current position.
  Returns new position.
*********************************************************************/
int iw_fset_set_pos (fsetList *fset, int step, BOOL relative);

/*********************************************************************
  Return length of file set.
*********************************************************************/
int iw_fset_get_length (fsetList *fset);

/*********************************************************************
  Add files to the file set. If
    1. file 'name' exists it is added, otherwise
    2. 'name' contains '%t' or '%T', maching files from the directory
       are added, otherwise
    3. the files {'sprintf (file, name, 0++)'} are checked.
  File names r R y Y e E f F are interpreted as flags for
    RGB|YUV|use_ext|frameadv.
  r | y: All folowing files are RGB|YUV images.
  E    : Files are only opened to check the frame count if
         a known extension for movie files is used.
  f | F: Return frames according to the FPS value |
         Return only the frames stored in the movie
  Returns number of entries currently in the file set.
*********************************************************************/
int iw_fset_add (fsetList **fset, char *name);

/*********************************************************************
  Read current image and return it, including different meta
  information about the image.
  direction: 0 | IW_MOVIE_NEXT_FRAME | IW_MOVIE_PREV_FRAME
*********************************************************************/
iwImgStatus iw_fset_get (fsetList *fset, fsetImage *i, int direction);

#ifdef __cplusplus
}
#endif

#endif /* iw_fileset_H */
