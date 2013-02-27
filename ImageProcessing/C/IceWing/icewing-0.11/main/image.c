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

#include "image.h"
#include "gui/Gtools.h"
#include "gui/Gpolygon.h"

/* Number of buffers for iw_img_get_buffer() */
#define BUF_MAX			10
static int buf_number = 0;

/*********************************************************************
  Return a pointer to an internal intermediate buffer. If the buffer
  is smaller than size bytes, the buffer is reallocated.
  Calls to iw_img_get_buffer() can be nested. iw_img_release_buffer()
  must be called if the buffer is not needed any more.
*********************************************************************/
void* iw_img_get_buffer (int size)
{
	static int buf_size[BUF_MAX] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	static void *buffer[BUF_MAX] = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

	iw_assert (buf_number >= 0 && buf_number < BUF_MAX,
			   "Buffer number (%d) out of range [0..%d]\n"
			   "\t(too many iw_img_release_buffer()/iw_img_get_buffer()-calls?)",
			   buf_number, BUF_MAX-1);

	if (size > buf_size[buf_number]) {
		if (!(buffer[buf_number] = realloc (buffer[buf_number], size)))
			iw_error ("Out of memory: image buffer");
		buf_size[buf_number] = size;
	}
	return buffer[buf_number++];
}

/*********************************************************************
  Release the buffer requested with iw_img_get_buffer().
*********************************************************************/
void iw_img_release_buffer (void)
{
	buf_number--;
}

#ifdef IW_DEBUG
/*********************************************************************
  Output color of image s at position (x,y) to stderr. If s has 3
  planes result of prev_yuvToRgbVis() is given additionally.
*********************************************************************/
void iw_img_col_debug (const char *str, const iwImage *s, int x, int y)
{
	int off = s->width*y+x;
	if (s->planes < 3) {
		iw_debug (2, "img_col (%s, %3d, %3d) = %3d",
				  str, x, y, s->data[0][off]);
	} else {
		uchar r,g,b;
		prev_yuvToRgbVis (s->data[0][off], s->data[1][off],
						  s->data[2][off], &r, &g, &b);
		iw_debug (2, "img_col (%s, %3d, %3d) = YUV %d %d %d -> RGB %d %d %d",
				  str, x, y, s->data[0][off], s->data[1][off],
				  s->data[2][off], r, g, b);
	}
}
#else
/*********************************************************************
  Dummy function to prevent changing the interface.
*********************************************************************/
void iw_img_col_debug (const char *str, const iwImage *s, int x, int y) {}
#endif

/*********************************************************************
  Downsample source by factor (downw,downh) and put it to dest.
  dest->data is freed and newly allocated if the size or the type
  of source has changed. Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_downsample (const iwImage *source, iwImage *dest,
						int downw, int downh)
{
	int d_width = source->width/downw, d_height = source->height/downh;
	int bytes = IW_TYPE_SIZE(source), planes = source->planes;
	uchar *d, *s;

	if (dest->width != d_width || dest->height != d_height ||
		dest->planes != planes || dest->type != source->type) {
		iw_img_free (dest, IW_IMG_FREE_PLANE|IW_IMG_FREE_PLANEPTR);
		dest->width = d_width;
		dest->height = d_height;
		dest->planes = planes;
		dest->type = source->type;
		if (!iw_img_allocate (dest))
			iw_error ("Unable to allocate memory for downsampled image");
	}

	planes--;
	if (downw == 1 && downh == 1) {
		for (; planes>=0; planes--)
			memcpy (dest->data[planes], source->data[planes],
					d_width*d_height*bytes);
	} else {
		int line = (downh-1)*source->width*bytes + (source->width%downw)*bytes;
		int xs, ys;

		downw *= bytes;

		for (; planes>=0; planes--) {
			d = dest->data[planes];
			s = source->data[planes];
			/* Special case 8U to speed it up */
			if (source->type == IW_8U) {
				for (ys=0; ys<d_height; ys++) {
					for (xs=0; xs<d_width; xs++) {
						*d++ = *s;
						s += downw;
					}
					s += line;
				}
			} else {
				for (ys=0; ys<d_height; ys++) {
					for (xs=0; xs<d_width; xs++) {
						memcpy (d, s, bytes);
						d += bytes;
						s += downw;
					}
					s += line;
				}
			}
		}
	}
}

/*********************************************************************
  The code for iw_img_resize() is based on the resizing code from Gimp
  Version 1.2, which is released under the following copyright:

  The GIMP -- an image manipulation program
  Copyright (C) 1995 Spencer Kimball and Peter Mattis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*********************************************************************/

/*********************************************************************
  Resize image srcPR to size of image dstPR and save it in dstPR.
*********************************************************************/
static void img_resize_no_resample (const iwImage *srcPR, iwImage *dstPR)
{
	int x, y, *offset_x, *offset_y;
	float step_x, step_y;
	uchar *s, *d;

	uchar **src = srcPR->data;
	uchar **dst = dstPR->data;
	int planes = srcPR->planes - 1;
	int src_w = srcPR->width;
	int src_h = srcPR->height;
	int dst_w = dstPR->width;
	int dst_h = dstPR->height;

	if( src_w == dst_w && src_h == dst_h ) {
		for (; planes>=0; planes--)
			memcpy (dst[planes], src[planes], src_w * src_h);
		return;
	}

	offset_x = iw_img_get_buffer(sizeof(int) * (dst_w + dst_h));
	offset_y = offset_x + dst_w;
	if (dst_w > 1)
		step_x = (float)(src_w - 1) / (dst_w - 1);
	else
		step_x = 0;
	if (dst_h > 1)
		step_y = (float)(src_h - 1) / (dst_h - 1);
	else
		step_y = 0;

	for (x = 0; x < dst_w; x++ )
		offset_x[x] = (int)(step_x * x + 0.5);
	for( y = 0; y < dst_h; y++ )
		offset_y[y] = (int)(step_y * y + 0.5);

	for (; planes>=0; planes--) {
		d = *dst;
		for (y = 0; y < dst_h; y++) {
			s = *src + offset_y[y] * src_w;

			for (x = 0; x < dst_w; x++ )
				d[x] = s[offset_x[x]];

			d += dst_w;
		}
		src++;
		dst++;
	}

	iw_img_release_buffer();
}

static void rotate_pointers (double **p, int n)
{
	int i;
	void *tmp;

	tmp = p[0];
	for (i = 0; i < n-1; i++)
		p[i] = p[i+1];
	p[i] = tmp;
}

static void expand_line (double *dest, double *src, int planes,
						 int old_width, int width)
{
	double ratio;
	int x,b;
	int src_col;
	double frac;
	double *s;
	ratio = old_width/(double)width;

	/* This could be opimized much more by precalculating
	   the coeficients for each x */
	for (x = 0; x < width; x++) {
		src_col = ((int)((x) * ratio + 2.0 - 0.5)) - 2;
		/* +2, -2 is there because (int) rounds towards 0 and we need
		   to round down */
		frac = ((x) * ratio - 0.5) - src_col;
		s = &src[src_col * planes];
		for (b = 0; b < planes; b++)
			dest[b] = ((s[b + planes] - s[b]) * frac + s[b]);
		dest += planes;
	}
}

static void shrink_line (double *dest, double *src, int planes,
						 int old_width, int width)
{
	int x, b;
	double *source, *destp;
	register double accum;
	register unsigned int max;
	register double mant, tmp;
	register const double step = old_width/(double)width;
	register const double inv_step = 1.0/step;
	double position;

	for (b = 0; b < planes; b++) {
		source = &src[b];
		destp = &dest[b];
		position = -1;

		mant = *source;

		for (x = 0; x < width; x++) {
			source+= planes;
			accum = 0;
			max = ((int)(position+step)) - ((int)(position));
			max--;
			while (max) {
				accum += *source;
				source += planes;
				max--;
			}
			tmp = accum + mant;
			mant = ((position+step) - (int)(position + step));
			mant *= *source;
			tmp += mant;
			tmp *= inv_step;
			mant = *source - mant;
			*(destp) = tmp;
			destp += planes;
			position += step;
		}
	}
}

static void get_scaled_row (double **src, int y, int new_width,
							const iwImage *srcPR, double *row)
{
	/* Get the necesary lines from the source image, scale them,
	   and put them into src[] */
	int planes = srcPR->planes;
	int b, x, p, offset;

	rotate_pointers (src, 4);
	if (y < 0) y = 0;

	if (y < srcPR->height) {

		/* Get image data */
		offset = y * srcPR->width;
		x = 0;
		for (b=0; b<srcPR->width; b++)
			for (p = 0; p < planes; p++)
				row[x++] = srcPR->data[p][b+offset];

		/* Set the off edge pixels to their nearest neighbor */
		for (b = 0; b < 2 * planes; b++)
			row[-2*planes + b] = row[(b%planes)];
		for (b = 0; b < planes * 2; b++)
			row[srcPR->width*planes + b] = row[(srcPR->width - 1) * planes + (b%planes)];

		if (new_width > srcPR->width)
			expand_line(src[3], row, planes,
						srcPR->width, new_width);
		else if (srcPR->width > new_width)
			shrink_line(src[3], row, planes,
						srcPR->width, new_width);
		else /* No scaling needed */
			memcpy (src[3], row, sizeof (double) * new_width * planes);
	} else
		memcpy(src[3], src[2], sizeof (double) * new_width * planes);
}

/*********************************************************************
  Resize image srcPR to size of image dstPR and save it in dstPR.
  interpolate==TRUE: Use linear interpolation.
*********************************************************************/
void iw_img_resize (const iwImage *srcPR, iwImage *destPR, BOOL interpolate)
{
	double  *src[4];
	double *row, *accum;
	int planes;
	int width, height;
	int orig_width, orig_height;
	double y_rat;
	int old_y = -4, new_y;
	int x, y, p, b, offset;

	if (!interpolate) {
		img_resize_no_resample (srcPR, destPR);
		return;
	}

	orig_width = srcPR->width;
	orig_height = srcPR->height;

	width = destPR->width;
	height = destPR->height;

	/* Find the ratios of old y to new y */
	y_rat = (double) orig_height / (double) height;

	planes = destPR->planes;

	/* The data pointers... */
	accum = iw_img_get_buffer (sizeof(double) *
							(width * planes * 5 + (orig_width + 2*2) * planes));
	for (x = 0; x < 4; x++) {
		src[x] = accum;
		accum += width * planes;
	}

	/* Offset the row pointer by 2*planes so the range of the array
	   is [-2*planes] to [(orig_width + 2)*planes] */
	row = accum;
	row += planes*2;

	accum += (orig_width + 2*2) * planes;

	/* Scale the selected region */
	for (y = 0; y < height;  y++) {
		if (height < orig_height) {
			int max;
			double frac;
			const double inv_ratio = 1.0 / y_rat;
			if (y == 0) /* Load the first row if this it the first time through */
				get_scaled_row (&src[0], 0, width, srcPR, row);
			new_y = (int)((y) * y_rat);
			frac = 1.0 - (y*y_rat - new_y);
			for (x  = 0; x < width*planes; x++)
				accum[x] = src[3][x] * frac;

			max = ((int)((y+1) *y_rat)) - (new_y);
			max--;

			get_scaled_row (&src[0], ++new_y, width, srcPR, row);

			while (max > 0) {
				for (x = 0; x < width*planes; x++)
					accum[x] += src[3][x];
				get_scaled_row (&src[0], ++new_y, width, srcPR, row);
				max--;
			}
			frac = (y + 1)*y_rat - ((int)((y + 1)*y_rat));
			for (x = 0; x < width*planes; x++) {
				accum[x] += frac * src[3][x];
				accum[x] *= inv_ratio;
			}
		} else if (height > orig_height) {
			new_y = floor((y) * y_rat - 0.5);

			while (old_y <= new_y) {
				/* Get the necesary lines from the source image, scale them,
				   and put them into src[] */
				get_scaled_row (&src[0], old_y + 2, width, srcPR, row);
				old_y++;
			}

			{
				double idy = ((y) * y_rat - 0.5) - new_y;
				double dy = 1.0 - idy;
				for (x = 0; x < width * planes; x++)
					accum[x] = dy * src[1][x] + idy * src[2][x];
			}
		} else {	/* height == orig_height */

			get_scaled_row (&src[0], y, width, srcPR, row);
			memcpy (accum, src[3], sizeof(double) * width * planes);
		}

		offset = y * destPR->width;
		x = 0;
		for (b = 0; b < width; b++) {
			for (p = 0; p < planes; p++) {
				if (accum[x] < 0.0)
					destPR->data[p][b+offset] = 0;
				else if (accum[x] > (float)IW_COLMAX)
					destPR->data[p][b+offset] = IW_COLMAX;
				else
					destPR->data[p][b+offset] = rint(accum[x]);
				x++;
			}
		}
	}

	/* Free up temporary arrays */
	iw_img_release_buffer();
}

/*********************************************************************
  Resize image src (size: src_w x src_h with 'planes' planes)
  to size dst_w x dst_h and save it in dst.
  interpolate==TRUE: Use linear interpolation.
*********************************************************************/
void iw_img_resize_raw (const uchar **src, uchar **dst,
						int src_w, int src_h, int dst_w, int dst_h,
						int planes, BOOL interpolate)
{
	iwImage srcPR, dstPR;

	srcPR.width = src_w;
	srcPR.height = src_h;
	srcPR.planes = planes;
	srcPR.data = (uchar**)src; /* const */

	dstPR.width = dst_w;
	dstPR.height = dst_h;
	dstPR.planes = planes;
	dstPR.data = dst;

	iw_img_resize (&srcPR, &dstPR, interpolate);
}

/*********************************************************************
  Set border of size border in image src to 0.
*********************************************************************/
void iw_img_border (uchar *src, int width, int height, int border)
{
	uchar *s;
	int x,y;

	for (y=border; y<height-border; y++) {
		s = src+y*width;
		for (x=border; x>0; x--)
			*s++ = 0;
		s += width-2*border;
		for (x=border; x>0; x--)
			*s++ = 0;
	}
	memset (src,0,width*border);
	memset (src+(height-border)*width,0,width*border);
}

/*********************************************************************
  Copy border of width border from src to dest.
*********************************************************************/
void iw_img_copy_border (const uchar *src, uchar *dest,
						 int width, int height, int border)
{
	const uchar *s;
	uchar *d;
	int x,y;

	for (y=border; y<height-border; y++) {
		s = src+y*width;
		d = dest+y*width;
		for (x=border; x>0; x--)
			*d++ = *s++;
		s += width-2*border;
		d += width-2*border;
		for (x=border; x>0; x--)
			*d++ = *s++;
	}
	memcpy (dest, src, width*border);
	memcpy (dest+(height-border)*width, src+(height-border)*width, width*border);
}

/*********************************************************************
  Put median smoothed image (mask size: size*2+1) from s to d.
*********************************************************************/
/*
void median_alt (uchar *s, uchar *d, int width, int height, int size)
{
	int h[IW_COLCNT], haelfte, dest, mitte, sub, add, anz, i, x, y;
	uchar *dpos, *spos;
	iw_time_add_static (time_med, "Median");

	if (size<=0) return;

	iw_time_start (time_med);

	dest = size;
	size = size*2+1;
	haelfte = (size*size+1)/2;
	for (y=0; y<=height-size; y++) {
		dpos = d+(y+dest)*width+dest;
		for (i=0; i<IW_COLCNT; i++) h[i]=0;
		for (i=y; i<y+size; i++) {
			spos = s+i*width;
			for (x=0; x<size; x++) h[*spos++]++;
		}

		anz = 0;
		for (mitte=-1; anz<haelfte; mitte++) anz += h[mitte+1];
		*dpos++ = mitte;
		for (x=0; x<width-size; x++) {
			for (i=y; i<size+y; i++) {
				sub = s[i*width+x];
				add = s[i*width+x+size];
				h[sub]--;
				h[add]++;
			}
			anz = 0;
			for (mitte=-1; anz<haelfte; mitte++) anz += h[mitte+1];
			*dpos++ = mitte;
		}
	}
	iw_time_stop (time_med, FALSE);
}
*/

/*********************************************************************
  Put median smoothed image (mask size: size*2+1) from s to d.
*********************************************************************/
void iw_img_median (const uchar *s, uchar *d, int width, int height, int size)
{
	int h[IW_COLCNT], h2[IW_COLCNT], half, half2, i, x, y;
	int median, sub, add, left;
	const uchar *spos;
	iw_time_add_static (time_med, "Median");

	if (size <= 0) return;

	iw_time_start (time_med);

	iw_img_border (d, width, height, size);
	d += size*width+size;

	size = size*2+1;
	half = (size*size)/2;
	half2 = (size*size+1)/2;

	memset (h2, 0, sizeof(int)*IW_COLCNT);
	for (i=0; i<size; i++) {
		spos = s+i*width;
		for (x=0; x<size; x++) h2[*spos++]++;
	}

	for (y=0; y<=height-size; y++) {
		if (y > 0) {
			for (x=0; x<size; x++) {
				h2[*(s+(y-1)*width+x)]--;
				h2[*(s+(y+size-1)*width+x)]++;
			}
		}

		memcpy (h, h2, sizeof(int)*IW_COLCNT);

		left = 0;
		for (median=-1; left<=half; median++) left += h[median+1];
		left = left - h[median];

		d[y*width] = median;

		for (x=0; x<width-size; x++) {
			spos = s+y*width+x;
			for (i=size; i>0; i--) {
				sub = *spos;
				add = *(spos+size);
				spos += width;
				h[sub]--;
				h[add]++;

				if (sub < median) left--;
				if (add < median) left++;
			}
			if (left > half) {
				do {
					median--;
					left -= h[median];
				} while (left > half);
			} else if (left+h[median] < half2) {
				do {
					left += h[median];
					median++;
				} while (left+h[median] < half2);
			}
			d[y*width+x+1] = median;
		}
	}
	iw_time_stop (time_med, FALSE);
}

/*********************************************************************
  Put median smoothed image (mask size: size*2+1) from s to d.
  s is only tested on >0 (-> BLACK/WHITE).
*********************************************************************/
void iw_img_medianBW (const uchar *s, uchar *d, int width, int height, int size)
{
	int hB, hW, dest, i, x, y;
	const uchar *spos;
	uchar *dpos;
	iw_time_add_static (time_medbw, "MedianBW");

	if (size<=0) return;

	iw_time_start (time_medbw);

	iw_img_border (d,width,height,size);

	dest = size;
	size = size*2+1;
	for (y=0; y<=height-size; y++) {
		dpos = d+(y+dest)*width+dest;
		hB = hW = 0;
		for (i=y; i<y+size; i++) {
			spos = s+i*width;
			for (x=0; x<size; x++)
				if (*spos++ > 0) hW++;
				else hB++;
		}
		if (hB>hW) *dpos++ = 0;
		else *dpos++ = IW_COLMAX;
		for (x=0; x<width-size; x++) {
			for (i=y; i<size+y; i++) {
				s[i*width+x]>0 ? hW--:hB--;
				s[i*width+x+size]>0 ? hW++:hB++;
			}
			if (hB>hW) *dpos++ = 0;
			else *dpos++ = IW_COLMAX;
		}
	}
	iw_time_stop (time_medbw, FALSE);
}

/*********************************************************************
  Put smoothed image (mask size: size*2+1, center of mask gets color
  which occurs most frequently) from s to d.
*********************************************************************/
void iw_img_max (const uchar *s, uchar *d, int width, int height, int size)
{
	int h[IW_COLCNT], h2[IW_COLCNT], half, dest, i, x, y, max, maxind;
	const uchar *spos, *spos2;
	iw_time_add_static (time_imgmax, "SmoothMax");

	if (size<=0) return;

	iw_time_start (time_imgmax);

	iw_img_border (d,width,height,size);

	dest = size;
	size = size*2+1;
	half = (size*size+1)/2;

	for (i=0; i<IW_COLCNT; i++) h2[i]=0;
	for (i=0; i<size; i++) {
		spos = s+i*width;
		for (x=0; x<size; x++) h2[*spos++]++;
	}

	for (x=0; x<=width-size; x++) {
		if (x>0) {
			for (y=0; y<size; y++) {
				h2[*(s+y*width+x-1)]--;
				h2[*(s+y*width+x+size-1)]++;
			}
		}

		memcpy (h,h2,sizeof(int)*IW_COLCNT);

		max = h[0];
		maxind = 0;
		for (i=0; i<IW_COLCNT; i++) {
			if (h[i] > max) {
				max = h[i];
				maxind = i;
			}
		}
		d[dest*width+dest+x] = maxind;

		for (y=0; y<height-size; y++) {
			spos = s+y*width+x;
			spos2 = s+(y+size)*width+x;
			for (i=x; i<size+x; i++) {
				h[*spos++]--;
				h[*spos2++]++;
			}
			if (h[maxind] < half) {
				max = h[0];
				maxind = 0;
				for (i=0; i<IW_COLCNT; i++) {
					if (h[i] > max) {
						max = h[i];
						maxind = i;
					}
				}
			}
			d[(y+dest+1)*width+dest+x] = maxind;
		}
	}
	iw_time_stop (time_imgmax, FALSE);
}

/*********************************************************************
  Histogram equalize src and put it to dest.
*********************************************************************/
void iw_img_histeq (const uchar *src, uchar *dest, int width, int height)
{
	int pixels = width * height, pixsum = 0, max = 0;
	const uchar *s;
	uchar lumamap[IW_COLCNT];
	int x, hist[IW_COLCNT];
	double lscale;

	memset (hist, 0, IW_COLCNT*sizeof(int));
	s = src;
	for (x = pixels; x>0; x--) hist[*s++]++;

	for (x = 0; x < IW_COLCNT; x++) {
		if (hist[x] > 0) max = x;

		lumamap[x] = pixsum * IW_COLMAX / pixels;
		pixsum += hist[x];
	}

	/* Normalise so that the brightest pixels are set to maxval */
	lscale = (double)IW_COLMAX / (lumamap[max]>0 ? lumamap[max] : IW_COLMAX);
	for (x = 0; x < IW_COLCNT; x++) {
		long l = gui_lrint (lumamap[x] * lscale);
		lumamap[x] = (uchar) MIN (IW_COLMAX, l);
	}

	for (x = pixels; x>0; x--)
		*dest++ = lumamap[*src++];
}

/*********************************************************************
  Contrast autostretch src and put it to dest.
  Ignore the first min_limit % and the last max_limit % pixels.
*********************************************************************/
void iw_img_cstretch (const uchar *src, uchar *dest, int width, int height,
					  int min_limit, int max_limit)
{
	int pixels = width * height;
	const uchar *s;
	uchar lumamap[IW_COLCNT];
	int x, min, max, hist[IW_COLCNT];
	double lscale;
	iw_time_add_static (time_cstretch, "CStretch");

	iw_time_start (time_cstretch);

	memset (hist, 0, IW_COLCNT*sizeof(int));
	s = src;
	for (x = pixels; x>0; x--) hist[*s++]++;

	min_limit = MAX (1, pixels*min_limit/100);
	max_limit = MAX (1, pixels*max_limit/100);
	x = 0;
	min = 0;
	while (x < min_limit && x<pixels) {
		x += hist[min];
		min++;
	}
	x = 0;
	max = IW_COLMAX;
	while (x < max_limit && x<pixels) {
		x += hist[max];
		max--;
	}
	if (min >= max) {
		min = (max+min)/2;
		max = min+1;
	}

	lscale = (double)IW_COLMAX / (max-min);
	for (x = 0; x < IW_COLCNT; x++) {
		long l = gui_lrint ((x-min) * lscale);
		lumamap[x] = (uchar) CLAMP (l, 0, IW_COLMAX);
	}
	for (x = pixels; x>0; x--)
		*dest++ = lumamap[*src++];

	iw_time_stop (time_cstretch, FALSE);
}

/*********************************************************************
  Apply a sobel filter in x and y direction to src and save |dx|+|dy|
  to dest. If thresh>0 threshold dest.
  A border of one pixel is copied unchanged.
*********************************************************************/
void iw_img_sobel (const uchar *src, uchar *dest, int width, int height, int thresh)
{
	int x, y, c;

	iw_img_border (dest, width, height, 1);

	src += width+1;
	dest += width+1;

	for (y = height-2; y>0; y--) {
		for (x = width-2; x>0; x--) {
			c = (abs((*(src-width-1) + 2*(*(src-width)) + *(src-width+1))-
					 (*(src+width-1) + 2*(*(src+width)) + *(src+width+1))) +
				 abs((*(src-width-1) + 2*(*(src-1)) + *(src+width-1))-
					 (*(src-width+1) + 2*(*(src+1)) + *(src+width+1))))/6;
			if (thresh > 0) {
				if (c<thresh )
					*dest++ = IW_COLMAX;
				else
					*dest++ = 0;
			} else if (c>IW_COLMAX) {
				*dest++ = IW_COLMAX;
			} else {
				*dest++ = c;
			}
			src++;
		}
		src += 2;
		dest += 2;
	}
}

/*********************************************************************
  Perform RGB to YUV conversion with intervall reduction.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_rgbToYuvVis (iwImage *img)
{
	float yf;
	int x;

	if (img->planes < 3) return;
	/* Interleaved images not yet supported */
	if (img->rowstride) return;

	if (img->ctab < IW_COLFORMAT_MAX) img->ctab = IW_YUV;

	switch (img->type) {
		case IW_8U: {
			guchar *y=img->data[0], *u=img->data[1], *v=img->data[2];
			for (x = img->width*img->height; x>0; x--) {
				prev_inline_rgbToYuvVis (*y, *u, *v, y, u, v);
				y++;
				u++;
				v++;
			}
		}
		break;
		case IW_16U: {
			guint16 *y = (guint16*)img->data[0],
				*u = (guint16*)img->data[1],
				*v = (guint16*)img->data[2];
			guint16 r, g, b;
			for (x = img->width*img->height; x>0; x--) {
				r = *y; g = *u; b = *v;

				yf = 0.257*r + 0.504*g + 0.0979*b;
				*y++ = gui_lrint (yf+16.0*256);
				*u++ = gui_lrint (0.495*b - 0.577*yf + 128.0*256);
				*v++ = gui_lrint (0.626*r - 0.729*yf + 128.0*256);
			}
		}
		break;
		case IW_32S: {
			gint32 *y = (gint32*)img->data[0],
				*u = (gint32*)img->data[1],
				*v = (gint32*)img->data[2];
			gint32 r, g, b;
			for (x = img->width*img->height; x>0; x--) {
				r = *y; g = *u; b = *v;

				yf = 0.257*r + 0.504*g + 0.0979*b;
				*y++ = gui_lrint (yf);
				*u++ = gui_lrint (0.495*b - 0.577*yf);
				*v++ = gui_lrint (0.626*r - 0.729*yf);
			}
		}
		break;
		case IW_FLOAT: {
			gfloat *y = (gfloat*)img->data[0],
				*u = (gfloat*)img->data[1],
				*v = (gfloat*)img->data[2];
			gfloat r, g, b;
			for (x = img->width*img->height; x>0; x--) {
				r = *y; g = *u; b = *v;

				yf = 0.257*r + 0.504*g + 0.0979*b;
				*y++ = yf;
				*u++ = 0.495*b - 0.577*yf;
				*v++ = 0.626*r - 0.729*yf;
			}
		}
		break;
		case IW_DOUBLE: {
			gdouble *y = (gdouble*)img->data[0],
				*u = (gdouble*)img->data[1],
				*v = (gdouble*)img->data[2];
			gdouble r, g, b, yf;
			for (x = img->width*img->height; x>0; x--) {
				r = *y; g = *u; b = *v;

				yf = 0.257*r + 0.504*g + 0.0979*b;
				*y++ = yf;
				*u++ = 0.495*b - 0.577*yf;
				*v++ = 0.626*r - 0.729*yf;
			}
		}
		break;
	}
}

/*********************************************************************
  Perform YUV to RGB conversion with intervall expansion.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
void iw_img_yuvToRgbVis (iwImage *img)
{
#define TORGB_REAL(y,u,v,r,g,b) { \
		Y = (y) * 1.164; \
		U = (u); \
		V = (v); \
		(r) = Y+1.597*V; \
		(g) = Y-0.392*U-0.816*V; \
		(b) = Y+2.018*U; \
	}

	int x;
	if (img->planes < 3) return;

	if (img->ctab < IW_COLFORMAT_MAX) img->ctab = IW_RGB;
	if (img->rowstride) {
		int planes = img->planes, height = img->height, width = img->width;
		int y;

		for (y=0; y<height; y++) {
			switch (img->type) {
				case IW_8U: {
					guchar *d = img->data[0] + y*img->rowstride;
					for (x=0; x<width; x++) {
						prev_inline_yuvToRgbVis (d[0], d[1], d[2],
												 d, d+1, d+2);
						d += planes;
					}
				}
				break;
				case IW_16U: {
					guint16 *d = (guint16*)img->data[0] + y*img->rowstride;
					float Y, U, V;
					int r, g, b;
					for (x=0; x<width; x++) {
						Y = 1.164*(d[0] - 16*256);
						U = d[1] - 128*256;
						V = d[2] - 128*256;
						r = gui_lrint (Y+1.597*V);
						g = gui_lrint (Y-0.392*U-0.816*V);
						b = gui_lrint (Y+2.018*U);
						d[0] = r < 0 ? 0 : (r > 256*256-1 ? 256*256-1 : (guint16)r);
						d[1] = g < 0 ? 0 : (g > 256*256-1 ? 256*256-1 : (guint16)g);
						d[2] = b < 0 ? 0 : (b > 256*256-1 ? 256*256-1 : (guint16)b);
						d += planes;
					}
				}
				break;
				case IW_32S: {
					gint32 *d = (gint32*)img->data[0] + y*img->rowstride;
					float Y, U, V, r, g, b;
					for (x=0; x<width; x++) {
						TORGB_REAL(d[0], d[1], d[2], r, g, b);
						d[0] = r < G_MININT ? G_MININT : (r > G_MAXINT ? G_MAXINT: gui_lrint(r));
						d[1] = g < G_MININT ? G_MININT : (g > G_MAXINT ? G_MAXINT: gui_lrint(g));
						d[2] = b < G_MININT ? G_MININT : (b > G_MAXINT ? G_MAXINT: gui_lrint(b));
						d += planes;
					}
				}
				break;
				case IW_FLOAT: {
					gfloat Y, U, V;
					gfloat *d = (gfloat*)img->data[0] + y*img->rowstride;
					for (x=0; x<width; x++) {
						TORGB_REAL(d[0], d[1], d[2], d[0], d[1], d[2]);
						d += planes;
					}
				}
				break;
				case IW_DOUBLE: {
					gdouble Y, U, V;
					gdouble *d = (gdouble*)img->data[0] + y*img->rowstride;
					for (x=0; x<width; x++) {
						TORGB_REAL(d[0], d[1], d[2], d[0], d[1], d[2]);
						d += planes;
					}
				}
				break;
			}
		}
	} else {
		switch (img->type) {
			case IW_8U: {
				guchar *y = img->data[0], *u = img->data[1], *v = img->data[2];
				for (x = img->width*img->height; x>0; x--) {
					prev_inline_yuvToRgbVis (*y, *u, *v, y, u, v);
					y++;
					u++;
					v++;
				}
			}
			break;
			case IW_16U: {
				guint16 *y = (guint16*)img->data[0],
					*u = (guint16*)img->data[1],
					*v = (guint16*)img->data[2];
				float Y, U, V;
				int r, g, b;
				for (x = img->width*img->height; x>0; x--) {
					Y = 1.164*(*y - 16*256);
					U = *u - 128*256;
					V = *v - 128*256;
					r = gui_lrint (Y+1.597*V);
					g = gui_lrint (Y-0.392*U-0.816*V);
					b = gui_lrint (Y+2.018*U);
					*y++ = r < 0 ? 0 : (r > 256*256-1 ? 256*256-1 : (guint16)r);
					*u++ = g < 0 ? 0 : (g > 256*256-1 ? 256*256-1 : (guint16)g);
					*v++ = b < 0 ? 0 : (b > 256*256-1 ? 256*256-1 : (guint16)b);
				}
			}
			break;
			case IW_32S: {
				gint32 *y = (gint32*)img->data[0],
					*u = (gint32*)img->data[1],
					*v = (gint32*)img->data[2];
				float Y, U, V, r, g, b;
				for (x = img->width*img->height; x>0; x--) {
					TORGB_REAL(*y, *u, *v, r, g, b);
					*y++ = r < G_MININT ? G_MININT : (r > G_MAXINT ? G_MAXINT: (gint32)r);
					*u++ = g < G_MININT ? G_MININT : (g > G_MAXINT ? G_MAXINT: (gint32)g);
					*v++ = b < G_MININT ? G_MININT : (b > G_MAXINT ? G_MAXINT: (gint32)b);
				}
			}
			break;
			case IW_FLOAT: {
				gfloat Y, U, V;
				gfloat *y = (gfloat*)img->data[0],
					*u = (gfloat*)img->data[1],
					*v = (gfloat*)img->data[2];
				for (x = img->width*img->height; x>0; x--)
					TORGB_REAL(*y, *u, *v, *y++, *u++, *v++);
			}
			break;
			case IW_DOUBLE: {
				gdouble Y, U, V;
				gdouble *y = (gdouble*)img->data[0],
					*u = (gdouble*)img->data[1],
					*v = (gdouble*)img->data[2];
				for (x = img->width*img->height; x>0; x--)
					TORGB_REAL(*y, *u, *v, *y++, *u++, *v++);
			}
			break;
		}
	}
}

/*********************************************************************
  Convert an interleaved image 'src' to a planed one and vice versa.
  Save the result in the newly allocated image 'dst'. If dst is NULL,
  additionally allocate a new image structure. Returns dst.
  Supports IW_8U - IW_DOUBLE.
*********************************************************************/
iwImage *iw_img_convert_layout (const iwImage *src, iwImage *dst)
{
	int x, y, p, width, planes;
	iwImgFree freePart = IW_IMG_FREE_DATA;

	if (!src) return NULL;

	if (!dst) {
		dst = iw_img_new();
		freePart |= IW_IMG_FREE_STRUCT;
		if (!dst) return NULL;
	}

	width = src->width;
	planes = src->planes;
	*dst = *src;
	if (dst->rowstride)
		dst->rowstride = 0;
	else
		dst->rowstride = width * planes * IW_TYPE_SIZE(src);
	iw_img_copy_ctab (src, dst);
	if (!iw_img_allocate(dst)) {
		iw_img_free (dst, freePart);
		return NULL;
	}

	if (src->rowstride == 0) {
		int pixels = src->width*src->height;
		for (p = 0; p < planes; p++) {
			switch (src->type) {
				case IW_8U: {
					guchar *s = (guchar*)src->data[p];
					guchar *d = (guchar*)dst->data[0] + p;
					for (x = 0; x < pixels; x++)
						d[planes*x] = s[x];
					break;
				}
				case IW_16U: {
					guint16 *s = (guint16*)src->data[p];
					guint16 *d = (guint16*)dst->data[0] + p;
					for (x = 0; x < pixels; x++)
						d[planes*x] = s[x];
					break;
				}
				case IW_32S: {
					gint32 *s = (gint32*)src->data[p];
					gint32 *d = (gint32*)dst->data[0] + p;
					for (x = 0; x < pixels; x++)
						d[planes*x] = s[x];
					break;
				}
				case IW_FLOAT: {
					gfloat *s = (gfloat*)src->data[p];
					gfloat *d = (gfloat*)dst->data[0] + p;
					for (x = 0; x < pixels; x++)
						d[planes*x] = s[x];
					break;
				}
				case IW_DOUBLE: {
					gdouble *s = (gdouble*)src->data[p];
					gdouble *d = (gdouble*)dst->data[0] + p;
					for (x = 0; x < pixels; x++)
						d[planes*x] = s[x];
					break;
				}
			}
		}
	} else {
		for (p = 0; p < planes; p++) {
			guchar *data = src->data[0];
			for (y = 0; y < src->height; y++) {
				switch (src->type) {
					case IW_8U: {
						guchar *s = (guchar*)data + p;
						guchar *d = (guchar*)dst->data[p] + y*dst->width;
						for (x = 0; x < width; x++)
							d[x] = s[planes*x];
						break;
					}
					case IW_16U: {
						guint16 *s = (guint16*)data + p;
						guint16 *d = (guint16*)dst->data[p] + y*dst->width;
						for (x = 0; x < width; x++)
							d[x] = s[planes*x];
						break;
					}
					case IW_32S: {
						gint32 *s = (gint32*)data + p;
						gint32 *d = (gint32*)dst->data[p] + y*dst->width;
						for (x = 0; x < width; x++)
							d[x] = s[planes*x];
						break;
					}
					case IW_FLOAT: {
						gfloat *s = (gfloat*)data + p;
						gfloat *d = (gfloat*)dst->data[p];
						for (x = 0; x < width; x++)
							d[x] = s[planes*x];
						break;
					}
					case IW_DOUBLE: {
						gdouble *s = (gdouble*)data + p;
						gdouble *d = (gdouble*)dst->data[p] + y*dst->width;
						for (x = 0; x < width; x++)
							d[x] = s[planes*x];
						break;
					}
				}
				data += src->rowstride;
			}
		}
	}
	return dst;
}

typedef struct imgFPolyData {
	imgLineFunc fkt;
	iwImage *img;
	void *data;
} imgFPolyData;

static void cb_img_fillPoly (int sx, int sy, int cnt, imgFPolyData *data)
{
	iwImage *img = data->img;
	int i, off = sy*img->width + sx;
	uchar *ri, *gi, *bi;
	uchar r = 0, g = 0, b = 0;

	if (!data->fkt && data->data) {
		r = ((iwColor*)(data->data))->r;
		g = ((iwColor*)(data->data))->g;
		b = ((iwColor*)(data->data))->b;
	}

	ri = img->data[0]+off;
	if (img->planes == 3) {
		gi = img->data[1]+off;
		bi = img->data[2]+off;
		if (data->fkt) {
			data->fkt (img, ri, gi, bi, cnt, data->data);
		} else {
			for (i=0; i<cnt; i++) {
				*ri++ = r;
				*gi++ = g;
				*bi++ = b;
			}
		}
	} else {
			if (data->fkt) {
				data->fkt (img, ri, NULL, NULL, cnt, data->data);
			} else {
				for (i=0; i<cnt; i++)
					*ri++ = r;
			}
	}
}
/*********************************************************************
  npts: number of points , pts: the points
  invers==FALSE: Fill the polygon defined by the points ptsIn.
  invers==TRUE: Fill everything except the polygon.
  fkt!=NULL: fkt() is called (instead of changing the image directly.
  fkt==NULL: The polygon (or the inverse) is filled,
             data!=NULL: data defines the color to use.
*********************************************************************/
BOOL iw_img_fillPoly (iwImage *img, int npts, const prevDataPoint *pts,
					  BOOL invers, imgLineFunc fkt, void *data)
{
	imgFPolyData d;
	d.fkt = fkt;
	d.img = img;
	d.data = data;
	return prev_drawFPoly_raw (img->width, img->height, npts, pts, invers,
							   (prevPolyFunc)cb_img_fillPoly, &d);
}

/*********************************************************************
  count: number of points , ptsIn: the points
  invers==FALSE: Fill the polygon defined by the points ptsIn.
  invers==TRUE: Fill everything except the polygon.
  fkt!=NULL: fkt() is called (instead of changing the image directly.
  fkt==NULL: The polygon (or the inverse) is filled,
		data!=NULL: data defines the color to use.
*********************************************************************/
BOOL iw_img_fillPoly_raw (uchar **s, int width, int height, int planes,
						  const Polygon_t *p, BOOL invers,
						  imgLineFunc fkt, void *data)
{
	int i;
	iwImage img;
	prevDataPoint *pts = malloc(sizeof(prevDataPoint)*p->n_punkte);
	BOOL ret;

	img.width = width;
	img.height = height;
	img.planes = planes;
	img.rowstride = 0;
	img.data = s;

	for (i=0; i<p->n_punkte; i++) {
		pts[i].x = p->punkt[i]->x;
		pts[i].y = p->punkt[i]->y;
	}
	ret = iw_img_fillPoly (&img, p->n_punkte, pts, invers, fkt, data);
	free (pts);
	return ret;
}
