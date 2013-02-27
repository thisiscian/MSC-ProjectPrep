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

#ifndef iw_AVOptions_H
#define iw_AVOptions_H

#include "tools/tools.h"

#define AVCAP_ARGSEP			':'

/* Option parsing originally from optstr.c from transcode,
   Copyright (C) Tilmann Bitterberg 2002, GNU GPL */

/*********************************************************************
  Find the _exact_ needle in haystack.
  Return values: A pointer into haystack when needle is found,
                 NULL if needle is not found in haystack.
*********************************************************************/
char *AVcap_optstr_lookup (char *haystack, char *needle);

/*********************************************************************
  Find the first option in haystack which starts with needle.
  If postfix!=NULL: Copy the rest of option to postfix (of size pSize).
  If value!=NULL: Copy everything after '=' to value (of size vSize).
  Return: First char after needle in haystack when needle is
          found, NULL if needle is not found in haystack.
*********************************************************************/
char *AVcap_optstr_lookup_prefix (char *haystack, char *needle,
								  char *postfix, int pSize, char *value, int vSize);

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
int AVcap_optstr_get (char *options, char *name, char *fmt, ...) G_GNUC_SCANF(3, 4);

/*********************************************************************
  Remove any double options and unneeded spaces from 'options'.
*********************************************************************/
BOOL AVcap_optstr_cleanup (char *options);

/*********************************************************************
  Return a string describing the grabbing command line "-sg".
*********************************************************************/
char *AVDriverHelp (void);

/*********************************************************************
  Show error including errno and a string describing errno on stderr.
*********************************************************************/
void AVcap_error (char *str, ...) G_GNUC_PRINTF(1, 2);

/*********************************************************************
  Show a warning on stderr.
*********************************************************************/
void AVcap_warning (char *str, ...) G_GNUC_PRINTF(1, 2);

#endif /* iw_AVOptions_H */
