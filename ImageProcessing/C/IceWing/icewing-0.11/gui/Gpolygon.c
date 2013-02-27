/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*********************************************************************

Copyright 1987, 1998  The Open Group

All Rights Reserved.

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

						All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*********************************************************************/

#include "config.h"
#include <stdlib.h>
#include "Gpolygon.h"

/*
 *	   scanfill.h
 *
 *	   Written by Brian Kelleher; Jan 1985
 *
 *	   This file contains a few macros to help track
 *	   the edge of a filled object.	 The object is assumed
 *	   to be filled in scanline order, and thus the
 *	   algorithm used is an extension of Bresenham's line
 *	   drawing algorithm which assumes that y is always the
 *	   major axis.
 *	   Since these pieces of code are the same for any filled shape,
 *	   it is more convenient to gather the library in one
 *	   place, but since these pieces of code are also in
 *	   the inner loops of output primitives, procedure call
 *	   overhead is out of the question.
 *	   See the author for a derivation if needed.
 */

/*
 *	In scan converting polygons, we want to choose those pixels
 *	which are inside the polygon.  Thus, we add .5 to the starting
 *	x coordinate for both left and right edges.	 Now we choose the
 *	first pixel which is inside the pgon for the left edge and the
 *	first pixel which is outside the pgon for the right edge.
 *	Draw the left pixel, but not the right.
 *
 *	How to add .5 to the starting x coordinate:
 *		If the edge is moving to the right, then subtract dy from the
 *	error term from the general form of the algorithm.
 *		If the edge is moving to the left, then add dy to the error term.
 *
 *	The reason for the difference between edges moving to the left
 *	and edges moving to the right is simple:  If an edge is moving
 *	to the right, then we want the algorithm to flip immediately.
 *	If it is moving to the left, then we don't want it to flip until
 *	we traverse an entire pixel.
 */
#define BRESINITPGON(dy, x1, x2, xStart, d, m, m1, incr1, incr2) { \
	int dx;		 /* Local storage */ \
\
	/* \
	 *	If the edge is horizontal, then it is ignored \
	 *	and assumed not to be processed.  Otherwise, do this stuff. \
	 */ \
	if ((dy) != 0) { \
		xStart = (x1); \
		dx = (x2) - xStart; \
		if (dx < 0) { \
			m = dx / (dy); \
			m1 = m - 1; \
			incr1 = -2 * dx + 2 * (dy) * m1; \
			incr2 = -2 * dx + 2 * (dy) * m; \
			d = 2 * m * (dy) - 2 * dx - 2 * (dy); \
		} else { \
			m = dx / (dy); \
			m1 = m + 1; \
			incr1 = 2 * dx - 2 * (dy) * m1; \
			incr2 = 2 * dx - 2 * (dy) * m; \
			d = -2 * m * (dy) + 2 * dx; \
		} \
	} \
}

#define BRESINCRPGON(d, minval, m, m1, incr1, incr2) { \
	if (m1 > 0) { \
		if (d > 0) { \
			minval += m1; \
			d += incr1; \
		} \
		else { \
			minval += m; \
			d += incr2; \
		} \
	} else {\
		if (d >= 0) { \
			minval += m1; \
			d += incr1; \
		} \
		else { \
			minval += m; \
			d += incr2; \
		} \
	} \
}

/*
 *	   This structure contains all of the information needed
 *	   to run the bresenham algorithm.
 *	   The variables may be hardcoded into the declarations
 *	   instead of using this structure to make use of
 *	   register declarations.
 */
typedef struct {
	int minor;			/* Minor axis */
	int d;				/* Decision variable */
	int m, m1;			/* Slope and slope+1 */
	int incr1, incr2;	/* Error increments */
} BRESINFO;

/*
 *	   fill.h
 *
 *	   Created by Brian Kelleher; Oct 1985
 *
 *	   Include file for filled polygon routines.
 *
 *	   These are the data structures needed to scan
 *	   convert regions.	 Two different scan conversion
 *	   methods are available -- the even-odd method, and
 *	   the winding number method (REMOVED (Frank)).
 *	   The even-odd rule states that a point is inside
 *	   the polygon if a ray drawn from that point in any
 *	   direction will pass through an odd number of
 *	   path segments.
 *	   By the winding number rule, a point is decided
 *	   to be inside the polygon if a ray drawn from that
 *	   point in any direction passes through a different
 *	   number of clockwise and counter-clockwise path
 *	   segments.
 *
 *	   These data structures are adapted somewhat from
 *	   the algorithm in (Foley/Van Dam) for scan converting
 *	   polygons.
 *	   The basic algorithm is to start at the top (smallest y)
 *	   of the polygon, stepping down to the bottom of
 *	   the polygon by incrementing the y coordinate.  We
 *	   keep a list of edges which the current scanline crosses,
 *	   sorted by x.	 This list is called the Active Edge Table (AET).
 *	   As we change the y-coordinate, we update each entry in
 *	   the active edge table to reflect the edges new xcoord.
 *	   This list must be sorted at each scanline in case
 *	   two edges intersect.
 *	   We also keep a data structure known as the Edge Table (ET),
 *	   which keeps track of all the edges which the current
 *	   scanline has not yet reached.  The ET is basically a
 *	   list of ScanLineList structures containing a list of
 *	   edges which are entered at a given scanline.	 There is one
 *	   ScanLineList per scanline at which an edge is entered.
 *	   When we enter a new edge, we move it from the ET to the AET.
 *
 *	   From the AET, we can implement the even-odd rule as in
 *	   (Foley/Van Dam).
 *	   The winding number rule is a little trickier.  We also
 *	   keep the EdgeTableEntries in the AET linked by the
 *	   nextWETE (winding EdgeTableEntry) link.	This allows
 *	   the edges to be linked just as before for updating
 *	   purposes, but only uses the edges linked by the nextWETE
 *	   link as edges representing spans of the polygon to
 *	   drawn (as with the even-odd rule).
 */

typedef struct _EdgeTableEntry {
	int ymax;			   /* ycoord at which we exit this edge. */
	BRESINFO bres;		   /* Bresenham info to run the edge */
	BOOL first, last;
	int horizf, horizl;
	struct _EdgeTableEntry *next;		 /* Next in the list */
	struct _EdgeTableEntry *back;		 /* For insertion sort */
} EdgeTableEntry;


typedef struct _ScanLineList {
	int scanline;				/* The scanline represented */
	EdgeTableEntry *edgelist;	/* Header node */
	struct _ScanLineList *next;	/* Next in the list	*/
} ScanLineList;


typedef struct {
	 int ymax;				   /* ymax for the polygon */
	 int ymin;				   /* ymin for the polygon */
	 ScanLineList scanlines;   /* Header node */
} EdgeTable;


/*
 * Here is a struct to help with storage allocation
 * so we can allocate a big chunk at a time, and then take
 * pieces from this heap when we need to.
 */
#define SLLSPERBLOCK 25

typedef struct _ScanLineListBlock {
	 ScanLineList SLLs[SLLSPERBLOCK];
	 struct _ScanLineListBlock *next;
} ScanLineListBlock;

/*
 * number of points to buffer before sending them off
 * to scanlines() :	 Must be an even number
 */
#define NUMPTSTOBUFFER 200

/*
 *
 *	   A few macros for the inner loops of the fill code where
 *	   performance considerations don't allow a procedure call.
 *
 */

/*
 *	   Evaluate the given edge at the given scanline.
 *	   If the edge has expired, then we leave it and fix up
 *	   the active edge table; otherwise, we increment the
 *	   x value to be ready for the next scanline.
 *	   The even-odd rule is in effect.
 */
#define EVALUATEEDGEEVENODD_NEXT(pAET, pPrevAET) { \
		pPrevAET->next = pAET->next;	   /* Leaving this edge */ \
		pAET = pPrevAET->next; \
		if (pAET) \
				pAET->back = pPrevAET; \
}
#define EVALUATEEDGEEVENODD(pAET, pPrevAET, y) { \
	if (pAET->ymax < y) {				   /* Leaving this edge */ \
		EVALUATEEDGEEVENODD_NEXT(pAET, pPrevAET); \
	} else { \
		pAET->first = FALSE; \
		pAET->last = (pAET->ymax == y); \
		BRESINCRPGON(pAET->bres.d, pAET->bres.minor, pAET->bres.m, \
					 pAET->bres.m1, pAET->bres.incr1, pAET->bres.incr2); \
		pPrevAET = pAET; \
		pAET = pAET->next; \
	} \
}

/* invers==FALSE: Draw in img n lines with starting points pts and length pwidth
   invers==TRUE : Fill in img the regions between the lines. If last==TRUE fill after
                  the last point to the end of the image as well. */
static void FillSpans (int width, int height, prevPolyFunc fkt, void *data,
					   int n, prevDataPoint *pts, int *pwidth,
					   gboolean invers, gboolean last, prevDataPoint *lastPt)
{
	int fullX1, fullX2, fullY1;

	while (n--) {
		fullX1 = pts->x;
		fullY1 = pts->y;
		fullX2 = fullX1 + (int) *pwidth;
		pts++;
		pwidth++;

		if (fullY1 < 0 || fullY1 >= height) continue;
		if (fullX1 < 0) fullX1 = 0;
		if (fullX2 >= width) fullX2 = width-1;
		if (fullX1 > fullX2) continue;

		if (invers) {
			int cnt = fullX1-lastPt->x + (fullY1-lastPt->y)*width - 1;
			if (cnt > 0)
				fkt (lastPt->x + 1, lastPt->y, cnt, data);
			lastPt->x = fullX2;
			lastPt->y = fullY1;
		} else {
			fkt (fullX1, fullY1, fullX2-fullX1+1, data);
		}
	}
	if (last && invers) {
		int cnt = width*height - (lastPt->y*width+lastPt->x+1);
		if (cnt > 0)
			fkt (lastPt->x+1, lastPt->y, cnt, data);
		lastPt->x = -1;
		lastPt->y = 0;
	}
}

/*
 *	   fillUtils.c
 *
 *	   Written by Brian Kelleher;  Oct. 1985
 *
 *	   This module contains all of the utility functions
 *	   needed to scan convert a polygon.
 *
 */

/*
 *	   Clean up our act.
 */
static void miFreeStorage (register ScanLineListBlock *pSLLBlock)
{
	register ScanLineListBlock	 *tmpSLLBlock;

	while (pSLLBlock) {
		tmpSLLBlock = pSLLBlock->next;
		free (pSLLBlock);
		pSLLBlock = tmpSLLBlock;
	}
}

/*
 *	   InsertEdgeInET
 *
 *	   Insert the given edge into the edge table.
 *	   First we must find the correct bucket in the
 *	   Edge table, then find the right slot in the
 *	   bucket. Finally, we can insert it.
 *
 */
static gboolean miInsertEdgeInET (EdgeTable *ET, EdgeTableEntry *ETE, int scanline,
								  ScanLineListBlock **SLLBlock, int *iSLLBlock)
{
	register EdgeTableEntry *start, *prev;
	register ScanLineList *pSLL, *pPrevSLL;
	ScanLineListBlock *tmpSLLBlock;

	/*
	 * find the right bucket to put the edge into
	 */
	pPrevSLL = &ET->scanlines;
	pSLL = pPrevSLL->next;
	while (pSLL && (pSLL->scanline < scanline)) {
		pPrevSLL = pSLL;
		pSLL = pSLL->next;
	}

	/*
	 * reassign pSLL (pointer to ScanLineList) if necessary
	 */
	if ((!pSLL) || (pSLL->scanline > scanline)) {
		if (*iSLLBlock > SLLSPERBLOCK-1) {
			tmpSLLBlock =
				(ScanLineListBlock *)malloc(sizeof(ScanLineListBlock));
			if (!tmpSLLBlock)
				return FALSE;
			(*SLLBlock)->next = tmpSLLBlock;
			tmpSLLBlock->next = (ScanLineListBlock *)NULL;
			*SLLBlock = tmpSLLBlock;
			*iSLLBlock = 0;
		}
		pSLL = &((*SLLBlock)->SLLs[(*iSLLBlock)++]);

		pSLL->next = pPrevSLL->next;
		pSLL->edgelist = (EdgeTableEntry *)NULL;
		pPrevSLL->next = pSLL;
	}
	pSLL->scanline = scanline;

	/*
	 * now insert the edge in the right bucket
	 */
	prev = (EdgeTableEntry *)NULL;
	start = pSLL->edgelist;
	while (start && (start->bres.minor < ETE->bres.minor)) {
		prev = start;
		start = start->next;
	}
	ETE->next = start;

	if (prev)
		prev->next = ETE;
	else
		pSLL->edgelist = ETE;
	return TRUE;
}

/*
 * Get longest horizontal line to the right.
 */
static int miGetHoriz (const prevDataPoint *pts, int count, int start)
{
	int horiz = 0, i;

	i = start;
	while (1) {
		i = (i-1+count)%count;
		if (i == start || pts[i].y != pts[start].y) break;
		if (pts[i].x - pts[start].x > horiz)
			horiz = pts[i].x - pts[start].x;
	}

	i = start;
	while (1) {
		i = (i+1)%count;
		if (i == start || pts[i].y != pts[start].y) break;
		if (pts[i].x - pts[start].x > horiz)
			horiz = pts[i].x - pts[start].x;
	}

	return horiz;
}

/*
 *	   CreateEdgeTable
 *
 *	   This routine creates the edge table for
 *	   scan converting polygons.
 *	   The Edge Table (ET) looks like:
 *
 *	  EdgeTable
 *	   --------
 *	  |	 ymax  |		ScanLineLists
 *	  |scanline|-->------------>-------------->...
 *	   --------	  |scanline|   |scanline|
 *				  |edgelist|   |edgelist|
 *				  ---------	   ---------
 *					  |				|
 *					  |				|
 *					  V				V
 *				list of ETEs   list of ETEs
 *
 *	   where ETE is an EdgeTableEntry data structure,
 *	   and there is one ScanLineList per scanline at
 *	   which an edge is initially entered.
 *
 */
static gboolean miCreateETandAET (register int count, register const prevDataPoint *pts,
								  EdgeTable *ET, EdgeTableEntry *AET,
								  register EdgeTableEntry *pETEs, ScanLineListBlock *pSLLBlock)
{
	int iSLLBlock = 0;
	int top, bottom, dy, i, prev;

	if (count < 2)	return TRUE;

	/*
	 *	initialize the Active Edge Table
	 */
	AET->next = (EdgeTableEntry *)NULL;
	AET->back = (EdgeTableEntry *)NULL;
	AET->bres.minor = G_MININT;

	/*
	 *	initialize the Edge Table.
	 */
	ET->scanlines.next = (ScanLineList *)NULL;
	ET->ymax = G_MININT;
	ET->ymin = G_MAXINT;
	pSLLBlock->next = (ScanLineListBlock *)NULL;

	/*
	 *	For each vertex in the array of points.
	 *	In this loop we are dealing with two vertices at
	 *	a time -- these make up one edge of the polygon.
	 */
	prev = count-1;
	for (i=0; i<count; i++) {
		/*
		 * Don't add horizontal edges to the Edge table.
		 */
		if (pts[prev].y != pts[i].y) {
			/* Find out which point is above and which is below. */
			if (pts[prev].y > pts[i].y) {
				bottom = prev, top = i;
			} else {
				bottom = i, top = prev;
			}
			pETEs->horizf = miGetHoriz (pts, count, top);
			pETEs->horizl = miGetHoriz (pts, count, bottom);

			pETEs->ymax = pts[bottom].y-1;	/* -1 so we don't get last scanline */
			pETEs->first = TRUE;
			pETEs->last = FALSE;

			/* Initialize integer edge algorithm */
			dy = pts[bottom].y - pts[top].y;
			BRESINITPGON (dy, pts[top].x, pts[bottom].x, pETEs->bres.minor, pETEs->bres.d,
						  pETEs->bres.m, pETEs->bres.m1, pETEs->bres.incr1, pETEs->bres.incr2);

			if (!miInsertEdgeInET(ET, pETEs, pts[top].y, &pSLLBlock, &iSLLBlock)) {
				miFreeStorage(pSLLBlock->next);
				return FALSE;
			}

			if (ET->ymax < pts[i].y) ET->ymax = pts[i].y;
			if (ET->ymin > pts[i].y) ET->ymin = pts[i].y;
			pETEs++;
		}

		prev = i;
	}
	return TRUE;
}

/*
 *	   loadAET
 *
 *	   This routine moves EdgeTableEntries from the
 *	   EdgeTable into the Active Edge Table,
 *	   leaving them sorted by smaller x coordinate.
 */
static void miloadAET (register EdgeTableEntry *AET, register EdgeTableEntry *ETEs)
{
	register EdgeTableEntry *pPrevAET;
	register EdgeTableEntry *tmp;

	pPrevAET = AET;
	AET = AET->next;
	while (ETEs) {
		while (AET && (AET->bres.minor < ETEs->bres.minor)) {
			pPrevAET = AET;
			AET = AET->next;
		}
		tmp = ETEs->next;
		ETEs->next = AET;
		if (AET)
			AET->back = ETEs;
		ETEs->back = pPrevAET;
		pPrevAET->next = ETEs;
		pPrevAET = ETEs;

		ETEs = tmp;
	}
}

/*
 *	   InsertionSort
 *
 *	   Just a simple insertion sort using
 *	   pointers and back pointers to sort the Active
 *	   Edge Table.
 */
static int miInsertionSort (register EdgeTableEntry *AET)
{
	register EdgeTableEntry *pETEchase;
	register EdgeTableEntry *pETEinsert;
	register EdgeTableEntry *pETEchaseBackTMP;
	register int changed = 0;

	AET = AET->next;
	while (AET) {
		pETEinsert = AET;
		pETEchase = AET;
		while (pETEchase->back->bres.minor > AET->bres.minor)
			pETEchase = pETEchase->back;

		AET = AET->next;
		if (pETEchase != pETEinsert) {
			pETEchaseBackTMP = pETEchase->back;
			pETEinsert->back->next = AET;
			if (AET)
				AET->back = pETEinsert->back;
			pETEinsert->next = pETEchase;
			pETEchase->back->next = pETEinsert;
			pETEchase->back = pETEinsert;
			pETEinsert->back = pETEchaseBackTMP;
			changed = 1;
		}
	}
	return(changed);
}

/*********************************************************************
  Fill the polygon defined by the points pts (or everything except
  the polygon if invers==TRUE).
  width, height: clipping range for the polygon
  npts         : number of points
  fkt          : is called with data as argument
*********************************************************************/
gboolean prev_drawFPoly_raw (int width, int height, int npts, const prevDataPoint *pts,
							 gboolean invers, prevPolyFunc fkt, void *data)
{
	register EdgeTableEntry *pAET;	/* The Active Edge Table   */
	register int y;					/* The current scanline	   */
	register int nPtsOut = 0;		/* Number of pts in buffer */
	register ScanLineList *pSLL;	/* Current ScanLineList	   */
	register prevDataPoint *ptsOut;		/* ptr to output buffers   */
	int *pwidth, end, e;
	prevDataPoint FirstPoint[NUMPTSTOBUFFER]; /* The output buffers */
	prevDataPoint lastPt = {-1,0};
	int FirstWidth[NUMPTSTOBUFFER];
	EdgeTableEntry *pPrevAET;		/* Previous AET entry	   */
	EdgeTable ET;					/* Edge Table header node  */
	EdgeTableEntry AET;				/* Active ET header node   */
	EdgeTableEntry *pETEs;			/* Edge Table Entries buff */
	ScanLineListBlock SLLBlock;		/* Header for ScanLineList */

	if (npts < 3) return (TRUE);

	if (!(pETEs = (EdgeTableEntry *) malloc (sizeof(EdgeTableEntry) * npts)))
		return (FALSE);

	ptsOut = FirstPoint;
	pwidth = FirstWidth;

	if (!miCreateETandAET(npts, pts, &ET, &AET, pETEs, &SLLBlock)) {
		free (pETEs);
		return(FALSE);
	}
	pSLL = ET.scanlines.next;

	/*
	 *	for each scanline
	 */
	for (y = ET.ymin; y <= ET.ymax; y++) {
		/*
		 *	Add a new edge to the active edge table when we
		 *	get to the next edge.
		 */
		if (pSLL && y == pSLL->scanline) {
			miloadAET(&AET, pSLL->edgelist);
			pSLL = pSLL->next;
		}
		pPrevAET = &AET;
		pAET = AET.next;

		/*
		 *	for each active edge
		 */
		while (pAET) {
			ptsOut->x = pAET->bres.minor;
			ptsOut->y = y;

			*pwidth = end = pAET->bres.minor;
			do {
				if (pAET->last) {
					e = pAET->bres.minor + pAET->horizl;
					if (e > end) end = e;
					EVALUATEEDGEEVENODD_NEXT(pAET, pPrevAET);
				} else {
					do {
						EVALUATEEDGEEVENODD(pAET, pPrevAET, y);
						e = pAET->bres.minor;
						if (pAET->first)
							e += pAET->horizf;
						else if (pAET->last)
							e += pAET->horizl;
						if (e > end) end = e;
					} while (pAET && pAET->last);
					EVALUATEEDGEEVENODD(pAET, pPrevAET, y);
				}
			} while (pAET && pAET->bres.minor <= end+1);
			*pwidth = end - *pwidth;

			ptsOut++;
			pwidth++;
			nPtsOut++;

			/*
			 *	send out the buffer when its full
			 */
			if (nPtsOut == NUMPTSTOBUFFER) {
				FillSpans (width, height, fkt, data,
						   nPtsOut, FirstPoint, FirstWidth,
						   invers, FALSE, &lastPt);
				ptsOut = FirstPoint;
				pwidth = FirstWidth;
				nPtsOut = 0;
			}
		}
		miInsertionSort (&AET);
	}

	/*
	 *	   Get any spans that we missed by buffering
	 */
	FillSpans (width, height, fkt, data, nPtsOut, FirstPoint, FirstWidth,
			   invers, TRUE, &lastPt);
	free (pETEs);
	miFreeStorage (SLLBlock.next);
	return (TRUE);
}

/*
 *	   Find the index of the point with the smallest y.
 */
static int getPolyYBounds (const prevDataPoint *pts, int n, int *by, int *ty)
{
	register const prevDataPoint *ptMin;
	int ymin, ymax;
	const prevDataPoint *ptsStart = pts;

	ptMin = pts;
	ymin = ymax = (pts++)->y;

	while (--n > 0) {
		if (pts->y < ymin) {
			ptMin = pts;
			ymin = pts->y;
		}
		if (pts->y > ymax)
			ymax = pts->y;
		pts++;
	}

	*by = ymin;
	*ty = ymax;
	return (ptMin-ptsStart);
}

/*
 *	   convexpoly.c
 *
 *	   Written by Brian Kelleher; Dec. 1985.
 *
 *	   Fill a convex polygon.  If the given polygon
 *	   is not convex, then the result is undefined.
 *	   The algorithm is to order the edges from smallest
 *	   y to largest by partitioning the array into a left
 *	   edge list and a right edge list.	 The algorithm used
 *	   to traverse each edge is an extension of Bresenham's
 *	   line algorithm with y as the major axis.
 *	   For a derivation of the algorithm, see the author of
 *	   this code.
 */
gboolean prev_drawConvexPoly_raw (int width, int height, int npts, const prevDataPoint *pts,
								  gboolean invers, prevPolyFunc fkt, void *data)
{
	register int xl = 0, xr = 0; /* x vals of left and right edges */
	register int dl = 0, dr = 0; /* Decision variables			   */
	register int ml = 0, m1l = 0;/* Left edge slope and slope+1	   */
	int mr = 0, m1r = 0;		 /* Right edge slope and slope+1   */
	int incr1l = 0, incr2l = 0;	 /* Left edge error increments	   */
	int incr1r = 0, incr2r = 0;	 /* Right edge error increments	   */
	int dy;						 /* Delta y						   */
	int y;						 /* Current scanline			   */
	int left, right;			 /* Indices to first endpoints	   */
	int i;						 /* Loop counter				   */
	int nextleft, nextright;	 /* Indices to second endpoints	   */
	prevDataPoint *ptsOut, *FirstPoint; /* Output buffer		   */
	prevDataPoint lastPt = {-1,0};
	int *pwidth, *FirstWidth;	 /* Output buffer					*/
	int imin;					 /* Index of smallest vertex (in y) */
	int ymin;					 /* y-extents of polygon			*/
	int ymax;

	/*
	 *	find leftx, bottomy, rightx, topy, and the index
	 *	of bottomy. Also translate the points.
	 */
	imin = getPolyYBounds (pts, npts, &ymin, &ymax);

	dy = ymax - ymin + 1;
	if (npts < 3 || dy < 0) return (TRUE);

	ptsOut = FirstPoint = (prevDataPoint *)malloc(sizeof(prevDataPoint)*dy);
	pwidth = FirstWidth = (int *)malloc(sizeof(int) * dy);
	if (!FirstPoint || !FirstWidth) {
		if (FirstWidth) free (FirstWidth);
		if (FirstPoint) free (FirstPoint);
		return (FALSE);
	}

	nextleft = nextright = imin;
	y = pts[nextleft].y;

	/*
	 *	Loop through all edges of the polygon
	 */
	do {
		/* Add a left edge if we need to */
		if (pts[nextleft].y == y) {
			left = nextleft;

			/* Find the next edge, considering the end
			   conditions of the array. */
			nextleft++;
			if (nextleft >= npts)
				nextleft = 0;

			/* Now compute all of the random information
			   needed to run the iterative algorithm. */
			dy = pts[nextleft].y-pts[left].y;
			if (dy != 0) {
				BRESINITPGON(dy, pts[left].x, pts[nextleft].x,
							 xl, dl, ml, m1l, incr1l, incr2l);
			} else {
				xl =  pts[nextleft].x;
			}
		}

		/* Add a right edge if we need to */
		if (pts[nextright].y == y) {
			right = nextright;

			/* Find the next edge, considering the end
			   conditions of the array. */
			nextright--;
			if (nextright < 0)
				nextright = npts-1;

			/* Now compute all of the random information
			   needed to run the iterative algorithm. */
			dy = pts[nextright].y-pts[right].y;
			if (dy != 0) {
				BRESINITPGON(dy, pts[right].x, pts[nextright].x,
							 xr, dr, mr, m1r, incr1r, incr2r);
			} else {
				xr =  pts[nextright].x;
			}
		}

		/* Generate scans to fill while we still have
		   a right edge as well as a left edge. */
		i = MIN(pts[nextleft].y, pts[nextright].y) - y;
		/* In case we're called with non-convex polygon */
		if (i < 0) {
			free (FirstWidth);
			free (FirstPoint);
			return (TRUE);
		}
		while (i-- > 0) {
			ptsOut->y = y;

			/* Reverse the edges if necessary */
			if (xl < xr) {
				*(pwidth++) = xr - xl;
				(ptsOut++)->x = xl;
			} else {
				*(pwidth++) = xl - xr;
				(ptsOut++)->x = xr;
			}
			y++;

			/* Increment down the edges */
			BRESINCRPGON(dl, xl, ml, m1l, incr1l, incr2l);
			BRESINCRPGON(dr, xr, mr, m1r, incr1r, incr2r);
		}
	} while (y != ymax);

	/* Last line */
	ptsOut->y = y;
	if (xl < xr) {
		*(pwidth++) = xr - xl;
		(ptsOut++)->x = xl;
	} else {
		*(pwidth++) = xl - xr;
		(ptsOut++)->x = xr;
	}

	/* Finally, fill the spans */
	FillSpans (width, height, fkt, data, ptsOut-FirstPoint, FirstPoint, FirstWidth,
			   invers, TRUE, &lastPt);

	free (FirstWidth);
	free (FirstPoint);
	return (TRUE);
}
