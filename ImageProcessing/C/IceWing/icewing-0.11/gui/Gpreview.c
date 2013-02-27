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
#include <gtk/gtk.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "tools/tools.h"
#include "Gtools.h"
#include "Ggui_i.h"
#include "Gpreferences.h"
#include "Gdialog.h"
#include "Gsession.h"
#include "Goptions_i.h"
#include "Grender.h"
#include "GrenderImg.h"
#include "Ginfo.h"
#include "Gpreview.h"
#include "Gpreview_i.h"

#define PREV_WIDTH_MIN	100
#define PREV_HEIGHT_MIN	40
#define PREV_COLS		2

#define OPT_SAVE_RDATA	"Remember Data"

#define VEC_STATUS_DATA		(IW_IMG_STATUS_MAX+1)
#define VEC_STATUS_REMEMBER	(IW_IMG_STATUS_MAX+2)

typedef enum {
	ZOOM_IN,
	ZOOM_OUT,
	ZOOM_FIT,
	ZOOM_ONE
} prevZoom;

typedef enum {
	PAN_LEFT,
	PAN_RIGHT,
	PAN_TOP,
	PAN_BOTTOM
} prevPan;

typedef struct {
	prevEvent sigset;				/* Events this window should receive */
	prevSignalFunc cback;			/* The function which should be called */
	prevButtonFunc cbackB;			/* The old compatibility function */
	void *data;						/* Data passed to the callback */
} prevCBack;

typedef struct {
	char *title;
	GtkCTreeNode *parent;			/* Parent node in the ctree */
	int children;					/* Number of children */
} prevParent;

struct _prevSettings {				/* Data for the settings window */
	GtkWidget *window;
	GtkWidget *vbox;
	char *title;
	int page_num;
};

/* Data needed for repainting of the type 'type' with mode disp_mode */
typedef struct {
	prevType type;
	void *data;
	int disp_mode;
	prevGC gc;
} prevRData;

/* List of all windows opend with prev_new_window() */
static GtkWidget *prev_ctree = NULL;
static GHashTable *prev_parents = NULL;
static prevBuffer *buffer = NULL;

static struct {
	int prev_width, prev_height;	/* Default width and height of a preview window */
	int save_cnt;					/* Number of saved images (for constructing a file name) */
	prevType prev_draw;				/* Render type for the preview manipulation */
	prevPrefs *prefs;				/* Settings which can be manipulated via the GUI */
} prev_Mdata;

/* Mutex for saving, especially to guard prevBuffer.save_avifile */
static GStaticMutex save_mutex = G_STATIC_MUTEX_INIT;

/*********************************************************************
  Must be called on access of a prevBuffer->buffer.
  Attention: gdk-/X11-mutex must not be locked before locking this
             mutex.
*********************************************************************/
static GStaticMutex buffer_mutex = G_STATIC_MUTEX_INIT;
void prev_buffer_lock (void) { g_static_mutex_lock (&buffer_mutex); }
void prev_buffer_unlock (void) { g_static_mutex_unlock (&buffer_mutex); }

typedef struct {
	renderDbOptsFunc opts_fkt;
	renderDbFreeFunc free_fkt;
	renderDbCopyFunc copy_fkt;
	renderDbRenderFunc render_fkt;
	renderDbInfoFunc info_fkt;
	renderDbVectorFunc vector_fkt;
} prevRenderDb;
static GHashTable *renderDb = NULL;
static pthread_mutex_t renderDb_mutex = PTHREAD_MUTEX_INITIALIZER;

/*********************************************************************
  Return biggest key of renderDb in type.
  If 'type' == PREV_NEW use the first unused value and return it.
*********************************************************************/
static void cb_prev_renderdb_register (gpointer key, gpointer value, gpointer type)
{
	if (GPOINTER_TO_INT(key) >= *(prevType*)type)
		*(prevType*)type = GPOINTER_TO_INT(key)+1;
}

/*********************************************************************
  Register a new render type 'type'.
  If 'type' == PREV_NEW use the first unused value and return it.
*********************************************************************/
prevType prev_renderdb_register (prevType type, renderDbOptsFunc o_fkt,
								 renderDbFreeFunc f_fkt, renderDbCopyFunc c_fkt,
								 renderDbRenderFunc r_fkt)
{
	prevRenderDb *rdb = g_malloc (sizeof(prevRenderDb));

	rdb->opts_fkt = o_fkt;
	rdb->free_fkt = f_fkt;
	rdb->copy_fkt = c_fkt;
	rdb->render_fkt = r_fkt;
	rdb->info_fkt = NULL;
	rdb->vector_fkt = NULL;

	pthread_mutex_lock (&renderDb_mutex);
	if (!renderDb) renderDb = g_hash_table_new (NULL, NULL);
	if (type == PREV_NEW) {
		type++;
		g_hash_table_foreach (renderDb, cb_prev_renderdb_register, &type);
	}
	g_hash_table_insert (renderDb, GINT_TO_POINTER(type), rdb);
	pthread_mutex_unlock (&renderDb_mutex);

	return type;
}

/*********************************************************************
  Register a function which is called for the "Info Window".
*********************************************************************/
void prev_renderdb_register_info (prevType type, renderDbInfoFunc i_fkt)
{
	prevRenderDb *rdb;
	iw_assert (renderDb!=NULL, "No render types registered");

	pthread_mutex_lock (&renderDb_mutex);
	rdb = g_hash_table_lookup (renderDb, GINT_TO_POINTER(type));
	if (rdb)
		rdb->info_fkt = i_fkt;
	pthread_mutex_unlock (&renderDb_mutex);
}

/*********************************************************************
  Register a function which is called for the "Info Window".
*********************************************************************/
void prev_renderdb_register_vector (prevType type, renderDbVectorFunc v_fkt)
{
	prevRenderDb *rdb;
	iw_assert (renderDb!=NULL, "No render types registered");

	pthread_mutex_lock (&renderDb_mutex);
	rdb = g_hash_table_lookup (renderDb, GINT_TO_POINTER(type));
	if (rdb)
		rdb->vector_fkt = v_fkt;
	pthread_mutex_unlock (&renderDb_mutex);
}

/*********************************************************************
  Return the function associated with type.
*********************************************************************/
#define prev_renderdb_get(selfkt) \
	prevRenderDb *rdb;												\
																	\
	pthread_mutex_lock (&renderDb_mutex);							\
	if (!renderDb) {												\
		pthread_mutex_unlock (&renderDb_mutex);						\
		return NULL;												\
	}																\
	rdb = g_hash_table_lookup (renderDb, GINT_TO_POINTER(type));	\
	pthread_mutex_unlock (&renderDb_mutex);							\
	if (rdb) return rdb->selfkt##_fkt;								\
																	\
	return NULL;

static renderDbOptsFunc prev_renderdb_get_opts (prevType type)
{ prev_renderdb_get (opts) }

static renderDbFreeFunc prev_renderdb_get_free (prevType type)
{ prev_renderdb_get (free) }

static renderDbCopyFunc prev_renderdb_get_copy (prevType type)
{ prev_renderdb_get (copy) }

static renderDbInfoFunc prev_renderdb_get_info (prevType type)
{ prev_renderdb_get (info) }

static renderDbVectorFunc prev_renderdb_get_vector (prevType type)
{ prev_renderdb_get (vector) }

renderDbRenderFunc prev_renderdb_get_render (prevType type)
{ prev_renderdb_get (render) }

/*********************************************************************
  Return the deafult width/height for new preview windows.
*********************************************************************/
int prev_get_default_width (void)
{
	return prev_Mdata.prev_width;
}
int prev_get_default_height (void)
{
	return prev_Mdata.prev_height;
}

/*********************************************************************
  Return zoom factor to get something of size
  b->gc.width x b->gc.height into buffer b.
*********************************************************************/
float prev_get_zoom (prevBuffer *b)
{
	if (b->zoom) {
		return b->zoom;
	} else {
		int down = (b->gc.width + b->width-1) / b->width;
		int down2 = (b->gc.height + b->height-1) / b->height;
		if (down2 > down) down = down2;
		if (down)
			return 1.0/down;
		else
			return 1.0;
	}
}
float prev_get_zoom_full (prevBuffer *b, int width, int height)
{
	if (b->zoom) {
		return b->zoom;
	} else {
		int down = (width + b->width-1) / b->width;
		int down2 = (height + b->height-1) / b->height;
		if (down2 > down) down = down2;

		return 1.0/down;
	}
}

static void prev_set_wmname (GtkWindow *window, char *orig)
{
	static char class[PATH_MAX] = "";
	char name[256];
	int i;

	if (!class[0]) {
		gui_convert_prg_rc (class, NULL, FALSE);
		class[1] = toupper ((int)class[1]);
	}
	for (i=0; i<255 && *orig; ) {
		if (isalnum((int)*orig)) {
			name[i] = *orig;
			i++;
		} else if (i>0 && name[i-1] != '_') {
			name[i] = '_';
			i++;
		}
		orig++;
	}
	name[i--] = '\0';
	while (i>0 && name[i] == '_')
		name[i--] = '\0';
	gtk_window_set_wmclass (window, name, &class[1]);
}

static void prev_set_title (prevBuffer *b)
{
	char name[200], *oldname = NULL;
	float zoom;
	int zin = 1, zout = 1;

	zoom = prev_get_zoom (b);
	if (zoom < 1)
		zout = 1.0/zoom+0.5;
	else
		zin = zoom+0.5;
	g_snprintf (name, 200, "%s (%d saved [%d:%d] x %d y %d)",
				b->title, b->save_cnt, zin, zout, b->x, b->y);

	gtk_object_get (GTK_OBJECT(b->window), "GtkWindow::title", &oldname, NULL);

	if (!oldname || strcmp (name, oldname))
		gtk_window_set_title (GTK_WINDOW(b->window), name);

	if (oldname)
		g_free (oldname);
}

static void avi_close__nl (prevBuffer *b)
{
	if (b->save_avifile) {
		iw_movie_close (b->save_avifile);
		b->save_avifile = NULL;
	}
}
/*********************************************************************
  Lock saveing-mutex and close AVI-file.
*********************************************************************/
static void avi_close (prevBuffer *b)
{
	g_static_mutex_lock (&save_mutex);
	avi_close__nl (b);
	g_static_mutex_unlock (&save_mutex);
}

/*********************************************************************
  Lock saveing-mutex and close all AVI-files of all buffer.
*********************************************************************/
void prev_close_avi_all (void)
{
	prevBuffer *b;
	g_static_mutex_lock (&save_mutex);
	for (b = buffer; b; b = b->next)
		avi_close__nl (b);
	g_static_mutex_unlock (&save_mutex);
}

static iwImgStatus prev_save_vector (prevBuffer *b, const iwImage *img, iwImgFormat format,
									 char *fname)
{
	iwImgStatus status = IW_IMG_STATUS_OK;
	prevVectorSave save;

	if (format != IW_IMG_FORMAT_SVG)
		return IW_IMG_STATUS_FORMAT;

	if (!img && (!b->rdata || b->rdata->len < 1)) {
		if (b->save_rdata)
			return VEC_STATUS_DATA;
		else
			return VEC_STATUS_REMEMBER;
	}

	if (!(save.file = fopen (fname, "wb")))
		return IW_IMG_STATUS_OPEN;
	save.fname = fname;
	save.zoom = 1;
	save.rdata = NULL;
	save.format = format;

	if (fprintf (save.file,
				 "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
				 "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n"
				 " \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
				 "<!-- Created with %s V%s -->\n"
				 "<svg width=\"%d\" height=\"%d\"\n"
				 "     xmlns:sodipodi=\"http://inkscape.sourceforge.net/DTD/sodipodi-0.dtd\"\n"
				 "     xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\"\n"
				 "     xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n",
				 GUI_PRG_NAME, GUI_VERSION, b->gc.width, b->gc.height) <= 0) {
		fclose (save.file);
		return IW_IMG_STATUS_WRITE;
	}

	if (img) {
		prevDataImage i;
		iwImage img_b = *img;

		/* Prevent any color transformations during saving */
		if (img_b.ctab <= IW_COLFORMAT_MAX)
			img_b.ctab = IW_RGB;
		i.i = &img_b;
		i.x = i.y = 0;
		status = prev_render_image_vector (b, &save, &i, 0);
	} else {
		int i;
		gboolean first = TRUE;
		iwImgStatus vstatus;
		prevGC orig_gc = b->gc;

		for (i=0; i<b->rdata->len; i++) {
			prevRData *rd = &g_array_index (b->rdata, prevRData, i);
			renderDbVectorFunc v_fkt = prev_renderdb_get_vector (rd->type);
			if (v_fkt) {
				b->gc = rd->gc;
				if (first) {
					fprintf (save.file, "  <sodipodi:namedview pagecolor=\"%s\""
							 " inkscape:pageopacity=\"1.0\"/>\n",
							 prev_colString(IW_RGB, b->gc.bg_r, b->gc.bg_g, b->gc.bg_b));
					first = FALSE;
				}
				vstatus = v_fkt (b, &save, rd->data, rd->disp_mode);
				/* Ignore it, if some renderer do not support the current format */
				if (vstatus != IW_IMG_STATUS_FORMAT)
					status = vstatus;
				if (status != IW_IMG_STATUS_OK)
					break;
			}
		}
		b->gc = orig_gc;
	}
	if (status == IW_IMG_STATUS_OK)
		if (fprintf (save.file, "</svg>\n") <= 0)
			status = IW_IMG_STATUS_WRITE;

	fclose (save.file);
	return status;
}

/*********************************************************************
  If b->save!=SAVE_NONE:
      img!=NULL: Save image planes img.
      img==NULL: Save image b->buffer (size: b->width x b->height).
  Attention: prev_buffer_(un)lock is called.
*********************************************************************/
static void prev_chk_save_intern (prevBuffer *b, const iwImage *img, BOOL leave_x11)
{
	char fname[PATH_MAX];
	iwImgStatus status = IW_IMG_STATUS_OK;
	iwImgFormat format;
	iwImage img_b;
	guchar *planeptr[1];
	iwImgFileData *data;
	guchar *buffer = NULL;
	prevSave save = b->save;

	struct timeval time;
	int iargs[3];
	static char *sargs[3] = {NULL, NULL, NULL};

	if (save == SAVE_NONE) return;

	if (leave_x11)
		gdk_threads_leave();
	prev_buffer_lock();
	if (save < SAVE_SEQ) b->save = SAVE_NONE;

	if (!sargs[0]) {
		static char hostname[256];
		struct passwd *pass = getpwuid (getuid());
		if (pass && pass->pw_name)
			sargs[0] = pass->pw_name;
		else
			sargs[0] = "<unknown>";
		if (gethostname (hostname, 256) == 0)
			sargs[1] = hostname;
		else
			sargs[1] = "<unknown>";
	}

	if (prev_Mdata.prefs->reset_cnt) {
		prev_Mdata.prefs->reset_cnt = FALSE;
		prev_Mdata.save_cnt = 0;
	}

	gettimeofday (&time, NULL);
	iargs[0] = prev_Mdata.save_cnt;
	iargs[1] = time.tv_sec;
	iargs[2] = time.tv_usec/1000;
	{
		char *s, *d;
		s = b->title;
		d = sargs[2] = malloc (strlen(s)+1);
		while (*s) {
			if (*s == '\t' || *s == ' ' || *s == '_' || *s == '|' || *s == '.')
				*d++ = '_';
			else if (*s == '-' || isalnum((int)(*s)))
				*d++ = *s;
			s++;
		}
		*d = *s;
	}
	gui_sprintf (fname, PATH_MAX, prev_Mdata.prefs->fname,
				 "dTt", iargs, "bhw", sargs);
	free (sargs[2]);

	g_static_mutex_lock (&save_mutex);
	if (!b->window) goto cleanup;

	format = iw_img_save_get_format (prev_Mdata.prefs->format, fname);
	if (format < IW_IMG_FORMAT_MOVIE || format > IW_IMG_FORMAT_MOVIE_MAX)
		avi_close__nl (b);

	if (format >= IW_IMG_FORMAT_VECTOR) {
		status = prev_save_vector (b, img, format, fname);
	} else {
		if (!img) {
			iw_img_init (&img_b);
			img_b.data = planeptr;
			img_b.planes = b->gray ? 1:3;
			img_b.type = IW_8U;
			img_b.ctab = b->gray ? IW_BW:IW_RGB;
			if (prev_Mdata.prefs->save_full) {
				img_b.width = b->width;
				img_b.height = b->height;
				img_b.rowstride = b->gray ? img_b.width:img_b.width*3;
				img_b.data[0] = b->buffer;
			} else {
				float zoom = prev_get_zoom (b);

				img_b.width = b->width/zoom;
				if (img_b.width+b->x > b->gc.width) img_b.width = b->gc.width - b->x;
				img_b.width *= zoom;
				if (img_b.width < 1) img_b.width = 1;

				img_b.height = b->height/zoom;
				if (img_b.height+b->y > b->gc.height) img_b.height = b->gc.height - b->y;
				img_b.height *= zoom;
				if (img_b.height < 1) img_b.height = 1;

				img_b.rowstride = b->gray ? img_b.width:img_b.width*3;
				if (img_b.width == b->width) {
					img_b.data[0] = b->buffer;
				} else {
					int y, line;
					buffer = gui_get_buffer (img_b.rowstride*img_b.height);
					line = b->gray ? b->width:b->width*3;
					for (y=0; y<img_b.height; y++)
						memcpy (buffer+y*img_b.rowstride,
								b->buffer+y*line, img_b.rowstride);
					img_b.data[0] = buffer;
				}
			}
			img = &img_b;
		}

		data = iw_img_data_create();
		iw_img_data_set_movie (data, &b->save_avifile, prev_Mdata.prefs->framerate);
		iw_img_data_set_quality (data, prev_Mdata.prefs->quality);
		status = iw_img_save (img, format, fname, data);
		iw_img_data_free (data);
	}

	if (status == IW_IMG_STATUS_OK) {
		prev_Mdata.save_cnt++;
		b->save_cnt++;
		prev_set_title (b);
	}

 cleanup:
	g_static_mutex_unlock (&save_mutex);
	if (buffer) gui_release_buffer (buffer);
	prev_buffer_unlock();
	if (leave_x11)
		gdk_threads_enter();

	if (save < SAVE_SEQ) {
		if (status != IW_IMG_STATUS_OK) {
			char *err;
			switch ((int)status) {
				case IW_IMG_STATUS_OPEN:
					err = "Unable to open file '%s' for saving the image!";
					break;
				case IW_IMG_STATUS_MEM:
					err = "Unable to save image as '%s'!\nNot enough memory.";
					break;
				case IW_IMG_STATUS_FORMAT:
					err = "Unable to save image as '%s'!\nUnsupported file format.";
					break;
				case VEC_STATUS_DATA:
					err = "No rendered data available to save in '%s'!";
					break;
				case VEC_STATUS_REMEMBER:
					err = "Unable to save image as '%s'!\n\n"
						"'Remember Data' must be selected to save in a vector format.";
					break;
				default:
					err = "Unable to save image as '%s'!";
			}
			gui_message_show ("Error", err, fname);
		} else if (prev_Mdata.prefs->show_message) {
			gui_message_show ("Confirm", "Image saved as '%s'.", fname);
		}
	}
}
void prev_chk_save (prevBuffer *b, const iwImage *img)
{
	prev_chk_save_intern (b, img, FALSE);
}

static void cb_prev_wake_main_loop (gpointer data, gint source, GdkInputCondition condition)
{
	char ch;

	if (condition == GDK_INPUT_READ)
		read (source, &ch, 1);
}

/*********************************************************************
  HACK: If XSync() is called events get transfered to the local queue.
  If this happens outside of the glib main thread, the main loop
  may miss these events. -> Wake up the main poll().
*********************************************************************/
static void prev_wake_main_loop (void)
{
	static int wake_up_pipe[2] = {-1, -1};

	if (wake_up_pipe[0] < 0) {
		if (pipe (wake_up_pipe) < 0)
			iw_error ("Cannot create wake-up pipe: %s",
					  g_strerror (errno));
		gdk_input_add (wake_up_pipe[0], GDK_INPUT_READ,
					   cb_prev_wake_main_loop, NULL);
	}
	write (wake_up_pipe[1], "A", 1);
}

/*********************************************************************
  Contrast autostretch of src (size: width x height x bpp).
  Ignore lower and upper limit % pixels.
*********************************************************************/
static void prev_draw_cstretch (guchar *src, int width, int height, int bpp, int limit)
{
	int x, min, max, hist[3][IW_COLCNT];
	guchar *s, lumamap[IW_COLCNT];
	double lscale;
	int pixels = width*height;

	limit = pixels*bpp*CLAMP (limit, 0, 100) / 100;

	memset (hist, 0, bpp*IW_COLCNT*sizeof(int));
	s = src;
	if (bpp == 3) {
		for (x = pixels; x>0; x--) {
			hist[0][*s++]++;
			hist[1][*s++]++;
			hist[2][*s++]++;
		}
		min = 0;
		x = hist[0][min] + hist[1][min] + hist[2][min];
		while (min < IW_COLMAX && x < limit) {
			x += hist[0][min] + hist[1][min] + hist[2][min];
			min++;
		}
		while (min < IW_COLMAX && !hist[0][min] && !hist[1][min] && !hist[2][min])
			min++;
		max = IW_COLMAX;
		x = hist[0][max] + hist[1][max] + hist[2][max];
		while (max > 0 && x < limit) {
			x += hist[0][max] + hist[1][max] + hist[2][max];
			max--;
		}
		while (max > 0 && !hist[0][max] && !hist[1][max] && !hist[2][max])
			max--;
	} else {
		for (x = pixels; x>0; x--)
			hist[0][*s++]++;
		min = 0;
		x = hist[0][min];
		while (min < IW_COLMAX && x < limit) {
			min++;
			x += hist[0][min];
		}
		while (min < IW_COLMAX && !hist[0][min])
			min++;
		max = IW_COLMAX;
		x = hist[0][max];
		while (max > 0 && x < limit) {
			max--;
			x += hist[0][max];
		}
		while (max > 0 && !hist[0][max])
			max--;
	}

	if (min >= max) {
		min = (max+min)/2;
		max = min+1;
	}

	lscale = (double)IW_COLMAX / (max-min);
	for (x = 0; x < IW_COLCNT; x++) {
		long l = gui_lrint ((x-min) * lscale);
		lumamap[x] = (guchar) CLAMP (l, 0, IW_COLMAX);
	}
	for (x = pixels*bpp; x>0; x--) {
		*src = lumamap[*src];
		src++;
	}
}

/*********************************************************************
  Histogram equalize of src (size: width x height x bpp).
*********************************************************************/
static void prev_draw_histeq (guchar *src, int width, int height, int bpp)
{
	int pixels = width * height, pixsum;
	int b, x, max, hist[3][IW_COLCNT];
	guchar *s, lumamap[3][IW_COLCNT];
	double lscale;

	memset (hist, 0, bpp*IW_COLCNT*sizeof(int));
	s = src;
	if (bpp == 3) {
		for (x = width*height; x>0; x--) {
			hist[0][*s++]++;
			hist[1][*s++]++;
			hist[2][*s++]++;
		}
	} else
		for (x = width*height; x>0; x--) hist[0][*s++]++;

	for (b=0; b<bpp; b++) {
		max = 0;
		pixsum = 0;
		for (x = 0; x < IW_COLCNT; x++) {
			if (hist[b][x] > 0) max = x;

			lumamap[b][x] = pixsum * IW_COLMAX / pixels;
			pixsum += hist[b][x];
		}

		/* Normalise so that the brightest pixels are set to maxval */
		lscale = (double)IW_COLMAX / (lumamap[b][max]>0 ? lumamap[b][max] : IW_COLMAX);
		for (x = 0; x < IW_COLCNT; x++) {
			long l = gui_lrint (lumamap[b][x] * lscale);
			lumamap[b][x] = (guchar) MIN (IW_COLMAX, l);
		}
	}
	if (bpp == 3) {
		for (x = pixels; x>0; x--) {
			*src = lumamap[0][*src];
			src++;
			*src = lumamap[1][*src];
			src++;
			*src = lumamap[2][*src];
			src++;
		}
	} else
		for (x = pixels; x>0; x--) {
			*src = lumamap[0][*src];
			src++;
		}
}

/*********************************************************************
  Randomize luminance of src (size: width x height x bpp).
*********************************************************************/
static void prev_draw_random (guchar *src, int width, int height, int bpp)
{
	guchar lumamap[IW_COLCNT];
	int x;

	lumamap[0] = 0;
	for (x = 1; x < IW_COLCNT; x++)
		lumamap[x] = rand()%IW_COLCNT;

	for (x = bpp*width*height; x>0; x--) {
		*src = lumamap[*src];
		src++;
	}
}

#define DRAW_NORMAL		0
#define DRAW_CSTRETCH	1
#define DRAW_CSTRETCH5	2
#define DRAW_CSTRETCH10	3
#define DRAW_HISTEQ		4
#define DRAW_RANDOM		5
static void prev_draw_opts (prevBuffer *b)
{
	static char *drawOpts[] = {"Normal","CStretch","CStretch5","CStretch10",
							   "HistEQ","Random",NULL};
	int *draw = (int*)prev_opts_store (b, prev_Mdata.prev_draw,
									   GINT_TO_POINTER(DRAW_NORMAL), FALSE);

	opts_radio_create ((long)b, "Preview", "Color manipulation mode",
					   drawOpts, draw);
}
static void prev_draw_buffer__nl (prevBuffer *b, gboolean do_color)
{
	if (b->window) {
		int *draw;
		gboolean check = !GUI_IS_GUI_THREAD();

		if (do_color && (draw = (int*)prev_opts_get (b, prev_Mdata.prev_draw)) && *draw) {
			if (*draw == DRAW_CSTRETCH)
				prev_draw_cstretch (b->buffer, b->width, b->height,
									b->gray ? 1:3, 0);
			else if (*draw == DRAW_CSTRETCH5)
				prev_draw_cstretch (b->buffer, b->width, b->height,
									b->gray ? 1:3, 5);
			else if (*draw == DRAW_CSTRETCH10)
				prev_draw_cstretch (b->buffer, b->width, b->height,
									b->gray ? 1:3, 10);
			else if (*draw == DRAW_HISTEQ)
				prev_draw_histeq (b->buffer, b->width, b->height,
								  b->gray ? 1:3);
			else if (*draw == DRAW_RANDOM)
				prev_draw_random (b->buffer, b->width, b->height,
								  b->gray ? 1:3);
		}

		check = check && XEventsQueued(GDK_WINDOW_XDISPLAY(b->window->window),
									   QueuedAlready) == 0;
		if (b->gray)
			gdk_draw_gray_image (b->drawing->window, b->g_gc, (b->draw_width-b->width)/2,
								 (b->draw_height-b->height)/2, b->width, b->height,
								 GDK_RGB_DITHER_NORMAL,b->buffer,b->width);
		else
			gdk_draw_rgb_image (b->drawing->window, b->g_gc, (b->draw_width-b->width)/2,
								(b->draw_height-b->height)/2, b->width, b->height,
								GDK_RGB_DITHER_NORMAL,b->buffer,b->width * 3);
		ginfo_render (b);
		prev_set_title (b);
		gdk_flush();

		/* HACK
		   If XSync() is called events get transfered to the local queue.
		   If this happens outside of the glib main thread, the main loop
		   may miss these events. -> Wake up the main poll(). */
		if (check && XEventsQueued(GDK_WINDOW_XDISPLAY(b->window->window),
								   QueuedAlready) > 0)
			prev_wake_main_loop();
	}
}

/*********************************************************************
  If b->window is open, display b->buffer.
*********************************************************************/
void prev_draw_buffer (prevBuffer *b)
{
	if (b->window) {
		gui_threads_enter();
		prev_draw_buffer__nl (b, TRUE);
		gui_threads_leave();

		if (b->save != SAVE_NONE) {
			if (b->save == SAVE_SEQ || !b->save_done)
				prev_chk_save (b, NULL);
			else
				b->save_done = FALSE;
		}
	}
}

typedef struct {
	void *data;
	gboolean free_data;
} prevOpts;
static pthread_mutex_t opts_mutex = PTHREAD_MUTEX_INITIALIZER;

static gboolean prev_opts_check (prevBuffer *b, prevType type)
{
	return b->opts &&
		g_hash_table_lookup (b->opts, GINT_TO_POINTER(type));
}
static gboolean cb_prev_opts_free (gpointer key, gpointer value, gpointer user_data)
{
	prevOpts *dat = value;

	if (dat->free_data) free (dat->data);
	g_free (dat);
	return TRUE;
}
static void prev_opts_free (prevBuffer *b)
{
	pthread_mutex_lock (&opts_mutex);
	if (b->opts) {
		g_hash_table_foreach_remove (b->opts, cb_prev_opts_free, NULL);
		g_hash_table_destroy (b->opts);
		b->opts = NULL;
	}
	pthread_mutex_unlock (&opts_mutex);
}
/*********************************************************************
  Store a value associated to type and return its address.
  free_data==TRUE: If the window b is freed, data is freed by calling
				   free().
  Used in renderDbOptsFunc() to store the value of the menu entry.
*********************************************************************/
void** prev_opts_store (prevBuffer *b, prevType type, void *data,
						gboolean free_data)
{
	prevOpts *dat = g_malloc (sizeof(prevOpts));

	dat->data = data;
	dat->free_data = free_data;

	pthread_mutex_lock (&opts_mutex);
	/* Check if type is already included in opts for b */
	iw_assert (prev_opts_check(b, type)==FALSE,
			   "Type %d already used for buffer %s", type, b->title);

	if (!b->opts) b->opts = g_hash_table_new (NULL, NULL);
	g_hash_table_insert (b->opts, GINT_TO_POINTER(type), dat);
	pthread_mutex_unlock (&opts_mutex);

	return &dat->data;
}
/*********************************************************************
  Get the value associated to type stored with prev_opts_store().
*********************************************************************/
void** prev_opts_get (prevBuffer *b, prevType type)
{
	prevOpts *dat;
	void **data = NULL;

	if (!b->opts)
		prev_opts_append (b, type, -1);

	pthread_mutex_lock (&opts_mutex);
	if (b->opts) {
		dat = g_hash_table_lookup (b->opts, GINT_TO_POINTER(type));

		if (!dat) {
			pthread_mutex_unlock (&opts_mutex);
			prev_opts_append (b, type, -1);
			pthread_mutex_lock (&opts_mutex);

			dat = g_hash_table_lookup (b->opts, GINT_TO_POINTER(type));
		}
		if (dat)
			data = &dat->data;
	}
	pthread_mutex_unlock (&opts_mutex);

	return data;
}
/*********************************************************************
  Append option widgets to the window specific menu according to
  the given types. Last argument must be '-1'.
*********************************************************************/
void prev_opts_append (prevBuffer *b, prevType type, ...)
{
	va_list var_args;
	gboolean check;

	va_start (var_args, type);
	while (type != -1) {
		pthread_mutex_lock (&opts_mutex);
		check = prev_opts_check (b, type);
		pthread_mutex_unlock (&opts_mutex);

		if (!check) {
			renderDbOptsFunc o_fkt = prev_renderdb_get_opts (type);
			if (o_fkt) o_fkt (b);
		}
		type = va_arg (var_args, prevType);
	}
	va_end (var_args);
}

/*********************************************************************
  If widget is given, reallocate b->buffer (after a size change).
  Redraw the buffer b with the data stored in b->rdata and show it on
  the screen.
  BOTH buffer-mutex and gdk-mutex are locked.
*********************************************************************/
static void prev_redraw (prevBuffer *b, GtkWidget *widget)
{
	prev_buffer_lock();
	gdk_threads_enter();

	if (widget) {
		b->width = b->draw_width;
		b->height = b->draw_height;
		if (b->buffer) g_free (b->buffer);
		b->buffer = g_malloc0 (b->width*b->height*(b->gray ? 1:3));

		/* Set window after allocating the buffer, as it is used as an
		   indication for the possibility to draw inside the buffer */
		b->window = gtk_widget_get_toplevel(widget);
	}

	if (b->save_rdata && b->rdata && b->window) {
		gboolean first = TRUE;
		int i;
		renderDbRenderFunc r_fkt;

		for (i=0; i<b->rdata->len; i++) {
			prevRData *rd = &g_array_index (b->rdata, prevRData, i);

			r_fkt = prev_renderdb_get_render (rd->type);
			if (r_fkt) {
				b->gc = rd->gc;
				if (first) {
					prev_drawBackground (b);
					first = FALSE;
				}
				r_fkt (b, rd->data, rd->disp_mode);
			}
		}
	}
	prev_buffer_unlock();

	if (b->save_rdata && b->rdata && b->window)
		prev_draw_buffer__nl (b, TRUE);
	gdk_threads_leave();
}

/*********************************************************************
  PRIVATE: Call the renderdb info-funktion for all data stored with
  prev_rdata_append(). Return a newly allocated string of all strings
  returned from the funktions (separated by '\n').
*********************************************************************/
char* prev_rdata_get_info (prevBuffer *b, int x, int y, int radius)
{
	int i, s_pos;
	prevGC orig_gc;
	char **strings, *string = NULL;

	prev_buffer_lock();
	if (!b->rdata || b->rdata->len < 1) {
		prev_buffer_unlock();
		return NULL;
	}

	strings = g_malloc (sizeof(char*) * (b->rdata->len+1));
	s_pos = 0;

	orig_gc = b->gc;
	for (i=b->rdata->len-1; i>=0; i--) {
		prevRData *rd = &g_array_index (b->rdata, prevRData, i);
		renderDbInfoFunc i_fkt = prev_renderdb_get_info (rd->type);
		if (i_fkt) {
			b->gc = rd->gc;
			strings[s_pos] = i_fkt (b, rd->data, rd->disp_mode, x, y, radius);
			if (strings[s_pos])
				s_pos++;
		}
	}
	strings[s_pos] = NULL;
	b->gc = orig_gc;

	if (s_pos) {
		string = g_strjoinv ("\n", strings);
		for (i=0; strings[i]; i++)
			g_free (strings[i]);
		g_free (strings);
	}

	prev_buffer_unlock();

	return string;
}

/*********************************************************************
  PRIVATE: Free the data stored with prev_rdata_append(). Should be
  called if the buffer is cleared (RENDER_CLEAR specified).
  full: Free also the maintenance structures.
*********************************************************************/
void prev_rdata_free (prevBuffer *b, gboolean full)
{
	int i;

	if (!b->rdata) return;

	for (i=0; i<b->rdata->len; i++) {
		prevRData *rd = &g_array_index (b->rdata, prevRData, i);
		renderDbFreeFunc f_fkt = prev_renderdb_get_free (rd->type);
		if (f_fkt) f_fkt (rd->data);
	}
	if (full)
		g_array_free (b->rdata, TRUE);
	else
		g_array_set_size (b->rdata, 0);
}

/*********************************************************************
  PRIVATE: Store data needed for repainting of the type 'type' with
  mode disp_mode in b. Copying of the data is done by calling
  renderDbCopyFunc().
*********************************************************************/
void prev_rdata_append (prevBuffer *b, prevType type, const void *data, int disp_mode)
{
	prevRData rdata;
	renderDbCopyFunc c_fkt;

	if (!b->save_rdata) return;

	if ((c_fkt = prev_renderdb_get_copy(type))) {
		rdata.type = type;
		rdata.data = c_fkt (data);
		rdata.disp_mode = disp_mode;
		rdata.gc = b->gc;

		if (!b->rdata) b->rdata = g_array_new (FALSE, TRUE, sizeof(prevRData));
		g_array_append_val (b->rdata, rdata);
	}
}

/*********************************************************************
  Redraw the buffer b with the data stored in b->rdata.
*********************************************************************/
static void prev_rdata_redraw (prevBuffer *b)
{
	gdk_threads_leave();
	prev_redraw (b, NULL);
	gdk_threads_enter();
}

/*********************************************************************
  Remove all registered signal handlers.
*********************************************************************/
static void prev_signal_remove (prevBuffer *b)
{
	GSList *cback;

	for (cback = b->cback; cback; cback = cback->next) {
		if (cback->data)
			g_free (cback->data);
	}
	g_slist_free (b->cback);
	b->cback = NULL;
}

/*********************************************************************
  Call cback with data as last argument if one of the signals
  specified with sigset occured.
*********************************************************************/
void prev_signal_connect (prevBuffer *b, prevEvent sigset,
						  prevButtonFunc cback, void *data)
{
	prevCBack *cb = g_malloc (sizeof(prevCBack));

	cb->sigset = sigset;
	cb->cback = NULL;
	cb->cbackB = cback;
	cb->data = data;

	b->cback = g_slist_append (b->cback, cb);
}
void prev_signal_connect2 (prevBuffer *b, prevEvent sigset,
						   prevSignalFunc cback, void *data)
{
	prevCBack *cb = g_malloc (sizeof(prevCBack));

	cb->sigset = sigset;
	cb->cback = cback;
	cb->cbackB = NULL;
	cb->data = data;

	b->cback = g_slist_append (b->cback, cb);
}

static gint prev_expose_handler (prevBuffer *b)
{
	gdk_threads_enter();
	if (b->draw_width > b->width || b->draw_height > b->height)
		gdk_draw_rectangle (b->drawing->window, b->g_gc, TRUE,
							0, 0, b->draw_width, b->draw_height);
	prev_draw_buffer__nl (b, FALSE);
	b->expose_id = 0;
	gdk_threads_leave();
	return FALSE;
}

static gint cb_prev_expose_event (GtkWidget *widget, GdkEventExpose *event,
								  prevBuffer *b)
{
	if (b->expose_id) return FALSE;
	b->expose_id = gtk_idle_add ((GtkFunction)prev_expose_handler, b);
	return FALSE;
}

static gint cb_prev_configure_event (GtkWidget *widget, GdkEventConfigure *event,
									 prevBuffer *b)
{
	b->draw_width = widget->allocation.width;
	b->draw_height = widget->allocation.height;
	b->drawing = widget;

	if (b->g_gc) gdk_gc_destroy (b->g_gc);
	b->g_gc = gdk_gc_new (widget->window);
	gdk_gc_copy (b->g_gc, widget->style->white_gc);

	gdk_threads_leave();
	prev_redraw (b, widget);
	gdk_threads_enter();

	return TRUE;
}

static prevBuffer* prev_new_buffer (int width, int height, gboolean gray,
									const char *title)
{
	prevBuffer *b = g_malloc0 (sizeof(prevBuffer));

	b->width = width;
	b->height = height;
	b->gray = gray;
	b->buffer = NULL;	/* Allocated on window open in cb_prev_configure_event() */
	b->title = g_strdup (title);
	b->save_rdata = TRUE;
	b->save = SAVE_NONE;
	b->save_done = FALSE;
	b->save_avifile = NULL;
	b->cback = NULL;
	prev_gc_init (&b->gc);
	b->next = buffer;
	buffer = b;

	opts_page_add__nl (NULL, b, b->title);
	return b;
}

static void cb_prev_save (GtkWidget *widget, prevSave mode)
{
	iwImgFormat format;
	GtkItemFactory *factory;
	prevBuffer *b;

	factory = gtk_item_factory_from_widget (widget);
	if (!factory || !(b = gtk_object_get_user_data(GTK_OBJECT(factory))))
		return;

	if (b->save == SAVE_SEQ || b->save == SAVE_ORIGSEQ) {
		if (mode != b->save)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (b->save_widget), FALSE);
		avi_close (b);
	}
	b->save = SAVE_NONE;

	if (mode == SAVE_SEQ || mode == SAVE_ORIGSEQ) {
		if (!GTK_CHECK_MENU_ITEM(widget)->active)
			return;
		b->save_widget = widget;
	}

	if (prev_Mdata.prefs->fname[0] == '\0') {
		gui_message_show ("Error", "No file name for saving the image given!");
		return;
	}

	b->save = mode;
	b->save_done = TRUE;
	format = iw_img_save_get_format (prev_Mdata.prefs->format, prev_Mdata.prefs->fname);
	if (format < IW_IMG_FORMAT_MOVIE || format > IW_IMG_FORMAT_MOVIE_MAX)
		avi_close (b);

	if (mode == SAVE_RENDER || mode == SAVE_SEQ)
		prev_chk_save_intern (b, NULL, TRUE);
}

/*********************************************************************
  If zoom >= 0, change zoom level of the preview.
  If x >= 0 or y >= 0 pan the preview to that position.
*********************************************************************/
void prev_pan_zoom__nl (prevBuffer *b, int x, int y, float zoom)
{
	float old_bzoom = b->zoom, old_zoom = prev_get_zoom (b);

	if (iw_iszerof(zoom))
		b->zoom = 0;
	else if (zoom > 0)
		b->zoom = zoom;
	if (!iw_iszerof(b->zoom)) {
		if (old_bzoom != b->zoom) {
			/* Zoom to center of image */
			if (x < 0) {
				b->x = b->x+b->width/(2*old_zoom) - b->width/(2*b->zoom);
				if (b->x + b->width/b->zoom > b->gc.width)
					b->x = b->gc.width - b->width/b->zoom;
			}
			if (y < 0) {
				b->y = b->y+b->height/(2*old_zoom) - b->height/(2*b->zoom);
				if (b->y + b->height/b->zoom > b->gc.height)
					b->y = b->gc.height - b->height/b->zoom;
			}
		}
		if (x >= 0) b->x = x;
		if (y >= 0) b->y = y;
		if (b->x < 0) b->x = 0;
		if (b->y < 0) b->y = 0;
	} else {
		b->x = b->y = 0;
	}
	prev_rdata_redraw (b);
}
void prev_pan_zoom (prevBuffer *b, int x, int y, float zoom)
{
	gui_threads_enter();
	prev_pan_zoom__nl (b, x, y, zoom);
	gui_threads_leave();
}

/*********************************************************************
  Return a changed zoom level for b: Zoom in or out.
*********************************************************************/
static float zoom_window (prevBuffer *b, float old_zoom, gboolean zoom_in)
{
	float zoom = b->zoom;
	if (iw_iszerof(zoom)) {
		zoom = log(old_zoom)/log(2);
		if (zoom < 0) {
			if (zoom_in)
				zoom = floor(zoom);
			else
				zoom = ceil(zoom);
		} else
			zoom = rint(zoom);
		zoom = pow(2, zoom);
	}
	if (zoom < 1) {
		if (zoom_in) {
			zoom *= 2;
			if (zoom > 1) zoom = 1;
		} else {
			zoom /= 2;
			if (zoom < 0.031250)
				zoom = 0.031250;
		}
	} else {
		if (zoom_in) {
			zoom++;
		} else {
			zoom--;
			if (zoom < 1) zoom = 0.5;
		}
	}
	return zoom;
}

/*********************************************************************
  Handle button and key events for the menu popup, zooming/panning,
  or plugin callbacks.
*********************************************************************/
static gint cb_signal_event (GtkWidget *draw, GdkEvent *event, prevBuffer *b)
{
	prevEvent signal = 0;
	int x = -1, y = -1, button = -1;
	guint mod_state = 0;

	if (!event || event->any.window != draw->window)
		return FALSE;

	/* Get signal type, button and pointer position */
	if (event->type == GDK_BUTTON_PRESS) {
		x = event->button.x;
		y = event->button.y;
		button = event->button.button;
		mod_state = event->button.state;
		signal = PREV_BUTTON_PRESS;
	} else if (event->type == GDK_BUTTON_RELEASE) {
		x = event->button.x;
		y = event->button.y;
		button = event->button.button;
		mod_state = event->button.state;
		signal = PREV_BUTTON_RELEASE;
	} else if (event->type == GDK_MOTION_NOTIFY) {
		GdkModifierType state;
		if (event->motion.is_hint) {
			gdk_window_get_pointer (event->motion.window, &x, &y, &state);
		} else {
			x = event->button.x;
			y = event->button.y;
			state = event->motion.state;
		}
		button = ((state & GDK_BUTTON1_MASK) ? 1 :
				  ((state & GDK_BUTTON2_MASK) ? 2 :
				   ((state & GDK_BUTTON3_MASK) ? 3 : 0)));
		mod_state = event->motion.state;
		signal = PREV_BUTTON_MOTION;
	} else if (event->type == GDK_KEY_PRESS) {
		signal = PREV_KEY_PRESS;
	} else if (event->type == GDK_KEY_RELEASE) {
		signal = PREV_KEY_RELEASE;
#if GTK_CHECK_VERSION(2,0,0)
	} else if (event->type == GDK_SCROLL) {
		x = event->scroll.x;
		y = event->scroll.y;
		switch (event->scroll.direction) {
			case GDK_SCROLL_UP:
				button = 4;
				break;
			case GDK_SCROLL_DOWN:
				button = 5;
				break;
			default:
				return FALSE;
		}
		mod_state = event->scroll.state;
		signal = PREV_BUTTON_PRESS;
#endif
	} else
		return FALSE;

	if (button == 1 || button == 2) {
		static gboolean pointer_grabbed = FALSE;

		if (signal == PREV_BUTTON_RELEASE && pointer_grabbed) {
			gdk_pointer_ungrab (0);
			pointer_grabbed = FALSE;
		} else if (signal == PREV_BUTTON_PRESS) {
			pointer_grabbed = (gdk_pointer_grab (draw->window, FALSE,
												 GDK_BUTTON_RELEASE_MASK |
												 GDK_BUTTON_MOTION_MASK |
												 GDK_POINTER_MOTION_HINT_MASK,
												 NULL, NULL, 0) == 0);
		}
	}

	if (button >= 0) {
		int ret = ginfo_update (x, y, button, signal, b);
		if (ret > 1)
			prev_draw_buffer__nl (b, FALSE);
		if (ret > 0)
			return TRUE;
	}

	/* Button 3 -> menu */
	if (button == 3 && signal == PREV_BUTTON_PRESS) {
		gtk_menu_popup (GTK_MENU(b->menu), NULL, NULL, NULL, NULL,
						event->button.button, event->button.time);
		gtk_signal_emit_stop_by_name (GTK_OBJECT(draw), "button_press_event");

		return TRUE;
	}

	/* Scroll wheel <-> button 4/5 -> zoom and pan */
	if ((button == 4 || button == 5) && signal == PREV_BUTTON_PRESS) {
		float old_bzoom = b->zoom, old_zoom = prev_get_zoom (b);

		button = button*2-9;
		if (mod_state & GDK_CONTROL_MASK) {
			b->zoom = zoom_window (b, old_zoom, button < 0);
		} else if (mod_state & GDK_SHIFT_MASK) {
			b->x = b->x + button * b->width / (5*old_zoom);
		} else {
			b->y = b->y + button * b->height / (5*old_zoom);
		}

		if (!iw_iszerof(b->zoom)) {
			if (old_bzoom != b->zoom) {
				/* Zoom such that the pixel under the mouse
				   does not change it's position */
				b->x = b->x + (int)(x/old_zoom) - (int)(x/b->zoom);
				b->y = b->y + (int)(y/old_zoom) - (int)(y/b->zoom);

				if (b->x + b->width/b->zoom > b->gc.width)
					b->x = b->gc.width - b->width/b->zoom;
				if (b->y + b->height/b->zoom > b->gc.height)
					b->y = b->gc.height - b->height/b->zoom;
			}
			if (b->x < 0) b->x = 0;
			if (b->y < 0) b->y = 0;
		} else {
			b->x = b->y = 0;
		}

		prev_rdata_redraw (b);
		return TRUE;
	}

	/* Button 2 -> zoom and pan */
	if (button == 2) {
		static float old_x = -999, old_y = -999;
		float old_bzoom = b->zoom, old_zoom = prev_get_zoom (b);

		/* Stop panning */
		if (signal == PREV_BUTTON_RELEASE) {
			gdk_window_set_cursor (draw->window, NULL);
			old_x = -999;
			old_y = -999;
			return TRUE;
		}

		/* Adjust zoom level */
		if (signal == PREV_BUTTON_PRESS) {
			GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
			gdk_window_set_cursor (draw->window, cursor);
			gdk_cursor_destroy (cursor);

			if (mod_state & GDK_MOD1_MASK) {
				b->zoom = 0;
			} else if ((mod_state & GDK_SHIFT_MASK) ||
					   (mod_state & GDK_CONTROL_MASK)) {
				b->zoom = zoom_window (b, old_zoom, mod_state & GDK_SHIFT_MASK);
			}
		}

		/* Adjust panning position */
		if (!iw_iszerof(b->zoom)) {
			if (signal == PREV_BUTTON_PRESS) {
				if (old_bzoom != b->zoom) {
					/* Zoom such that the pixel under the mouse
					   does not change it's position */
					b->x = b->x + (int)(x/old_zoom) - (int)(x/b->zoom);
					b->y = b->y + (int)(y/old_zoom) - (int)(y/b->zoom);

					if (b->x + b->width/b->zoom > b->gc.width)
						b->x = b->gc.width - b->width/b->zoom;
					if (b->y + b->height/b->zoom > b->gc.height)
						b->y = b->gc.height - b->height/b->zoom;
				}
				old_x = x;
				old_y = y;
			} else if (old_x > -100) {
				int delta = (x-old_x)*b->gc.width/b->width;
				b->x += delta;
				old_x += (float)delta*b->width/b->gc.width;

				delta = (y-old_y)*b->gc.height/b->height;
				b->y += delta;
				old_y += (float)delta*b->height/b->gc.height;
			}
			if (b->x < 0) b->x = 0;
			if (b->y < 0) b->y = 0;
		} else {
			b->x = b->y = 0;
		}

		prev_rdata_redraw (b);
		return TRUE;
	}

	/* Button 1/key -> program callback */
	if (button == 1 || signal == PREV_KEY_PRESS || signal == PREV_KEY_RELEASE) {
		GSList *cback = b->cback;
		prevCBack *cb;
		prevEventData data;
		gboolean ret = FALSE;

		while (cback) {
			cb = cback->data;
			if ((cb->cback || cb->cbackB) && (signal & cb->sigset)) {
				data.button.type = signal;
				switch (signal) {
					case PREV_BUTTON_PRESS:
					case PREV_BUTTON_RELEASE:
						data.button.time = event->button.time;
						data.button.state = mod_state;
						break;
					case PREV_BUTTON_MOTION:
						data.motion.time = event->motion.time;
						data.motion.state = mod_state;
						break;
					case PREV_KEY_PRESS:
					case PREV_KEY_RELEASE:
						data.key.time = event->key.time;
						data.key.state = event->key.state;
						data.key.keyval = event->key.keyval;
						data.key.length = event->key.length;
						data.key.string = event->key.string;
						break;
				}
				if (button == 1) {
					float zoom = prev_get_zoom (b);
					int xz, yz;
					xz = x/zoom + b->x;
					yz = y/zoom + b->y;

					if (xz < 0) xz = 0;
					if (xz >= b->gc.width) xz = b->gc.width-1;
					if (yz < 0) yz = 0;
					if (yz >= b->gc.height) yz = b->gc.height-1;
					data.button.x = xz;
					data.button.y = yz;
					data.button.x_orig = x;
					data.button.y_orig = y;
					data.button.button = button;
				}
				if (cb->cback)
					(cb->cback) (b, &data, cb->data);
				else
					(cb->cbackB) (b, data.button.type,
								  data.button.x, data.button.y, cb->data);
				ret = TRUE;
			}
			cback = cback->next;
		}
		if (ret) return ret;
	}

	return FALSE;
}

/*********************************************************************
  Zoom menu entry from a context menu was selected.
*********************************************************************/
static void cb_prev_zoom (prevBuffer *b, prevZoom zoom_mode)
{
	float zoom = prev_get_zoom (b);
	switch (zoom_mode) {
		case ZOOM_IN:
			zoom = zoom_window (b, zoom, TRUE);
			break;
		case ZOOM_OUT:
			zoom = zoom_window (b, zoom, FALSE);
			break;
		case ZOOM_FIT:
			zoom = 0;
			break;
		case ZOOM_ONE:
			zoom = 1;
			break;
	}
	prev_pan_zoom__nl (b, -1, -1, zoom);
}

/*********************************************************************
  Pan menu entry from a context menu was selected.
*********************************************************************/
static void cb_prev_pan (prevBuffer *b, prevPan pan_mode)
{
	float zoom = prev_get_zoom (b);
	switch (pan_mode) {
		case PAN_LEFT:
			prev_pan_zoom__nl (b, b->x - b->width/(5*zoom), -1, -1);
			break;
		case PAN_RIGHT:
			prev_pan_zoom__nl (b, b->x + b->width/(5*zoom), -1, -1);
			break;
		case PAN_TOP:
			prev_pan_zoom__nl (b, -1, b->y - b->height/(5*zoom), -1);
			break;
		case PAN_BOTTOM:
			prev_pan_zoom__nl (b, -1, b->y + b->height/(5*zoom), -1);
			break;
	}
}

/*********************************************************************
  Close the preview settings window and free all resources.
*********************************************************************/
static void prev_settings_remove (prevBuffer *b)
{
	prevSettings *dat = b->settings;
	if (dat) {
		if (dat->window)
			gtk_widget_destroy (dat->window);
		if (dat->title)
			g_free (dat->title);
		g_free (dat);
		b->settings = NULL;
	}
}

/*********************************************************************
  Open the preview settings window (must be already initialised).
*********************************************************************/
static void prev_settings_open (prevBuffer *b)
{
	prevSettings *dat = b->settings;

	iw_session_set (dat->window, dat->title, TRUE, TRUE);
	gtk_widget_show (dat->window);
	gdk_window_raise (dat->window->window);
}

static void cb_settings_open (GtkWidget *widget, prevBuffer *b)
{
	prev_settings_open (b);
}

static gint cb_settings_delete (GtkWidget *widget, GdkEvent *event, prevSettings *dat)
{
	gui_dialog_hide_window (widget);
	iw_session_remove (dat->title);
	return TRUE;
}

static void cb_settings_close (GtkWidget *widget, prevSettings *dat)
{
	cb_settings_delete (dat->window, NULL, dat);
}

/*********************************************************************
  Return the page number for the preview settings window. Allows to
  create any widgets with the help of the opts_() functions.
*********************************************************************/
int prev_get_page (prevBuffer *b)
{
	prevSettings *dat = b->settings;

	gui_threads_enter();
	if (dat->page_num <= 0) {
		static GtkItemFactoryEntry item = {
			"/Settings...", "<control>p", cb_settings_open, 0, "<Item>"};

		dat->page_num = opts_page_add__nl (dat->vbox, NULL, dat->title);
		gtk_item_factory_create_item (b->factory, &item, b, 2);
		gtk_menu_reorder_child (GTK_MENU(b->menu),
								gtk_item_factory_get_widget (b->factory, item.path),
								6);
	}
	gui_threads_leave();

	return dat->page_num;
}

/*********************************************************************
  Init the preview settings window.
*********************************************************************/
static void prev_settings_init (prevBuffer *b)
{
#define SET_TITLE		" - Settings"
	prevSettings *dat;
	GtkWidget *vbox, *widget;
	GtkAccelGroup *accel;

	b->settings = dat = g_malloc (sizeof(prevSettings));
	dat->page_num = -1;
	dat->title = g_malloc (strlen(b->title) + strlen(SET_TITLE) + 1);
	strcpy (dat->title, b->title);
	strcat (dat->title, SET_TITLE);

	accel = gtk_accel_group_new();
	dat->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	prev_set_wmname (GTK_WINDOW(dat->window), "Settings");

	gtk_signal_connect (GTK_OBJECT(dat->window), "delete_event",
						GTK_SIGNAL_FUNC(cb_settings_delete), dat);
	gtk_window_set_title (GTK_WINDOW (dat->window), dat->title);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER (dat->window), vbox);

	dat->vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (vbox), dat->vbox);

	widget = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label("Close");
	gtk_signal_connect (GTK_OBJECT(widget), "clicked",
						GTK_SIGNAL_FUNC(cb_settings_close), dat);
	gtk_widget_add_accelerator (widget, "clicked", accel, GDK_Escape, 0, 0);
	gtk_box_pack_start (GTK_BOX(vbox), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);
	gui_apply_icon (dat->window);
	gtk_window_add_accel_group (GTK_WINDOW(dat->window), accel);

	if (iw_session_find (dat->title)) prev_settings_open(b);
}

static void prev_close_window (prevBuffer *b)
{
	GtkCTreeNode *node;

	if (b->window) {
		iw_session_remove (b->title);
		gtk_widget_destroy (b->window);
		b->window = NULL;
		gdk_gc_destroy (b->g_gc);
		b->g_gc = NULL;
	}
	node = gtk_ctree_find_by_row_data (GTK_CTREE(prev_ctree), NULL, b);
	gui_tree_show_bool (prev_ctree, node, 0, FALSE);

	avi_close (b);
}

static void cb_prev_delete (GtkWidget *widget, GdkEvent *event, prevBuffer *b)
{
	prev_close_window (b);
}

static void prev_open_window (prevBuffer *b)
{
	GtkWidget *window, *drawing_area;
	GtkCTreeNode *node;

	node = gtk_ctree_find_by_row_data (GTK_CTREE(prev_ctree), NULL, b);
	gui_tree_show_bool (prev_ctree, node, 0, TRUE);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	prev_set_wmname (GTK_WINDOW(window), b->title);
	gtk_window_add_accel_group (GTK_WINDOW (window), b->accel);

	gtk_window_set_default_size (GTK_WINDOW(window), b->width, b->height);
	/*  gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, TRUE); */
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
						GTK_SIGNAL_FUNC(cb_prev_delete), b);

	drawing_area = gtk_drawing_area_new ();
	gtk_widget_set_double_buffered (drawing_area, FALSE);
	gtk_widget_set_usize (drawing_area, PREV_WIDTH_MIN, PREV_HEIGHT_MIN);

	gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
						(GtkSignalFunc) cb_prev_expose_event, b);
	gtk_signal_connect (GTK_OBJECT(drawing_area),"configure_event",
						(GtkSignalFunc) cb_prev_configure_event, b);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "button_press_event",
						(GtkSignalFunc) cb_signal_event, b);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "button_release_event",
						(GtkSignalFunc) cb_signal_event, b);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "motion_notify_event",
						(GtkSignalFunc) cb_signal_event, b);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "key_press_event",
						(GtkSignalFunc) cb_signal_event, b);
	gtk_signal_connect (GTK_OBJECT (drawing_area), "key_release_event",
						(GtkSignalFunc) cb_signal_event, b);
#if GTK_CHECK_VERSION(2,0,0)
	gtk_signal_connect (GTK_OBJECT (drawing_area), "scroll_event",
						(GtkSignalFunc) cb_signal_event, b);
#endif

	gtk_object_set_user_data (GTK_OBJECT(window), b);
	gtk_widget_set_events (drawing_area,
						   GDK_EXPOSURE_MASK |
						   GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
						   GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
						   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	gtk_container_add (GTK_CONTAINER (window), drawing_area);

	iw_session_set_prev (window, b, b->title, TRUE, TRUE);
	gui_apply_icon (window);
	gtk_widget_show_all (window);

	GTK_WIDGET_SET_FLAGS (drawing_area, GTK_CAN_FOCUS);
	gtk_widget_grab_focus (drawing_area);
}

/*********************************************************************
  Close preview window b (if necessary), remove it from the CTree and
  free its memory.
*********************************************************************/
void prev_free_window (prevBuffer *b)
{
	char *title, *t_parent;
	GtkCTreeNode *node;
	int read = -1;

	if (!b) return;

	gui_threads_enter();

	if (b->window) prev_close_window (b);

	if (b == buffer) {
		buffer = b->next;
	} else {
		prevBuffer *pos = buffer;
		while (pos->next != b) pos = pos->next;
		pos->next = b->next;
	}

	node = gtk_ctree_find_by_row_data (GTK_CTREE(prev_ctree), NULL, b);
	gtk_ctree_remove_node (GTK_CTREE(prev_ctree), node);

	gui_sscanf (b->title, "%a.%n", &t_parent, &read);
	if (read > 0 && prev_parents) {
		prevParent *parent = g_hash_table_lookup (prev_parents, t_parent);
		if (parent) {
			parent->children--;
			if (parent->children <= 0) {
				g_hash_table_remove (prev_parents, t_parent);
				gtk_ctree_remove_node (GTK_CTREE(prev_ctree), parent->parent);
				g_free (parent->title);
				g_free (parent);
			}
		}
	}
	if (t_parent) g_free (t_parent);

	title = g_malloc (strlen(b->title)+strlen(OPT_SAVE_RDATA)+2);
	strcpy (title, b->title);
	strcat (title, "."OPT_SAVE_RDATA);
	opts_widget_remove__nl (title);
	g_free (title);

	prev_settings_remove (b);
	if (b->factory) gtk_object_destroy (GTK_OBJECT(b->factory));
	prev_signal_remove (b);
	opts_page_remove__nl (b->title);

	gui_threads_leave();
	prev_buffer_lock();

	prev_rdata_free (b, TRUE);
	g_free (b->title);
	if (b->buffer) g_free (b->buffer);
	prev_opts_free (b);
	g_free (b);

	prev_buffer_unlock();
}

typedef enum {
	ID_SAVE,
	ID_SAVE_ORIG,
	ID_SAVE_SES,
	ID_CLEAR_SES,
	ID_CLOSE_WIN,
	ID_ZOOM_IN,
	ID_ZOOM_OUT,
	ID_ZOOM_FIT,
	ID_ZOOM_ONE,
	ID_PAN_LEFT,
	ID_PAN_RIGHT,
	ID_PAN_TOP,
	ID_PAN_BOTTOM,
	ID_INFO_WIN
} prevMenuID;

static void cb_menu (GtkWidget *widget, int button, void *data)
{
	GtkItemFactory *factory;
	prevBuffer *b;
	prevMenuID id = GPOINTER_TO_INT(data);

	factory = gtk_item_factory_from_widget (widget);
	if (!factory || !(b = gtk_object_get_user_data(GTK_OBJECT(factory))))
		return;

	switch (id) {
		case ID_SAVE:
			cb_prev_save (widget, SAVE_RENDER);
			break;
		case ID_SAVE_ORIG:
			cb_prev_save (widget, SAVE_ORIG);
			break;
		case ID_SAVE_SES:
			iw_session_save();
			break;
		case ID_CLEAR_SES:
			iw_session_clear();
			break;
		case ID_CLOSE_WIN:
			prev_close_window (b);
			break;
		case ID_ZOOM_IN:
			cb_prev_zoom (b, ZOOM_IN);
			break;
		case ID_ZOOM_OUT:
			cb_prev_zoom (b, ZOOM_OUT);
			break;
		case ID_ZOOM_FIT:
			cb_prev_zoom (b, ZOOM_FIT);
			break;
		case ID_ZOOM_ONE:
			cb_prev_zoom (b, ZOOM_ONE);
			break;
		case ID_PAN_LEFT:
			cb_prev_pan (b, PAN_LEFT);
			break;
		case ID_PAN_RIGHT:
			cb_prev_pan (b, PAN_RIGHT);
			break;
		case ID_PAN_TOP:
			cb_prev_pan (b, PAN_TOP);
			break;
		case ID_PAN_BOTTOM:
			cb_prev_pan (b, PAN_BOTTOM);
			break;
		case ID_INFO_WIN:
			ginfo_open();
			break;
	}
}

typedef enum {
	FAC_TEAR,
	FAC_CHECK,
	FAC_SEP,
	FAC_ITEM
} prevFactoryType;
typedef struct prevFactoryItem {	/* Definition of the context menu items */
	char *title;
	char *ttip;
	optsCbackFunc cback;
	prevFactoryType type;
	int data;
} prevFactoryItem;

static prevFactoryItem prev_items[] = {
	{"/Tear", NULL, NULL, FAC_TEAR, 0},
	{"/File/Tear", NULL, NULL, FAC_TEAR, 0},
	{".File/Save<<control>s>", NULL, cb_menu, FAC_ITEM, ID_SAVE},
	{".File/Save Original<<control>o>", NULL, cb_menu, FAC_ITEM, ID_SAVE_ORIG},
	{"/File/Save Seq", NULL, (optsCbackFunc)cb_prev_save, FAC_CHECK, SAVE_SEQ},
	{"/File/Save OrigSeq", NULL, (optsCbackFunc)cb_prev_save, FAC_CHECK, SAVE_ORIGSEQ},
	{".File/Sep1", NULL, NULL, FAC_SEP, 0},
	{".File/Save Session", NULL, cb_menu, FAC_ITEM, ID_SAVE_SES},
	{".File/Clear Session", NULL, cb_menu, FAC_ITEM, ID_CLEAR_SES},
	{".File/Sep2", NULL, NULL, FAC_SEP, 0},
	{".File/Close Window<<control>w>", NULL, cb_menu, FAC_ITEM, ID_CLOSE_WIN},
	{"/View/Tear", NULL, NULL, FAC_TEAR, 0},
	{".View/Zoom In<<control>plus>", NULL, cb_menu, FAC_ITEM, ID_ZOOM_IN},
	{".View/Zoom Out<<control>minus>", NULL, cb_menu, FAC_ITEM, ID_ZOOM_OUT},
	{".View/Zoom to Fit Window<<control>f>", NULL, cb_menu, FAC_ITEM, ID_ZOOM_FIT},
	{".View/Zoom 1:1<<control>1>", NULL, cb_menu, FAC_ITEM, ID_ZOOM_ONE},
	{".View/Sep1", NULL, NULL, FAC_SEP, 0},
	{".View/Pan Left", NULL, cb_menu, FAC_ITEM, ID_PAN_LEFT},
	{".View/Pan Right", NULL, cb_menu, FAC_ITEM, ID_PAN_RIGHT},
	{".View/Pan Up", NULL, cb_menu, FAC_ITEM, ID_PAN_TOP},
	{".View/Pan Down", NULL, cb_menu, FAC_ITEM, ID_PAN_BOTTOM},
	{".Info Window...<<control>i>", NULL, cb_menu, FAC_ITEM, ID_INFO_WIN},
	{".Sep1", NULL, NULL, FAC_SEP, 0},
	{NULL, NULL, NULL, 0, 0}
};

static void prev_init_menu (prevBuffer *b)
{
	prevFactoryItem *item;
	GtkItemFactoryEntry ifac;
	char *title, *title_cont;

	b->accel = gtk_accel_group_new ();
	b->factory = gtk_item_factory_new (GTK_TYPE_MENU, "<pcontext>",  b->accel);

	title = malloc (strlen(b->title)+100);
	strcpy (title, b->title);
	title_cont = title+strlen(b->title);

	/* Init main menu items */
	for (item = prev_items; item->title; item++) {
		switch (item->type) {
			case FAC_TEAR:
			case FAC_CHECK:
				ifac.path = item->title;
				ifac.accelerator = NULL;
				ifac.callback = item->cback;
				ifac.callback_action = 0;
				if (item->type == FAC_TEAR)
					ifac.item_type = "<Tearoff>";
				else
					ifac.item_type = "<CheckItem>";
				gtk_item_factory_create_item (b->factory, &ifac, GINT_TO_POINTER(item->data), 2);
				break;
			case FAC_SEP:
				gui_threads_leave();
				strcpy (title_cont, item->title);
				opts_separator_create (-1, title);
				gui_threads_enter();
				break;
			case FAC_ITEM:
				gui_threads_leave();
				strcpy (title_cont, item->title);
				opts_buttoncb_create (-1, title, item->ttip, item->cback, GINT_TO_POINTER(item->data));
				gui_threads_enter();
				break;
		}
	}
	free (title);

	gtk_object_set_user_data (GTK_OBJECT(b->factory), b);
	b->menu = gtk_item_factory_get_widget (b->factory, "<pcontext>");
	gtk_widget_show_all (b->menu);
}

/*********************************************************************
  Initialise a new preview window (Title: title, size: width x height,
  depth: 24bit (gray==FALSE) or 8 Bit (gray==TRUE)).
  show == TRUE: The window is shown immediately.
  width, heigth < 0 : Default width, height is used.
  title contains '.' (a'.'b): Window is shown in the list of windows
                              under node a with entry b.
*********************************************************************/
prevBuffer *prev_new_window (const char *title, int width, int height,
							 gboolean gray, gboolean show)
{
	static int use_tree = -1;	/* widget style: uninitialised  */

	char *column[PREV_COLS] = {NULL,NULL};
	prevBuffer *b;
	prevParent *parent = NULL;
	GtkCTreeNode *node;
	char *t_parent = NULL;
	int read = -1;

	iw_assert (title && *title, "No window title given");
	iw_assert (prev_ctree != NULL, "Gpreview not initialized");

	gui_threads_enter();

	if (width < 0) width = prev_Mdata.prev_width;
	if (height < 0) height = prev_Mdata.prev_height;
	b = prev_new_buffer (width, height, gray, title);

	prev_init_menu (b);
	prev_settings_init (b);

	/* Initialise widget style */
	if (use_tree < 0) {
		use_tree = prev_Mdata.prefs->use_tree;
		use_tree = (use_tree == PREF_TREE_IMAGES || use_tree == PREF_TREE_BOTH);
		if (use_tree)
			gtk_ctree_set_line_style (GTK_CTREE(prev_ctree), GTK_CTREE_LINES_SOLID);
		else
			gtk_ctree_set_line_style (GTK_CTREE(prev_ctree), GTK_CTREE_LINES_NONE);
	}
	gui_sscanf (title, "%a.%n", &t_parent, &read);
	if (read > 0 && use_tree) {
		if (!prev_parents)
			prev_parents = g_hash_table_new (g_str_hash, g_str_equal);
		parent = g_hash_table_lookup (prev_parents, t_parent);
		if (!parent) {
			parent = g_malloc (sizeof(prevParent));
			column[1] = t_parent;
			parent->parent = gtk_ctree_insert_node (GTK_CTREE(prev_ctree), NULL, NULL,
													column, 0, NULL, NULL, NULL, NULL,
													FALSE, TRUE);
			parent->children = 0;
			parent->title = g_strdup(t_parent);
			g_hash_table_insert (prev_parents, parent->title, parent);
		}
		column[1] = (char*)&title[read]; /* const */
		node = gtk_ctree_insert_node (GTK_CTREE(prev_ctree), parent->parent, NULL,
									  column, 0, NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
		parent->children++;
	} else {
		column[1] = (char*)title; /* const */
		node = gtk_ctree_insert_node (GTK_CTREE(prev_ctree), NULL, NULL,
									  column, 0, NULL, NULL, NULL, NULL,
									  TRUE, TRUE);
		if (!use_tree)
			gtk_ctree_node_set_shift (GTK_CTREE(prev_ctree), node, 1, 0, -20);
	}
	if (t_parent) g_free (t_parent);

	gtk_ctree_node_set_row_data (GTK_CTREE(prev_ctree), node, b);
	if (show || iw_session_find (b->title))
		prev_open_window (b);

	gui_threads_leave();

	opts_toggle_create ((long)b, OPT_SAVE_RDATA,
						"Save original data for immediate redraws",
						&b->save_rdata);
	prev_opts_append (b, prev_Mdata.prev_draw, -1);

	return b;
}

static gint cb_prev_button_press (GtkWidget *ctree, GdkEventButton *event, gpointer data)
{
	gint row, column;
	if (event->type == GDK_2BUTTON_PRESS) {
		if (gtk_clist_get_selection_info (GTK_CLIST(ctree), event->x, event->y,
										  &row, &column)) {
			prevBuffer *b = gtk_clist_get_row_data (GTK_CLIST(ctree), row);
			if (b) {
				if (b->window)
					prev_close_window (b);
				else
					prev_open_window (b);
			}
		}
		return TRUE;
	}
	return FALSE;
}

/*********************************************************************
  PRIVATE: Initialise the preview window selection list. prev_width
  and prev_height are the default sizes for a preview window.
*********************************************************************/
void prev_init (GtkBox *vbox, int prev_width, int prev_height)
{
	GtkWidget *widget, *ctree;
	static char *prev_title[PREV_COLS] = {"","Window"};

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
									GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (vbox), widget);

	ctree = gtk_ctree_new_with_titles (PREV_COLS, 1, prev_title);
	prev_ctree = ctree;
	gtk_ctree_set_indent (GTK_CTREE(ctree), 15);
	gtk_clist_set_selection_mode (GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_passive (GTK_CLIST(ctree));
	gtk_clist_set_column_justification (GTK_CLIST(ctree), 0, GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification (GTK_CLIST(ctree), 1, GTK_JUSTIFY_LEFT);

	gtk_clist_set_column_width (GTK_CLIST(ctree), 0, 20);
	gtk_clist_set_column_width (GTK_CLIST(ctree), 1, 180);
	gtk_widget_set_usize (ctree, 180, 100);

	gtk_container_add (GTK_CONTAINER (widget), ctree);

	gtk_signal_connect (GTK_OBJECT(ctree), "button_press_event",
						GTK_SIGNAL_FUNC(cb_prev_button_press), NULL);

	prev_Mdata.prev_width = prev_width;
	prev_Mdata.prev_height = prev_height;
	prev_Mdata.save_cnt = 0;
	prev_Mdata.prefs = iw_prefs_get();
	ginfo_init();

	prev_Mdata.prev_draw = prev_renderdb_register (PREV_NEW, prev_draw_opts,
												   NULL, NULL, NULL);
	prev_render_init();
}

/*********************************************************************
  Copy the content of a preview window into an iceWing image. The
  preview window needs to be open (buffer->window != NULL), otherwise
  NULL is returned. If save_full the full visible preview window size
  is used, otherwise the size of the displayed data is used.
*********************************************************************/
iwImage *prev_copy_to_image (prevBuffer *b, gboolean save_full)
{
	iwImage img; /* The preview buffer image */
	guchar *buf = NULL;
	guchar *planeptr[1];
	iwImage *image;

	if (!b || !b->window) return NULL;

	prev_buffer_lock();
	iw_img_init (&img);
	img.data = planeptr;
	img.type = IW_8U;
	if (b->gray) {
		img.planes = 1;
		img.ctab = IW_BW;
	} else {
		img.planes = 3;
		img.ctab = IW_RGB;
	}
	if (save_full) {
		img.width = b->width;
		img.height = b->height;
		img.rowstride = b->gray ? img.width : img.width*3;
		img.data[0] = b->buffer;
	} else {
		float zoom = prev_get_zoom(b);
		/* Adapt width */
		img.width = b->width / zoom;
		if (img.width + b->x > b->gc.width)
			img.width = b->gc.width - b->x;
		img.width *= zoom;
		if (img.width < 1)
			img.width = 1;
		/* Adapt height */
		img.height = b->height / zoom;
		if (img.height + b->y > b->gc.height)
			img.height = b->gc.height - b->y;
		img.height *= zoom;
		if (img.height < 1)
			img.height = 1;
		/* Get planes */
		img.rowstride = b->gray ? img.width : img.width*3;
		if (img.width == b->width) {
			img.data[0] = b->buffer;
		} else {
			int y, line;
			buf = gui_get_buffer (img.rowstride*img.height);
			line = b->gray ? b->width : b->width*3;
			for (y = 0; y < img.height; y++)
				memcpy (buf+y*img.rowstride, b->buffer+y*line, img.rowstride);
			img.data[0] = buf;
		}
	}
	image = iw_img_duplicate (&img);
	if (buf) gui_release_buffer (buf);
	prev_buffer_unlock();
	return image;
}
