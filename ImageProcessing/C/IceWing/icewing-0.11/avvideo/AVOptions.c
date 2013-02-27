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
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "AVOptions.h"

#define OPTS_MAXIMUM	30
#define ARG_MAXIMUM		16

/* Option parsing based originally on optstr.c from transcode,
   Copyright (C) Tilmann Bitterberg 2002, GNU GPL */
static char *AVcap_optstr_lookup_do (char *haystack, char *needle, BOOL prefix)
{
	char *ch = haystack;
	int len = strlen (needle);

	if (!ch) return NULL;
	while (1) {
		ch = strstr (ch, needle);
		/* Not in string */
		if (!ch) return NULL;

		/* Do we want this hit? Ie is it exact? */
		if ((prefix || ch[len] == '\0' || ch[len] == '=' || ch[len] == AVCAP_ARGSEP) &&
			(ch == haystack || ch[-1] == AVCAP_ARGSEP || isspace((int)ch[-1]))) {
			ch += len;
			return ch;
		}
		/* Go a little further */
		ch++;
	}
}

/*********************************************************************
  Find the _exact_ needle in haystack.
  Return values: A pointer into haystack when needle is found,
                 NULL if needle is not found in haystack.
*********************************************************************/
char *AVcap_optstr_lookup (char *haystack, char *needle)
{
	return AVcap_optstr_lookup_do (haystack, needle, FALSE);
}

/*********************************************************************
  Find the first option in haystack which starts with needle.
  If postfix!=NULL: Copy the rest of option to postfix (of size pSize).
  If value!=NULL: Copy everything after '=' to value (of size vSize).
  Return: First char after needle in haystack when needle is
          found, NULL if needle is not found in haystack.
*********************************************************************/
char *AVcap_optstr_lookup_prefix (char *haystack, char *needle,
								  char *postfix, int pSize, char *value, int vSize)
{
	char *start = AVcap_optstr_lookup_do (haystack, needle, TRUE);
	if (start && (postfix || value)) {
		char *pos = start;
		int i = 0;
		while (*pos && *pos != '=' && *pos != AVCAP_ARGSEP) {
			if (postfix && i < pSize-1) postfix[i++] = *pos;
			pos++;
		}
		if (postfix) postfix[i] = '\0';
		if (value) {
			i = 0;
			if (*pos == '=') {
				pos++;
				while (i < vSize-1 && *pos && *pos != AVCAP_ARGSEP)
					value[i++] = *pos++;
			}
			value[i] = '\0';
		}
	}
	return start;
}

/*********************************************************************
  Extract values from option string.
    options: A null terminated string of options to parse, syntax is
             "opt1=val1:opt_bool:opt2=val1-val2"
              where ':' is the seperator.
    name:    The name to look for in options; eg: "opt2"
    fmt:     The format to scan values (printf format); eg: "%d-%d"
    (...):   Variables to assign; eg: &lower, &upper
  Return values:
    -1       `name' is not in `options'
     0       `name' is in `options'
    positiv  number of arguments assigned
*********************************************************************/
int AVcap_optstr_get (char *options, char *name, char *fmt, ...)
{
	va_list ap;     /* Points to each unnamed arg in turn */
	int numargs = 0;
	int n = 0;
	int pos;
	char *ch = NULL, *sep = NULL;

	ch = AVcap_optstr_lookup_do (options, name, FALSE);
	if (!ch) return -1;

	/* Find how many arguments we expect */
	n = strlen(fmt);
	for (pos=0; pos < n; pos++) {
		if (fmt[pos] == '%') {
			numargs++;
			/* Is this one quoted with '%%' */
			if (pos+1 < n && fmt[pos+1] == '%') {
				numargs--;
				pos++;
			}
		}
	}
	/* Bool argument */
	if (numargs <= 0) return 0;

	while (*ch && *ch != '=' && *ch != AVCAP_ARGSEP) ch++;
	if (*ch == '=') ch++;

	/* Stop parsing at AVCAP_ARGSEP */
	if ((sep = strchr (ch, AVCAP_ARGSEP)))
		*sep = '\0';

	va_start (ap, fmt);

#ifndef HAVE_VSSCANF
	{
		void *temp[ARG_MAXIMUM];
		if (numargs > ARG_MAXIMUM) {
			fprintf (stderr, "Internal Overflow; ARG_MAXIMUM (%d -> %d) too small\n",
					 ARG_MAXIMUM, numargs);
			return -2;
		}
		for (n=0; n<numargs; n++)
			temp[n] = va_arg(ap, void *);
		n = sscanf (ch, fmt,
					temp[0],  temp[1],  temp[2],  temp[3], temp[4],
					temp[5],  temp[6],  temp[7],  temp[8], temp[9],
					temp[10], temp[11], temp[12], temp[13], temp[14],
					temp[15]);
	}
#else
	n = vsscanf (ch, fmt, ap);
#endif
	if (n < 0) n = 0;
	va_end (ap);

	if (sep) *sep = AVCAP_ARGSEP;

	return n;
}

/*********************************************************************
  Remove any double options and unneeded spaces from 'options'.
*********************************************************************/
BOOL AVcap_optstr_cleanup (char *options)
{
	int i;
	char *start, *end, *pos = options;
	char *opts[OPTS_MAXIMUM];
	char optsEnds[OPTS_MAXIMUM];
	int optsCnt = 0;
	char old;
	BOOL changed = FALSE;

	while (*pos) {
		start = pos;
		/* Cleanup till start of next option */
		while (isspace((int)*pos) || *pos == AVCAP_ARGSEP || *pos == '=') pos++;
		if (pos != start) {
			if (*pos)
				memmove (start, pos, strlen(pos)+1);
			else
				*start = '\0';
			changed = TRUE;
		}
		pos = start;

		/* Find end of option */
		while (*pos && !isspace((int)*pos) && *pos != AVCAP_ARGSEP && *pos != '=') pos++;
		if (pos == start) break;
		old = *pos;
		*pos = '\0';

		/* Check if option already exists */
		for (i=0; i<optsCnt; i++)
			if (!strcmp(opts[i], start))
				break;
		if (i < optsCnt) {
			*pos = old;
			end = strchr (pos, AVCAP_ARGSEP);
			if (end)
				memmove (start, end+1, strlen(end+1)+1);
			else
				*start = '\0';
			changed = TRUE;
			pos = start;
		} else {
			optsEnds[optsCnt] = old;
			opts[optsCnt] = start;
			optsCnt++;
			if (optsCnt >= OPTS_MAXIMUM) break;
			if (old != ':') {
				if (old && (end = strchr (pos+1, AVCAP_ARGSEP)))
					pos = end+1;
			} else
				pos++;
		}
	}
	for (i=0; i<optsCnt; i++)
		opts[i][strlen(opts[i])] = optsEnds[i];
	if (optsCnt) {
		i = strlen(opts[optsCnt-1])-1;
		if (opts[optsCnt-1][i] == ':') {
			opts[optsCnt-1][i] = '\0';
			changed = TRUE;
		}
	}

	return changed;
}

/*********************************************************************
  Show error including errno and a string describing errno on stderr.
*********************************************************************/
void AVcap_error (char *str, ...)
{
	va_list args;
	int errno_save = errno;

	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);

	if (errno_save > 0)
		fprintf (stderr, " (%d: %s)!\n", errno_save, strerror(errno_save));
	else
		fprintf (stderr, "!\n");
}

/*********************************************************************
  Show a warning on stderr.
*********************************************************************/
void AVcap_warning (char *str, ...)
{
	va_list args;

	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);

	fprintf (stderr, "!\n");
}
