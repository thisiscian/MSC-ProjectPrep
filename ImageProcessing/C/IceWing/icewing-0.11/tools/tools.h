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

#ifndef iw_tools_H
#define iw_tools_H

#include <sys/types.h>
#include <limits.h>
#include <glib.h>

#ifndef HAVE_UCHAR
#if defined(__linux__) || defined(__sun) || defined(__APPLE__)
typedef unsigned char uchar;
#endif
#endif

extern char *ICEWING_NAME;			/* == "iceWing" */
extern char *ICEWING_VERSION;		/* iceWing version string */
#define ICEWING_NAME_D	"iceWing"

/* Should the time measurement be performed ? */
#define IW_TIME_MESSURE

/*
#if (defined __alpha) || (defined __sun)
#ifndef __GNUC__
#define __FUNCTION__ __FILE__
#endif
#endif
*/
#define __FUNCTION__ __FILE__

#ifndef HAVE_BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define FALSE 0
#define TRUE 1
#endif

#ifndef true
#define false FALSE
#define true TRUE
#endif

#if !defined(PATH_MAX) && defined(_POSIX_PATH_MAX)
#define PATH_MAX _POSIX_PATH_MAX
#endif
#ifndef PATH_MAX
#define PATH_MAX 256
#endif

/* Return the difference in ms between the timeval's t1 and t2 */
#define IW_TIME_DIFF(t1,t2)		(((t1).tv_sec-(t2).tv_sec) * 1000 \
								+ ((t1).tv_usec - (t2).tv_usec) / 1000)

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef  CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifndef G_PI
#define G_PI    3.14159265358979323846  /* Somehow in the wrong file ... */
#endif
#ifndef G_PI_2
#define G_PI_2  1.57079632679489661923
#endif
#ifndef G_PI_4
#define G_PI_4  0.78539816339744830962
#endif
#ifndef G_GINT64_FORMAT
#define G_GINT64_FORMAT		"li"
#define GINT64_PRINTTYPE	long
#else
#define GINT64_PRINTTYPE	gint64
#endif

/* Indicator for "no number given" for optional options in parse_args() */
#define IW_ARG_NO_NUMBER	((void*)G_MININT)

#ifdef __cplusplus
extern "C" {
#endif

#define iw_error_1(a1)					iw_error_x (__FUNCTION__,__LINE__,a1)
#define iw_error_2(a1,a2)				iw_error_x (__FUNCTION__,__LINE__,a1,a2)
#define iw_error_3(a1,a2,a3)			iw_error_x (__FUNCTION__,__LINE__,a1,a2,a3)
#define iw_error_4(a1,a2,a3,a4)			iw_error_x (__FUNCTION__,__LINE__,a1,a2,a3,a4)
#define iw_error_5(a1,a2,a3,a4,a5)		iw_error_x (__FUNCTION__,__LINE__,a1,a2,a3,a4,a5)
#define iw_error_6(a1,a2,a3,a4,a5,a6)	iw_error_x (__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6)
#define iw_warning_1(a1)				iw_warning_x (__FUNCTION__,__LINE__,a1)
#define iw_warning_2(a1,a2)				iw_warning_x (__FUNCTION__,__LINE__,a1,a2)
#define iw_warning_3(a1,a2,a3)			iw_warning_x (__FUNCTION__,__LINE__,a1,a2,a3)
#define iw_warning_4(a1,a2,a3,a4)		iw_warning_x (__FUNCTION__,__LINE__,a1,a2,a3,a4)
#define iw_warning_5(a1,a2,a3,a4,a5)	iw_warning_x (__FUNCTION__,__LINE__,a1,a2,a3,a4,a5)
#define iw_warning_6(a1,a2,a3,a4,a5,a6)	iw_warning_x (__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define iw_error(...)					iw_error_x (__FUNCTION__,__LINE__,__VA_ARGS__)
#  define iw_warning(...)				iw_warning_x (__FUNCTION__,__LINE__,__VA_ARGS__)
#  define iw_error_errno(...)			iw_error_errno_x (__FUNCTION__,__LINE__,__VA_ARGS__)
#  define iw_warning_errno(...)			iw_warning_errno_x (__FUNCTION__,__LINE__,__VA_ARGS__)
#elif defined(__GNUC__)
#  define iw_error(format...)			iw_error_x (__FUNCTION__,__LINE__,format)
#  define iw_warning(format...)			iw_warning_x (__FUNCTION__,__LINE__,format)
#  define iw_error_errno(format...)		iw_error_errno_x (__FUNCTION__,__LINE__,format)
#  define iw_warning_errno(format...)	iw_warning_errno_x (__FUNCTION__,__LINE__,format)
#else
void iw_error (const char* str, ...);
void iw_warning (const char* str, ...);
void iw_error_errno (const char* str, ...);
void iw_warning_errno (const char* str, ...);
#endif

#ifdef IW_DEBUG

#  define iw_debug_1(t,a1)					iw_debug_x (t,__FUNCTION__,__LINE__,a1)
#  define iw_debug_2(t,a1,a2)				iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2)
#  define iw_debug_3(t,a1,a2,a3)			iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3)
#  define iw_debug_4(t,a1,a2,a3,a4)			iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4)
#  define iw_debug_5(t,a1,a2,a3,a4,a5)		iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5)
#  define iw_debug_6(t,a1,a2,a3,a4,a5,a6)	iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6)
#  define iw_debug_7(t,a1,a2,a3,a4,a5,a6,a7)			   \
						iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6,a7)
#  define iw_debug_8(t,a1,a2,a3,a4,a5,a6,a7,a8)		   \
						iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6,a7,a8)
#  define iw_debug_9(t,a1,a2,a3,a4,a5,a6,a7,a8,a9)	   \
						iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6,a7,a8,a9)
#  define iw_debug_10(t,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10) \
						iw_debug_x (t,__FUNCTION__,__LINE__,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10)
#  define iw_assert_1(ex,a1)				((ex) ? (void)0 : iw_error_1(a1))
#  define iw_assert_2(ex,a1,a2)				((ex) ? (void)0 : iw_error_2(a1,a2))
#  define iw_assert_3(ex,a1,a2,a3)			((ex) ? (void)0 : iw_error_3(a1,a2,a3))
#  define iw_assert_4(ex,a1,a2,a3,a4)		((ex) ? (void)0 : iw_error_4(a1,a2,a3,a4))
#  define iw_assert_5(ex,a1,a2,a3,a4,a5)	((ex) ? (void)0 : iw_error_5(a1,a2,a3,a4,a5))

#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#    define iw_debug(t,...)					iw_debug_x (t,__FUNCTION__,__LINE__,__VA_ARGS__)
#    define iw_assert(ex,...)				((ex) ? (void)0 : iw_error(__VA_ARGS__))
#  elif defined(__GNUC__)
#    define iw_debug(t,format...)			iw_debug_x (t,__FUNCTION__,__LINE__,format)
#    define iw_assert(ex,format...)			((ex) ? (void)0 : iw_error(format))
#  else
void iw_debug (int level, const char* str, ...);
void iw_assert (long expression, const char* str, ...);
#  endif

#else

#  define iw_debug_1(t,a1)
#  define iw_debug_2(t,a1,a2)
#  define iw_debug_3(t,a1,a2,a3)
#  define iw_debug_4(t,a1,a2,a3,a4)
#  define iw_debug_5(t,a1,a2,a3,a4,a5)
#  define iw_debug_6(t,a1,a2,a3,a4,a5,a6)
#  define iw_debug_7(t,a1,a2,a3,a4,a5,a6,a7)
#  define iw_debug_8(t,a1,a2,a3,a4,a5,a6,a7,a8)
#  define iw_debug_9(t,a1,a2,a3,a4,a5,a6,a7,a8,a9)
#  define iw_debug_10(t,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10)
#  define iw_assert_1(ex,a1)
#  define iw_assert_2(ex,a1,a2)
#  define iw_assert_3(ex,a1,a2,a3)
#  define iw_assert_4(ex,a1,a2,a3,a4)
#  define iw_assert_5(ex,a1,a2,a3,a4,a5)

#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#    define iw_debug(t,...)
#    define iw_assert(ex,...)
#  elif defined(__GNUC__)
#    define iw_debug(t,format...)
#    define iw_assert(ex,format...)
#  else
static inline void iw_debug (int level, const char* str, ...) {}
static inline void iw_assert (long expression, const char* str, ...) {}
#  endif

#endif /* IW_DEBUG */

/*********************************************************************
  Call malloc() / calloc() / realloc().
  If err!=NULL and memory could not be allocated, exit with
  'out of memory: "err"' by calling iw_error().
*********************************************************************/
void* iw_malloc (size_t size, const char *err);
void* iw_malloc0 (size_t size, const char *err);
void* iw_realloc (void *ptr, size_t size, const char *err);

/*********************************************************************
  Allocate 16 byte aligned memory. Must be freed with iw_free_align().
  If err!=NULL and memory could not be allocated, exit with
  'out of memory: "err"' by calling iw_error().
*********************************************************************/
void* iw_malloc_align (size_t size, const char *err);
void iw_free_align (void *ptr);

/*********************************************************************
  Return in 'array' a NULL terminated array of strings pointing to
  the parts of 'str', which are separated by spaces. In 'str' '\0'
  are inserted. In 'array_len' the number of entries in 'array' is
  returned.
*********************************************************************/
void iw_string_split (char *str, int *array_len, char ***array);

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
					const char *pattern);

/*********************************************************************
  PRIVATE: If args, split args, otherwise use argv. Check for any
  '@file', load the file, exchange '@file' with the args from the
  file, and expand any vars, commands, and wildcards. nargv is newly
  allocated.
*********************************************************************/
BOOL iw_load_args (char *args, int argc, char **argv, int *nargc, char ***nargv);

/*********************************************************************
  Wrapper for usleep() for enhanced portability (the OSF alpha
  version is really strange).
*********************************************************************/
void iw_usleep (unsigned long usec);

/*********************************************************************
  On linux: Show the thread ID as a debug output via gettid().
*********************************************************************/
void iw_showtid (int level, const char *str);

#ifdef IW_TIME_MESSURE
#define iw_time_add_static(number,name)						\
		static int number = -1;								\
		if (number<0) number = iw_time_add (name);

#define iw_time_add_static2(number,name,number2,name2)		\
		static int number = -1, number2 = -1;				\
		if (number<0) {										\
			number = iw_time_add (name);					\
			number2 = iw_time_add (name2);					\
		}

#define iw_time_add_static3(number,name,number2,name2,number3,name3) \
		static int number = -1, number2 = -1, number3 = -1;	\
		if (number<0) {										\
			number = iw_time_add (name);					\
			number2 = iw_time_add (name2);					\
			number3 = iw_time_add (name3);					\
		}

/*********************************************************************
  Add a new timer and return its number (must be passed to the other
  time_...() functions). 'name' describes the new timer.
*********************************************************************/
int iw_time_add (const char *name);

/*********************************************************************
  Enable/Disable time_start(), time_stop(), and time_show().
*********************************************************************/
void iw_time_set_enabled (BOOL enabled);

/*********************************************************************
  Reset time measurement number nr (-> set mean value, count to 0).
*********************************************************************/
void iw_time_init (int nr);

/*********************************************************************
  Reset all timer, i.e. call iw_time_init() for all added timers.
*********************************************************************/
void iw_time_init_all (void);

/*********************************************************************
  Start the timer nr (which was added before with iw_time_add()).
*********************************************************************/
void iw_time_start (int nr);

/*********************************************************************
  Stop the timer number nr.
  show==TRUE: Print time and average time for number nr on stdout.
  Return the elapsed real time in ms.
*********************************************************************/
long iw_time_stop (int nr, BOOL show);

/*********************************************************************
  Print average time for all registered timers on stdout, for which
  at least one time time_stop() was called.
*********************************************************************/
void iw_time_show (void);

#else
#   define iw_time_add_static(number,name)
#   define iw_time_add_static2(number,name,number2,name2)
#   define iw_time_add_static3(number,name,number2,name2,number3,name3)
#   define iw_time_add(name)		((void)0)
#   define iw_time_set_enabled(enabled)
#   define iw_time_init_all()
#   define iw_time_init()
#   define iw_time_start(nr)
#   define iw_time_stop(nr,show)	((void)0)
#   define iw_time_show()
#endif

/*********************************************************************
  Output passed arguments (including a short header) on stderr and
  terminate the program.
*********************************************************************/
void iw_error_x (const char *proc, int line, const char* str, ...)
	G_GNUC_PRINTF(3, 4);

/*********************************************************************
  Output passed arguments (including a short header) on stderr.
*********************************************************************/
void iw_warning_x (const char *proc, int line, const char* str, ...)
	G_GNUC_PRINTF(3, 4);

/*********************************************************************
  Output passed arguments (including a short header) on stderr,
  append ': 'strerror(errno) and terminate the program.
*********************************************************************/
void iw_error_errno_x (const char *proc, int line, const char* str, ...)
	G_GNUC_PRINTF(3, 4);

/*********************************************************************
  Output passed arguments (including a short header) on stderr and
  append ': 'strerror(errno).
*********************************************************************/
void iw_warning_errno_x (const char *proc, int line, const char* str, ...)
	G_GNUC_PRINTF(3, 4);

/*********************************************************************
  Initialize the talklevel for following debug() calls. Debug
  messages are only given out if there level < talklevel.
*********************************************************************/
void iw_debug_set_level (int level);

#ifdef IW_DEBUG
/*********************************************************************
  Return the talklevel.
*********************************************************************/
int iw_debug_get_level (void);

/*********************************************************************
  Output passed arguments on stdout (with a short header)
  if level < talklevel.
*********************************************************************/
void iw_debug_x (int level, const char *proc, int line, const char* str, ...)
	G_GNUC_PRINTF(4, 5);
#endif

#ifdef __cplusplus
}
#endif

#endif /* iw_tools_H */
