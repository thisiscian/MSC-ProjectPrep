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

#ifndef iw_gsimple_image_H
#define iw_gsimple_image_H

typedef BOOL (*iwGsimpleFillFunc) (iwImage *s, uchar *d, int offset,
								   iwColor col, void *data);
typedef void (*iwGsimpleShrinkFunc) (int *ireg, int label, int offset);

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Fill in d (size: width x height) the region
    - which has in s a color which is not more differnt than (int)data
	  if (fkt==NULL) to (ycol,ucol,vcol) and
	- which starts at (xs , ys), (xs+-3 , ys), or (xs , ys+-3)
*********************************************************************/
void iw_gsimple_fill (iwImage *s, uchar *d, int xs, int ys,
					  iwColor col, iwGsimpleFillFunc fkt, void *data);

/*********************************************************************
  Call for every border pixel of the region with label 'label' the
  function fkt (img, label, pixel-offset) and shrink the region 'cnt'
  times by 1 pixel.
  img : region labeled image of size width x height.
  reg_x, reg_y, reg_w, reg_h: Part of the image which should be
							  searched for the region.
*********************************************************************/
void iw_gsimple_shrink (int *img, int width, int height, int reg_x, int reg_y,
						int reg_w, int reg_h, int label, int cnt,
						iwGsimpleShrinkFunc fkt);

#ifdef __cplusplus
}
#endif

#endif /* iw_gsimple_image_H */
