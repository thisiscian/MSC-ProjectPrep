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
#include <math.h>
#include <gtk/gtk.h>
#include "Grender.h"
#include "Gpreview_i.h"
#include "Gsession.h"
#include "Gdialog.h"
#include "Ggui_i.h"
#include "Gtools.h"
#include "Ginfo.h"
#include "images/colorPicker.xpm"
#include "images/measure.xpm"
#include "images/radius.xpm"

#ifndef G_PI
#define G_PI    3.14159265358979323846  /* Somehow in the wrong file ... */
#endif

#define INFO_TITLE		"Info Window"
#define MEAS_CNT		4
#define MEAS_SIZE		7
#define GRAB_SIZE		5

#define SEC_CNT			3

struct _infoWinData {
	GtkWidget *window;

	GtkWidget *labOrig[2];

	GtkWidget *labRGB[2][3];
	GtkWidget *labHSV[2][3];
	GtkWidget *labYUV[2][3];
	GtkWidget *labXY[2][2];

	GtkWidget *widSec[SEC_CNT];

	GtkWidget *hboxMeas;
	GtkWidget *labMeas[MEAS_CNT];

	GtkWidget *radius;
	GtkWidget *grab;
	GtkWidget *measure;
	GtkWidget *notebook;

	int grab_x, grab_y;
	int meas_x1, meas_y1, meas_x2, meas_y2;
	gboolean meas_active;
	gboolean grab_active;
};

static infoWinData *ginfo_iwd = NULL;

/*********************************************************************
  Return the coordinates of the currently selected rectangle from the
  ginfo measurement function.
*********************************************************************/
void ginfo_get_coords (int *x1, int *y1, int *x2, int *y2)
{
	infoWinData *iwd = ginfo_iwd;
	if (iwd) {
		*x1 = iwd->meas_x1;
		*y1 = iwd->meas_y1;
		*x2 = iwd->meas_x2;
		*y2 = iwd->meas_y2;
	} else {
		*x1 = *y1 = *x2 = *y2 = -1;
	}
}

static gint cb_ginfo_delete (GtkWidget *widget, GdkEvent *event, infoWinData *iwd)
{
	int r;

	gui_dialog_hide_window (iwd->window);

	for (r=0; r<3; r++) {
		gtk_label_set_text (GTK_LABEL(iwd->labRGB[1][r]), NULL);
		gtk_label_set_text (GTK_LABEL(iwd->labHSV[1][r]), NULL);
		gtk_label_set_text (GTK_LABEL(iwd->labYUV[1][r]), NULL);
	}
	gtk_label_set_text (GTK_LABEL(iwd->labOrig[1]), NULL);

	gtk_label_set_text (GTK_LABEL(iwd->labXY[1][0]), NULL);
	gtk_label_set_text (GTK_LABEL(iwd->labXY[1][1]), NULL);
	if (iwd->labMeas[0]) {
		for (r=0; r<MEAS_CNT; r++) {
			gtk_widget_destroy (iwd->labMeas[r]);
			iwd->labMeas[r] = NULL;
		}
	}
	for (r=0; r<SEC_CNT; r++)
		gtk_widget_hide (GTK_WIDGET(iwd->widSec[r]));
	gdk_window_resize (iwd->window->window, 1, 1);

	iwd->grab_x = -1;
	iwd->meas_x1 = -1;

	iw_session_remove (INFO_TITLE);
	return TRUE;
}

static void cb_measure_clicked (GtkWidget *widget, infoWinData *iwd)
{
	iwd->meas_x1 = -1;
	iwd->meas_x2 = -1;

	iwd->meas_active =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

static void cb_grab_clicked (GtkWidget *widget, infoWinData *iwd)
{
	iwd->grab_active =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

/*********************************************************************
  Draw ginfo specific markers into b->drawing->window.
*********************************************************************/
void ginfo_render (prevBuffer *buf)
{
	infoWinData *iwd = ginfo_iwd;
	int x1, y1, x2, y2, dx, dy;
	float zoom, angle;
	GdkDrawable *draw;

	if (!buf->window || !iwd || (iwd->grab_x < 0 && iwd->meas_x1 < 0))
		return;

	draw = buf->drawing->window;
	zoom = prev_get_zoom (buf);

	dx = (buf->draw_width-buf->width)/2 + zoom/2;
	dy = (buf->draw_height-buf->height)/2 + zoom/2;
	gdk_gc_set_function (buf->g_gc, GDK_XOR);

	if (iwd->grab_x >= 0) {
		x1 = (iwd->grab_x-buf->x)*zoom + dx;
		y1 = (iwd->grab_y-buf->y)*zoom + dy;

		gdk_draw_line (draw, buf->g_gc, x1-GRAB_SIZE, y1-GRAB_SIZE, x1+GRAB_SIZE, y1+GRAB_SIZE);
		gdk_draw_line (draw, buf->g_gc, x1-GRAB_SIZE, y1+GRAB_SIZE, x1+GRAB_SIZE, y1-GRAB_SIZE);
	}
	if (iwd->meas_x1 >= 0) {
		x1 = (iwd->meas_x1-buf->x)*zoom + dx;
		y1 = (iwd->meas_y1-buf->y)*zoom + dy;
		x2 = (iwd->meas_x2-buf->x)*zoom + dx;
		y2 = (iwd->meas_y2-buf->y)*zoom + dy;

		gdk_draw_line (draw, buf->g_gc, x1-MEAS_SIZE, y1, x1+20, y1);
		gdk_draw_line (draw, buf->g_gc, x1, y1-MEAS_SIZE, x1, y1+MEAS_SIZE);

		gdk_draw_line (draw, buf->g_gc, x2-MEAS_SIZE, y2, x2+MEAS_SIZE, y2);
		gdk_draw_line (draw, buf->g_gc, x2, y2-MEAS_SIZE, x2, y2+MEAS_SIZE);

		gdk_draw_line (draw, buf->g_gc, x1, y1, x2, y2);

		/* Rotate by 90 deg. and mirror on the y axis */
		dx = iwd->meas_y1 - iwd->meas_y2;
		dy = iwd->meas_x1 - iwd->meas_x2;
		if (dx != 0 || dy > 0) {
			angle = acos(dy/sqrt(dx*dx+dy*dy))*180/G_PI;
			if (dx > 0 || (dx == 0 && dy < 0))
				angle = -angle;
			angle += 180;
			gdk_draw_arc (draw, buf->g_gc, FALSE, x1-18, y1-18, 36, 36,
						  0, (int)(angle*64));
		}
	}
	gdk_gc_set_function (buf->g_gc, GDK_COPY);
}

static void label_meas_create (infoWinData *iwd)
{
	int i;

	if (iwd->labMeas[0]) return;

	for (i=0; i<MEAS_CNT; i++) {
		iwd->labMeas[i] = gtk_label_new ("");
		gtk_box_pack_start (GTK_BOX(iwd->hboxMeas), iwd->labMeas[i], TRUE, FALSE, 0);
		gtk_widget_show (iwd->labMeas[i]);
	}
	gtk_label_set_text (GTK_LABEL(iwd->labMeas[0]), "Dist");
	gtk_label_set_text (GTK_LABEL(iwd->labMeas[2]), "Angle");
}

static void label_grab_create (infoWinData *iwd)
{
	int i;

	if (GTK_WIDGET_VISIBLE(iwd->widSec[0])) return;

	for (i=0; i<SEC_CNT; i++)
		gtk_widget_show (GTK_WIDGET(iwd->widSec[i]));
}

static void label_set (GtkWidget *label, int val)
{
	gchar str[5];
	g_snprintf (str, sizeof(str), "%d", val);
	gtk_label_set_text (GTK_LABEL(label), str);
}

static void labels_set (GtkWidget *label[2][3], int v1, int v2, int v3, gboolean grab)
{
	label_set (label[0][0], v1);
	label_set (label[0][1], v2);
	label_set (label[0][2], v3);
	if (grab) {
		label_set (label[1][0], v1);
		label_set (label[1][1], v2);
		label_set (label[1][2], v3);
	}
}

/*********************************************************************
  Update information displayed in the info window.
  return value >0 : Button press was handled by this function.
               >1 : Window must be redrawn.
*********************************************************************/
int ginfo_update (int x, int y, int button, prevEvent signal, prevBuffer *buf)
{
	infoWinData *iwd = ginfo_iwd;
	int planes = (buf->gray?1:3), page;
	int grab = 0, meas = 0;

	if (!iwd || !GTK_WIDGET_VISIBLE(iwd->window))
		return 0;

	if (button == 1) {
		if (iwd->grab_active) {
			grab = 1;
			if (signal == PREV_BUTTON_RELEASE)
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(iwd->grab), FALSE);
		} else if (iwd->meas_active && (signal == PREV_BUTTON_PRESS ||
										signal == PREV_BUTTON_RELEASE ||
										signal == PREV_BUTTON_MOTION)) {
			if (signal == PREV_BUTTON_PRESS)
				grab = 1;
			meas = 1;
		}
	}
	if (x >= 0 && y >= 0 && x < buf->width && y < buf->height) {
		int radius = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(iwd->radius));
		float zoom = prev_get_zoom (buf);
		int xz = x/zoom + buf->x;
		int yz = y/zoom + buf->y;

		label_set (iwd->labXY[0][0], xz);
		label_set (iwd->labXY[0][1], yz);
		if (grab) {
			iwd->grab_x = xz;
			iwd->grab_y = yz;
			label_grab_create (iwd);
			label_set (iwd->labXY[1][0], xz);
			label_set (iwd->labXY[1][1], yz);
		}

		page = gtk_notebook_get_current_page (GTK_NOTEBOOK(iwd->notebook));

		/* Display page */
		if (page == 0 || grab) {
			int r = 0, g = 0, b = 0;
			guchar c1,c2,c3, *pos;
			int sx, sy, cnt = 0;
			for (sy=y-radius; sy<=y+radius; sy++) {
				for (sx=x-radius; sx<=x+radius; sx++) {
					if (sx < 0 || sy < 0 || sx >= buf->width || sy >= buf->height)
						continue;

					pos = buf->buffer + buf->width*sy*planes + sx*planes;
					r += *pos;
					if (planes > 1) {
						g += *(pos+1);
						b += *(pos+2);
					}
					cnt++;
				}
			}
			if (planes == 1) g = b = r;
			r /= cnt;
			g /= cnt;
			b /= cnt;
			labels_set (iwd->labRGB, r, g, b, grab);

			prev_rgbToHsv (r,g,b, &c1,&c2,&c3);
			labels_set (iwd->labHSV, c1, c2, c3, grab);

			prev_rgbToYuvVis (r,g,b, &c1,&c2,&c3);
			labels_set (iwd->labYUV, c1, c2, c3, grab);
		}

		/* Original page */
		if (page == 1 || grab) {
			char *orig;

			gdk_threads_leave();
			orig = prev_rdata_get_info (buf, xz, yz, radius);
			gdk_threads_enter();
			if (orig) {
				gtk_label_set_text (GTK_LABEL(iwd->labOrig[0]), orig);
				if (grab)
					gtk_label_set_text (GTK_LABEL(iwd->labOrig[1]), orig);
				g_free (orig);
			} else
				gtk_label_set_text (GTK_LABEL(iwd->labOrig[0]), "N/A");
		}

		if (meas) {
			gchar str[10];
			int dx, dy;
			float angle;

			label_meas_create (iwd);
			if (signal == PREV_BUTTON_PRESS) {
				int xs, ys;

				dx = (buf->draw_width-buf->width)/2 + zoom/2;
				dy = (buf->draw_height-buf->height)/2 + zoom/2;
				xs = (iwd->meas_x1-buf->x)*zoom + dx;
				ys = (iwd->meas_y1-buf->y)*zoom + dy;

				if (iwd->meas_x1 >= 0 && ABS(xs-x) < MEAS_SIZE && ABS(ys-y) < MEAS_SIZE) {
					iwd->meas_x1 = iwd->meas_x2;
					iwd->meas_y1 = iwd->meas_y2;
				} else {
					xs = (iwd->meas_x2-buf->x)*zoom + dx;
					ys = (iwd->meas_y2-buf->y)*zoom + dy;
					if (iwd->meas_x2 < 0 || ABS(xs-x) >= MEAS_SIZE || ABS(ys-y) >= MEAS_SIZE) {
						iwd->meas_x1 = xz;
						iwd->meas_y1 = yz;
					}
				}
			}
			iwd->meas_x2 = xz;
			iwd->meas_y2 = yz;

			/* Rotate by 90 deg. and mirror on the y axis */
			dx = iwd->meas_y1-yz;
			dy = iwd->meas_x1-xz;
			g_snprintf (str, sizeof(str), "%.2f", sqrt(dx*dx + dy*dy));
			gtk_label_set_text (GTK_LABEL(iwd->labMeas[1]), str);

			if (dx == 0 && dy == 0) {
				angle = 0;
			} else {
				angle = acos(dy/sqrt(dx*dx+dy*dy))*180/G_PI;
				if (dx > 0 || (dx == 0 && dy < 0))
					angle = -angle;
				angle += 180;
			}
			g_snprintf (str, sizeof(str), "%.2f", angle);
			gtk_label_set_text (GTK_LABEL(iwd->labMeas[3]), str);

		}
		return (grab | meas)*2;
	} else {
		int i;
		for (i=0; i<3; i++) {
			gtk_label_set_text (GTK_LABEL(iwd->labRGB[0][i]), "N/A");
			gtk_label_set_text (GTK_LABEL(iwd->labHSV[0][i]), "N/A");
			gtk_label_set_text (GTK_LABEL(iwd->labYUV[0][i]), "N/A");
		}
		gtk_label_set_text (GTK_LABEL(iwd->labOrig[0]), "N/A");
		gtk_label_set_text (GTK_LABEL(iwd->labXY[0][0]), "N/A");
		gtk_label_set_text (GTK_LABEL(iwd->labXY[0][1]), "N/A");
	}
	return grab | meas;
}

static void table_add (GtkWidget *table[2], int col, char *desc, GtkWidget *lab[2][3])
{
	GtkWidget *l;
	int i, j;

	for (i=0; i<2; i++) {
		l = gtk_label_new (desc);
		gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
		gtk_table_attach (GTK_TABLE (table[i]), l, col, col+1, 0, 1,
						  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
		gtk_table_set_row_spacing (GTK_TABLE (table[i]), 0, 6);
		for (j=0; j<3; j++) {
			lab[i][j] = gtk_label_new (i==0 ? "N/A":"");
			if (i == 0)
				gtk_widget_set_usize (lab[i][j],
									  gui_text_width (lab[i][j], "888"), 0);
			gtk_misc_set_alignment (GTK_MISC(lab[i][j]), 1.0, 0.5);
			gtk_table_attach (GTK_TABLE (table[i]), lab[i][j], col, col+1, j+1, j+2,
							  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
		}
	}
}

static GtkWidget *ginfo_toggle_new_with_pix (char **xpm)
{
	GtkWidget *widget, *pixwid;
	GdkPixmap *pix;
	GdkBitmap *mask;
	GdkColormap *colormap;

	widget = gtk_toggle_button_new();
	colormap = gtk_widget_get_colormap (widget);
	pix = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL, xpm);
	pixwid = gtk_pixmap_new (pix, mask);
	gdk_pixmap_unref (pix);
	gdk_bitmap_unref (mask);
	gtk_container_add (GTK_CONTAINER(widget), pixwid);
	return widget;
}

/*********************************************************************
  Open / show a window which displays information about the cursor
  position and the color at that position.
*********************************************************************/
void ginfo_open (void)
{
	infoWinData *iwd = ginfo_iwd;
	if (!iwd) {
		GtkAdjustment *adj;
		GtkWidget *vbox_main, *vbox, *hbox, *w, *scroll;
		GdkPixmap *pix;
		GdkBitmap *mask;
		int i;

		ginfo_iwd = iwd = g_malloc0 (sizeof(infoWinData));
		iwd->grab_x = -1;
		iwd->meas_x1 = -1;
		iwd->meas_x2 = -1;
		iwd->meas_active = FALSE;
		iwd->grab_active = FALSE;

		iwd->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_signal_connect (GTK_OBJECT(iwd->window), "delete_event",
							GTK_SIGNAL_FUNC(cb_ginfo_delete), iwd);
		gtk_window_set_title (GTK_WINDOW (iwd->window), INFO_TITLE);

		vbox_main = gtk_vbox_new (FALSE, 5);
		gtk_container_set_border_width (GTK_CONTAINER (vbox_main), 5);
		gtk_container_add (GTK_CONTAINER (iwd->window), vbox_main);

		iwd->notebook = gtk_notebook_new ();
		gtk_container_set_border_width (GTK_CONTAINER (iwd->notebook), 0);
		gtk_box_pack_start (GTK_BOX (vbox_main), iwd->notebook, TRUE, TRUE, 0);

		w = gtk_label_new ("Display");
		vbox = gtk_vbox_new (FALSE, 5);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);
		gtk_notebook_append_page (GTK_NOTEBOOK(iwd->notebook), vbox, w);

		  hbox = gtk_hbox_new (FALSE, 5);
		  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

		  {
			  GtkWidget *table[2];

			  for (i=0; i<2; i++) {
				  table[i] = gtk_table_new (3, 4, FALSE);
				  gtk_table_set_col_spacings (GTK_TABLE (table[i]), 4);
				  gtk_table_set_row_spacings (GTK_TABLE (table[i]), 4);
				  gtk_container_set_border_width (GTK_CONTAINER (table[i]), 4);
			  }
			  gtk_box_pack_start (GTK_BOX(hbox), table[0], TRUE, FALSE, 0);
			  iwd->widSec[0] = gtk_vseparator_new();
			  gtk_box_pack_start (GTK_BOX(hbox), iwd->widSec[0], FALSE, FALSE, 0);
			  gtk_box_pack_start (GTK_BOX(hbox), table[1], TRUE, FALSE, 0);
			  iwd->widSec[1] = table[1];

			  table_add (table, 0, "RGB", iwd->labRGB);
			  table_add (table, 1, "HSV", iwd->labHSV);
			  table_add (table, 2, "YUV", iwd->labYUV);
		  }

		w = gtk_label_new ("Original");
		scroll = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
										GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_notebook_append_page (GTK_NOTEBOOK(iwd->notebook), scroll, w);

		vbox = gtk_vbox_new (FALSE, 5);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll),
											   vbox);
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN(scroll)->child),
									  GTK_SHADOW_NONE);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

		  iwd->labOrig[0] = gtk_label_new ("N/A");
		  gtk_label_set_selectable (GTK_LABEL(iwd->labOrig[0]), TRUE);
		  gtk_label_set_justify (GTK_LABEL(iwd->labOrig[0]), GTK_JUSTIFY_CENTER);
		  gtk_label_set_line_wrap (GTK_LABEL(iwd->labOrig[0]), TRUE);
		  gtk_box_pack_start (GTK_BOX(vbox), iwd->labOrig[0], TRUE, TRUE, 0);

		  w = gtk_hseparator_new();
		  gtk_box_pack_start (GTK_BOX(vbox), w, TRUE, TRUE, 0);

		  iwd->labOrig[1] = gtk_label_new (NULL);
		  gtk_label_set_selectable (GTK_LABEL(iwd->labOrig[1]), TRUE);
		  gtk_label_set_justify (GTK_LABEL(iwd->labOrig[1]), GTK_JUSTIFY_CENTER);
		  gtk_label_set_line_wrap (GTK_LABEL(iwd->labOrig[1]), TRUE);
		  gtk_box_pack_start (GTK_BOX(vbox), iwd->labOrig[1], TRUE, TRUE, 0);

		hbox = gtk_hbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (vbox_main), hbox, FALSE, FALSE, 0);

		for (i=0; i<2; i++) {
			GtkWidget *hbox2 = gtk_hbox_new (FALSE, 5);
			gtk_box_pack_start (GTK_BOX (hbox), hbox2, TRUE, TRUE, 0);
			if (i == 1) iwd->widSec[2] = hbox2;

			w = gtk_label_new ("XY:");
			gtk_box_pack_start (GTK_BOX(hbox2), w, FALSE, FALSE, 0);

			iwd->labXY[i][0] = gtk_label_new ("");
			gtk_widget_set_usize (iwd->labXY[i][0],
								  gui_text_width(iwd->labXY[i][0], "8888"), 0);
			gtk_box_pack_start (GTK_BOX(hbox2), iwd->labXY[i][0], TRUE, FALSE, 0);

			iwd->labXY[i][1] = gtk_label_new ("");
			gtk_widget_set_usize (iwd->labXY[i][1],
								  gui_text_width(iwd->labXY[i][1], "8888"), 0);
			gtk_box_pack_start (GTK_BOX(hbox2), iwd->labXY[i][1], TRUE, FALSE, 0);
		}

		iwd->hboxMeas = gtk_hbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (vbox_main), iwd->hboxMeas, FALSE, FALSE, 0);

		hbox = gtk_hbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (vbox_main), hbox, FALSE, FALSE, 0);

		  pix = gdk_pixmap_colormap_create_from_xpm_d (NULL, gtk_widget_get_colormap (hbox),
													   &mask, NULL, radius_xpm);
		  w = gtk_pixmap_new (pix, mask);
		  gdk_pixmap_unref (pix);
		  gdk_bitmap_unref (mask);
		  gtk_box_pack_start (GTK_BOX(hbox), w, FALSE, TRUE, 0);

		  adj = (GtkAdjustment*)gtk_adjustment_new (0.0, 0.0, 30.0, 1.0, 5.0, 0.0);
		  iwd->radius = gtk_spin_button_new (adj, 0, 0);
		  gui_entry_set_width (iwd->radius, "88", 10);
		  gui_tooltip_set (iwd->radius,
						   "Radius of the square area from which the displayed color is determined");
		  gtk_box_pack_start (GTK_BOX(hbox), iwd->radius, FALSE, TRUE, 0);

		  iwd->grab = ginfo_toggle_new_with_pix (colorPicker_xpm);
		  gtk_signal_connect (GTK_OBJECT(iwd->grab), "clicked",
							  GTK_SIGNAL_FUNC(cb_grab_clicked), iwd);
		  gui_tooltip_set (iwd->grab,
						   "Display additional values for the next mouse click in one of the image windows");
		  gtk_box_pack_start (GTK_BOX(hbox), iwd->grab, TRUE, TRUE, 0);

		  iwd->measure = ginfo_toggle_new_with_pix (measure_xpm);
		  gui_tooltip_set (iwd->measure,
						   "Measure distances and angles in the image windows");
		  gtk_signal_connect (GTK_OBJECT(iwd->measure), "clicked",
							  GTK_SIGNAL_FUNC(cb_measure_clicked), iwd);
		  gtk_box_pack_start (GTK_BOX(hbox), iwd->measure, TRUE, TRUE, 0);

		gtk_widget_show_all (vbox_main);
		for (i=0; i<SEC_CNT; i++)
			gtk_widget_hide (iwd->widSec[i]);
		gui_apply_icon (iwd->window);
		iw_session_set (iwd->window, INFO_TITLE, TRUE, TRUE);
	} else
		iw_session_set (iwd->window, INFO_TITLE, FALSE, FALSE);

	gtk_widget_show (iwd->window);
	gdk_window_raise (iwd->window->window);
}

/*********************************************************************
  If info window was open in the last session, open it again.
*********************************************************************/
void ginfo_init (void)
{
	if (iw_session_find (INFO_TITLE))
		ginfo_open();
}
