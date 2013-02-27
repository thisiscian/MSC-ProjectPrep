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
#include "gui/Goptions.h"
#include "image.h"
#include "grab.h"
#include "main/sfb_iw.h"
#include "output.h"
#include "output_i.h"
#include "plugin_comm.h"

#define MODULE_TITLE		"iceWing"

#define YUV_IMAGE_TITLE		"YUV-Colorimage"
#define YUV_IMAGE_INHALT	(Y_BILD | U_BILD | YUV_V_BILD)
#define RGB_IMAGE_INHALT	(R_BILD | G_BILD | B_BILD)

#define STREAM_SYNC			1

#define KAMERA_UNDEF		((Kamera_t)-1)
#define GERAET_UNDEF		((Geraet_t)-1)

DACSentry_t *iw_dacs_entry = NULL;

static char *dacsName = NULL,
	*stream_status = NULL,
	*stream_images = NULL;

	/* Part of the image which should be given out */
static int image_x1 = 0, image_y1 = 0, image_x2 = 99999, image_y2 = 99999;

static BOOL out_do_output;

/*********************************************************************
  Free the image img.
  full==TRUE: Free the image data additionally.
*********************************************************************/
static void ndr_Bild_free (Bild_t *img, BOOL full)
{
	int p;
	if (img) {
		if (img->bild) {
			for (p = 0; p < img->anzahl; p++) {
				if (img->bild[p]) {
					if (full && img->bild[p][0]) free (img->bild[p][0]);
					free (img->bild[p]);
				}
			}
			free (img->bild);
		}
		free (img);
	}
}

/*********************************************************************
  Return a complete Bild_t structure without the image data. From
  'frame' only the time and the number of planes is used.
*********************************************************************/
static Bild_t *ndr_Bild_init (char *typ, const grabImageData *frame,
							  int new_w, int new_h)
{
	Bild_t *img = NULL;
	int p;

	if (frame->img.type != IW_8U)
		return NULL;

	if (!(img = malloc (sizeof(Bild_t))))
		return NULL;

	img->kopf.quelle	= MODULE_TITLE;
	img->kopf.typ		= typ;
	img->kopf.bewertung	= 0.0;
	img->kopf.sec		= frame->time.tv_sec;
	img->kopf.usec		= frame->time.tv_usec;
	img->kopf.sequenz_nr= frame->img_number;
	img->kopf.id		= -1;
	img->kopf.alter		= 0;
	img->kopf.stabilitaet = 0;
	img->kopf.typ_id	= SKK_Bild;
	img->kopf.n_basis	= 0;
	img->kopf.basis		= NULL;

	img->kamera	= KAMERA_UNDEF;
	img->geraet = GERAET_UNDEF;
	img->anzahl = frame->img.planes;
	if (frame->img.planes == 1)
		img->inhalt = SW_BILD;
	else if (frame->img.ctab == IW_RGB)
		img->inhalt = RGB_IMAGE_INHALT;
	else
		img->inhalt = YUV_IMAGE_INHALT;
	img->offset.x = 0;
	img->offset.y = 0;
	img->gesamt.x = new_w;
	img->gesamt.y = new_h;
	img->delta.x = new_w;
	img->delta.y = new_h;

	img->bild = NULL;

	/* Create line structure */
	if (!(img->bild = malloc (sizeof(char **) * img->anzahl))) {
		free (img);
		return NULL;
	}
	for (p = 0; p < img->anzahl; p++) {
		if (!(img->bild[p] = malloc(sizeof(unsigned char *) * img->delta.y))) {
			ndr_Bild_free (img, FALSE);
			return NULL;
		}
	}
	return img;
}

/*********************************************************************
  Convert (and downsample) frame to a SFB image of type Bild_t.
*********************************************************************/
static Bild_t *grab_to_ndr (char *typ, grabImageData *frame, int downsamp)
{
	int p, y, w = frame->img.width / downsamp, h = frame->img.height / downsamp;
	unsigned char *block;
	iwImage d;
	Bild_t *img = ndr_Bild_init (typ, frame, w, h);

	if (!img) return NULL;

	iw_img_init(&d);
	iw_img_downsample (&frame->img, &d, downsamp, downsamp);
	for (p = 0; p < img->anzahl; p++) {
		block = d.data[p];
		for (y = 0; y < img->delta.y; y++)
			img->bild[p][y] = block + y * img->delta.x;
	}
	iw_img_free (&d, IW_IMG_FREE_PLANEPTR);

	return img;
}

static void free_img (Bild_t* img)
{
	ndr_Bild_free (img, TRUE);
}

/*********************************************************************
  DACS function for fetching a grabbed image, selected by time or
  image number.
  in  : "[PLUG plugnum] (NUM imgnum|TIME sec usec|FTIME sec usec) down"
  out : Image requested with 'in'
*********************************************************************/
static char* output_getImgFunc (char *in, Bild_t **out, void *data)
{
	static char *errPara = "Format of input string must be "
		"\"['PLUG' plugnum] ('NUM' imgnum|'TIME' sec usec|'FTIME' sec usec) down\"";
	static char *errDown = "downsampling factor must be bigger than 0";
	static char *errImg = "no image with imgnum found";
	static char *errPlug = "no plugin with plugnum found";
	static char *errMem = "out of memory";
	grabImageData *img;
	int down, img_num = 0, plug_num, i;
	struct timeval time, *ptime = NULL;
	char *ret = NULL, plug_name[15];
	plugDefinition *plug;

	down = -99;
	plug_num = 1;

	while (in && isspace((int)*in)) in++;
	while (in && *in) {
		if (!strncasecmp("PLUG", in, 4)) {
			if (sscanf (in+4, "%d%n", &plug_num, &i) < 1)
				return errPara;
			in += 4+i;
		} else if (!strncasecmp("NUM", in, 3)) {
			/* Images based on image number */
			if (sscanf (in+3, "%d %d%n", &img_num, &down, &i) < 2)
				return errPara;
			in += 3+i;
		} else if (!strncasecmp("TIME", in, 4)) {
			/* Downsampled images based on time */
			long s, u;
			if (sscanf (in+4, "%ld %ld %d%n", &s, &u, &down, &i) < 3)
				return errPara;
			img_num = 1;
			time.tv_sec = s;
			time.tv_usec = u;
			ptime = &time;
			in += 4+i;
		} else if (!strncasecmp("FTIME", in, 5)) {
			/* Full images based on time */
			long s, u;
			if (sscanf (in+5, "%ld %ld %d%n", &s, &u, &down, &i) != 3)
				return errPara;
			img_num = -1;
			time.tv_sec = s;
			time.tv_usec = u;
			ptime = &time;
			in += 5+i;
		} else
			return errPara;
		while (isspace((int)*in)) in++;
	}
	if (down == -99) return errPara;
	if (down <= 0) return errDown;

	sprintf (plug_name, "GrabImage%d", plug_num);
	plug = plug_def_get_by_name (plug_name);
	if (!plug) return errPlug;

	img = grab_get_image_from_plug (plug, img_num, ptime);
	if (img) {
		if ((*out = grab_to_ndr (YUV_IMAGE_TITLE, img, down))
			== NULL)
			ret = errMem;
	} else
		ret = errImg;
	grab_release_image_from_plug (plug, img);

	return ret;
}

/*********************************************************************
  DACS function to set the region of the images which are given out
  on the stream.
  in : 4 Coordinates, referring to a not downsampled image
       format: "x1 y1 x2 y2"
  out: --- (not used)
*********************************************************************/
static char* output_setCropFunc (char *in, void *out, void *data)
{
	static char *errPara = "Format of input string must be \"x1 y1 x2 y2\"";
	int x1, y1, x2, y2;

	if (sscanf (in, "%d %d %d %d", &x1, &y1, &x2, &y2) != 4)
		return errPara;

	image_x1 = x1;
	image_y1 = y1;
	image_x2 = x2;
	image_y2 = y2;

	return NULL;
}

static BOOL cb_opts_load (void *data, char *buffer, int size)
{
	char **string = data, *pos;

	while (**string == '\n') (*string)++;
	pos = *string;
	while (*pos && *pos != '\n') pos++;

	if (pos != *string) {
		if (pos-(*string) >= size)
			pos = (*string)+size-1;
		if (pos-*string > 0)
			memcpy (buffer, *string, pos-*string);
		buffer[pos-*string] = '\0';
		*string = pos;
		return TRUE;
	} else
		return FALSE;
}

/*********************************************************************
  DACS function to remote-control the GUI
  in : a config-string in the form
	   '\"Tracking.Dist Init\" = 50\n\"GrabImage1.Interlace:\" = 0'"
  out: --- (not used)
*********************************************************************/
static char* output_controlFunc (char *in, void *out, void *data)
{
	opts_load (cb_opts_load, &in);
	return NULL;
}

typedef struct outSaveData {
	char *out;
	char *outpos;
	int len;
} outSaveData;
static BOOL cb_opts_save (void *data, char *string)
{
	outSaveData *dat = data;
	int len;

	if (!string) return TRUE;

	len = strlen (string);
	if (dat->outpos - dat->out + len+1 >= dat->len) {
		char *out = dat->out;
		dat->len += MAX(len+1,1000);
		dat->out = realloc (dat->out, dat->len);
		dat->outpos = dat->outpos - out + dat->out;
	}
	strcpy (dat->outpos, string);
	dat->outpos += len;

	return TRUE;
}

/*********************************************************************
  DACS function to get the GUI settings
  in : --- (not used)
  out: the complete GUI settings
*********************************************************************/
static char* output_getSettingsFunc (char *in, char **out, void *data)
{
	outSaveData dat;
	dat.out = dat.outpos = NULL;
	dat.len = 0;
	opts_save (cb_opts_save, &dat);
	*out = dat.out;
	return NULL;
}

/*********************************************************************
  Output a SYNC signal on stream 'stream'.
*********************************************************************/
void iw_output_sync (const char *stream)
{
	DACSstatus_t status;

	if (!out_do_output) return;

	if ((status = dacs_sync_stream (iw_dacs_entry, (char*)stream,
									STREAM_SYNC)) != D_OK)
		iw_warning ("Error %d syncing stream '%s'", status, stream);
}

/*********************************************************************
  Output 'data' on stream 'stream'.
*********************************************************************/
void iw_output_stream (const char *stream, const void *data)
{
	DACSstatus_t status;

	if (!out_do_output) return;

	if ((status = dacs_update_stream (iw_dacs_entry, (char*)stream, (char*)data)) != D_OK)
		iw_warning ("Error %d updating stream '%s'", status, stream);
}

/*********************************************************************
  Output img as Bild_t on stream 'stream' (!=NULL) or
  DACSNAME_images ('stream'==NULL).
*********************************************************************/
void iw_output_image (const grabImageData *img, const char *stream)
{
	static Bild_t *bild = NULL;
	DACSstatus_t status;
	int p, y, x, x1, x2, y1, y2;
	unsigned char *block;

	if (!stream) stream = stream_images;
	if (!out_do_output || !stream || !*stream) return;

	if (img->img.type != IW_8U) return;

	x1 = image_x1 / img->downw;
	x2 = image_x2 / img->downw;
	y1 = image_y1 / img->downh;
	y2 = image_y2 / img->downh;
	x1 = CLAMP (x1, 0, img->img.width-1);
	x2 = CLAMP (x2, 0, img->img.width-1);
	y1 = CLAMP (y1, 0, img->img.height-1);
	y2 = CLAMP (y2, 0, img->img.height-1);
	if (x1 > x2) {
		x = x1;
		x1 = x2;
		x2 = x;
	}
	if (y1 > y2) {
		y = y1;
		y1 = y2;
		y2 = y;
	}
	if (x1 != x2 && y1 != y2) {
		x = x2-x1+1;
		y = y2-y1+1;

		if (!bild || (bild->delta.x != x || bild->delta.y != y ||
					  bild->anzahl != img->img.planes)) {
			ndr_Bild_free (bild, FALSE);
			bild = ndr_Bild_init (YUV_IMAGE_TITLE, img, x, y);
			if (!bild) return;
		}

		for (p = 0; p < bild->anzahl; p++) {
			block = img->img.data[p] + y1 * img->img.width + x1;
			for (y = 0; y < bild->delta.y; y++)
				bild->bild[p][y] = block + y * img->img.width;
		}

		if ((status = dacs_update_stream (iw_dacs_entry, (char*)stream, bild))
			!= D_OK)
			iw_warning ("Error %d updating stream '%s'", status, stream);
		if ((status = dacs_sync_stream (iw_dacs_entry, (char*)stream, STREAM_SYNC))
			!= D_OK)
			iw_warning ("Error %d syncing stream '%s'", status, stream);
	}
}

/*********************************************************************
  Output regions as RegionHyp_t on stream 'stream' WITHOUT SYNC
  (data is entered in the Hypothesenkopf) and return the number of
  regions given out.
*********************************************************************/
int iw_output_regions (iwRegion *regions, int nregions,
					   struct timeval time, int img_num, const char *typ,
					   const char *stream)
{
	RegionHyp_t region_hyp = {
		{NULL, MODULE_TITLE,
		 0.0, 0, 0, -1, -1, -1, -1,
		 SKK_Region, 0, NULL},	/* kopf */
		KAMERA_UNDEF,			/* kamera */
		GERAET_UNDEF,			/* geraet */
		NULL					/* region */
	};
	char *dtyp, *data;
	DACSstatus_t status;
	int i, anzout = 0;
	static plugDataFunc *func = (plugDataFunc*)1;

	if (!out_do_output) return 0;

	if (func == (plugDataFunc*)1) {
		func = NULL;
		func = plug_function_get (IW_REG_DATA_IDENT, NULL);
	}
	region_hyp.kopf.alter = -1;
	region_hyp.kopf.sec = time.tv_sec;
	region_hyp.kopf.usec = time.tv_usec;
	region_hyp.kopf.sequenz_nr = img_num;

	for (i=0; i<nregions; i++) {
		if (regions[i].r.pixelanzahl > 0) {
			if (regions[i].data && func)
				data = ((iwRegDataFunc)func->func) (func->plug, &regions[i],
													IW_REG_DATA_CONVERT);
			else
				data = NULL;
			dtyp = malloc (sizeof(char)*strlen(typ) + 2 +
						   (data ? strlen(data):0));
			strcpy (dtyp, typ);
			if (data) {
				strcat (dtyp, " ");
				strcat (dtyp, data);
			}
			region_hyp.kopf.typ = dtyp;
			region_hyp.region = &regions[i].r;
			region_hyp.kopf.id = regions[i].id;
			region_hyp.kopf.alter = regions[i].alter;
			region_hyp.kopf.bewertung = regions[i].judgement;
			region_hyp.kopf.stabilitaet = regions[i].judge_kalman*100;

			if ((status = dacs_update_stream (iw_dacs_entry, (char*)stream, &region_hyp))
				!= D_OK)
				iw_warning ("Error %d updating stream '%s'", status, stream);
			if (data) free (data);
			if (dtyp) free (dtyp);
			anzout++;
		}
	}
	return anzout;
}

/*********************************************************************
  Distribute regions as RegionHyp_t on stream 'stream' with time
  'time' and sequenz_nr 'img_num'.
*********************************************************************/
void iw_output_hypos (iwRegion *regions, int nregions,
					  struct timeval time, int img_num, const char *stream)
{
	DACSstatus_t status;

	if (!out_do_output || !stream || !*stream) return;

	if (regions)
		iw_output_regions (regions, nregions, time, img_num,
						   IW_GHYP_TITLE, stream);

	if ((status = dacs_sync_stream (iw_dacs_entry, (char*)stream, STREAM_SYNC))
		!= D_OK)
		iw_warning ("Error %d syncing stream '%s'", status, stream);
}

/*********************************************************************
  Output status message msg on stream DACSNAME_status.
*********************************************************************/
void iw_output_status (const char *msg)
{
	DACSstatus_t status;

	if (!out_do_output || !stream_status || !*stream_status) return;

	if ((status = dacs_update_stream (iw_dacs_entry, stream_status, (char*)msg))
		!= D_OK)
		iw_warning ("Error %d updating stream '%s'", status, stream_status);
	if ((status = dacs_sync_stream (iw_dacs_entry, stream_status, STREAM_SYNC))
		!= D_OK)
		iw_warning ("Error %d syncing stream '%s'", status, stream_status);
}

/*********************************************************************
  Register with DACS. The other output_register_...() functions call
  this by themselves.
*********************************************************************/
void iw_output_register (void)
{
	if (!iw_dacs_entry)
		if (!(iw_dacs_entry = dacs_register(dacsName)))
			iw_error ("Can't register under DACS as '%s'", dacsName);
}

/*********************************************************************
  Register a DACS stream as DACSNAME'suffix' and return a pointer
  to this newly allocated name.
*********************************************************************/
char *iw_output_register_stream (const char *suffix, NDRfunction_t *fkt)
{
	char *name = malloc(strlen(dacsName)+strlen(suffix)+1);
	DACSstatus_t status;

	iw_output_register();

	sprintf (name, "%s%s", dacsName, suffix);
	if ((status = dacs_register_stream (iw_dacs_entry, name, fkt)) != D_OK)
		iw_error ("Unable to register output stream '%s', error %d", name, status);

	return name;
}

/*********************************************************************
  Register a DACS function as DACSNAME'suffix'.
*********************************************************************/
void iw_output_register_function_with_data (const char *suffix, DACSfunction_t* fkt,
											NDRfunction_t *ndr_in, NDRfunction_t *ndr_out,
											void (*out_kill_fkt)(void*), void *data)
{
	static BOOL serving = FALSE;
	char name[DACS_MAXNAMELEN];
	DACSstatus_t status;

	iw_output_register();

	sprintf (name, "%s%s", dacsName, suffix);
	if ((status = dacs_register_function (iw_dacs_entry, name, fkt,
										  ndr_in, ndr_out, out_kill_fkt, data))
		!= D_OK)
		iw_error ("Unable to register function '%s', error %d", name, status);

	if (!serving) {
		serving = TRUE;
		if ((status = dacs_serv_functions_async (iw_dacs_entry)) != D_OK)
			iw_error ("Unable to serv functions, error %d", status);
	}
}
void iw_output_register_function (const char *suffix, DACSfunction_t* fkt,
								  NDRfunction_t *ndr_in, NDRfunction_t *ndr_out,
								  void (*out_kill_fkt)(void*))
{
	iw_output_register_function_with_data (suffix, fkt, ndr_in, ndr_out,
										   out_kill_fkt, NULL);
}

/*********************************************************************
  PRIVATE: Set address of a flag which indicates if anything should
  be given out.
*********************************************************************/
BOOL *iw_output_get_output (void)
{
	return &out_do_output;
}

/*********************************************************************
  PRIVATE: Set name for DACS registration.
*********************************************************************/
void iw_output_set_name (char *name)
{
	dacsName = strdup (name);
}

/*********************************************************************
  PRIVATE: Close any DACS connections.
*********************************************************************/
void iw_output_cleanup (void)
{
	if (iw_dacs_entry) {
		dacs_unregister (iw_dacs_entry);
		iw_dacs_entry = NULL;
	}
}

/*********************************************************************
  PRIVATE: Init streams and function server for output of regions
  and images.
*********************************************************************/
void iw_output_init (int output)
{
	out_do_output = TRUE;

	if (output & IW_OUTPUT_FUNCTION) {
		iw_output_register_function ("_control", (DACSfunction_t*)output_controlFunc,
									 (NDRfunction_t*)ndr_string, NULL, NULL);

		iw_output_register_function ("_getSettings", (DACSfunction_t*)output_getSettingsFunc,
									 NULL, (NDRfunction_t*)ndr_string, free);

		iw_output_register_function ("_getImg", (DACSfunction_t*)output_getImgFunc,
									 (NDRfunction_t*)ndr_string, (NDRfunction_t*)ndr_Bild,
									 (void(*)(void*))free_img);
	}
	if (output & IW_OUTPUT_STREAM) {
		stream_images = iw_output_register_stream ("_images", (NDRfunction_t*)ndr_Bild);

		iw_output_register_function ("_setCrop", (DACSfunction_t*)output_setCropFunc,
									 (NDRfunction_t*)ndr_string, NULL, NULL);
	}
	if (output & IW_OUTPUT_STATUS)
		stream_status = iw_output_register_stream ("_status", (NDRfunction_t*)ndr_string);
}
