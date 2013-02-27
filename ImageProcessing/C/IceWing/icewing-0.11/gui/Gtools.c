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
#include "Gtools.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if !GTK_CHECK_VERSION(2,0,0)
#include <langinfo.h>
#endif

#include <glib.h>
#include "images/yes.xpm"

static GdkPixmap *yes_pixmap = NULL;
static GdkBitmap *yes_mask   = NULL;

static void yes_init (GtkWidget *widget)
{
	if (!yes_pixmap) {
		GdkColormap *colormap = gtk_widget_get_colormap (widget);
		yes_pixmap = gdk_pixmap_colormap_create_from_xpm_d
			(NULL, colormap, &yes_mask, NULL, yes_xpm);
	}
}

/*********************************************************************
  Show a pixmap in col/row of clist according to value.
*********************************************************************/
void gui_list_show_bool (GtkWidget *clist, int row, int col, gboolean value)
{
	yes_init (clist);
	if (value)
		gtk_clist_set_pixmap (GTK_CLIST(clist), row, col,
							  yes_pixmap, yes_mask);
	else
		gtk_clist_set_text (GTK_CLIST(clist), row, col, NULL);
}

/*********************************************************************
  Show a pixmap in column col of ctree according to value.
*********************************************************************/
void gui_tree_show_bool (GtkWidget *ctree, GtkCTreeNode *node,
						 int col, gboolean value)
{
	yes_init (ctree);
	if (value)
		gtk_ctree_node_set_pixmap (GTK_CTREE(ctree), node, col,
								   yes_pixmap, yes_mask);
	else
		gtk_ctree_node_set_text (GTK_CTREE(ctree), node, col, NULL);
}

/*********************************************************************
  Return a new button with an associated accelerator (e.g. "_OK")
  and add the accelerator key to ag.
*********************************************************************/
GtkWidget *gui_button_new_with_accel (char *label, GtkAccelGroup **ag)
{
	GtkWidget *lab, *button;
	guint key;

	lab = gtk_label_new (NULL);
	key = gtk_label_parse_uline (GTK_LABEL(lab), label);
	button = gtk_button_new();
	gtk_container_add (GTK_CONTAINER(button), lab);
	if (*ag == NULL)
		*ag = gtk_accel_group_new();
	gtk_widget_add_accelerator (button, "clicked", *ag, key, GDK_MOD1_MASK, 0);

	return button;
}

/*********************************************************************
  Get width in pixels of the string str.
*********************************************************************/
int gui_text_width (GtkWidget *widget, char *str)
{
	int width = 0;

#if GTK_CHECK_VERSION(2,0,0)
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (widget, str);
	pango_layout_get_pixel_size (layout, &width, NULL);
	g_object_unref (layout);
#else
	GtkStyle *style = gtk_widget_get_style (widget);
	if (style)
		width = gdk_text_width (style->font, str, strlen(str));
#endif

	return width;
}

/*********************************************************************
  Set width of the entry 'entry' such that 'str' is completely visible.
*********************************************************************/
void gui_entry_set_width (GtkWidget *entry, char *str, int minwidth)
{
	int w;

#if GTK_CHECK_VERSION(2,0,0)
	if (GTK_IS_SPIN_BUTTON(entry)) {
		/* FIXME minwidth */
		gtk_entry_set_width_chars (GTK_ENTRY(entry), strlen(str));
	} else {
		GtkStyle *style = gtk_widget_get_style (GTK_WIDGET (entry));
		w = gui_text_width (entry, str) + (style->xthickness + 2)*2+4;
		if (w < minwidth)
			w = minwidth;
		gtk_widget_set_usize (entry, w, 0);
	}
#else
	GtkStyle *style = gtk_widget_get_style (GTK_WIDGET (entry));
	w = gui_text_width (entry, str) + (style->klass->xthickness + 2)*2;
	if (GTK_IS_SPIN_BUTTON(entry))
		w += 17;
	if (w < minwidth)
		w = minwidth;
	gtk_widget_set_usize (entry, w, 0);
#endif
}

/*********************************************************************
  Assume that str is in UTF-8 and return a copy converted to Latin-1.
*********************************************************************/
gchar *gui_get_latin1 (const gchar *str)
{
	gchar *latin;

#if !GTK_CHECK_VERSION(2,0,0)
	static int is_utf8 = -1;

	if (is_utf8 < 0)
		is_utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") ? 0:1;
	if (!is_utf8) {
		latin = g_strdup (str);
	} else
#endif
	{
		int li, i, length;

		length = strlen (str);
		latin = g_malloc (length+1);
		li = 0;
		for (i = 0; i < length; i++) {
			if ((guchar)str[i] <= 0x7f) {
				latin[li++] = str[i];
			} else if ((guchar)str[i] <= 0xc3) {
				latin[li++] = (gchar)(((guchar)str[i] & 0x3) << 6 | ((guchar)str[i+1] & 0x3f));
				i++;
			} else {
				static const char skip[] = {1,2,2,2,2,3,3,4,5};
				latin[li++] = ' ';
				i += skip[(((guchar)str[i] & 0x3c) >> 2) - 7];
			}
		}
		latin[li] = '\0';
	}
	return latin;
}

/*********************************************************************
  If str is valid UTF-8, return a copy, otherwise return a copy
  converted from the current locale (if !=UTF8) or from ISO-8859-1
  to UTF-8.
*********************************************************************/
gchar* gui_get_utf8 (gchar *str)
{
#if GTK_CHECK_VERSION(2,0,0)
	if (!g_utf8_validate (str, -1, NULL)) {
		static const char *charset = NULL;
		if (!charset) {
			if (g_get_charset (&charset))
				charset = "ISO-8859-1";
		}
		return g_convert (str, -1, "UTF-8", charset, NULL, NULL, NULL);
	}
#endif
	return g_strdup (str);
}

static void convert_prg_rc (char *rcfile, int size, gboolean home)
{
	char *prg_name = GUI_PRG_NAME;
	int i = 0;

	if (home) {
		gui_strlcpy (rcfile, getenv("HOME"), size-3);
		strcat (rcfile, "/");
		i = strlen(rcfile);
	}
	rcfile[i++] = '.';
	for (; prg_name && *prg_name &&
			 *prg_name!=' ' && *prg_name!='\t' && i < size-2; prg_name++) {
		switch (*prg_name) {
			case '/':
			case '"':
			case '\'':
				break;
			default:
				rcfile[i++] = tolower ((int)*prg_name);
		}
	}
	if (home)
		rcfile[i++] = '/';
	rcfile[i] = '\0';
}

/*********************************************************************
  Return '$HOME"/.programName/"extension' (home == TRUE) or
  '".programName"extension' (home == FALSE) in rcfile.
  rcfile must have a length of at least PATH_MAX.
*********************************************************************/
void gui_convert_prg_rc (char *rcfile, char *extension, gboolean home)
{
	int extlen = extension ? strlen(extension):0;
	static gboolean initialized = FALSE;

	/* Check if there is already a config directory */
	if (!initialized) {
		struct stat statbuf;

		convert_prg_rc (rcfile, PATH_MAX, TRUE);
		if (stat(rcfile, &statbuf) == -1)
			mkdir (rcfile, 0755);
		initialized = TRUE;
	}

	convert_prg_rc (rcfile, PATH_MAX - extlen, home);
	if (extension) strcat (rcfile, extension);
}

static char skip_space (const char **line)
{
	if (!*line) return '\0';

	while (**line==' ' || **line=='\t')
		(*line)++;
	return **line;
}

/*********************************************************************
  Parse line in id and value according to
  line = ['"'] id ['"'] '=' value.
*********************************************************************/
gboolean gui_parse_line (char *line, char **id, char **value)
{
	char *end;

	if (skip_space((const char**)&line) == '#' || !line || !*line) return FALSE;

	if (*line == '"') {
		*id = ++line;
		while (*line != '"') {
			if (!*line) return FALSE;
			line++;
		}
		end = line;
		while (*line != '=') {
			if (!*line) return FALSE;
			line++;
		}
	} else {
		*id = line;
		while (*line != '=') {
			if (!*line) return FALSE;
			line++;
		}
		end = line;
		while (*(end-1)==' ' || *(end-1)=='\t') end--;
	}
	line++;
	skip_space((const char **)&line);

	*end = '\0';
	*value = line;
	return TRUE;
}

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
int gui_sscanf (const char *str, const char *format, ...)
{
	int nargs = 0, *iarg, read, bufsize;
	const char *pos;
	char *sarg, **aarg, delim;
	va_list args;

	va_start (args, format);

	pos = str;
	nargs = 0;
	while (*format) {
		skip_space (&pos);
		if (!pos) return nargs;

		if (*format == '%' && *(format+1) != '%') {
			format++;

			bufsize = 0;
			while (isdigit((int)*format)) {
				bufsize = 10*bufsize + (*format - '0');
				format++;
			}
			switch (*format) {
				case 'd':
					/* Read ints */
					iarg = va_arg (args, int*);
					if (sscanf (pos, "%d%n", iarg, &read) != 1)
						return nargs;
					pos += read;
					nargs++;
					break;
				case 'a':
				case 's':
					/* Read strings,
					   '\'', '"', *(format+1), or ' ' - delimited */
					delim = ' ';
					if (*pos == '"' || *pos == '\'') {
						delim = *pos;
						pos++;
					} else if (*(format+1) && *(format+1) != '%')
						delim = *(format+1);
					read = 0;

					/* Allocate enough space for the string */
					if (bufsize == 0) {
						const char *p = pos;
						while (*p && *p != delim) {
							if (*p == '\\' && (*(p+1) == delim || *(p+1) == '\\'))
								p++;
							p++;
							bufsize++;
						}
						bufsize++;
					}
					if (*format == 'a') {
						aarg = va_arg (args, char**);
						*aarg = g_malloc (bufsize);
						sarg = *aarg;
					} else
						sarg = va_arg (args, char*);
					/* Read the string */
					while (read < bufsize && *pos && *pos != delim) {
						if (*pos == '\\' && (*(pos+1) == delim || *(pos+1) == '\\'))
							pos++;
						*sarg++ = *pos++;
						read++;
					}
					if (delim == '"' || delim == '\'')
						pos++;
					*sarg = '\0';
					nargs++;
					break;
				case 'n':
					/* Return number of read chars */
					iarg = va_arg (args, int*);
					*iarg = pos-str;
					nargs++;
					break;
				default:
					return nargs;
			}
			format++;
		} else {
			/* Compare with part of the format string */
			while (isspace((int)*format))
				format++;
			while (*format && (*format != '%' || *(format+1) == '%')) {
				if (*format != *pos)
					return nargs;
				format++;
				pos++;
			}
		}
	}

	va_end (args);
	return nargs;
}

/*********************************************************************
  PRIVAT: Read one line from file, stopping at EOF or '\n'. If '\n'
  is read, replace it with '\0'. If the line is longer than size, s
  gets reallocated.
*********************************************************************/
char *gui_fgets (char **s, int *size, FILE *file)
{
	char *ret;
	int slast, pos;

	pos = 0;
	if (!*s || *size <= 0) {
		*size = 500;
		*s = g_malloc (*size);
		**s = '\0';
	}
	do {
		ret = fgets (&(*s)[pos], *size-pos, file);
		if (ret) {
			slast = strlen(*s) - 1;
			if ((*s)[slast] == '\n') {
				(*s)[slast] = '\0';
				return *s;
			} else if (feof(file)) {
				return *s;
			} else {
				pos = *size-1;
				*size += 500;
				*s = g_realloc (*s, *size);
			}
		}
	} while (ret);
	return NULL;
}

/*********************************************************************
  PRIVAT: Search in format for any sprintf()-specifiers ending with
  one of the conversion specifiers from imod or smod, replace them
  with the result of sprintf with "%d",ivar[imod-found] or
  "%s",svar[smod-found] and write the complete result to buf.
  Returned is the number of printed characters.
*********************************************************************/
int gui_sprintf (char *buf, int buflen, const char *format,
				 const char *imod, int *ivar, const char *smod, char **svar)
{
	int cnt = -1;
	char *bpos, *fpos, *fpos2, *mod;
	char *format2, ch;

	if (!format || !buf) return -1;

	format2 = strdup (format);

	fpos = format2;
	bpos = buf;
	while (*fpos) {
		if (bpos >= buf+buflen-1) break;

		if (*fpos == '%' && *(fpos+1) == '%') {
			*bpos++ = '%';
			fpos += 2;
		} else if (*fpos == '%') {
			fpos2 = fpos;
			while (strchr ("%0123456789#- +'.", *fpos2))
				fpos2++;
			if (imod && *imod && (mod = strchr (imod, *fpos2))) {
				*fpos2 = 'd';
				ch = *(fpos2+1);
				*(fpos2+1) = '\0';
				cnt = g_snprintf (bpos, buf+buflen-bpos, fpos, ivar[mod-imod]);
				*(fpos2+1) = ch;
				bpos += cnt;
				fpos = fpos2+1;
			} else if (smod && *smod && (mod=strchr (smod, *fpos2))) {
				*fpos2 = 's';
				ch = *(fpos2+1);
				*(fpos2+1) = '\0';
				cnt = g_snprintf (bpos, buf+buflen-bpos, fpos, svar[mod-smod]);
				*(fpos2+1) = ch;
				bpos += cnt;
				fpos = fpos2+1;
			} else {
				if (buf+buflen-bpos < fpos2-fpos)
					cnt = buf+buflen-bpos;
				else
					cnt = fpos2-fpos;
				memcpy (bpos, fpos, cnt);
				bpos += cnt;
				fpos = fpos2;
			}
		} else {
			*bpos++ = *fpos;
			fpos++;
		}
	}
	*bpos = '\0';
	free (format2);
	return bpos-buf;
}

/*********************************************************************
  Copy string src to buffer dest (of buffer size dest_size). Always
  NULL terminate dest (unless dest_size == 0).
  Return size of attempted result, strlen(src),
  so if retval >= dest_size, truncation occurred.
*********************************************************************/
gsize gui_strlcpy (gchar *dest, const gchar *src, gsize dest_size)
{
	register gchar *d = dest;
	register const gchar *s = src;
	register gsize n = dest_size;

	g_return_val_if_fail (dest != NULL, 0);
	g_return_val_if_fail (src  != NULL, 0);

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			register gchar c = *s++;

			*d++ = c;
			if (c == 0)
				break;
		} while (--n != 0);
	}
	/* If not enough room in dest, add 0 and traverse rest of src */
	if (n == 0) {
		if (dest_size != 0)
			*d = 0;
		while (*s++) ;
	}

	return s - src - 1;	 /* Count does not include 0 */
}

/*********************************************************************
  Copy string src to buffer dest (of buffer size dest_size) in reverse
  order (i.e. start from the end).
*********************************************************************/
void gui_strcpy_back (gchar *dest, const gchar *src, gsize dest_size)
{
	int length, i;

	length = strlen (src);
	if (dest_size > 0 && length >= dest_size)
		length = dest_size-1;
	dest[length] = '\0';
	for (i=length-1; i>=0; i--)
		dest[i] = src[i];
}

/*********************************************************************
  Return TRUE if pattern matches str. Supported are '?' (match exactly
  one character) and '*' (match any number of characters).
*********************************************************************/
gboolean gui_pattern_match (const char *pattern, const char *str)
{
	char p;

	while (1) {
		p = *pattern++;
		switch (p) {
			case '\0':
				return (*str == '\0');
			case '*':
				while (*pattern == '*')
					pattern++;
				if (*pattern == '\0')
					return TRUE;

				while (*str) {
					if (gui_pattern_match (pattern, str))
						return TRUE;
					str++;
				}
				return FALSE;
				break;
			case '?':
				if (*str == '\0')
					return FALSE;
				str++;
				break;
			default:
				if (p != *str)
					return FALSE;
				str++;
		}
	}
}

/* Number of buffers for gui_get_buffer() */
#define BUF_MAX			10
static pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;
static int buf_number = 0;
static int buf_size[BUF_MAX] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static void *buf_mem[BUF_MAX] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/*********************************************************************
  Return a pointer to an internal intermediate buffer. If the buffer
  is smaller than size bytes, the buffer is reallocated.
  Calls to gui_get_buffer() can be nested. gui_release_buffer() must
  be called if the buffer is not needed any more.
*********************************************************************/
void* gui_get_buffer (int size)
{
	int num;

	pthread_mutex_lock (&buf_mutex);

	num = buf_number;
	while (buf_size[num] < 0) {
		num = (num + 1) % BUF_MAX;
		if (num == buf_number) {
			pthread_mutex_unlock (&buf_mutex);
			return NULL;
		}
	}
	if (size > buf_size[num]) {
		buf_mem[num] = g_realloc (buf_mem[num], size);
		buf_size[num] = size;
	}
	buf_size[num] = -buf_size[num];
	buf_number = (num + 1) % BUF_MAX;

	pthread_mutex_unlock (&buf_mutex);
	return buf_mem[num];
}

/*********************************************************************
  Release the buffer requested with gui_get_buffer().
*********************************************************************/
void gui_release_buffer (void *mem)
{
	pthread_mutex_lock (&buf_mutex);

	buf_number--;
	while (buf_mem[buf_number] != mem)
		buf_number = (buf_number + 1) % BUF_MAX;
	buf_size[buf_number] = -buf_size[buf_number];

	pthread_mutex_unlock (&buf_mutex);
}
