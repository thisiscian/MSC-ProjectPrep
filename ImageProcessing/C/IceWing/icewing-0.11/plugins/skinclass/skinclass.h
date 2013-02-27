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

#ifndef iw_skinclass_H
#define iw_skinclass_H

#include "sclas_image.h"

typedef BOOL (*sclasClassifyFunc) (iwImage *img, uchar *d, int pix_cnt, int size, int thresh);
typedef void (*sclasUpdateFunc) (iwImage *img, int *ireg, iwRegion *regions, int numregs);

#define SCLAS_IDENT_CLASSIFY	"classify"
#define SCLAS_IDENT_UPDATE		"update"
#define SCLAS_IDENT_REGION		"sclasRegion"

#define OPT_SKIN_MOTION			"Skin Motion"
#define OPT_SKIN_CLASS			"Skin Segmentation"
#define OPT_DIFF_DO				OPT_SKIN_MOTION".Difference Image"
#define OPT_DIFF_THRESH			OPT_SKIN_MOTION".Diff. Threshold"
#define OPT_COMB_MODE			OPT_SKIN_CLASS".Comb. Mode:"
#define OPT_CLASS_MED_SIZE		OPT_SKIN_CLASS".Median Size"
#define OPT_CLASS_THRESH		OPT_SKIN_CLASS".Threshold"

#endif /* iw_skinclass_H */
