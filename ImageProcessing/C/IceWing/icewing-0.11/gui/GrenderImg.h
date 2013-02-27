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

#ifndef iw_GrenderImg_H
#define iw_GrenderImg_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Initialize the user interface for image rendering.
*********************************************************************/
void prev_render_image_opts (prevBuffer *b);

/*********************************************************************
  Free the image 'data'.
*********************************************************************/
void prev_render_image_free (void *data);

/*********************************************************************
  Copy the image 'data' and return it.
*********************************************************************/
void* prev_render_image_copy (const void *data);

/*********************************************************************
  Return information (a new string) about dImg at position x,y.
*********************************************************************/
char* prev_render_image_info (prevBuffer *b, const prevDataImage *dImg,
							  int disp_mode, int x, int y, int radius);

/*********************************************************************
  Display image dImg in buffer b.
*********************************************************************/
void prev_render_image (prevBuffer *b, const prevDataImage *dImg, int disp_mode);

/*********************************************************************
  Render the image dimg according the data in save.
*********************************************************************/
iwImgStatus prev_render_image_vector (prevBuffer *b, prevVectorSave *save,
									  const prevDataImage *dimg, int disp_mode);

#ifdef __cplusplus
}
#endif

#endif /* iw_GrenderImg_H */
