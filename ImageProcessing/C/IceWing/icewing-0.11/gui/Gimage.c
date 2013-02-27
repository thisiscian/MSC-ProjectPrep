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
#include <string.h>
#include <ctype.h>

#ifdef WITH_GPB
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef WITH_PNG
#include <png.h>
#endif

#ifdef WITH_JPEG
#include "jpeglib.h"
#endif

#include "tools/tools.h"
#include "Gtools.h"
#include "Gimage_i.h"

#include <setjmp.h>

/* ATTENTION: Must be dividable by 3*8 */
#define BUF_SIZE 3000

char *iwColorText[] = {
	"RGB",
	"BGR",
	"YUV",
	"YUV_CAL",
	"HSV",
	"LUV",
	"LAB",
	"GRAY",
	NULL
};
char *iwImgFormatText[] = {
	"By Extension",
	"PNM/ICE",
	"PNG",
	"JPEG",
	"AVI YUV444 (Movie)",
	"AVI YUV420 (Movie)",
	"AVI MJPEG (Movie)",
	"AVI FFV1 (Movie)",
	"AVI MPEG4 (Movie)",
	"SVG (Vector)",
	NULL
};
char *iwImgFormatTextBM[] = {
	"By Extension",
	"PNM/ICE",
	"PNG",
	"JPEG",
	"AVI YUV444 (Movie)",
	"AVI YUV420 (Movie)",
	"AVI MJPEG (Movie)",
	"AVI FFV1 (Movie)",
	"AVI MPEG4 (Movie)",
	NULL
};

static guchar def_col_tab_a[IW_CTAB_SIZE][3] = {
	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100},

	{ 80,  80,  80}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{255, 180,  70}, {  0,   0, 255}, {  0, 255,   0}, {255,   0, 255},
	{200, 170, 150}, { 20,  20,  20}, {  0,   0,   0}, {100, 100, 100},
	{100, 100, 100}, {100, 100, 100}, {100, 100, 100}, {100, 100, 100}};

iwColtab iw_def_col_tab = def_col_tab_a;

int iwTypeSize[] = {
	sizeof(guchar),
	sizeof(guint16),
	sizeof(gint32),
	sizeof(gfloat),
	sizeof(gdouble)
};
double iwTypeMin[] = {
	0,
	0,
	G_MININT,
	-G_MAXFLOAT,
	-G_MAXDOUBLE
};
double iwTypeMax[] = {
	IW_COLMAX,
	G_MAXSHORT*2+1,
	G_MAXINT,
	G_MAXFLOAT,
	G_MAXDOUBLE
};

static char *imgTypeText[] = {
	"8U",
	"16U",
	"32S",
	"FLOAT",
	"DOUBLE",
	NULL
};

/*********************************************************************
  Allocate memory only for img->data (and NOT img->data[]) according
  to the other settings in img.
  Return: TRUE if memory could be allocated.
*********************************************************************/
gboolean iw_img_alloc_planeptr (iwImage *img)
{
	/* Always at least 3 plane pointer. In theory unneeded, but might be
	   easier sometimes if all planes pointers are available.  */
	int planes = img->planes;
	if (planes < 3) planes = 3;
	if ((img->data = calloc (planes, sizeof(guchar*))))
		return TRUE;
	else
		return FALSE;
}

/*********************************************************************
  Allocate memory for img->data and img->data[] according to the
  other settings in img.
  Return: TRUE if memory could be allocated.
*********************************************************************/
gboolean iw_img_allocate (iwImage *img)
{
	int planes = img->planes;

	/* Sanity check */
	if (img->width <= 0 || img->height <= 0 || planes <= 0 ||
		( ((double)img->width+100) * ((double)img->height+100) *
		  planes * IW_TYPE_SIZE(img) ) >= INT_MAX)
		return FALSE;

	/* Always at least 3 plane pointer. In theory unneeded, but might be
	   easier sometimes if all planes pointers are available. */
	if (planes < 3) planes = 3;
	if (!(img->data = calloc (planes, sizeof(guchar*))))
		return FALSE;

	if (img->rowstride) {
		if (!(img->data[0] = iw_malloc_align (img->rowstride*img->height, NULL))) {
			free (img->data);
			img->data = NULL;
			return FALSE;
		}
	} else {
		int i, count = img->width*img->height*IW_TYPE_SIZE(img);

		for (i=0; i<img->planes; i++)
			if (!(img->data[i] = iw_malloc_align (count, NULL))) {
				while (i--) {
					iw_free_align (img->data[i]);
					img->data[i] = NULL;
				}
				free (img->data);
				img->data = NULL;
				return FALSE;
			}
	}
	return TRUE;
}

/*********************************************************************
  Initialise image img by setting everything to 0.
  Do NOT allocate any memory.
*********************************************************************/
void iw_img_init (iwImage *img)
{
	img->data = NULL;
	img->planes = 0;
	img->type = IW_8U;
	img->width = 0;
	img->height = 0;
	img->rowstride = 0;
	img->ctab = IW_RGB;
}

/*********************************************************************
  Allocate a new image and set everything to 0. -> Do NOT allocate
  any image data.
*********************************************************************/
iwImage* iw_img_new (void)
{
	return calloc (1, sizeof(iwImage));
}

/*********************************************************************
  Allocate and initialise a new img and allocate memory for
  img->data and img->data[].
*********************************************************************/
iwImage* iw_img_new_alloc (int width, int height, int planes, iwType type)
{
	iwImage *img = iw_img_new();
	img->width = width;
	img->height = height;
	img->planes = planes;
	img->type = type;
	if (!iw_img_allocate(img)) {
		iw_img_free (img, IW_IMG_FREE_ALL);
		return NULL;
	}
	return img;
}

/*********************************************************************
  Free parts of the image. 'what' specifies what to free.
*********************************************************************/
void iw_img_free (iwImage *img, iwImgFree what)
{
	if (!img) return;

	if (img->data) {
		if (what & IW_IMG_FREE_PLANE) {
			int p;
			if (img->rowstride)
				p = 0;
			else
				p = img->planes-1;
			for (; p>=0; p--) {
				if (img->data[p]) iw_free_align (img->data[p]);
				img->data[p] = NULL;
			}
		}
		if (what & IW_IMG_FREE_PLANEPTR) {
			free (img->data);
			img->data = NULL;
		}
	}
	if ((what & IW_IMG_FREE_CTAB) && img->ctab) {
		if (img->ctab > IW_COLFORMAT_MAX && img->ctab != IW_INDEX)
			free (img->ctab);
		img->ctab = IW_RGB;
	}
	if (what & IW_IMG_FREE_STRUCT)
		free (img);
}

/*********************************************************************
  If src has an own indexed color tabel, allocate a new one for dst
  and copy the table. Otherwise set the ctab entry of dst to the one
  of src.
*********************************************************************/
void iw_img_copy_ctab (const iwImage *src, iwImage *dst)
{
	if (src->ctab > IW_COLFORMAT_MAX && src->ctab != IW_INDEX) {
		dst->ctab = malloc (sizeof(src->ctab[0])*IW_CTAB_SIZE);
		memcpy (dst->ctab, src->ctab, sizeof(src->ctab[0])*IW_CTAB_SIZE);
	} else
		dst->ctab = src->ctab;
}

/*********************************************************************
  Copy src to dst, allocate all plane pointer, planes, and color
  tables, and return a pointer to dst, or NULL on error (no mem).
*********************************************************************/
iwImage* iw_img_copy (const iwImage *src, iwImage *dst)
{
	if (!src) return NULL;

	if (!dst) dst = iw_img_new();
	if (!dst) return NULL;

	*dst = *src;
	if (iw_img_allocate(dst)) {
		if (src->rowstride) {
			memcpy (dst->data[0], src->data[0], src->rowstride*src->height);
		} else {
			int i, count = src->width*src->height*IW_TYPE_SIZE(src);
			for (i=0; i<src->planes; i++)
				memcpy (dst->data[i], src->data[i], count);
		}
		iw_img_copy_ctab (src, dst);
	} else {
		return NULL;
	}
	return dst;
}

/*********************************************************************
  Return a copy of the image img. All plane pointer, planes, and
  color tables are also copied.
*********************************************************************/
iwImage* iw_img_duplicate (const iwImage *img)
{
	iwImage *dst;

	if (!img) return NULL;

	dst = iw_img_new();
	if (!iw_img_copy (img, dst)) {
		iw_img_free (dst, IW_IMG_FREE_ALL);
		return NULL;
	}
	return dst;
}

static guchar* swap_buffer (guchar *in, guchar *out, int cnt, iwType type, gboolean swap)
{
	if (swap) {
		int i;

		cnt /= iwTypeSize[type];
		switch (type) {
			case IW_16U:
				for (i=0; i<cnt; i++)
					((guint16*)out)[i] = GUINT16_SWAP_LE_BE(((guint16*)in)[i]);
				return out;
			case IW_32S:
				for (i=0; i<cnt; i++)
					((gint32*)out)[i] = (gint32)GUINT32_SWAP_LE_BE(((gint32*)in)[i]);
				return out;
			case IW_FLOAT:
				iw_assert (sizeof(gfloat) == sizeof(gint32), "No IEEE float type");
				for (i=0; i<cnt; i++)
					((gint32*)out)[i] = (gint32)GUINT32_SWAP_LE_BE(((gint32*)in)[i]);
				return out;
			case IW_DOUBLE:
				iw_assert (sizeof(gdouble) == sizeof(gint64), "No IEEE double type");
				for (i=0; i<cnt; i++)
					((gint64*)out)[i] = (gint32)GUINT32_SWAP_LE_BE(((gint64*)in)[i]);
				return out;
			default:
				iw_error ("Can't swap buffer of type %d", type);
				break;
		}
	}
	return in;
}

#ifdef WITH_JPEG
typedef struct jpegError {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buf;
} jpegError;

static void jpeg_error_exit (j_common_ptr cinfo)
{
	jpegError *err = (jpegError*)cinfo->err;

	/* Display error message */
	/* (*cinfo->err->output_message) (cinfo); */
	longjmp (err->setjmp_buf, 1);
}

static void jpeg_error_emit (j_common_ptr cinfo, int msg_level)
{
	/* Ignore any messages */
	return;
}
#endif

/*********************************************************************
  Save image img in file fname in jpeg (jfif) format.
*********************************************************************/
static iwImgStatus img_save_jpeg (const iwImage *img, const char *fname, int quality)
{
#ifdef WITH_JPEG
	FILE *file;
	struct jpeg_compress_struct cinfo;
	struct jpegError jerr;
	int y, x, i, rowstride;
	JSAMPROW row;

	if (IW_TYPE_SIZE(img) > 1)
		return IW_IMG_STATUS_ERR;
	if (!(file = fopen(fname, "wb")))
		return IW_IMG_STATUS_OPEN;

	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	jerr.pub.emit_message = jpeg_error_emit;

	if (setjmp(jerr.setjmp_buf)) {
		/* If we get here, the JPEG code has signaled an error.
		   So clean up the JPEG object, close the input file, and return. */
		jpeg_destroy_compress (&cinfo);
		fclose (file);
		return IW_IMG_STATUS_ERR;
	}

	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, file);

	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	if (img->planes<3 && img->ctab <= IW_COLFORMAT_MAX) {
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	} else {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
	}
	/*
	  More correct would be:
	else if (img->ctab == IW_YUV || img->ctab == IW_YUV_CAL)
		cinfo.in_color_space = JCS_YCbCr;
	else if (img->ctab == IW_RGB)
		cinfo.in_color_space = JCS_RGB;
	else
		cinfo.in_color_space = JCS_UNKNOWN;
	*/

	jpeg_set_defaults (&cinfo);
	jpeg_set_quality (&cinfo, CLAMP(quality,0,100), TRUE);
	jpeg_start_compress (&cinfo, TRUE);

	rowstride = img->rowstride;
	if (!rowstride && img->planes < 3 && img->ctab <= IW_COLFORMAT_MAX)
		rowstride = img->width;

	row = malloc (img->width * cinfo.input_components * sizeof(JSAMPLE));
	if (rowstride) {
		int sbpp = img->planes;
		int dbpp = cinfo.input_components;
		guchar *r = img->data[0];

		for (y=0; y<img->height; y++) {
			if (!img->rowstride || sbpp == dbpp) {
				jpeg_write_scanlines (&cinfo, (JSAMPROW*)&r, 1);
			} else {
				for (x=0; x<img->width; x++)
					memcpy (row+x*dbpp, r+x*sbpp, dbpp);
				jpeg_write_scanlines (&cinfo, &row, 1);
			}
			r += rowstride;
		}
	} else {
		guchar *r = img->data[0], *g = img->data[1], *b = img->data[2];

		for (y=0; y<img->height; y++) {
			i = 0;
			for (x=0; x<img->width; x++) {
				if (img->ctab > IW_COLFORMAT_MAX) {
					row[i++] = img->ctab[*r][0];
					row[i++] = img->ctab[*r][1];
					row[i++] = img->ctab[*r++][2];
				} else {
					row[i++] = *r++;
					row[i++] = *g++;
					row[i++] = *b++;
				}
			}
			jpeg_write_scanlines (&cinfo, &row, 1);
		}
	}
	free (row);

	jpeg_finish_compress (&cinfo);
	fclose (file);
	jpeg_destroy_compress (&cinfo);

	return IW_IMG_STATUS_OK;
#else
	iw_warning ("Compiled without JFIF-saving-support");
	return IW_IMG_STATUS_FORMAT;
#endif
}

/*********************************************************************
  Save image img in file fname in png format. 16 bit and palette mode
  are supported.
*********************************************************************/
static iwImgStatus img_save_png (const iwImage *img, const char *fname)
{
#ifdef WITH_PNG
	FILE *file;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_color_8 sig_bit;
	int x, y, i, p, color, rowstride, planes;
	guchar *row;
	int bytes = IW_TYPE_SIZE(img);

	if (bytes > 2)
		return IW_IMG_STATUS_ERR;
	if (!(file = fopen(fname, "wb")))
		return IW_IMG_STATUS_OPEN;

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose (file);
		return IW_IMG_STATUS_MEM;
	}
	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
		fclose (file);
		return IW_IMG_STATUS_MEM;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		fclose (file);
		return IW_IMG_STATUS_ERR;
	}

	png_init_io (png_ptr, file);

	if (img->planes < 3) {
		if (img->ctab > IW_COLFORMAT_MAX)
			color = PNG_COLOR_TYPE_PALETTE;
		else
			color = PNG_COLOR_TYPE_GRAY;
	} else
		color = PNG_COLOR_TYPE_RGB;
	if (img->planes == 2 || img->planes > 3)
		color |= PNG_COLOR_MASK_ALPHA;

	png_set_IHDR (png_ptr, info_ptr, img->width, img->height, bytes*8,
				  color, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				  PNG_FILTER_TYPE_DEFAULT);

	if ((color & PNG_COLOR_TYPE_PALETTE) == PNG_COLOR_TYPE_PALETTE) {
		png_color pal[IW_CTAB_SIZE];
		for (i=0; i<IW_CTAB_SIZE; i++) {
			pal[i].red = img->ctab[i][0];
			pal[i].green = img->ctab[i][1];
			pal[i].blue = img->ctab[i][2];
		}
		png_set_PLTE (png_ptr, info_ptr, pal, IW_CTAB_SIZE);
	}

	sig_bit.red = bytes*8;
	sig_bit.green = bytes*8;
	sig_bit.blue = bytes*8;
	sig_bit.gray = bytes*8;
	sig_bit.alpha = bytes*8;
	png_set_sBIT (png_ptr, info_ptr, &sig_bit);

	png_write_info (png_ptr, info_ptr);

	if (bytes > 1 && G_BYTE_ORDER != G_BIG_ENDIAN)
		png_set_swap (png_ptr);

	rowstride = img->rowstride;
	if (!rowstride && img->planes == 1)
		rowstride = img->width * bytes;

	planes = img->planes < 4 ? img->planes:4;
	row = malloc (img->width * planes * bytes);
	if (rowstride) {
		guchar *r = img->data[0];
		int sbpp = bytes * img->planes;
		int dbpp = bytes * planes;

		for (y=0; y<img->height; y++) {
			if (img->planes <= 4) {
				png_write_row (png_ptr, (png_bytep)r);
			} else {
				for (x=0; x<img->width; x++)
					memcpy (row+x*dbpp, r+x*sbpp, dbpp);
				png_write_row (png_ptr, (png_bytep)row);
			}
			r += rowstride;
		}
	} else if (bytes == 1) {
		guchar **data = malloc (sizeof(guchar*)*planes);
		memcpy (data, img->data, sizeof(guchar*)*planes);

		for (y=0; y<img->height; y++) {
			i = 0;
			for (x=0; x<img->width; x++)
				for (p = 0; p < planes; p++)
					row[i++] = *(data[p])++;
			png_write_row (png_ptr, (png_bytep)row);
		}
		free (data);
	} else {
		guint16 *row16 = (guint16*)row;
		guint16 **data = malloc (sizeof(guint16*)*planes);
		memcpy (data, img->data, sizeof(guint16*)*planes);

		for (y=0; y<img->height; y++) {
			i = 0;
			for (x=0; x<img->width; x++)
				for (p = 0; p < planes; p++)
					row16[i++] = *(data[p])++;
			png_write_row (png_ptr, (png_bytep)row16);
		}
		free (data);
	}
	free (row);

	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);

	fclose (file);
	return IW_IMG_STATUS_OK;
#else
	iw_warning ("Compiled without PNG-saving-support");
	return IW_IMG_STATUS_FORMAT;
#endif
}

/*********************************************************************
  Save image img in file fname in PPM/PGM format (IW_8U and IW_16U)
  or in the PPM similar PI format (other types).
*********************************************************************/
static iwImgStatus img_save_ppm (const iwImage *img, const char *fname)
{
	FILE *file = NULL;
	int count, rowstride, i;
	guchar **data = NULL;
	guchar *swapBuf = NULL;
	gboolean swap;
	iwImgStatus status = IW_IMG_STATUS_OK;

	if (!(file = fopen(fname, "wb")))
		return IW_IMG_STATUS_OPEN;

	if (IW_TYPE_SIZE(img) > 2 || img->planes == 2 || img->planes > 3)
		fprintf (file, "PI\n%d %d %d %s\n-1\n",
				 img->width, img->height,
				 (img->planes==1 && img->ctab > IW_COLFORMAT_MAX) ? 3:img->planes,
				 imgTypeText[img->type]);
	else
		fprintf (file, "P%d\n%d %d\n%ld\n",
				 (img->planes==1 && img->ctab <= IW_COLFORMAT_MAX) ? 5:6,
				 img->width, img->height,
				 (((long)1) << (IW_TYPE_SIZE(img)*8)) - 1);

	rowstride = img->rowstride;
	if (!rowstride && img->planes == 1 && img->ctab <= IW_COLFORMAT_MAX)
		rowstride = img->width*IW_TYPE_SIZE(img);

	swap = IW_TYPE_SIZE(img) > 1 && G_BYTE_ORDER != G_BIG_ENDIAN;
	if (rowstride) {
		guchar *y = img->data[0];
		guchar *out;
		count = img->width*img->planes*IW_TYPE_SIZE(img);

		if (swap) swapBuf = malloc (count);
		for (i=0; i<img->height; i++) {
			out = swap_buffer (y, swapBuf, count, img->type, swap);
			if (fwrite (out, 1, count, file) != count) {
				status = IW_IMG_STATUS_WRITE;
				goto cleanup;
			}
			y += rowstride;
		}
	} else {
		int p, planes = img->planes;
		data = malloc (sizeof(guchar*)*planes);

		memcpy (data, img->data, sizeof(guchar*)*planes);
		count = img->width*img->height;

		if (IW_TYPE_SIZE(img) == 1) {
			/* Save 8 bit planed image */
			guchar *y = data[0];
			char buffer[BUF_SIZE];

			i = 0;
			while (count--) {
				if (img->ctab > IW_COLFORMAT_MAX) {
					buffer[i++] = img->ctab[*y][0];
					buffer[i++] = img->ctab[*y][1];
					buffer[i++] = img->ctab[*y++][2];
				} else {
					for (p = 0; p < planes; p++)
						buffer[i++] = *(data[p])++;
				}
				if (i > BUF_SIZE - planes) {
					if (fwrite (buffer, 1, i, file) != i) {
						status = IW_IMG_STATUS_WRITE;
						goto cleanup;
					}
					i = 0;
				}
			}
			if (i > 0) {
				if (fwrite (buffer, 1, i, file) != i) {
					status = IW_IMG_STATUS_WRITE;
					goto cleanup;
				}
			}
		} else {
			/* Save > 8 bit planed image */
			guchar *y = data[0];
			guchar buffer[BUF_SIZE];
			guchar *buf;
			int buf_size = BUF_SIZE / IW_TYPE_SIZE(img);
			int bytes = IW_TYPE_SIZE(img);

			i = 0;
			while (count--) {
				if (img->ctab > IW_COLFORMAT_MAX) {
					switch (img->type) {
						case IW_16U:
							((guint16*)buffer)[i++] = img->ctab[*(guint16*)y][0];
							((guint16*)buffer)[i++] = img->ctab[*(guint16*)y][1];
							((guint16*)buffer)[i++] = img->ctab[*(guint16*)y][2];
							break;
						case IW_32S:
							((gint32*)buffer)[i++] = img->ctab[*(gint32*)y][0];
							((gint32*)buffer)[i++] = img->ctab[*(gint32*)y][1];
							((gint32*)buffer)[i++] = img->ctab[*(gint32*)y][2];
							break;
						case IW_FLOAT:
							((gfloat*)buffer)[i++] = img->ctab[(int)*(gfloat*)y][0];
							((gfloat*)buffer)[i++] = img->ctab[(int)*(gfloat*)y][1];
							((gfloat*)buffer)[i++] = img->ctab[(int)*(gfloat*)y][2];
							break;
						case IW_DOUBLE:
							((gdouble*)buffer)[i++] = img->ctab[(int)*(gdouble*)y][0];
							((gdouble*)buffer)[i++] = img->ctab[(int)*(gdouble*)y][1];
							((gdouble*)buffer)[i++] = img->ctab[(int)*(gdouble*)y][2];
							break;
						default:
							break;
					}
					y += bytes;
				} else {
					switch (img->type) {
						case IW_16U:
							for (p = 0; p < planes; p++)
								((guint16*)buffer)[i++] = *(((guint16**)data)[p])++;
							break;
						case IW_32S:
							for (p = 0; p < planes; p++)
								((gint32*)buffer)[i++] = *(((gint32**)data)[p])++;;
							break;
						case IW_FLOAT:
							for (p = 0; p < planes; p++)
								((gfloat*)buffer)[i++] = *(((gfloat**)data)[p])++;;
							break;
						case IW_DOUBLE:
							for (p = 0; p < planes; p++)
								((gdouble*)buffer)[i++] = *(((gdouble**)data)[p])++;;
							break;
						default:
							break;
					}
				}
				if (i > buf_size - planes) {
					i *= bytes;
					buf = swap_buffer (buffer, buffer, i, img->type, swap);
					if (fwrite (buf, 1, i, file) != i) {
						status = IW_IMG_STATUS_WRITE;
						goto cleanup;
					}
					i = 0;
				}
			}
			if (i > 0) {
				i *= bytes;
				buf = swap_buffer (buffer, buffer, i, img->type, swap);
				if (fwrite (buf, 1, i, file) != i) {
					status = IW_IMG_STATUS_WRITE;
					goto cleanup;
				}
			}
		} /* if (IW_TYPE_SIZE(img) != 1) */
	} /* if (!rowstride) */

 cleanup:
	if (swapBuf) free (swapBuf);
	if (data) free (data);
	fclose (file);
	return status;
}

#ifdef COMMENT
/*********************************************************************
  Wrapper around img_save_ppm(). Save 'planed' (not interleaved)
  images of size width x height in file fname.
*********************************************************************/
iwImgStatus img_save_raw (const guchar **data, int width, int height, iwColtab ctab, const char *fname)
{
	iwImage img;

	img.data = data;
	img.planes = (ctab > IW_COLFORMAT_MAX || ctab == IW_BW)  ? 1:3;
	img.type = IW_8U;
	img.width = width;
	img.height = height;
	img.rowstride = 0;
	img.ctab = ctab;

	return (img_save_ppm (&img, fname));
}

/*********************************************************************
  Wrapper around img_save_ppm(). Save interleaved images of size
  width x height in file fname.
*********************************************************************/
iwImgStatus img_save_interleaved_raw (const guchar *data, int width, int height,
									  int rowstride, iwColtab ctab, const char *fname)
{
	iwImage img;
	guchar *planeptr[1];

	img.data = planeptr;
	img.data[0] = data;
	img.planes = (ctab > IW_COLFORMAT_MAX || ctab == IW_BW)  ? 1:3;
	img.type = IW_8U;
	img.width = width;
	img.height = height;
	img.rowstride = rowstride;
	img.ctab = ctab;

	return (img_save_ppm (&img, fname));
}
#endif

static void skip_comments (FILE *fp)
{
	char ch;

	do {
		while (isspace((int)(ch=getc(fp))));
		if (ch == '#') {
			while ((ch=getc(fp)) != '\n' && ch != EOF);
			if (ch == EOF) break;
		} else
			break;
	} while (1);
	ungetc (ch,fp);
}

static gboolean read_val (FILE *file, char *buffer, int *bufpos, int *bufcnt, int *val)
{
#define FILL	if (pos >= *bufcnt) { \
					if ((*bufcnt = fread (buffer, 1, BUF_SIZE, file)) <= 0) { \
						*val = sign*v; \
						return FALSE; \
					} \
					pos = 0; \
				}
	int v = 0, sign = 1, pos = *bufpos;

	do {
		pos++;
		FILL;
		if (buffer[pos] == '-')
			sign = -sign;
	} while (buffer[pos] < '0' || buffer[pos] > '9');
	do {
		v = v*10 + buffer[pos] - '0';
		pos++;
		FILL;
	} while (buffer[pos] >= '0' && buffer[pos] <= '9');
	*bufpos = pos;
	*val = sign*v;

	return TRUE;
#undef FILL
}

/*********************************************************************
  Native loader for ppm images. Loads an image of type P1 to P6 and PI
  from file fname and stores it in img. img is newly allocated without
  freeing it before.
*********************************************************************/
static iwImage* img_load_ppm (const char *fname, gboolean interleaved, iwImgStatus *status)
{
	FILE *file = NULL;
	char buffer[BUF_SIZE], type[100] = "";
	int width = 0, height = 0, planes = 0;
	int count, i, p;
	int ftype;
	iwImage *img = NULL;
	gboolean swap;

	if (!(file = fopen(fname, "rb"))) {
		if (status) *status = IW_IMG_STATUS_OPEN;
		return NULL;
	}

	skip_comments (file);

	if (!fgets(buffer, 1000, file) ||
		buffer[0]!='P' ||
		(buffer[1]!='1' && buffer[1]!='2' && buffer[1]!='3' &&
		 buffer[1]!='4' && buffer[1]!='5' && buffer[1]!='6' && buffer[1]!='I') ||
		(buffer[2]!='\0' && buffer[2]!='\n')) {
		if (status) *status = IW_IMG_STATUS_ERR;
		goto cleanup;
	}
	ftype = buffer[1] - '0';

	if (!(img = iw_img_new ())) {
		if (status) *status = IW_IMG_STATUS_MEM;
		goto cleanup;
	}

	skip_comments (file);
	if (buffer[1] == 'I') {
		fscanf (file, "%d%d%d%s", &width, &height, &planes, type);
		for (i=0; imgTypeText[i]; i++) {
			if (!strcmp (type, imgTypeText[i])) {
				img->type = i;
				break;
			}
		}
		if (!imgTypeText[i]) {
			if (status) *status = IW_IMG_STATUS_ERR;
			goto cleanup;
		}

		skip_comments (file);
		fscanf (file, "%d", &count);
	} else {
		if (ftype == 3 || ftype == 6)
			planes = 3;
		else
			planes = 1;
		fscanf (file, "%d%d", &width, &height);

		if (ftype != 1 && ftype != 4) {
			skip_comments (file);
			fscanf (file, "%d", &count);
		} else
			count = 1;
		if (count > 65535) {
			img->type = IW_32S;
		} else if (count > 255) {
			img->type = IW_16U;
		} else
			img->type = IW_8U;
	}

	if (planes == 1)
		img->ctab = IW_BW;
	if (interleaved)
		img->rowstride = width*planes;
	else
		img->rowstride = 0;
	img->width = width;
	img->height = height;
	img->planes = planes;
	if (!iw_img_allocate (img)) {
		if (status) *status = IW_IMG_STATUS_MEM;
		goto cleanup;
	}
	/*
	iw_debug (4, "File: '%s', Type: '%c', Size: %d x %d in %d maxvalues",
			  fname, buffer[1], width, height, count);
	*/
	fgetc (file);		/* Skip remaining newline */

	if (status) *status = IW_IMG_STATUS_READ;

	if (ftype == 1 ||  ftype == 2 ||  ftype == 3) {
		/* Plain ASCII format */
		int pos = 0, bufpos = -1, bufcnt = 0, val;

		count = width * height;
		if (planes > 1) {
			while (count--) {
				if (interleaved) {
					switch (img->type) {
						case IW_8U:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								img->data[0][pos++] = val;
							}
							break;
						case IW_16U:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								((guint16**)img->data)[0][pos++] = val;
							}
							break;
						case IW_32S:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								((gint32**)img->data)[0][pos++] = val;
							}
							break;
						default:
							break;
					}
				} else {
					switch (img->type) {
						case IW_8U:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								img->data[p][pos] = val;
							}
							break;
						case IW_16U:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								((guint16**)img->data)[p][pos] = val;
							}
							break;
						case IW_32S:
							for (p = 0; p<planes; p++) {
								if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
									goto cleanup;
								((gint32**)img->data)[p][pos] = val;
							}
							break;
						default:
							break;
					}
					pos++;
				}
			}
		} else {
			while (count--) {
				if (!read_val(file, buffer, &bufpos, &bufcnt, &val))
					goto cleanup;
				if (ftype == 1) {
					if (val == 0)
						val = 255;
					else if (val == 1)
						val = 0;
				}
				switch (img->type) {
					case IW_8U:
						img->data[0][pos] = val;
						break;
					case IW_16U:
						((guint16**)img->data)[0][pos] = val;
						break;
					case IW_32S:
						((gint32**)img->data)[0][pos] = val;
						break;
					default:
						break;
				}
				pos++;
			}
		}
	} else if (ftype == 4) {
		/* RAW BITMAP format */
		int bit, x, y, count = (width+7)/8;
		guchar *line = malloc (count);

		x = 0;
		for (y=0; y<height; y++) {
			if (fread (line, 1, count, file) != count) {
				free (line);
				goto cleanup;
			}
			for (i=0; i<width/8; i++) {
				for (bit=128; bit>0; bit/=2) {
					if (line[i] & bit)
						img->data[0][x++] = 0;
					else
						img->data[0][x++] = 255;
				};
			}
			bit = 128;
			for (i=width%8; i>0; i--) {
				if (line[count-1] & bit)
					img->data[0][x++] = 0;
				else
					img->data[0][x++] = 255;
				bit /= 2;
			}
		}
		free (line);
	} else {
		/* RAW BINARY format */
		count = width * height * planes * IW_TYPE_SIZE(img);
		swap = IW_TYPE_SIZE(img) > 1 && G_BYTE_ORDER != G_BIG_ENDIAN;

		if (!interleaved && planes > 1) {
			int pos = 0, read;

			while (count > 0) {
				i = BUF_SIZE - BUF_SIZE % (IW_TYPE_SIZE(img)*planes);
				read = fread (buffer, 1, i, file);
				if (read != i && read < count)
					goto cleanup;
				if (read > count) read = count;
				count -= read;
				swap_buffer ((guchar*)buffer, (guchar*)buffer, read, img->type, swap);
				i = 0;
				read /= IW_TYPE_SIZE(img)*planes;
				switch (img->type) {
					case IW_8U:
						for (; read > 0; read--) {
							for (p = 0; p<planes; p++)
								img->data[p][pos] = buffer[i++];
							pos++;
						}
						break;
					case IW_16U:
						for (; read>0; read--) {
							for (p = 0; p<planes; p++)
								((guint16**)img->data)[p][pos] = ((guint16*)buffer)[i++];
							pos++;
						}
						break;
					case IW_32S:
						for (; read>0; read--) {
							for (p = 0; p<planes; p++)
								((gint32**)img->data)[p][pos] = ((gint32*)buffer)[i++];
							pos++;
						}
						break;
					case IW_FLOAT:
						for (; read>0; read--) {
							for (p = 0; p<planes; p++)
								((gfloat**)img->data)[p][pos] = ((gfloat*)buffer)[i++];
							pos++;
						}
						break;
					case IW_DOUBLE:
						for (; read>0; read--) {
							for (p = 0; p<planes; p++)
								((gdouble**)img->data)[p][pos] = ((gdouble*)buffer)[i++];
							pos++;
						}
						break;
				}
			}
		} else {
			if (fread (img->data[0], 1, count, file) != count)
				goto cleanup;
			swap_buffer (img->data[0], img->data[0], count, img->type, swap);
		}
	} /* if (raw BINARY format) */
	if (status) *status = IW_IMG_STATUS_OK;

	/* Clean Up */
 cleanup:
	if (file) fclose (file);
	if (status && *status != IW_IMG_STATUS_OK && img) {
		iw_img_free (img, IW_IMG_FREE_ALL);
		img = NULL;
	}

	return img;
}

/*********************************************************************
  Native loader for png images. Loads an image of type PNG from file
  fname and stores it in img. img is newly allocated without freeing
  it before.
*********************************************************************/
static iwImage* img_load_png (const char *fname, gboolean interleaved, iwImgStatus *status)
{
#ifdef WITH_PNG
#define PNG_HEADSIZE	8
	FILE *file;
	iwImage *img = NULL;
	unsigned char header[PNG_HEADSIZE];
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	png_color_16p color_back;
	png_bytep *rows = NULL, data = NULL;
	int x, rowbytes;

	if (!(file = fopen (fname, "rb"))) {
		if (status) *status = IW_IMG_STATUS_OPEN;
		goto cleanup;
	}

	if (fread (header, 1, PNG_HEADSIZE, file) != PNG_HEADSIZE ||
		png_sig_cmp (header, 0, PNG_HEADSIZE)) {
		if (status) *status = IW_IMG_STATUS_ERR;
		goto cleanup;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		if (status) *status = IW_IMG_STATUS_ERR;
		goto cleanup;
	}
	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		if (status) *status = IW_IMG_STATUS_ERR;
		goto cleanup;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		if (status) *status = IW_IMG_STATUS_ERR;
		goto cleanup;
	}
	png_init_io (png_ptr, file);
	png_set_sig_bytes (png_ptr, PNG_HEADSIZE);

	png_read_info (png_ptr, info_ptr);
	png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				  &interlace_type, NULL, NULL);

	if (color_type == PNG_COLOR_TYPE_PALETTE || bit_depth < 8)
		png_set_expand (png_ptr);
/*
	png_color_8p sig_bit;
	if (png_get_sBIT (png_ptr, info_ptr, &sig_bit))
		png_set_shift (png_ptr, sig_bit);
*/
	/* FIXME: Read alpha channel */
	if (png_get_bKGD (png_ptr, info_ptr, &color_back)) {
		png_set_background (png_ptr, color_back,
							PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	} else {
		png_color_16 back = {0, 0, 0, 0, 0};
		png_set_background (png_ptr, &back,
							PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
	}
	if (G_BYTE_ORDER != G_BIG_ENDIAN && bit_depth == 16)
		png_set_swap (png_ptr);

	png_read_update_info (png_ptr, info_ptr);
	png_get_IHDR (png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
				  &interlace_type, NULL, NULL);

	rowbytes = png_get_rowbytes (png_ptr, info_ptr);
	data = malloc (rowbytes * height);
	rows = malloc (sizeof(png_bytep*) * height);
	for (x = 0; x < height; x++)
		rows[x] = data+x*rowbytes;
	png_read_image (png_ptr, rows);
	/* png_read_end (png_ptr, end_info); */

	img = iw_img_new();
	img->width = width;
	img->height = height;
	img->ctab = IW_RGB;

	if (color_type == PNG_COLOR_TYPE_GRAY)
		img->planes = 1;
	else
		img->planes = 3;
	if (bit_depth == 16)
		img->type = IW_16U;
	else
		img->type = IW_8U;

	if (interleaved || color_type == PNG_COLOR_TYPE_GRAY) {
		if (interleaved)
			img->rowstride = rowbytes;
		/* Just use the already loaded image, it's in the right format
		   -> Don't free data. */
		if (!iw_img_alloc_planeptr (img)) {
			if (status) *status = IW_IMG_STATUS_MEM;
			iw_img_free (img, IW_IMG_FREE_ALL);
			goto cleanup;
		}
		img->data[0] = data;
		data = NULL;
	} else {
		if (!iw_img_allocate (img)) {
			if (status) *status = IW_IMG_STATUS_MEM;
			iw_img_free (img, IW_IMG_FREE_ALL);
			goto cleanup;
		}
		if (bit_depth == 16) {
			guint16 *r = (guint16*)img->data[0],
				*g = (guint16*)img->data[1],
				*b = (guint16*)img->data[2],
				*s = (guint16*)data;
			for (x = img->width*img->height; x>0; x--) {
				*r++ = *s++;
				*g++ = *s++;
				*b++ = *s++;
			}
		} else {
			guchar *r = img->data[0], *g = img->data[1], *b = img->data[2];
			guchar *s = data;
			for (x = img->width*img->height; x>0; x--) {
				*r++ = *s++;
				*g++ = *s++;
				*b++ = *s++;
			}
		}
	}
	if (status) *status = IW_IMG_STATUS_OK;
 cleanup:
	if (png_ptr)
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	if (file) fclose (file);
	if (rows) free (rows);
	if (data) free (data);
	return img;
#else
	if (status) *status = IW_IMG_STATUS_FORMAT;
	return NULL;
#endif
}

/*********************************************************************
  Load image from file fname and store it in img. img is newly
  allocated without freeing it before.
*********************************************************************/
static iwImage* img_load_do (const char *fname, gboolean interleaved, iwImgStatus *status)
{
#ifdef WITH_GPB
	GdkPixbuf *pixbuf;
	int has_alpha = 0;
#endif
	iwImage *img;

	if ((img = img_load_png (fname, interleaved, status)) ||
		(status && *status == IW_IMG_STATUS_OPEN))
		return img;

	if ((img = img_load_ppm (fname, interleaved, status)) ||
		(status && *status == IW_IMG_STATUS_OPEN))
		return img;

#ifdef WITH_GPB
	if (!(pixbuf = gdk_pixbuf_new_from_file (fname, NULL))) {
		if (status) *status = IW_IMG_STATUS_ERR;
		return NULL;
	}

	if (gdk_pixbuf_get_bits_per_sample (pixbuf) != 8) {
		gdk_pixbuf_unref (pixbuf);
		if (status) *status = IW_IMG_STATUS_ERR;
		return NULL;
	}

	img = iw_img_new();

	/* FIXME: Read alpha channel */
	if (gdk_pixbuf_get_has_alpha(pixbuf))
		has_alpha = 1;
	img->width = gdk_pixbuf_get_width (pixbuf);
	img->height = gdk_pixbuf_get_height (pixbuf);
	img->planes = gdk_pixbuf_get_n_channels (pixbuf) - has_alpha;
	img->type = IW_8U;
	if (img->planes == 1)
		img->ctab = IW_BW;
	if (interleaved) {
		if (has_alpha)
			img->rowstride = img->width*img->planes*IW_TYPE_SIZE(img);
		else
			img->rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	} else
		img->rowstride = 0;
	/*
	iw_debug (4, "File: '%s', Size: %d x %d, Planes: %d",
			  fname, img->width, img->height, img->planes);
	*/
	if (!iw_img_allocate (img)) {
		if (status) *status = IW_IMG_STATUS_MEM;
		iw_img_free (img, IW_IMG_FREE_ALL);
		img = NULL;
	} else {
		if (!has_alpha && interleaved) {
			memcpy (img->data[0], gdk_pixbuf_get_pixels(pixbuf),
					img->rowstride * img->height);
		} else {
			guchar *y = img->data[0], *u = img->data[1], *v = img->data[2];
			guchar *s = gdk_pixbuf_get_pixels(pixbuf);
			int offset = ( gdk_pixbuf_get_rowstride (pixbuf) -
						   img->width*(img->planes+has_alpha) );
			int x, k;
			for (k = img->height; k>0; k--) {
				if (img->planes == 1)
					for (x=img->width; x>0; x--) {
						*y++ = *s++;
						if (has_alpha) s++;
					}
				else if (interleaved)
					for (x=img->width; x>0; x--) {
						*y++ = *s++;
						*y++ = *s++;
						*y++ = *s++;
						s++;
					}
				else
					for (x=img->width; x>0; x--) {
						*y++ = *s++;
						*u++ = *s++;
						*v++ = *s++;
						if (has_alpha) s++;
					}
				s += offset;
			}
		}
		if (status) *status = IW_IMG_STATUS_OK;
	}

	gdk_pixbuf_unref (pixbuf);
#endif
	return img;
}

/*********************************************************************
  Load image from file fname in 'planed' form.
*********************************************************************/
iwImage* iw_img_load (const char *fname, iwImgStatus *status)
{
	return img_load_do (fname, FALSE, status);
}

/*********************************************************************
  Load image from file fname and return an interleaved image.
*********************************************************************/
iwImage* iw_img_load_interleaved (const char *fname, iwImgStatus *status)
{
	return img_load_do (fname, TRUE, status);
}

/*********************************************************************
  Initialise struct holding settings for saving images.
*********************************************************************/
iwImgFileData *iw_img_data_create (void)
{
	iwImgFileData *data = malloc (sizeof(iwImgFileData));
	data->movie = NULL;
	data->framerate = 25;
	data->quality = 75;
	return data;
}

/*********************************************************************
  Free struct created with img_create_data().
*********************************************************************/
void iw_img_data_free (iwImgFileData *data)
{
	if (data) free (data);
}

/*********************************************************************
  Set parameters used for AVI saving.
*********************************************************************/
void iw_img_data_set_movie (iwImgFileData *data, iwMovie **movie, float framerate)
{
	data->movie = movie;
	data->framerate = framerate;
}

/*********************************************************************
  Set quality parameter (used for JPEG saving),
*********************************************************************/
void iw_img_data_set_quality (iwImgFileData *data, int quality)
{
	data->quality = quality;
}

/*********************************************************************
  If 'format' == IW_IMG_FORMAT_UNKNOWN return a format based on the
  extension of fname. Otherwise return 'format'.
*********************************************************************/
iwImgFormat iw_img_save_get_format (iwImgFormat format, const char *fname)
{
	static char *pnm[] = {".pnm", ".ppm", ".pgm", ".pbm", ".ice", NULL};
	static char *png[] = {".png", NULL};
	static char *jpeg[] = {".jpeg", ".jpg", ".jfif", NULL};
	static char *raw444[] = {".avi", ".raw", NULL};
	static char *raw420[] = {".avi", ".raw", NULL};
	static char *mjpeg[] = {".avi", ".mjpg", ".mjpeg", NULL};
	static char *ffv1[] = {".avi", ".ffv", ".ffv1", NULL};
	static char *mpeg4[] = {".avi", ".mpeg4", ".mp4", NULL};
	static char *svg[] = {".svg", NULL};
	static char **ext[] = {pnm, png, jpeg,
						   raw444, raw420, mjpeg, ffv1, mpeg4,
						   svg, NULL};
	char fext[10], *pos;
	int i, j;

	if (format == IW_IMG_FORMAT_UNKNOWN) {
		pos = strrchr (fname, '.');
		if (!pos || fname+strlen(fname)-pos > 8) return IW_IMG_FORMAT_UNKNOWN;

		for (i=0; pos[i]; i++)
			fext[i] = tolower((int)pos[i]);
		fext[i] = '\0';

		for (i=0; ext[i] && format == IW_IMG_FORMAT_UNKNOWN; i++) {
			for (j=0; ext[i][j]; j++) {
				if (!strcmp (ext[i][j], fext)) {
					format = IW_IMG_FORMAT_UNKNOWN + 1 + i;
					break;
				}
			}
		}
	}
	/* Skip the gap in the format enums */
	if (format > IW_IMG_FORMAT_MOVIE_MAX && format < IW_IMG_FORMAT_VECTOR)
		format += IW_IMG_FORMAT_VECTOR-IW_IMG_FORMAT_MOVIE_MAX-1;
	return format;
}

/*********************************************************************
  Save 'img' in file 'fname' in the format 'format'. If
  'format' == IW_IMG_FORMAT_UNKNOWN the extension of fname is used.
*********************************************************************/
iwImgStatus iw_img_save (const iwImage *img, iwImgFormat format,
						 const char *fname, const iwImgFileData *data)
{
	iwImgStatus status = IW_IMG_STATUS_FORMAT;
	int quality = 75;

	format = iw_img_save_get_format (format, fname);

	switch (format) {
		case IW_IMG_FORMAT_PNM:
			status = img_save_ppm (img, fname);
			break;
		case IW_IMG_FORMAT_PNG:
			status = img_save_png (img, fname);
			break;
		case IW_IMG_FORMAT_JPEG:
			if (data) quality = data->quality;
			status = img_save_jpeg (img, fname, quality);
			break;
		case IW_IMG_FORMAT_AVI_RAW444:
		case IW_IMG_FORMAT_AVI_RAW420:
		case IW_IMG_FORMAT_AVI_MJPEG:
		case IW_IMG_FORMAT_AVI_FFV1:
		case IW_IMG_FORMAT_AVI_MPEG4:
			if (data && data->movie)
				status = iw_movie_write (data, format, img, fname);
			else
				return IW_IMG_STATUS_ERR;
			break;
		default:
			break;
	}
	return status;
}
