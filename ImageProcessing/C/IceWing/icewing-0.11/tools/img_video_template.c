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

#ifndef UCLAMP

#define UCLAMP(x,y,z) ((y) < (z) ? CLAMP(x,y,z) : CLAMP(x,z,y))
#define SQR(x) ((x)*(x))

/* 0x94=10010100  0x16=00010110  0x61=01100001  0x49=01001001
   RGGB           BGGR           GRBG           GBRG */
static const int filters[4] = {0x94, 0x16, 0x61, 0x49};
#define FC(pattern,col,row) \
	(filters[pattern] >> ((((row) << 1 & 3) + ((col) & 1)) << 1) & 3)

static const double xyz_rgb[3][3] = {		/* RGB to XYZ */
	{ 0.412453, 0.357580, 0.180423 },
	{ 0.212671, 0.715160, 0.072169 },
	{ 0.019334, 0.119193, 0.950227 } };
static const float d65_white[3] = {0.950456, 1, 1.088754};

#endif

/*********************************************************************
  Decode stereo image by deinterlacing and saving the single images
  one after the other. length: size of one decoded image
*********************************************************************/
static void RENAME(img_stereo_decode) (IVtype *s, IVtype *d, int length)
{
	IVtype *d2 = d+length;
	int i, j;

	j = 0;
	for (i = length; i>0; i--) {
		d[j] = *s++;
		d2[j] = *s++;
		j++;
	}
}

/*********************************************************************
  Bilinear interpolation of the line y from src to dimg.
*********************************************************************/
static void RENAME(line_interpolate) (IVtype *src, iwImage *dimg,
									  iwImgBayerPattern pattern, int y)
{
	IVtype **dst = (IVtype **)dimg->data;
	int width = dimg->width, height = dimg->height;
	int x, mx, my, f, off, sum[6];

	for (x = 0; x < width; x++) {
		memset (sum, 0, sizeof(sum));
		for (my = y-1; my < y+2; my++)
			for (mx = x-1; mx < x+2; mx++)
				if (mx >= 0 && my >= 0 && mx < width && my < height) {
					f = FC (pattern, mx, my);
					sum[f] += src[my*width+mx];
					sum[f+3]++;
				}
		f = FC (pattern, x, y);
		off = y*width+x;
		dst[f][off] = src[off];
		f = (f+1) % 3;
		dst[f][off] = sum[f] / sum[f+3];
		f = (f+1) % 3;
		dst[f][off] = sum[f] / sum[f+3];
	}
}

/*********************************************************************
  Bilinear interpolation for a 3 pixel border.
*********************************************************************/
static void RENAME(border_interpolate) (IVtype *src, iwImage *dimg, iwImgBayerPattern pattern)
{
	IVtype **dst = (IVtype **)dimg->data;
	int width = dimg->width, height = dimg->height;
	int x, y, mx, my, f, sum[6];

	for (y = 0; y < 3 && y < height; y++) {
		RENAME(line_interpolate) (src, dimg, pattern, y);
		RENAME(line_interpolate) (src, dimg, pattern, height-1-y);
	}
	for (y = 3; y < height-3; y++) {
		for (x = 0; x < width; x++) {
			memset (sum, 0, sizeof(sum));
			for (my = y-1; my < y+2; my++)
				for (mx = x-1; mx < x+2; mx++)
					if (mx >= 0 && mx < width) {
						f = FC (pattern, mx, my);
						sum[f] += src[my*width+mx];
						sum[f+3]++;
					}
			f = FC (pattern, x, y);
			dst[f][y*width+x] = src[y*width+x];
			f = (f+1) % 3;
			dst[f][y*width+x] = sum[f] / sum[f+3];
			f = (f+1) % 3;
			dst[f][y*width+x] = sum[f] / sum[f+3];

			if (x == 2 && width > 6)
				x = width-4;
		}
	}
}

/*********************************************************************
  Edge sensing interpolation of the green pixels from s for
  IW_IMG_BAYER_RGGB and IW_IMG_BAYER_BGGR bayer pattern.
*********************************************************************/
static void RENAME(bayer_edge_green_cggc) (IVtype *s, IVtype *dg, int w, int h)
{
	int x, y, dh, dv;

	dg[0] = (s[1]+s[w]) / 2;
	for (x=1; x<w-1; x+=2) {
		dg[x] = s[x];
		dg[x+1] = (s[x]+s[x+2]+s[x+w+1]) / 3;
	}
	dg[x] = s[x];
	dg += w;
	s += w;
	h -= 2;
	for (y=1; y<h; y+=2) {
		dg[0] = s[0];
		dg[w] = (s[0]+s[w+1]+s[2*w]) / 3;
		dg++; s++;
		for (x=1; x<w-1; x+=2) {
			dh = ABS(s[-1]-s[1]);
			dv = ABS(s[-w]-s[w]);
			if (dh < dv)
				dg[0] = (s[-1]+s[1]) / 2;
			else if (dh > dv)
				dg[0] = (s[-w]+s[w]) / 2;
			else
				dg[0] = (s[-w]+s[-1]+s[1]+s[w]) / 4;
			dg[1] = s[1];
			dg[w] = s[w];

			dh = ABS(s[w]-s[w+2]);
			dv = ABS(s[1]-s[2*w+1]);
			if (dh < dv)
				dg[w+1] = (s[w]+s[w+2]) / 2;
			else if (dh > dv)
				dg[w+1] = (s[1]+s[2*w+1]) / 2;
			else
				dg[w+1] = (s[1]+s[w]+s[w+2]+s[2*w+1]) / 4;
			dg += 2;
			s += 2;
		}
		dg[0] = (s[-w]+s[-1]+s[w]) / 3;
		dg[w] = s[w];
		dg += 1+w;
		s += 1+w;
	}
	dg[0] = s[0];
	for (x=1; x<w-1; x+=2) {
		dg[x] = (s[x-w]+s[x-1]+s[x+1]) / 3;
		dg[x+1] = s[x+1];
	}
	dg[x] = (s[x-w]+s[x-1]) / 2;
}

/*********************************************************************
  Edge sensing interpolation of the green pixels from s for
  IW_IMG_BAYER_GRBG and IW_IMG_BAYER_GBRG bayer pattern.
*********************************************************************/
static void RENAME(bayer_edge_green_gccg) (IVtype *s, IVtype *dg, int w, int h)
{
	int x, y, dh, dv;

	dg[0] = s[0];
	for (x=1; x<w-1; x+=2) {
		dg[x] = (s[x-1]+s[x+1]+s[x+w]) / 3;
		dg[x+1] = s[x+1];
	}
	dg[x] = (s[x-1]+s[x+w]) / 2;
	dg += w;
	s += w;
	h -= 2;
	for (y=1; y<h; y+=2) {
		dg[0] = (s[-w]+s[1]+s[w]) / 3;
		dg[w] = s[w];
		dg++; s++;
		for (x=1; x<w-1; x+=2) {
			dg[0] = s[0];

			dh = ABS(s[0]-s[2]);
			dv = ABS(s[-w+1]-s[w+1]);
			if (dh < dv)
				dg[1] = (s[0]+s[2]) / 2;
			else if (dh > dv)
				dg[1] = (s[-w+1]+s[w+1]) / 2;
			else
				dg[1] = (s[-w+1]+s[0]+s[2]+s[w+1]) / 4;
			dh = ABS(s[w-1]-s[w+1]);
			dv = ABS(s[0]-s[2*w]);
			if (dh < dv)
				dg[w] = (s[w-1]+s[w+1]) / 2;
			else if (dh > dv)
				dg[w] = (s[0]+s[2*w]) / 2;
			else
				dg[w] = (s[0]+s[w-1]+s[w+1]+s[2*w]) / 4;
			dg[w+1] = s[w+1];
			dg += 2;
			s += 2;
		}
		dg[0] = s[0];
		dg[w] = (s[0]+s[-1+w]+s[2*w]) / 3;
		dg += 1+w;
		s += 1+w;
	}
	dg[0] = (s[-w]+s[1]) / 2;
	for (x=1; x<w-1; x+=2) {
		dg[x] = s[x];
		dg[x+1] = (s[x-w+1]+s[x]+s[x+2]) / 3;
	}
	dg[x] = s[x];
}

/*********************************************************************
  Bilinear interpolation of the green pixels from s for
  IW_IMG_BAYER_RGGB and IW_IMG_BAYER_BGGR bayer pattern.
*********************************************************************/
static void RENAME(bayer_bilinear_green_cggc) (IVtype *s, IVtype *dg, int w, int h)
{
	int x, y;

	dg[0] = (s[1]+s[w]) / 2;
	for (x=1; x<w-1; x+=2) {
		dg[x] = s[x];
		dg[x+1] = (s[x]+s[x+2]+s[x+w+1]) / 3;
	}
	dg[x] = s[x];
	dg += w;
	s += w;
	h -= 2;
	for (y=1; y<h; y+=2) {
		dg[0] = s[0];
		dg[w] = (s[0]+s[w+1]+s[2*w]) / 3;
		dg++; s++;
		for (x=1; x<w-1; x+=2) {
			dg[0] = (s[-w]+s[-1]+s[1]+s[w]) / 4;
			dg[1] = s[1];
			dg[w] = s[w];
			dg[w+1] = (s[1]+s[w]+s[w+2]+s[2*w+1]) / 4;
			dg += 2;
			s += 2;
		}
		dg[0] = (s[-w]+s[-1]+s[w]) / 3;
		dg[w] = s[w];
		dg += 1+w;
		s += 1+w;
	}
	dg[0] = s[0];
	for (x=1; x<w-1; x+=2) {
		dg[x] = (s[x-w]+s[x-1]+s[x+1]) / 3;
		dg[x+1] = s[x+1];
	}
	dg[x] = (s[x-w]+s[x-1]) / 2;
}

/*********************************************************************
  Bilinear interpolation of the green pixels from s for
  IW_IMG_BAYER_GRBG and IW_IMG_BAYER_GBRG bayer pattern.
*********************************************************************/
static void RENAME(bayer_bilinear_green_gccg) (IVtype *s, IVtype *dg, int w, int h)
{
	int x, y;

	dg[0] = s[0];
	for (x=1; x<w-1; x+=2) {
		dg[x] = (s[x-1]+s[x+1]+s[x+w]) / 3;
		dg[x+1] = s[x+1];
	}
	dg[x] = (s[x-1]+s[x+w]) / 2;
	dg += w;
	s += w;
	h -= 2;
	for (y=1; y<h; y+=2) {
		dg[0] = (s[-w]+s[1]+s[w]) / 3;
		dg[w] = s[w];
		dg++; s++;
		for (x=1; x<w-1; x+=2) {
			dg[0] = s[0];
			dg[1] = (s[-w+1]+s[0]+s[2]+s[w+1]) / 4;
			dg[w] = (s[0]+s[w-1]+s[w+1]+s[2*w]) / 4;
			dg[w+1] = s[w+1];
			dg += 2;
			s += 2;
		}
		dg[0] = s[0];
		dg[w] = (s[0]+s[-1+w]+s[2*w]) / 3;
		dg += 1+w;
		s += 1+w;
	}
	dg[0] = (s[-w]+s[1]) / 2;
	for (x=1; x<w-1; x+=2) {
		dg[x] = s[x];
		dg[x+1] = (s[x-w+1]+s[x]+s[x+2]) / 3;
	}
	dg[x] = s[x];
}

/*********************************************************************
  Smooth hue transition interpolation (edge==FALSE) and
  edge sensing interpolation (edge==TRUE) for bayer decomposition.
  See http://www-ise.stanford.edu/~tingchen/main.htm
*********************************************************************/
static void RENAME(img_bayer_hue) (IVtype *s, iwImage *dimg,
								   iwImgBayerPattern pattern, BOOL edge)
{
#define H1(p1,p2,dest)	\
	val = dg[p1]*s[p2]; \
	if (dg[p2] != 0) val /= dg[p2]; \
	if (val > IVsize) \
		d##dest[p1] = IVsize; \
	else \
		d##dest[p1] = val;

#if IVsize==255
#define H2(p1,p2,p3,dest) \
	if (dg[p2] != 0 && dg[p3] != 0) { \
		val = dg[p1]*(s[p2]*dg[p3]+s[p3]*dg[p2]) / (2*dg[p2]*dg[p3]); \
		if (val > IVsize) \
			d##dest[p1] = IVsize; \
		else \
			d##dest[p1] = val; \
	} else \
		d##dest[p1]	= (s[p2]+s[p3])/2;
#else
#define H2(p1,p2,p3,dest) \
	if (dg[p2] != 0 && dg[p3] != 0) { \
		float f = dg[p1]*((float)s[p2]/dg[p2] + (float)s[p3]/dg[p3]) / 2; \
		if (f > IVsize) \
			d##dest[p1] = IVsize; \
		else \
			d##dest[p1] = gui_lrint(f); \
	} else \
		d##dest[p1] = (s[p2]+s[p3])/2;
#endif

#define H4(p,dest) \
	if (dg[p-w-1] != 0 && dg[p-w+1] != 0 && dg[p+w-1] != 0 && dg[p+w+1] != 0) { \
		float f = dg[p]*((float)s[p-w-1]/dg[p-w-1]+(float)s[p-w+1]/dg[p-w+1]+ \
						 (float)s[p+w-1]/dg[p+w-1]+(float)s[p+w+1]/dg[p+w+1]) / 4; \
		if (f > IVsize) \
			d##dest[p] = IVsize; \
		else \
			d##dest[p] = gui_lrint(f); \
	} else \
		d##dest[p] = (s[p-w-1]+s[p-w+1]+s[p+w-1]+s[p+w+1])/4;

	IVtype *dr, *dg, *db, **d = (IVtype **)dimg->data;
	int x, y, w, h;
	unsigned int val;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_GRBG) {
		dr = d[0];
		db = d[2];
	} else {
		dr = d[2];
		db = d[0];
	}
	dg = d[1];
	w = dimg->width;
	h = dimg->height;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_BGGR) {
		if (edge)
			RENAME(bayer_edge_green_cggc) (s, dg, w, h);
		else
			RENAME(bayer_bilinear_green_cggc) (s, dg, w, h);

		/* Red / Blue */
		dr[0] = s[0];
		H1(0, w+1, b);
		for (x=1; x<w-1; x+=2) {
			H2 (x, x-1, x+1, r);
			dr[x+1] = s[x+1];
			H1(x, x+w, b);
			H2(x+1, x+w, x+w+2, b);
		}
		H1(x, x-1, r);
		H1(x, x+w, b);
		dg += w;
		dr += w;
		db += w;
		s += w;
		h -= 2;
		for (y=1; y<h; y+=2) {
			H2(0, -w, w, r);
			dr[w] = s[w];
			H1(0, 1, b);
			H2(w, 1, 2*w+1, b);
			dg++; dr++; db++;
			s++;
			for (x=1; x<w-1; x+=2) {
				H4(0, r);
				H2(1, -w+1, w+1, r);
				H2(w, w-1, w+1, r);
				dr[w+1] = s[w+1];

				db[0] = s[0];
				H2(1, 0, 2, b);
				H2(w, 0, 2*w, b);
				H4(w+1, b);
				dg += 2;
				dr += 2;
				db += 2;
				s += 2;
			}
			H2(0, -w-1, w-1, r);
			H1(w, w-1, r);
			db[0] = s[0];
			H2(w, 0, 2*w, b);
			dg += 1+w;
			dr += 1+w;
			db += 1+w;
			s += 1+w;
		}
		H1(0, -w, r);
		H1(0, 1, b);
		for (x=1; x<w-1; x+=2) {
			H2(x, x-w-1, x-w+1, r);
			H1(x+1, x-w+1, r);
			db[x] = s[x];
			H2(x+1, x, x+2, b);
		}
		H1(x, x-w-1, r);
		db[x] = s[x];
	} else {
		if (edge)
			RENAME(bayer_edge_green_gccg) (s, dg, w, h);
		else
			RENAME(bayer_bilinear_green_gccg) (s, dg, w, h);

		/* Red / Blue */
		H1(0,1,r);
		H1(0,w,b);
		for (x=1; x<w-1; x+=2) {
			dr[x] = s[x];
			H2(x+1, x, x+2, r);
			H2(x, x+w-1, x+w+1, b);
			H1(x+1, x+w+1, b);
		}
		dr[x] = s[x];
		H1(x, x+w-1, b);

		dg += w;
		dr += w;
		db += w;
		s += w;
		h -= 2;
		for (y=1; y<h; y+=2) {
			H2(0, -w+1, w+1, r);
			H1(w, w+1, r);
			db[0] = s[0];
			H2(w, 0, 2*w, b);

			dg++; dr++; db++;
			s++;
			for (x=1; x<w-1; x+=2) {
				H2(0, -w, w, r);
				H4(1, r);
				dr[w] = s[w];
				H2(w+1, w, w+2, r);

				H2(0, -1, 1, b);
				db[1] = s[1];
				H4(w, b);
				H2(w+1, 1, 2*w+1, b);
				dg += 2;
				dr += 2;
				db += 2;
				s += 2;
			}
			H2(0, -w, w, r);
			dr[w] = s[w];
			H1(0, -1, b);
			H2(w, -1, 2*w-1, b);

			dg += 1+w;
			dr += 1+w;
			db += 1+w;
			s += 1+w;
		}
		H1(0, -w+1, r);
		db[0] = s[0];
		for (x=1; x<w-1; x+=2) {
			H1(x, x-w, r);
			H2(x+1, x-w, x-w+2, r);
			H2(x, x-1, x+1, b);
			db[x+1] = s[x+1];
		}
		H1(x, x-w, r);
		H1(x, x-1, b);
	}
	if (dimg->height & 1)
		RENAME(line_interpolate) (s, dimg, pattern, dimg->height-1);
#undef H1
#undef H2
#undef H4
}

/*********************************************************************
  Bilinear interpolation for bayer decomposition.
  See http://www-ise.stanford.edu/~tingchen/main.htm
*********************************************************************/
static void RENAME(img_bayer_bilinear) (IVtype *s, iwImage *dimg, iwImgBayerPattern pattern)
{
#define H2(p1,p2,p3,dest) d##dest[p1] = (s[p2]+s[p3])/2;
#define H4(p,dest) d##dest[p]  = (s[p-w-1]+s[p-w+1]+s[p+w-1]+s[p+w+1])/4;

	IVtype *dr, *dg, *db, **d = (IVtype **)dimg->data;
	int x, y, w, h;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_GRBG) {
		dr = d[0];
		db = d[2];
	} else {
		dr = d[2];
		db = d[0];
	}
	dg = d[1];
	w = dimg->width;
	h = dimg->height;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_BGGR) {
		RENAME(bayer_bilinear_green_cggc) (s, dg, w, h);

		/* Red / Blue */
		dr[0] = s[0];
		db[0] = s[w+1];
		for (x=1; x<w-1; x+=2) {
			H2 (x, x-1, x+1, r);
			dr[x+1] = s[x+1];
			db[x] = s[x+w];
			H2(x+1, x+w, x+w+2, b);
		}
		dr[x] = s[x-1];
		db[x] = s[x+w];
		dg += w;
		dr += w;
		db += w;
		s += w;
		h -= 2;
		for (y=1; y<h; y+=2) {
			H2(0, -w, w, r);
			dr[w] = s[w];
			db[0] = s[1];
			H2(w, 1, 2*w+1, b);
			dg++; dr++; db++;
			s++;
			for (x=1; x<w-1; x+=2) {
				H4(0, r);
				H2(1, -w+1, w+1, r);
				H2(w, w-1, w+1, r);
				dr[w+1] = s[w+1];

				db[0] = s[0];
				H2(1, 0, 2, b);
				H2(w, 0, 2*w, b);
				H4(w+1, b);
				dg += 2;
				dr += 2;
				db += 2;
				s += 2;
			}
			H2(0, -w-1, w-1, r);
			dr[w] = s[w-1];
			db[0] = s[0];
			H2(w, 0, 2*w, b);
			dg += 1+w;
			dr += 1+w;
			db += 1+w;
			s += 1+w;
		}
		dr[0] = s[-w];
		db[0] = s[1];
		for (x=1; x<w-1; x+=2) {
			H2(x, x-w-1, x-w+1, r);
			dr[x+1] = s[x-w+1];
			db[x] = s[x];
			H2(x+1, x, x+2, b);
		}
		dr[x] = s[x-w-1];
		db[x] = s[x];
	} else {
		RENAME(bayer_bilinear_green_gccg) (s, dg, w, h);

		/* Red / Blue */
		dr[0] = s[1];
		db[0] = s[w];
		for (x=1; x<w-1; x+=2) {
			dr[x] = s[x];
			H2(x+1, x, x+2, r);
			H2(x, x+w-1, x+w+1, b);
			db[x+1] = s[x+w+1];
		}
		dr[x] = s[x];
		db[x] = s[x+w-1];

		dg += w;
		dr += w;
		db += w;
		s += w;
		h -= 2;
		for (y=1; y<h; y+=2) {
			H2(0, -w+1, w+1, r);
			dr[w] = s[w+1];
			db[0] = s[0];
			H2(w, 0, 2*w, b);

			dg++; dr++; db++;
			s++;
			for (x=1; x<w-1; x+=2) {
				H2(0, -w, w, r);
				H4(1, r);
				dr[w] = s[w];
				H2(w+1, w, w+2, r);

				H2(0, -1, 1, b);
				db[1] = s[1];
				H4(w, b);
				H2(w+1, 1, 2*w+1, b);
				dg += 2;
				dr += 2;
				db += 2;
				s += 2;
			}
			H2(0, -w, w, r);
			dr[w] = s[w];
			db[0] = s[-1];
			H2(w, -1, 2*w-1, b);

			dg += 1+w;
			dr += 1+w;
			db += 1+w;
			s += 1+w;
		}
		dr[0] = s[-w+1];
		db[0] = s[0];
		for (x=1; x<w-1; x+=2) {
			dr[x] = s[x-w];
			H2(x+1, x-w, x-w+2, r);
			H2(x, x-1, x+1, b);
			db[x+1] = s[x+1];
		}
		dr[x] = s[x-w];
		db[x] = s[x-1];
	}
	if (dimg->height & 1)
		RENAME(line_interpolate) (s, dimg, pattern, dimg->height-1);
#undef H2
#undef H4
}

/*********************************************************************
  Nearest neighbor interpolation for bayer decomposition.
  See http://www-ise.stanford.edu/~tingchen/main.htm
*********************************************************************/
static void RENAME(img_bayer_neighbor) (IVtype *s, iwImage *dimg, iwImgBayerPattern pattern)
{
	IVtype *dr, *dg, *db, **d = (IVtype **)dimg->data;
	int x, y, w, h;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_GRBG) {
		dr = d[0];
		db = d[2];
	} else {
		dr = d[2];
		db = d[0];
	}
	dg = d[1];
	w = dimg->width;
	h = dimg->height-1;

	/* Bayer mono -> RGB */
	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_BGGR) {
		for (y=0; y<h; y+=2) {
			for (x=0; x<w; x+=2) {
				*dr = *s;
				*(dr+1) = *s;
				*(dr+w) = *s;
				*(dr+w+1) = *s;

				*dg = *(s+1);
				*(dg+1) = *(s+1);
				*(dg+w) = *(s+w);
				*(dg+w+1) = *(s+w);

				*db = *(s+w+1);
				*(db+1) = *(s+w+1);
				*(db+w) = *(s+w+1);
				*(db+w+1) = *(s+w+1);

				dr += 2;
				dg += 2;
				db += 2;
				s += 2;
			}
			dr += w;
			dg += w;
			db += w;
			s += w;
		}
	} else {
		for (y=0; y<h; y+=2) {
			for (x=0; x<w; x+=2) {
				*dr = *(s+1);
				*(dr+1) = *(s+1);
				*(dr+w) = *(s+1);
				*(dr+w+1) = *(s+1);

				*dg = *(s);
				*(dg+1) = *(s);
				*(dg+w) = *(s+w+1);
				*(dg+w+1) = *(s+w+1);

				*db = *(s+w);
				*(db+1) = *(s+w);
				*(db+w) = *(s+w);
				*(db+w+1) = *(s+w);

				dr += 2;
				dg += 2;
				db += 2;
				s += 2;
			}
			dr += w;
			dg += w;
			db += w;
			s += w;
		}
	}
	if (dimg->height & 1)
		RENAME(line_interpolate) (s, dimg, pattern, dimg->height-1);
}

/*********************************************************************
  Bayer decomposition by downsampling.
*********************************************************************/
static void RENAME(img_bayer_down) (IVtype *s, iwImage *dimg, iwImgBayerPattern pattern)
{
	IVtype *dr, *dg, *db, **d = (IVtype **)dimg->data;
	int x, y, w, h;

	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_GRBG) {
		dr = d[0];
		db = d[2];
	} else {
		dr = d[2];
		db = d[0];
	}
	dg = d[1];
	w = dimg->width;
	h = dimg->height;

	/* Bayer mono -> RGB */
	if (pattern == IW_IMG_BAYER_RGGB || pattern == IW_IMG_BAYER_BGGR) {
		for (y=0; y<h; y++) {
			for (x=0; x<w; x++) {
				*dr++ = *s;
				*dg++ = (*(s+1) + *(s+2*w))/2;
				*db++ = *(s+2*w+1);
				s += 2;
			}
			s += 2*w;
		}
	} else {
		for (y=0; y<h; y++) {
			for (x=0; x<w; x++) {
				*dg++ = (*(s) + *(s+2*w+1))/2;
				*dr++ = *(s+1);
				*db++ = *(s+2*w);
				s += 2;
			}
			s += 2*w;
		}
	}
}

static void RENAME(rgb_to_cielab) (IVtype rgb[3], short lab[3])
{
	static BOOL initialized = FALSE;
	static float cbrt[IVsize+1], xyz[3][3];
	int i, j;
	float r, x, y, z;

	if (!initialized) {
		for (i = 0; i <= IVsize; i++) {
			r = i / (float)IVsize;
			cbrt[i] = r > 0.008856 ? pow(r,1/3.0) : 7.787*r + 16/116.0;
		}
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				xyz[i][j] = xyz_rgb[i][j] / d65_white[i];
		initialized = TRUE;
	}
	i = gui_lrint(xyz[0][0]*rgb[0] + xyz[0][1]*rgb[1] + xyz[0][2]*rgb[2]);
	x = cbrt[CLAMP(i, 0, IVsize)];
	i = gui_lrint(xyz[1][0]*rgb[0] + xyz[1][1]*rgb[1] + xyz[1][2]*rgb[2]);
	y = cbrt[CLAMP(i, 0, IVsize)];
	i = gui_lrint(xyz[2][0]*rgb[0] + xyz[2][1]*rgb[1] + xyz[2][2]*rgb[2]);
	z = cbrt[CLAMP(i, 0, IVsize)];

	/* Return CIE LAB values multiplied by 64 */
	lab[0] = gui_lrint(64 * (116 * y - 16));
	lab[1] = gui_lrint(64 * 500 * (x - y));
	lab[2] = gui_lrint(64 * 200 * (y - z));
}

/*********************************************************************
  Adaptive homogeneity-directed bayer decomposition ported from dcraw
  (http://www.cybercom.net/~dcoffin/dcraw). Based on the work of
  Keigo Hirakawa and Thomas Parks for the algorithm and Paul Lee for
  the implementation in dcraw.
*********************************************************************/
#define TS 256		/* Tile Size */
static void RENAME(img_bayer_ahd) (IVtype *src, iwImage *dimg, iwImgBayerPattern pattern)
{
	static const int dir[4] = { -1, 1, -TS, TS };
	IVtype *pix;
	IVtype **dst = (IVtype **)dimg->data;
	int width = dimg->width, height = dimg->height;
	int i, top, left, y, x, tr, tc, c, d, val, hm[2];
	unsigned int ldiff[2][4], abdiff[2][4], leps, abeps;
	IVtype (*rgb)[TS][TS][3], (*rix)[3];
	short (*lab)[TS][TS][3];
	char (*homo)[TS][TS], *buffer, *h;

	RENAME(border_interpolate) (src, dimg, pattern);

	buffer = iw_malloc ((sizeof(IVtype)*6+14)*TS*TS, "ahd_interpolate()");
	rgb  = (IVtype(*)[TS][TS][3]) buffer;
	lab  = (short (*)[TS][TS][3])(buffer + sizeof(IVtype)*6*TS*TS);
	homo = (char  (*)[TS][TS])   (buffer + (sizeof(IVtype)*6+12)*TS*TS);

	for (top = 0; top < height; top += TS-6) {
		for (left = 0; left < width; left += TS-6) {
			/* Interpolate green horizontally and vertically */
			for (y = top < 2 ? 2:top; y < top+TS && y < height-2; y++) {
				x = left + (FC(pattern,left,y) == 1);
				if (x < 2) x += 2;
				for (; x < left+TS && x < width-2; x+=2) {
					pix = src + y*width+x;
					val = ((pix[-1] + pix[0] + pix[1]) * 2
						   - pix[-2] - pix[2]) >> 2;
					rgb[0][y-top][x-left][1] = UCLAMP(val,pix[-1],pix[1]);
					val = ((pix[-width] + pix[0] + pix[width]) * 2
						   - pix[-2*width] - pix[2*width]) >> 2;
					rgb[1][y-top][x-left][1] = UCLAMP(val,pix[-width],pix[width]);
				}
			}
			/* Interpolate red and blue, and convert to CIELab */
			for (d = 0; d < 2; d++) {
				for (y = top+1; y < top+TS-1 && y < height-1; y++) {
					for (x = left+1; x < left+TS-1 && x < width-1; x++) {
						pix = src + y*width+x;
						rix = &rgb[d][y-top][x-left];
						if ((c = 2 - FC(pattern,x,y)) == 1) {
							c = FC(pattern,x,y+1);
							val = pix[0] + (( pix[-1] + pix[1]
											  - rix[-1][1] - rix[1][1] ) >> 1);
							rix[0][2-c] = CLAMP(val, 0, IVsize);
							val = pix[0] + (( pix[-width] + pix[width]
											  - rix[-TS][1] - rix[TS][1] ) >> 1);
						} else
							val = rix[0][1] + (( pix[-width-1] + pix[-width+1]
												 + pix[width-1] + pix[width+1]
												 - rix[-TS-1][1] - rix[-TS+1][1]
												 - rix[+TS-1][1] - rix[+TS+1][1] + 1) >> 2);
						rix[0][c] = CLAMP(val, 0, IVsize);
						rix[0][FC(pattern,x,y)] = pix[0];
						RENAME(rgb_to_cielab) (rix[0], lab[d][y-top][x-left]);
					}
				}
			}
			/* Build homogeneity maps from the CIELab images */
			memset (homo, 0, 2*TS*TS);
			for (y = 2; y < TS-2 && y < height-top; y++) {
				for (x = 2; x < TS-2 && x < width-left; x++) {
					for (d = 0; d < 2; d++)
						for (i = 0; i < 4; i++) {
							val = lab[d][y][x][0]-lab[d][y][x+dir[i]][0];
							ldiff[d][i] = ABS(val);
						}
					leps = MIN(MAX(ldiff[0][0],ldiff[0][1]),
							   MAX(ldiff[1][2],ldiff[1][3]));
					for (d = 0; d < 2; d++)
						for (i = 0; i < 4; i++)
							if (i >> 1 == d || ldiff[d][i] <= leps)
								abdiff[d][i] =
									SQR(lab[d][y][x][1]-lab[d][y][x+dir[i]][1])
									+ SQR(lab[d][y][x][2]-lab[d][y][x+dir[i]][2]);
					abeps = MIN(MAX(abdiff[0][0],abdiff[0][1]),
								MAX(abdiff[1][2],abdiff[1][3]));
					for (d = 0; d < 2; d++)
						for (i = 0; i < 4; i++)
							if (ldiff[d][i] <= leps && abdiff[d][i] <= abeps)
								homo[d][y][x]++;
				}
			}
			/* Combine the most homogenous pixels for the final result */
			for (y = top+3; y < top+TS-3 && y < height-3; y++) {
				tr = y-top;
				for (x = left+3; x < left+TS-3 && x < width-3; x++) {
					tc = x-left;
					for (d = 0; d < 2; d++) {
						h = &homo[d][tr-1][tc-1];
						hm[d] =
							h[0] + h[1] + h[2] +
							h[TS+0] + h[TS+1] + h[TS+2] +
							h[TS+TS+0] + h[TS+TS+1] + h[TS+TS+2];
					}
					val = y*width+x;
					if (hm[0] != hm[1]) {
						c = hm[1] > hm[0];
						dst[0][val] = rgb[c][tr][tc][0];
						dst[1][val] = rgb[c][tr][tc][1];
						dst[2][val] = rgb[c][tr][tc][2];
					} else {
						dst[0][val] = (rgb[0][tr][tc][0] + rgb[1][tr][tc][0]) >> 1;
						dst[1][val] = (rgb[0][tr][tc][1] + rgb[1][tr][tc][1]) >> 1;
						dst[2][val] = (rgb[0][tr][tc][2] + rgb[1][tr][tc][2]) >> 1;
					}
				}
			}
			/* Missing: Interpolation Artifact Reduction
				repeat m times
					R = median_8(R-G) + G		// median without center pixel (correct?)
					B = median_8(B-G) + G
					G = 1/2(median_4(G-R) + median_4(G-B) + R + B)
				end
			*/
		}
	}
	free (buffer);
}
#undef TS
