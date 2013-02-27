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

#ifndef iw_Gimage_H
#define iw_Gimage_H

#include <sys/time.h>
#include <glib.h>

#define IW_FOURCC(a,b,c,d) \
		((unsigned int)((((unsigned int)d)<<24)+(((unsigned int)c)<<16)+(((unsigned int)b)<<8)+a))

#define IW_CTAB_SIZE	256
typedef guchar (*iwColtab)[3];

extern iwColtab iw_def_col_tab;

/* Color formats of the data for prev_render...() */
#define IW_RGB			((iwColtab)0)
#define IW_BGR			((iwColtab)1)
#define IW_YUV			((iwColtab)2)
#define IW_YUV_CAL		((iwColtab)3)		/* !Not rendered correctly! */
#define IW_HSV			((iwColtab)4)
#define IW_LUV			((iwColtab)5)		/* !Not rendered correctly! */
#define IW_LAB			((iwColtab)6)		/* !Not rendered correctly! */
#define IW_BW			((iwColtab)7)		/* Only the first plane is used */
#define IW_GRAY			IW_BW				/* Only the first plane is used */
#define IW_INDEX		(iw_def_col_tab)

#define IW_COLFORMAT_MAX	IW_BW			/* Last 'special' definition */

#define IW_COLMAX		255
#define IW_COLCNT		256

	/* For iw_movie_read(): Return the next/previous frame */
#define IW_MOVIE_NEXT_FRAME	(-1)
#define IW_MOVIE_PREV_FRAME	(-2)

extern char *iwColorText[];	/* PRIVATE, only for next macro */
#define IW_COLOR_TEXT(i) \
	((i)->ctab <= IW_COLFORMAT_MAX ? iwColorText[(long)(i)->ctab]:"PALETTE")

typedef struct _iwImgFileData iwImgFileData;
typedef struct _iwMovie iwMovie;

typedef struct iwColor {
	guchar r,g,b;
} iwColor;

/* Possible image file formats */
typedef enum {
	IW_IMG_FORMAT_UNKNOWN,
	IW_IMG_FORMAT_PNM,
	IW_IMG_FORMAT_PNG,
	IW_IMG_FORMAT_JPEG,

	IW_IMG_FORMAT_MOVIE,			/* First movie format */
	IW_IMG_FORMAT_AVI_RAW444 = IW_IMG_FORMAT_MOVIE,
	IW_IMG_FORMAT_AVI_RAW420,
	IW_IMG_FORMAT_AVI_MJPEG,
	IW_IMG_FORMAT_AVI_FFV1,
	IW_IMG_FORMAT_AVI_MPEG4,
	IW_IMG_FORMAT_MOVIE_MAX = IW_IMG_FORMAT_AVI_MPEG4,	/* Last movie format */

	IW_IMG_FORMAT_VECTOR = 200,
	IW_IMG_FORMAT_SVG = IW_IMG_FORMAT_VECTOR
} iwImgFormat;

extern char *iwImgFormatText[];		/* All formats */
extern char *iwImgFormatTextBM[];	/* Only bitmap formats */

/* Possible image data types */
typedef enum {
	IW_8U,
	IW_16U,
	IW_32S,
	IW_FLOAT,
	IW_DOUBLE
} iwType;

extern int iwTypeSize[];		/* PRIVATE, only for next macros */
extern double iwTypeMin[];		/* PRIVATE, only for next macros */
extern double iwTypeMax[];		/* PRIVATE, only for next macros */
/* Size of image data type in bytes */
#define IW_TYPE_SIZE(i)			(iwTypeSize[(i)->type])
/* Min. representable value for the image data type */
#define IW_TYPE_MIN(i)			(iwTypeMin[(i)->type])
/* Max. representable value for the image data type */
#define IW_TYPE_MAX(i)			(iwTypeMax[(i)->type])

typedef struct iwImage {
	guchar **data;
	int planes;			/* Number of image planes */
	iwType type;		/* Real data type of "data" (instead of guchar) */
	int width, height;	/* Image width/height in pixels */
	int rowstride;		/* 0 : data[0] - data[planes] contain the different image planes;
						   >0: image planes are interleaved in data[0],
						       value is distance in bytes between two lines */
	iwColtab ctab;		/* Color format of data, e.g. for rendering with prev_render() */
	void *reserved;
} iwImage;

typedef enum {
	IW_IMG_STATUS_OK,
	IW_IMG_STATUS_OPEN,		/* Unable to open file */
	IW_IMG_STATUS_READ,		/* Read error */
	IW_IMG_STATUS_WRITE,	/* Write error */
	IW_IMG_STATUS_MEM,		/* Unable to allocate memory */
	IW_IMG_STATUS_FORMAT,	/* File format not supported (png, ...)*/
	IW_IMG_STATUS_ERR,		/* Another error */
	IW_IMG_STATUS_MAX
} iwImgStatus;

/* Flags specifying what to free during iw_img_free() */
typedef enum {
	IW_IMG_FREE_PLANE		= 1 << 0,		/* img->data[x] */
	IW_IMG_FREE_PLANEPTR	= 1 << 1,		/* img->data */
	IW_IMG_FREE_CTAB		= 1 << 2,		/* img->ctab */
	IW_IMG_FREE_STRUCT		= 1 << 3		/* img */
} iwImgFree;
#define IW_IMG_FREE_DATA ((iwImgFree)(IW_IMG_FREE_PLANE | IW_IMG_FREE_CTAB | IW_IMG_FREE_PLANEPTR))
#define IW_IMG_FREE_ALL	((iwImgFree)(-1))

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Allocate memory for img->data and img->data[] according to the
  other settings in img.
  Return: TRUE if memory could be allocated.
*********************************************************************/
gboolean iw_img_allocate (iwImage *img);

/*********************************************************************
  Allocate memory only for img->data (and NOT img->data[]) according
  to the other settings in img.
  Return: TRUE if memory could be allocated.
*********************************************************************/
gboolean iw_img_alloc_planeptr (iwImage *img);

/*********************************************************************
  Initialise image img by setting everything to 0.
  Do NOT allocate any memory.
*********************************************************************/
void iw_img_init (iwImage *img);

/*********************************************************************
  Allocate a new image and set everything to 0. -> Do NOT allocate
  any image data.
*********************************************************************/
iwImage* iw_img_new (void);

/*********************************************************************
  Allocate and initialise a new img and allocate memory for
  img->data and img->data[].
*********************************************************************/
iwImage* iw_img_new_alloc (int width, int height, int planes, iwType type);

/*********************************************************************
  Free parts of the image. 'what' specifies what to free.
*********************************************************************/
void iw_img_free (iwImage *img, iwImgFree what);

/*********************************************************************
  If src has an own indexed color tabel, allocate a new one for dst
  and copy the table. Otherwise set the ctab entry of dst to the one
  of src.
*********************************************************************/
void iw_img_copy_ctab (const iwImage *src, iwImage *dst);

/*********************************************************************
  Copy src to dst, allocate all plane pointer, planes, and color
  tables, and return a pointer to dst, or NULL on error (no mem).
*********************************************************************/
iwImage* iw_img_copy (const iwImage *src, iwImage *dst);

/*********************************************************************
  Return a copy of the image img. All plane pointer, planes, and
  color tables are also copied.
*********************************************************************/
iwImage* iw_img_duplicate (const iwImage *img);

/*********************************************************************
  Load image from file fname in 'planed' form.
*********************************************************************/
iwImage* iw_img_load (const char *fname, iwImgStatus *status);

/*********************************************************************
  Load image from file fname and return an interleaved image.
*********************************************************************/
iwImage* iw_img_load_interleaved (const char *fname, iwImgStatus *status);

/*********************************************************************
  Close movie file if it is != NULL and free all associated data.
*********************************************************************/
void iw_movie_close (iwMovie *movie);

/*********************************************************************
  Open the movie file fname for reading.
*********************************************************************/
iwMovie* iw_movie_open (const char *fname, iwImgStatus *status);

/*********************************************************************
  Open the movie 'fname' (if not already open) and return its frame
  'frame'.
  ATTENTION: The returned frame is a pointer to an internally used
  image which is freed by iw_movie_close().
*********************************************************************/
iwImage* iw_movie_read (iwMovie **movie, const char *fname,
						int frame, iwImgStatus *status);

/*********************************************************************
  Return frame rate of 'movie'.
*********************************************************************/
float iw_movie_get_framerate (const iwMovie *movie);

/*********************************************************************
  Return number of frames of 'movie'.
*********************************************************************/
int iw_movie_get_framecnt (const iwMovie *movie);

/*********************************************************************
  Return the number of the frame, which was the result of the
  last call to iw_movie_read()
*********************************************************************/
int iw_movie_get_framepos (iwMovie *movie);

/*********************************************************************
  Initialise struct holding settings for saving images.
*********************************************************************/
iwImgFileData *iw_img_data_create (void);

/*********************************************************************
  Free struct created with iw_img_create_data().
*********************************************************************/
void iw_img_data_free (iwImgFileData *data);

/*********************************************************************
  Set parameters used for MOVIE saving.
*********************************************************************/
void iw_img_data_set_movie (iwImgFileData *data, iwMovie **movie, float framerate);

/*********************************************************************
  Set quality parameter (used for JPEG saving),
*********************************************************************/
void iw_img_data_set_quality (iwImgFileData *data, int quality);

/*********************************************************************
  If 'format' == IW_IMG_FORMAT_UNKNOWN return a format based on the
  extension of fname. Otherwise return 'format'.
*********************************************************************/
iwImgFormat iw_img_save_get_format (iwImgFormat format, const char *fname);

/*********************************************************************
  Save 'img' in file 'fname' in the format 'format'. If
  'format' == IW_IMG_FORMAT_UNKNOWN the extension of fname is used.
*********************************************************************/
iwImgStatus iw_img_save (const iwImage *img, iwImgFormat format,
						 const char *fname, const iwImgFileData *data);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gimage_H */
