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
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_WORDEXP
#include <wordexp.h>
#endif

#if defined(HAVE_MEMALIGN)
#  include <malloc.h>
#elif defined(HAVE_MMMALLOC)
#  include <mm_malloc.h>
#endif

#if defined(__linux__) && defined(IW_DEBUG)
#include <linux/unistd.h>
static pid_t gettid (void)
{
#ifdef __NR_gettid
	return syscall(__NR_gettid);
#else
	return -1;
#endif
}
#endif

#include "gui/Gtools.h"
#include "gui/Ggui.h"
#include "tools.h"

/*********************************************************************
  Call malloc() / calloc() / realloc().
  If err!=NULL and memory could not be allocated, exit with
  'out of memory: "err"' by calling iw_error().
*********************************************************************/
void* iw_malloc (size_t size, const char *err)
{
	void *ptr = malloc (size);
	if (!ptr && err) iw_error ("Out of memory: %s", err);
	return ptr;
}
void* iw_malloc0 (size_t size, const char *err)
{
	void *ptr = calloc (1, size);
	if (!ptr && err) iw_error ("Out of memory: %s", err);
	return ptr;
}
void* iw_realloc (void *ptr, size_t size, const char *err)
{
	ptr = realloc (ptr, size);
	if (!ptr && err) iw_error ("Out of memory: %s", err);
	return ptr;
}

/*********************************************************************
  Allocate 16 byte aligned memory. Must be freed with iw_free_align().
  If err!=NULL and memory could not be allocated, exit with
  'out of memory: "err"' by calling iw_error().
*********************************************************************/
void* iw_malloc_align (size_t size, const char *err)
{
#define IW_ALIGN		16
	void *ptr;

#if defined(HAVE_MEMALIGN)
	ptr = memalign (IW_ALIGN, size);
#elif defined(HAVE_MMMALLOC)
	ptr = _mm_malloc (size, IW_ALIGN);
#else
	ptr = malloc (size+IW_ALIGN+1);
	if (ptr) {
		size_t diff = ((-((size_t)ptr+1) - 1)&(IW_ALIGN-1)) + 2;
		ptr += diff;
		((char*)ptr)[-1] = diff;
	}
#endif

	if (!ptr && err) iw_error ("Out of memory: %s", err);
	return ptr;
}

void iw_free_align (void *ptr)
{
	if (ptr) {
#if defined(HAVE_MEMALIGN)
		free (ptr);
#elif defined(HAVE_MMMALLOC)
		_mm_free (ptr);
#else
		free (ptr - ((char*)ptr)[-1]);
#endif
	}
}

static inline BOOL skip_comment (char **pos, BOOL rmcom)
{
	if (!rmcom || **pos != '#') return FALSE;

	/* Comment */
	while (**pos && **pos != '\n')
		(*pos)++;
	return TRUE;
}

/*********************************************************************
  Return in 'array' a NULL terminated array of strings pointing to
  the parts of 'str', which are separated by spaces. In 'str' '\0'
  are inserted. In 'array_len' the number of entries in 'array' is
  returned.
*********************************************************************/
static void string_split_sep (char *str, int *array_len, char ***array,
							  BOOL rmsep, BOOL rmcom)
{
	char *pos = str, *act, delim;
	int len = 0;

	/* Get number of arguments */
	while (pos && *pos) {
		skip_comment (&pos, rmcom);

		if (!isspace((int)*pos) && (pos == str || isspace((int)*(pos-1)))) {
			/* New start of an argument */
			len++;
		}

		/* Check for '"' and '\'' */
		if (*pos == '\\' && (*(pos+1) == '"' || *(pos+1) == '\'' || *(pos+1) == '#' || *(pos+1) == '\\')) {
			pos++;
		} else if (*pos == '"' || *pos == '\'') {
			delim = *pos;
			do {
				if (*pos == '\\' && (*(pos+1) == delim || *(pos+1) == '\\'))
					pos++;
				pos++;
			} while (*pos && *pos != delim);
		}
		if (*pos) pos++;
	}
	*array = malloc (sizeof(char*)*(len+1));
	*array_len = len;

	len = 0;
	act = pos = str;

	/* Copy arguments to array, remove any '"' if rmsep */
#define SEPCONT {if (rmsep) pos++; else *act++ = *pos++;}

	while (pos && *pos) {
		if (!isspace((int)*pos) && *pos != '#' && (pos == str || !*(pos-1) || isspace((int)*(pos-1)))) {
			/* New start of an argument */
			(*array)[len++] = act;
		}

		/* Check for '"' and '\'' */
		if (*pos == '\\' && (*(pos+1) == '"' || *(pos+1) == '\'' || *(pos+1) == '#' || *(pos+1) == '\\')) {
			SEPCONT;
		} else if (*pos == '"' || *pos == '\'') {
			delim = *pos;
			SEPCONT;
			while (*pos && *pos != delim) {
				if (*pos == '\\' && (*(pos+1) == delim || *(pos+1) == '\\'))
					SEPCONT;
				*act++ = *pos++;
			}
			if (*pos) SEPCONT;
		}

		/* Skip comments.
		   End of an argument? */
		if (skip_comment (&pos, rmcom) ||
			(pos>str && !isspace((int)*(pos-1)) && isspace((int)*pos))) {
			*pos = '\0';
			*act++ = *pos++;
		} else {
			*act = *pos;
			if (*pos) {
				act++;
				pos++;
			}
		}
	}
	if (pos)
		*act = *pos;
	(*array)[len] = NULL;
#undef SEPCONT
}

void iw_string_split (char *str, int *array_len, char ***array)
{
	string_split_sep (str, array_len, array, TRUE, FALSE);
}

/*********************************************************************
  Perform wordexp() on all args from argv. Return a new copy of all
  arguments in argv. argv itselve is only reallocated.
*********************************************************************/
static void load_args_exp (int *argc, char ***argv)
{
	int i, j;
#ifdef HAVE_WORDEXP
	wordexp_t expand;
	int ac, ret, flags = 0;
	char **av;

	expand.we_wordc = 0;
	expand.we_wordv = NULL;
	expand.we_offs = 0;
	ac = *argc;
	av = *argv;
	for (i=0; i<ac; i++) {
		ret = wordexp (av[i], &expand, flags);
		if (ret != 0) {
			/* On error the memory can be freed without problems
			   only if WRDE_NOSPACE, otherwise a SegV may happen. */
			if (ret == WRDE_NOSPACE)
				wordfree (&expand);
			expand.we_wordc = 0;
			break;
		}
		flags |= WRDE_APPEND;
	}
	if (expand.we_wordc > 0) {
		ac = expand.we_wordc;
		av = realloc (av, sizeof(char*) * ac);
		for (i=0, j=0; i<ac; i++, j++) {
			av[j] = g_locale_to_utf8 (expand.we_wordv[i], -1,
									  NULL, NULL, NULL);
			if (!av[j])
				j--;
		}
		wordfree (&expand);
		*argc = ac +j-i;
		*argv = av;
		return;
	}
#endif
	/* If wordexp() failed, copy args and remove any quotation marks */
	for (i=0, j=0; i<*argc; i++, j++) {
		int len = strlen((*argv)[i]);
		if (((*argv)[i][0] == '"' || (*argv)[i][0] == '\'') &&
			(*argv)[i][len-1] == (*argv)[i][0]) {
			char *d = malloc (len-1);
			memcpy (d, &((*argv)[i][1]), len-2);
			d[len-2] = '\0';
			(*argv)[j] = g_locale_to_utf8 (d, -1, NULL, NULL, NULL);
			free (d);
		} else
			(*argv)[j] = g_locale_to_utf8 ((*argv)[i], -1,
										   NULL, NULL, NULL);
		if (!(*argv)[j])
			j--;
	}
	*argc = *argc +j-i;
}

typedef struct argsMem {
	char **buf;
	int b_len, b_cnt;
} argsMem;

/*********************************************************************
  PRIVATE: If args, split args, otherwise use argv. Check for any
  '@file', load the file, exchange '@file' with the args from the
  file, and expand any vars, commands, and wildcards. nargv is newly
  allocated.
*********************************************************************/
static BOOL load_args_do (int deep, int argc, char **argv,
						  int *nargc, char ***nargv, BOOL rmsep,
						  BOOL exp_all, argsMem *mem)
{
	int i, j, nc, bufc, splitc;
	struct stat stbuf;
	char *buf, **nv, **bufv, **splitv;
	char file[PATH_MAX];
	FILE *fp;

	if (deep > 100) {
		fprintf (stderr, "Seems there is a recursive argument file include?\n"
				 "This is not allowed!\n");
		return FALSE;
	}

	nc = argc;
	nv = malloc (sizeof(char*) * nc);
	memcpy (nv, argv, sizeof(char*) * nc);

	/* Init return values (in case of an error) */
	*nargc = nc;
	*nargv = nv;

	for (i=0; i<nc; i++) {
		/* Check for '@file' */
		file[PATH_MAX-1] = '\0';
		if (nv[i][0] == '@') {
			strncpy (file, &nv[i][1], PATH_MAX-1);
		} else if (!rmsep && (nv[i][0] == '"' || nv[i][0] == '\'') && nv[i][1] == '@') {
			j = strlen(nv[i]);
			if (nv[i][j-1] == nv[i][0])
				strncpy (file, &nv[i][2], MIN(PATH_MAX-1, j-3));
			else
				continue;
		} else
			continue;

		if (stat(file, &stbuf) != 0) {
			fprintf (stderr, "Unable to open argument file '%s'!\n", file);
			return FALSE;
		}
		/* Sanity check */
		if (stbuf.st_size > 4000000) {
			fprintf (stderr, "Argument files bigger than 4000000 bytes are not allowed\n"
					 "('%s' has a size of %ld)!\n", file, (long)stbuf.st_size);
			return FALSE;
		}
		if (!(fp=fopen(file, "r"))) {
			fprintf (stderr, "Unable to open argument file '%s'!\n", file);
			return FALSE;
		}

		/* Read file from '@file' */
		if (mem->b_cnt >= mem->b_len) {
			mem->b_len += 10;
			mem->buf = realloc (mem->buf, sizeof(char*)*mem->b_len);
		}
		buf = mem->buf[mem->b_cnt++] = malloc (stbuf.st_size+1);
		buf[stbuf.st_size] = '\0';
		if (fread (buf, 1, stbuf.st_size, fp) != stbuf.st_size) {
			fprintf (stderr, "Unable to read from argument file '%s'!\n", file);
			fclose (fp);
			return FALSE;
		}
		fclose (fp);

		/* Exchange '@file' with the args from the file */
		string_split_sep (buf, &splitc, &splitv, FALSE, TRUE);
		if (!load_args_do (deep+1, splitc, splitv, &bufc, &bufv, rmsep, exp_all, mem))
			return FALSE;
		if (deep == 0 && !exp_all)
			load_args_exp (&bufc, &bufv);

		free (splitv);

		nv = realloc (nv, sizeof(char*) * (bufc+nc-1));
		for (j=nc-1; j>i; j--)
			nv[j+bufc-1] = nv[j];
		for (j=0; j<bufc; j++)
			nv[i+j] = bufv[j];
		nc += bufc-1;
		free (bufv);

		*nargc = nc;
		*nargv = nv;

		i--;
	}

	return TRUE;
}
BOOL iw_load_args (char *args, int argc, char **argv, int *nargc, char ***nargv)
{
	int i, splitc;
	char **splitv;
	BOOL ret, rmsep = TRUE;
	argsMem mem;

#ifdef HAVE_WORDEXP
	rmsep = FALSE;
#endif

	if (args) {
		string_split_sep (args, &splitc, &splitv, rmsep, FALSE);
	} else {
		splitc = argc;
		splitv = argv;
	}
	mem.buf = NULL;
	mem.b_len = mem.b_cnt = 0;
	ret = load_args_do (0, splitc, splitv, nargc, nargv, rmsep,
						args!=NULL, &mem);
	if (args) {
		free (splitv);
		load_args_exp (nargc, nargv);
	}
	if (mem.buf) {
		for (i=0; i<mem.b_cnt; i++)
			free (mem.buf[i]);
		free (mem.buf);
	}
	return ret;
}

/*********************************************************************
  Checks if argv[nr] can be found in pattern, the check is case
  insensitive. Increment nr afterwards. Format of pattern in EBNF:
     { "-" token ":" ch ["r"|"ro"|"i"|"io"|"f"|"fo"|"c"] " " }
     token: string without " " and without ":"
     ch   : Any character, given back if -token is found
     "r"  : Argument is required -> returned in *arg
     "i"  : Int argument is required -> returned in *arg
     "f"  : Float argument is required -> returned in *arg
     "c"  : Token can be continued in any way -> returned in *arg
     "o"  : Argument is optional
  Example: "-i:Ii -s:Sr -h:H -help:H --help:H"
  On error, print an error message on stderr and return '\0'.
*********************************************************************/
char iw_parse_args (int argc, char **argv, int *nr, void **arg,
					const char *pattern)
{
	const char *pos, *pat;
	char a_up[21], *pattern_up;
	int len, part;
	BOOL argfound;

	if (arg) *arg = NULL;

	/* Convert pattern arguments to upper case */
	pattern_up = strdup (pattern);
	for (len=0; pattern_up[len]; len++)
		pattern_up[len] = toupper((int)pattern_up[len]);

	/* Convert current arg to upper case */
	for (len=0; len<20 && argv[*nr][len]; len++)
		a_up[len] = toupper((int)argv[*nr][len]);
	a_up[len] = '\0';
	(*nr)++;

	/* Search for complete arg in pattern_up */
	pat = pattern_up;
	part = len;
	argfound = FALSE;
	while ((pos = strstr(pat, a_up))) {
		argfound = (pos==pattern_up || *(pos-1)==' ') && *(pos+part)==':';
		if (argfound) {
			/* pos: position of found arg in pattern */
			pos = pattern + (pos - pattern_up);
			break;
		} else
			pat = pos+1;
	}
	while (part>1 && !argfound) {
		/* Not found -> search for shorter version of arg a_up */
		a_up[--part] = '\0';
		pat = pattern_up;
		while ((pos = strstr(pat, a_up))) {
			argfound = (pos==pattern_up || *(pos-1)==' ') && *(pos+part)==':' && *(pos+part+2)=='C';
			if (argfound) {
				/* pos: position of found arg in pattern */
				pos = pattern + (pos - pattern_up);
					break;
			} else
				pat = pos+1;
		}
	}
	free (pattern_up);

	if (argfound) {
		/* Found arg -> check further arguments of arg */
		BOOL optional = FALSE;
		char ret = *(pos+part+1);

		pos += part;
		if (part != len) {
			if (*(pos+2) == 'c') {
				*arg = argv[*nr-1] + part;
				return ret;
			} else {
				fprintf (stderr, "Unknown option '%s'!\n", argv[*nr-1]);
				return '\0';
			}
		}

		if (*(pos+2) == 'o' || *(pos+3) == 'o')
			optional = TRUE;

		if (*(pos+2) == 'r' || *(pos+2) == 'i' || *(pos+2) == 'f') {
			if (*(pos+2) == 'i' || *(pos+2) == 'f') {
				*arg = IW_ARG_NO_NUMBER;
			}
			if (*nr == argc && !optional) {
				fprintf (stderr, "Option '%s' needs an argument!\n", argv[*nr-1]);
				return '\0';
			}
			if (!optional || (*nr < argc && argv[*nr][0] != '-')) {
				if (*(pos+2) == 'r') {
					*arg = argv[*nr];
					(*nr)++;
				} else if (*(pos+2) == 'i') {
					char *end = NULL;
					int int_arg = strtol (argv[*nr], &end, 10);

					if (!end || *end) {
						if (!optional) {
							fprintf (stderr, "Argument '%s' of option '%s' must be an int!\n",
									 argv[*nr], argv[*nr-1]);
							return '\0';
						}
					} else {
						*arg = (void*)(long)int_arg;
						(*nr)++;
					}
				} else {
					char *end = NULL;
					float float_arg;
					int cnt;
					if (sscanf (argv[*nr], "%f%n", &float_arg, &cnt) > 0)
						end = argv[*nr]+cnt;
					if (!end || *end) {
						if (!optional) {
							fprintf (stderr, "Argument '%s' of option '%s' must be a float!\n",
									 argv[*nr], argv[*nr-1]);
							return '\0';
						}
					} else {
						memcpy (&arg, &float_arg, sizeof(float));
						(*nr)++;
					}
				}
			}
		}
		return ret;
	}
	if (len > 0) fprintf (stderr, "Unknown option '%s'!\n", argv[*nr-1]);
	return '\0';
}

#ifdef __alpha
extern int usleep __((useconds_t ));
#endif

/*********************************************************************
  Wrapper for usleep() for enhanced portability (the OSF alpha
  version is really strange).
*********************************************************************/
void iw_usleep (unsigned long usec)
{
	if (usec >= 1000000) {
		sleep (usec / 1000000);
		usec %= 1000000;
	}
	usleep (usec);
}

/*********************************************************************
  On linux: Show the thread ID as a debug output via gettid().
*********************************************************************/
void iw_showtid (int level, const char *str)
{
#if defined(__linux__)
	iw_debug (level, "%s thread ID: %d", str, gettid());
#endif
}

#ifdef IW_TIME_MESSURE

typedef struct {
	struct timeval start;
	struct tms start_user;
	unsigned long long start_ticks;
	int cnt;
	long total_ms, total_sys, total_user;
	long long total_ticks;
	char *name;
} toolsTime;

static int time_maxlen = 8;
static int time_enabled = TRUE;
static long time_clk_ticks = 1;
static double time_cpu_mhz = -1;
static int timer_cnt = 0;
static int timer_max = 0;
static toolsTime *timer;

/*********************************************************************
  Return value of the CPU time-stamp counter.
*********************************************************************/
static unsigned long long time_rdtsc (void)
{
#if defined(__linux__) && defined(__GNUC__)
	unsigned int l, h;
	__asm__ __volatile__("rdtsc" : "=a" (l), "=d" (h));
	return ((unsigned long long)h << 32) + l;
#else
	return clock();
#endif
}

static void time_print (const char *str, long ms)
{
	if (ms >= 10000) {
		float s = (float)ms / 1000;
		if (ms >= 1000000)
			printf ("%s%5.0fs", str, s);
		else if (ms >= 100000)
			printf ("%s%5.1fs", str, s);
		else
			printf ("%s%5.2fs", str, s);
	} else
		printf ("%s%4ldms", str, ms);
}

static void time_display (int nr, long ms, long ms_user, long ms_sys, long long ms_ticks)
{
	long avg, avg_user, avg_sys;
	long avg_ticks;
	char format[30];

	if (timer[nr].cnt <= 0) return;

	avg = timer[nr].total_ms/timer[nr].cnt;
	avg_user = timer[nr].total_user/timer[nr].cnt;
	avg_sys = timer[nr].total_sys/timer[nr].cnt;
	if (time_cpu_mhz > 0)
		avg_ticks = timer[nr].total_ticks/(timer[nr].cnt*time_cpu_mhz);
	else
		avg_ticks = 0;

	if (ms >= 0) {
		sprintf (format, "Nr %%2d/%%-%ds: ", time_maxlen);
		printf (format, nr, timer[nr].name);
		time_print ("Times: ", ms);
		time_print ("(L) ", avg);
		time_print ("(A)  User: ", ms_user);
		time_print ("(L) ", avg_user);
		time_print ("(A)  Sys: ", ms_sys);
		time_print ("(L) ", avg_sys);

		/* CPU time-stamp counter */
		if (avg_ticks > 0)
			printf ("(A) TSC: %7ldns(L) %7ldns(A)\n",
					(long)(ms_ticks/time_cpu_mhz), avg_ticks);
		else
			printf ("(A)\n");
	} else {
		sprintf (format, "  Nr %%2d/%%-%ds: ", time_maxlen);
		printf (format, nr, timer[nr].name);
		time_print ("Times:", avg);
		time_print ("  User:", avg_user);
		time_print ("  Sys:", avg_sys);

		/* CPU time-stamp counter */
		if (avg_ticks > 0)
			printf (" TSC:%7ldns\n", avg_ticks);
		else
			printf ("\n");
	}
}

/*********************************************************************
  Enable/Disable iw_time_start(), iw_time_stop(), and iw_time_show().
*********************************************************************/
void iw_time_set_enabled (BOOL enabled)
{
	time_enabled = enabled;
}

/*********************************************************************
  Reset time measurement number nr (-> set mean value, count to 0).
*********************************************************************/
void iw_time_init (int nr)
{
	iw_assert (nr>=0 && nr<timer_cnt,
			   "index %d for iw_time_init() out of bounds", nr);
	timer[nr].cnt = 0;
	timer[nr].total_ms = 0;
	timer[nr].total_sys = 0;
	timer[nr].total_user = 0;
	timer[nr].total_ticks = 0;
}

/*********************************************************************
  Reset all timer, i.e. call iw_time_init() for all added timers.
*********************************************************************/
void iw_time_init_all (void)
{
	int i;
	for (i=0; i<timer_cnt; i++) iw_time_init (i);
}

/*********************************************************************
  Add a new timer and return its number (must be passed to the other
  time_...() functions). 'name' describes the new timer.
*********************************************************************/
int iw_time_add (const char *name)
{
	if (timer_cnt == 0) {
#if defined(__linux__) && defined(__GNUC__)
		double mhz = 0;
		BOOL tsc = FALSE;
		char line[300];
		FILE *fp;

		/* Get CPU frequency from the proc FS */
		if ((fp = fopen ("/proc/cpuinfo", "r")) != NULL) {
			while (fgets (line, sizeof(line), fp)) {
				if (sscanf ( line, "cpu MHz : %lf", &mhz) == 1)
					time_cpu_mhz = mhz;
				if (strstr (line, "tsc"))
					tsc = TRUE;
			}
			fclose (fp);
		}
		if (!tsc)
			time_cpu_mhz = 0;
#endif

		time_clk_ticks = sysconf (_SC_CLK_TCK);
	}

	if (timer_cnt >= timer_max) {
		timer_max += 10;
		timer = realloc (timer, sizeof(toolsTime)*timer_max);
	}
	if (name)
		timer[timer_cnt].name = strdup(name);
	else
		timer[timer_cnt].name = "?";
	if (strlen(timer[timer_cnt].name) > time_maxlen)
		time_maxlen = strlen(timer[timer_cnt].name);
	timer_cnt++;

	iw_time_init (timer_cnt-1);
	return timer_cnt-1;
}

/*********************************************************************
  Start the timer nr (which was added before with iw_time_add()).
*********************************************************************/
void iw_time_start (int nr)
{
	if (!time_enabled) return;

	iw_assert (nr>=0 && nr<timer_cnt,
			   "index %d for start_time out of bounds", nr);

	gettimeofday (&timer[nr].start, NULL);
	times (&timer[nr].start_user);
	timer[nr].start_ticks = time_rdtsc();
}

/*********************************************************************
  Stop time measurement number nr.
  show==TRUE: Print time and average time for number nr on stdout.
  Return the elapsed real time in ms.
*********************************************************************/
long iw_time_stop (int nr, BOOL show)
{
	struct timeval real;
	struct tms user;
	unsigned long long ticks, ms_ticks;
	long ms, ms_user, ms_sys;

	if (!time_enabled) return -1;

	iw_assert (nr>=0 && nr<timer_cnt,
			   "index %d for start_time out of bounds", nr);

	ticks = time_rdtsc();
	gettimeofday (&real, NULL);
	times (&user);

	ms = IW_TIME_DIFF(real, timer[nr].start);
	ms_user = (user.tms_utime - timer[nr].start_user.tms_utime) * 1000 / time_clk_ticks;
	ms_sys = (user.tms_stime - timer[nr].start_user.tms_stime) * 1000 / time_clk_ticks;
	ms_ticks = ticks - timer[nr].start_ticks;

	timer[nr].cnt++;
	timer[nr].total_ms += ms;
	timer[nr].total_sys += ms_sys;
	timer[nr].total_user += ms_user;
	timer[nr].total_ticks += ms_ticks;
	if (show)
		time_display (nr, ms, ms_user, ms_sys, ms_ticks);
	return ms;
}

/*********************************************************************
  Print average time for all registered timers on stdout, for which
  at least one time time_stop() was called.
*********************************************************************/
void iw_time_show (void)
{
	int nr;

	if (!time_enabled) return;

	printf ("Average times:\n");
	for (nr=0; nr<timer_cnt; nr++)
		if (timer[nr].cnt > 0)
			time_display (nr, -1, -1, -1, -1);
}

#endif  /* IW_TIME_MESSURE */

/*********************************************************************
  Output passed arguments (including a short header) on stderr and
  terminate the program.
*********************************************************************/
void iw_error_x (const char *proc, int line, const char* str, ...)
{
	va_list args;
	fprintf (stderr, "%s, Error in %s at %d: ", ICEWING_NAME, proc, line);
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	fprintf (stderr, "!\n");
	gui_exit (1);
}

/*********************************************************************
  Output passed arguments (including a short header) on stderr.
*********************************************************************/
void iw_warning_x (const char *proc, int line, const char* str, ...)
{
	va_list args;
	fprintf (stderr, "%s, Warning in %s at %d: ", ICEWING_NAME, proc, line);
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	fprintf (stderr, "!\n");
	fflush (stderr);
}

/*********************************************************************
  Output passed arguments (including a short header) on stderr,
  append ': 'strerror(errno) and terminate the program.
*********************************************************************/
void iw_error_errno_x (const char *proc, int line, const char* str, ...)
{
	va_list args;
	int errno_save = errno;

	fprintf (stderr, "%s, Error in %s at %d: ", ICEWING_NAME, proc, line);
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	if (errno_save > 0)
		fprintf (stderr, ": %s!\n", strerror(errno_save));
	else
		fprintf (stderr, "!\n");
	gui_exit (1);
}

/*********************************************************************
  Output passed arguments (including a short header) on stderr and
  append ': 'strerror(errno).
*********************************************************************/
void iw_warning_errno_x (const char *proc, int line, const char* str, ...)
{
	va_list args;
	int errno_save = errno;

	fprintf (stderr, "%s, Warning in %s at %d: ", ICEWING_NAME, proc, line);
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	if (errno_save > 0)
		fprintf (stderr, ": %s!\n", strerror(errno_save));
	else
		fprintf (stderr, "!\n");
	fflush (stderr);
}

static int talklevel = 5;

/*********************************************************************
  Initialize the talklevel for following debug() calls. Debug
  messages are only given out if there level < talklevel.
*********************************************************************/
void iw_debug_set_level (int level)
{
	talklevel = level;
}

/*********************************************************************
  Return the talklevel.
*********************************************************************/
int iw_debug_get_level (void) {return talklevel;}

/*********************************************************************
  Output passed arguments on stdout (with a short header)
  if level < talklevel.
*********************************************************************/
void iw_debug_x (int level, const char *proc, int line, const char* str, ...)
{
	if (level < talklevel) {
		va_list args;
		const char *proc_file = strrchr (proc, '/');
		if (proc_file)
			proc_file++;
		else
			proc_file = proc;
		while (*str == '\n') {
			fputc ('\n', stdout);
			str++;
		}
		fprintf (stdout, "> %-15s(%4d): ", proc_file, line);
		va_start (args, str);
		vfprintf (stdout, str, args);
		va_end (args);
		fprintf (stdout, "\n");
		fflush (stdout);
	}
}

/*********************************************************************
  Do not change the ABI interface -> always include the functions.
*********************************************************************/
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__GNUC__)
#undef iw_error
#undef iw_warning
#undef iw_error_errno
#undef iw_warning_errno
#undef iw_debug
#undef iw_assert
#endif

/*********************************************************************
  Fallback functions, if the compiler does not support macros with
  a variable number of arguments.
*********************************************************************/
void iw_error (const char* str, ...)
{
	va_list args;
	fprintf (stderr, "%s, Error in %s: ", ICEWING_NAME, "iw_error()");
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	fprintf (stderr, "!\n");
	gui_exit (1);
}
void iw_warning (const char* str, ...)
{
	va_list args;
	fprintf (stderr, "%s, Warning in %s: ", ICEWING_NAME, "iw_warning()");
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	fprintf (stderr, "!\n");
	fflush (stderr);
}
void iw_error_errno (const char* str, ...)
{
	va_list args;
	int errno_save = errno;

	fprintf (stderr, "%s, Error in %s: ", ICEWING_NAME, "iw_error()");
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	if (errno_save > 0)
		fprintf (stderr, ": %s!\n", strerror(errno_save));
	else
		fprintf (stderr, "!\n");
	gui_exit (1);
}
void iw_warning_errno (const char* str, ...)
{
	va_list args;
	int errno_save = errno;

	fprintf (stderr, "%s, Warning in %s: ", ICEWING_NAME, "iw_warning()");
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	if (errno_save > 0)
		fprintf (stderr, ": %s!\n", strerror(errno_save));
	else
		fprintf (stderr, "!\n");
	fflush (stderr);
}
void iw_debug (int level, const char* str, ...)
{
	if (level < talklevel) {
		va_list args;
		while (*str == '\n') {
			fputc ('\n', stdout);
			str++;
		}
		fprintf (stdout, "> %-21s: ", "iw_debug()");
		va_start (args, str);
		vfprintf (stdout, str, args);
		va_end (args);
		fprintf (stdout, "\n");
		fflush (stdout);
	}
}
void iw_assert (long expression, const char* str, ...)
{
	va_list args;
	if (expression) return;

	fprintf (stderr, "%s, Error in %s: ", ICEWING_NAME, "iw_assert()");
	va_start (args, str);
	vfprintf (stderr, str, args);
	va_end (args);
	fprintf (stderr, "!\n");
	gui_exit (1);
}
