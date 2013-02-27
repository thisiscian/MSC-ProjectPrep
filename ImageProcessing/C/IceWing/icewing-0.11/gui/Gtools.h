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

#ifndef iw_Gtools_H
#define iw_Gtools_H

#ifndef ISOC9X_SOURCE
#  define _ISOC9X_SOURCE
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>

extern char *GUI_PRG_NAME;
extern char *GUI_VERSION;

#undef iw_isequal
#undef iw_isequalf
#undef iw_iszero
#undef iw_iszerof
#define iw_isequal(a,b)		(fabs((a)-(b)) < 0.00001)
#define iw_isequalf(a,b)	(fabsf((a)-(b)) < 0.00001)
#define iw_iszero(a)		(fabs(a) < 0.00001)
#define iw_iszerof(a)		(fabsf(a) < 0.00001)

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef  CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifndef gui_lrint

#if defined(__GNUC__) && defined(__i386__)
static inline long __gui_lrint_code (double v)
{
	long ret;
	__asm__ __volatile__ ("fistpl %0" : "=m" (ret) : "t" (v) : "st") ;
	return ret;
}
#elif defined(__GLIBC__) && defined(__GNUC__) && __GNUC__ >= 3
#define __gui_lrint_code(v)	lrint(v)
#else
static inline long __gui_lrint_code (double v)
{
	if (v > 0)
		return (long)(v+0.5);
	else
		return (long)(v-0.5);
}
#endif

#  define gui_lrint(v)	__gui_lrint_code(v)

#endif

#ifndef GTK_CHECK_VERSION
#define GTK_CHECK_VERSION(a,b,c) 0
#endif
#if !GTK_CHECK_VERSION(2,0,0)
#define g_locale_to_utf8(s, l, r, w, e) g_strdup(s)
#define gdk_threads_init()
#define gtk_get_current_event_time() GDK_CURRENT_TIME
#define gtk_widget_set_double_buffered(w, b)
#define gtk_label_set_selectable(w, s)
#define gdk_pixbuf_new_from_file(n,e) gdk_pixbuf_new_from_file(n)
#define gtk_window_resize(win, x, y) gdk_window_resize(GTK_WIDGET(win)->window, x, y)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Show a pixmap in col/row of clist according to value.
*********************************************************************/
void gui_list_show_bool (GtkWidget *clist, int row, int col, gboolean value);

/*********************************************************************
  Show a pixmap in column col of ctree according to value.
*********************************************************************/
void gui_tree_show_bool (GtkWidget *ctree, GtkCTreeNode *node,
						 int col, gboolean value);

/*********************************************************************
  Return a new button with an associated accelerator (e.g. "_OK")
  and add the accelerator key to ag.
*********************************************************************/
GtkWidget *gui_button_new_with_accel (char *label, GtkAccelGroup **ag);

/*********************************************************************
  Get width in pixels of the string str.
*********************************************************************/
int gui_text_width (GtkWidget *widget, char *str);

/*********************************************************************
  Set width of the entry 'entry' such that 'str' is completely visible.
*********************************************************************/
void gui_entry_set_width (GtkWidget *entry, char *str, int minwidth);

/*********************************************************************
  Assume that str is in UTF-8 and return a copy converted to Latin-1.
*********************************************************************/
gchar *gui_get_latin1 (const gchar *str);

/*********************************************************************
  If str is valid UTF-8, return a copy, otherwise return a copy
  converted from the current locale (if !=UTF8) or from ISO-8859-1
  to UTF-8.
*********************************************************************/
gchar* gui_get_utf8 (gchar *str);

/*********************************************************************
  Return '$HOME"/.programName/"extension' (home == TRUE) or
  '".programName"extension' (home == FALSE) in rcfile.
  rcfile must have a length of at least PATH_MAX.
*********************************************************************/
void gui_convert_prg_rc (char *rcfile, char *extension, gboolean home);

/*********************************************************************
  Parse line in id and value according to
  line = ['"'] id ['"'] '=' value.
*********************************************************************/
gboolean gui_parse_line (char *line, char **id, char **value);

/*********************************************************************
  PRIVAT: Scan in str for values specified in format and return number
  of read values (every processed %x).
  supported format specifier:
    %d   : int
    %[n]s: char array of length n (if n not given, unlimited is
            assumed) (delimiter: '\'', '"', *(format+1), or ' ')
    %[n]a: *char, buffer is allocated (length n or needed length)
    %n   : Number of chars read from str
    other: Match with chars in str, return if different
*********************************************************************/
int gui_sscanf (const char *str, const char *format, ...);

/*********************************************************************
  PRIVAT: Read one line from file, stopping at EOF or '\n'. If '\n'
  is read, replace it with '\0'. If the line is longer than size, s
  gets reallocated.
*********************************************************************/
char *gui_fgets (char **s, int *size, FILE *file);

/*********************************************************************
  PRIVAT: Search in format for any sprintf()-specifiers ending with
  one of the conversion specifiers from imod or smod, replace them
  with the result of sprintf with "%d",ivar[imod-found] or
  "%s",svar[smod-found] and write the complete result to buf.
  Returned is the number of printed characters.
*********************************************************************/
int gui_sprintf (char *buf, int buflen, const char *format,
				 const char *imod, int *ivar, const char *smod, char **svar);

/*********************************************************************
  Copy string src to buffer dest (of buffer size dest_size). Always
  NULL terminate dest (unless dest_size == 0).
  Return size of attempted result, strlen(src),
  so if retval >= dest_size, truncation occurred.
*********************************************************************/
gsize gui_strlcpy (gchar *dest, const gchar *src, gsize dest_size);

/*********************************************************************
  Copy string src to buffer dest (of buffer size dest_size) in reverse
  order (i.e. start from the end).
*********************************************************************/
void gui_strcpy_back (gchar *dest, const gchar *src, gsize dest_size);

/*********************************************************************
  Return TRUE if pattern matches str. Supported are '?' (match exactly
  one character) and '*' (match any number of characters).
*********************************************************************/
gboolean gui_pattern_match (const char *pattern, const char *str);

/*********************************************************************
  Return a pointer to an internal intermediate buffer. If the buffer
  is smaller than size bytes, the buffer is reallocated.
  Calls to gui_get_buffer() can be nested. gui_release_buffer() must
  be called if the buffer is not needed any more.
*********************************************************************/
void* gui_get_buffer (int size);

/*********************************************************************
  Release the buffer requested with gui_get_buffer().
*********************************************************************/
void gui_release_buffer (void *mem);

#ifdef __cplusplus
}
#endif

#endif /* iw_Gtools_H */
