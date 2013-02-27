/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 2005-2009
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

/*
 * This program is based on fileman.c an exmaple from the GNU readline
 * library, which comes under the following copyright/license:
 *
 * Copyright (C) 1987-2002 Free Software Foundation, Inc.
 *
 * This file is part of the GNU Readline Library, a library for
 * reading lines of text with interactive input and history editing.
 *
 * The GNU Readline Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2, or
 * (at your option) any later version.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#ifdef WITH_DACS
#include "dacs.h"
#endif
#ifdef WITH_XCF
#include "xcf/xcf.hpp"
#endif

#define PRGNAME	"icewing-control"
#define PROMPT	"iwCtrl"
#define MENU_ID	"MENURC"

typedef int BOOL;

#ifndef TRUE
#define FALSE 0
#define TRUE 1
#endif

	/* Default host and port for socket communication */
#define SOCK_NAME		"localhost"
#define SOCK_PORT		4208
#define SOCK_COM_LOAD	"control"
#define SOCK_COM_SAVE	"getSettings"
static const char *sock_name = NULL;
	/* socket() file descriptor */
static int sock_fd = 0;

#ifdef WITH_DACS
static DACSentry_t *dacs_entry = NULL;
	/* Default iceWing DACS name */
#define DACS_NAME	"icewing"
	/* iceWing DACS name */
static const char *dacs_name = NULL;
#endif

#ifdef WITH_XCF
static XCF::RemoteServerPtr xcf_server;
	/* Default remotectrl XCF server name */
#define XCF_NAME	"xcf"
	/* remotectrl XCF server name */
static const char *xcf_name = NULL;
#endif

	/* TRUE, if the quit command was entered */
static BOOL prg_exit = FALSE;

	/* Names of all iceWing widgets, result of last com_get(). */
static char **titles = NULL;

	/* Information on the commands this program can understand. */
typedef enum {
	COM_NONE,
	COM_GET,
	COM_SET,
	COM_HELP,
	COM_QUIT,
} COM_COMMAND;
typedef struct {
	const char *name;		 /* User printable name of the function */
	COM_COMMAND command;
	rl_icpfunc_t *func;		 /* Function to call to do the job */
	const char *help;		 /* Documentation for this function */
} COMMAND;

static int com_get (char *arg);
static int com_set (char *arg);
static int com_help (char *arg);
static int com_quit (char *arg);
COMMAND commands[] = {
	{"get", COM_GET, com_get,
	 "Get all iceWing settings"},
	{"set", COM_SET, com_set,
	 "Set one widget in iceWing, e.g.: \"set GrabImage1.Wait Time = 94\""},
	{"help", COM_HELP, com_help,
	 "Display this text"},
	{"?", COM_HELP, com_help,
	 "Synonym for the command `help'"},
	{"quit", COM_QUIT, com_quit,
	 "Exit this program"},
	{NULL, COM_NONE, NULL, NULL }
};

/*****************************************************************
  Automaticly called on program exit (-> clean up).
*****************************************************************/
void stop_it (void)
{
#ifdef WITH_DACS
	if (dacs_entry) {
		dacs_unregister (dacs_entry);
		dacs_entry = NULL;
	}
#endif
	if (sock_fd) {
		close (sock_fd);
		sock_fd = 0;
	}
}

/*****************************************************************
  Automaticly called on SIGINT and -TERM (-> clean up).
*****************************************************************/
void stop_it_sig (int sig)
{
	exit (0);
}

/*********************************************************************
  Send one packet (len + buf (of length len)) to socket s.
*********************************************************************/
static ssize_t sock_send (int s, const void *buf, size_t len)
{
	size_t total = 0;
	int cnt;
	uint32_t len_n;

	len_n = htonl(len);
	do {
		cnt = send (s, &len_n, sizeof(len_n), 0);
	} while (cnt == -1 && errno == EINTR);
	if (cnt != sizeof(len_n))
		return -1;

	while (total < len) {
		do {
			cnt = send (s, (char*)buf+total, len - total, 0);
		} while (cnt == -1 && errno == EINTR);
		if (cnt < 0)
			return -1;
		total += cnt;
	}
	return total;
}

/*********************************************************************
  Receive one packet (string length + string) from socket s and save
  the string to *buf (of size *len).
*********************************************************************/
static ssize_t sock_recv (int s, char **buf, int *len)
{
	uint32_t packetlen;
	size_t total = 0;
	int cnt;

	do {
		cnt = recv (s, &packetlen, sizeof(packetlen), 0);
	} while (cnt == -1 && errno == EINTR);
	if (cnt < 0)
		return -1;
	else if (cnt != sizeof(packetlen))
		return 0;
	packetlen = ntohl (packetlen);

	if (*len < (int)packetlen) {
		*len = packetlen;
		*buf = (char*)realloc (*buf, packetlen);
	}
	while (total < packetlen) {
		do {
			cnt = recv (s, (*buf)+total, packetlen - total, 0);
		} while (cnt == -1 && errno == EINTR);
		if (cnt <= 0)
			return cnt;
		total += cnt;
	}
	return total;
}

/*****************************************************************
   If arg is no valid argument for caller print an error message
   and return FALSE else return TRUE.
*****************************************************************/
static BOOL check_argument (const char *caller, char *arg)
{
	if (!arg || !*arg) {
		fprintf (stderr, "%s: Argument required.\n", caller);
		return FALSE;
	}
	return TRUE;
}

/*****************************************************************
  Print out help for arg, or for all of the commands if arg is
  not present.
*****************************************************************/
static int com_help (char *arg)
{
	int i, printed = 0;

	for (i = 0; commands[i].name; i++) {
		if (!*arg || !strcmp (arg, commands[i].name)) {
			printf ("%s\t\t%s.\n", commands[i].name, commands[i].help);
			printed++;
		}
	}

	if (!printed) {
		printf ("No commands match `%s'. Possibilties are:\n", arg);

		for (i = 0; commands[i].name; i++) {
			/* Print in six columns. */
			if (printed == 6) {
				printed = 0;
				printf ("\n");
			}

			printf ("%s\t", commands[i].name);
			printed++;
		}

		if (printed)
			printf ("\n");
	}
	return 0;
}

/*****************************************************************
  Set prg_exit to indicate the quit command was called.
*****************************************************************/
static int com_quit (char *arg)
{
	prg_exit = TRUE;
	return 0;
}

/*****************************************************************
  Get all widget settings from icewing.
*****************************************************************/
static int com_get_do (BOOL show)
{
#ifdef WITH_DACS
	char *dacs_ret = NULL;
#endif
	char *sock_ret = NULL, *ret = NULL;
	int err = TRUE;

	if (sock_fd) {
		if (sock_send (sock_fd, SOCK_COM_SAVE, sizeof(SOCK_COM_SAVE))
			== sizeof(SOCK_COM_SAVE)) {
			int retlen = 0;
			err = sock_recv (sock_fd, &ret, &retlen) <= 0;
			sock_ret = ret;
			if (err)
				perror ("get: Unable to recv() settings");
		} else
			perror ("get: Unable to send() for settings");
	}

#ifdef WITH_DACS
	if (dacs_entry && err) {
		DACSfunction_handle_t fnc_handle;
		char fname[DACS_MAXNAMELEN];

		sprintf (fname, "%s_getSettings", dacs_name);
		fnc_handle = dacs_call_function (dacs_entry, DACS_BLOCK, fname,
										 NULL, NULL,
										 (void **)&ret, (NDRfunction_t*)ndr_string);
		dacs_ret = ret;
		err = fnc_handle.status != D_OK;
		if (err)
			fprintf (stderr, "get: Error calling function '%s'.\n"
					 "\tstatus %d, error message: %s\n",
					 fname, fnc_handle.status, fnc_handle.error_message);
	}
#endif

#ifdef WITH_XCF
	std::string result;
	if (xcf_server && err) {
		try {
			xcf_server->callMethod ("getSettings", "", result);
			ret = (char*)result.c_str();
			err = FALSE;
		} catch (const XCF::GenericException& ex) {
			fprintf (stderr, "get: Error calling function '%s::getSettings'.\n"
					 "\tXCF::GenericException: %s\n",
					 xcf_name, ex.reason.c_str());
			err = TRUE;
		}
	}
#endif

	if (show && ret && *ret)
		printf ("%s", ret);

	/* Parse completion possibilities for the set command
	   and save them in titles. */
	if (ret && *ret) {
		BOOL menu = FALSE;
		int i, lines;
		char *str = ret, *start;

		if (titles) {
			for (i = 0; titles[i]; i++)
				free (titles[i]);
			free (titles);
		}

		/* One NULL entry at the end of the array. */
		lines = 2;
		for (i = 0; str[i]; i++)
			if (str[i] == '\n') lines++;
		titles = (char**)calloc (lines, sizeof(char*));

		lines = 0;
		while (*str) {
			while (*str==' ' || *str=='\t') str++;

			start = str;
			if (!strncmp (MENU_ID, str, strlen(MENU_ID))) {
				menu = !menu;
				while (*str && *str != '\n') str++;
				while (*str=='\n' || *str==' ' || *str=='\t') str--;
			} else if (menu) {
				if (*str == ';')
					start = str+1;
				while (*str && *str != '\n') str++;
				while (*str=='\n' || *str==' ' || *str=='\t') str--;
			} else if (*str == '"') {
				start = ++str;
				while (*str && *str != '"') str++;
				if (*str) str--;
			} else if (*str && *str != '#' && *str != '\n') {
				while (*str && *str != '=') str++;
				if (*str) str--;
				while (*str==' ' || *str=='\t') str--;
			} else
				start = NULL;

			if (start && str >= start && *str) {
				titles[lines] = (char*)malloc (str-start+2);
				memcpy (titles[lines], start, str-start+1);
				titles[lines][str-start+1] = '\0';
				lines++;
			}

			while (*str && *str != '\n') str++;
			if (*str) str++;
		}
	}

	if (sock_ret)
		free (sock_ret);
#ifdef WITH_DACS
	if (dacs_ret)
		ndr_free ((void **)&dacs_ret, (NDRfunction_t*)ndr_string);
#endif
	return err;
}

/*****************************************************************
  Get all widget settings from icewing.
*****************************************************************/
static int com_get (char *arg)
{
	return com_get_do (TRUE);
}

/*****************************************************************
  Set widgets of icewing to new values.
*****************************************************************/
static int com_set (char *arg)
{
	int err = TRUE;
	char *str;

	if (!check_argument ("set", arg)) return 1;

	str = arg;
	if (*str == '"') str++;
	if (!strncmp (str, "(menu-path", 10) || !strncmp (str, "(gtk_accel", 10)) {
		str = (char*)malloc (strlen(arg)+2*sizeof(MENU_ID)+sizeof("START")+sizeof("END")+2);
		sprintf (str, MENU_ID" START\n%s\n"MENU_ID" END\n", arg);
	} else
		str = arg;

	if (sock_fd) {
		int len = sizeof(SOCK_COM_LOAD) + strlen(str);
		char *sock_str = (char*)malloc (len);

		strcpy (sock_str, SOCK_COM_LOAD);
		strcat (sock_str, str);
		err = sock_send (sock_fd, sock_str, len) != len;
		if (err)
			perror ("set: Unable to send() settings");
		else
			printf ("set: send() settings to %s.\n", sock_name);
		free (sock_str);
	}

#ifdef WITH_DACS
	if (dacs_entry && err) {
		char fname[256];
		DACSfunction_handle_t fnc_handle;
		sprintf (fname, "%s_control", dacs_name);
		fnc_handle = dacs_call_function (dacs_entry, DACS_BLOCK, fname,
										 (void *)str, (NDRfunction_t*)ndr_string,
										 NULL, NULL);
		err = fnc_handle.status != D_OK;
		if (err)
			fprintf (stderr, "set: Error calling function '%s'.\n"
					 "\tstatus %d, error message: %s\n",
					 fname, fnc_handle.status, fnc_handle.error_message);
		else
			printf ("set: Called function %s.\n", fname);
	}
#endif

#ifdef WITH_XCF
	if (xcf_server && err) {
		try {
			xcf_server->callMethod ("control", std::string(str));
			err = FALSE;
			printf ("set: Called function %s::control.\n", xcf_name);
		} catch (const XCF::GenericException& ex) {
			fprintf (stderr, "set: Error calling function '%s::control'.\n"
					 "\tXCF::GenericException: %s\n",
					 xcf_name, ex.reason.c_str());
			err = TRUE;
		}
	}
#endif

	if (str != arg)
		free (str);

	return err;
}

/*****************************************************************
  Generator function for command completion. 'state' lets us
  know whether to start from scratch; without any state
  (i.e. 'state' == 0), then we start at the top of the list.
*****************************************************************/
static char *completion_command (const char *text, int state)
{
	static int list_index, len;
	const char *name;

	/* If this is a new word to complete, initialize now. This
	   includes saving the length of TEXT for efficiency, and
	   initializing the index variable to 0. */
	if (!state) {
		list_index = 0;
		len = strlen (text);
	}

	/* Return the next name which partially matches from the
	   command list. */
	while ((name = commands[list_index].name)) {
		list_index++;

		if (!strncmp (name, text, len))
			return (strdup(name));
	}

	/* If no names matched, then return NULL. */
	return NULL;
}

static char *completion_arg (const char *text, int state)
{
	static COM_COMMAND command;
	static int list_index, len;
	char *str;
	int i;

	if (!state) {
		/* Check for which command an argument should be completed. */
		command = COM_NONE;
		str = rl_line_buffer;
		while (whitespace(*str)) str++;

		for (i = 0; commands[i].name; i++) {
			if (!strncmp (commands[i].name, str, strlen(commands[i].name))) {
				command = commands[i].command;
				break;
			}
		}
		/* Remove the special word break character from the buffer. */
		for (; *str; str++) {
			if (*str == '\1')
				*str = ' ';
		}
	}
	switch (command) {
		case COM_SET:
			if (!state) {
				if (!titles)
					com_get_do (FALSE);
				list_index = 0;
				len = strlen (text);
			}
			if (!titles) return NULL;

			/* Return the next name which partially matches from the titles list. */
			while ((str = titles[list_index])) {
				list_index++;

				if (!strncmp (str, text, len))
					return (strdup(str));
			}
			break;
		case COM_HELP:
			return completion_command (text, state);
		default:
			break;
	}
	return NULL;
}

/*****************************************************************
  Attempt to complete on the contents of TEXT. START and END
  bound the region of rl_line_buffer that contains the word to
  complete. TEXT is the word to complete. We can use the entire
  contents of rl_line_buffer in case we want to do some simple
  parsing. Return the array of matches, or NULL if there aren't
  any.
*****************************************************************/
static char **iwctrl_completion (const char *text, int start, int end)
{
	int i;

	/* Never do filename completion. */
	rl_attempted_completion_over = 1;

	/* If this word is at the start of the line, then it is a command
	   to complete. Otherwise it is an argument to a command. */
	for (i = 0; i < start ; i++) {
		if (!whitespace(rl_line_buffer[i]))
			return rl_completion_matches (text, completion_arg);
	}

	return rl_completion_matches (text, completion_command);
}

/*****************************************************************
  If argument completion should be done, change word break
  character to '\1' and start the argument with '\1'.
*****************************************************************/
static char *iwctrl_wordbreak (void)
{
	char *str = rl_line_buffer;
	int i;

	i = 0;
	while (whitespace(str[i])) i++;
	if (!str[i]) return NULL;

	while (str[i] && !whitespace(str[i])) i++;
	if (rl_point <= i) return NULL;

	while (whitespace(str[i])) i++;
	str[i-1] = '\1';
	if (str[i] == '"')
		rl_completer_quote_characters = "\"";
	else
		rl_completer_quote_characters = "";

	return (char*)"\1";
}

/*****************************************************************
  Look up NAME as the name of a command and return a pointer to
  that command or NULL if NAME isn't a command name.
*****************************************************************/
static COMMAND *find_command (char *name)
{
	register int i;

	for (i = 0; commands[i].name; i++)
		if (!strcmp (name, commands[i].name))
			return (&commands[i]);

	return NULL;
}

/*****************************************************************
  Execute a command line.
*****************************************************************/
static int execute_line (char *line)
{
	register int i;
	COMMAND *command;
	char *word;

	/* Isolate the command word. */
	i = 0;
	while (line[i] && whitespace (line[i]))
		i++;
	word = line + i;

	while (line[i] && !whitespace (line[i]))
		i++;
	if (line[i])
		line[i++] = '\0';

	command = find_command (word);
	if (!command) {
		fprintf (stderr, "%s: No such command for "PRGNAME".\n", word);
		return -1;
	}

	/* Get argument to command, if any. */
	while (whitespace (line[i]))
		i++;

	/* Call the function. */
	return ((*(command->func)) (line + i));
}

static void help (void)
{
	fprintf (stderr, "\n"PRGNAME" V0.3 (c) 2005-2009 by Frank Loemker\n"
			 "Usage: "PRGNAME" [-p [host:port]]"
#ifdef WITH_DACS
			 " [-n [name]]"
#endif
#ifdef WITH_XCF
			 " [-x [xcf-server]]"
#endif
			 " [-g] [-s config-setting]\n"
			 "-p    Socket communication to the icewing remotectrl plugin,\n"
			 "      default: %s:%d\n",
			 SOCK_NAME, SOCK_PORT);
#ifdef WITH_DACS
	fprintf (stderr,
			 "-n    DACS name of the icewing program, default: %s\n", DACS_NAME);
#endif
#ifdef WITH_XCF
	fprintf (stderr,
			 "-x    XCF server name used for the icewing remotectrl plugin, default: %s\n",
			 XCF_NAME);
#endif
	fprintf (stderr,
			 "-g    Call the 'getSettings(void)' function to get all current widget settings\n"
			 "-s    Call the 'control(char)' function to set the specified widgets to the\n"
			 "      specified values\n"
			 "\n"
			 "If neither any -g nor any -s argument is given, an interactive shell is\n"
			 "started.");
#if defined(WITH_DACS) || defined(WITH_XCF)
	fprintf (stderr, " If neither -p, -n, nor -x is given, '-p' is used for communication.\n");
#else
	fprintf (stderr, "\n");
#endif
	exit (0);
}

int main (int argc, char **argv)
{
	int interactive = TRUE;
	char *optarg;
	int n;

	if (argc>1 && (!strcmp (argv[1], "--help") ||
				   !strcmp (argv[1], "-help") ||
				   !strcmp (argv[1], "-h")))
		help();

	atexit (stop_it);
	signal (SIGINT, stop_it_sig);
	signal (SIGTERM, stop_it_sig);

	int getarg = FALSE;
	char *setarg = NULL;
	int setlen = 0;

	for (n = 1; n < argc; n++) {
		if (argv[n][0] == '-' && argv[n][2] == '\0') {
			if (n < argc-1 && argv[n+1][0] != '-')
				optarg = argv[n+1];
			else
				optarg = NULL;
			switch (toupper (argv[n][1])) {
				case 'P':
					if (optarg) {
						sock_name = optarg;
						n++;
					} else
						sock_name = SOCK_NAME;
					break;
#ifdef WITH_DACS
				case 'N':
					if (optarg) {
						dacs_name = optarg;
						n++;
					} else
						dacs_name = DACS_NAME;
					break;
#endif
#ifdef WITH_XCF
				case 'X':
					if (optarg) {
						xcf_name = optarg;
						n++;
					} else
						xcf_name = XCF_NAME;
					break;
#endif
				case 'G':
					getarg = TRUE;
					interactive = FALSE;
					break;
				case 'S':
					if (!optarg) {
						fprintf (stderr, "Config setting for option '-s' is missing.\n");
						help();
					}
					setlen += strlen(optarg)+1;
					if (!setarg) {
						setarg = (char*)malloc(setlen+1);
						*setarg = '\0';
					} else
						setarg = (char*)realloc (setarg, setlen+1);
					strcat (setarg, optarg);
					strcat (setarg, "\n");
					interactive = FALSE;
					n++;
					break;
				case 'H':
					help();
				default:
					fprintf (stderr, "Unknown option '%s'.\n", argv[n]);
					help();
			} /* switch */
		} else {
			fprintf (stderr, "Unknown option '%s'.\n", argv[n]);
			help();
		} /* if */
	} /* for */

	if (!sock_name
#if defined(WITH_DACS)
		&& !dacs_name
#endif
#if defined(WITH_XCF)
		&& !xcf_name
#endif
		) {
		sock_name = SOCK_NAME;
	}

	if (sock_name) {
		struct hostent *he;
		struct sockaddr_in addr;
		char *name = strdup (sock_name);
		char *portname;
		int port = SOCK_PORT;

		/* Get destination host and port */
		if ((portname = strchr (name, ':'))) {
			char *pend = NULL;
			*portname++ = '\0';
			port = strtol (portname, &pend, 10);
			if (!pend || *pend) {
				fprintf (stderr, "Port specifier from option '-p' is not an int.\n");
				help();
			}
		}
		if ((he = gethostbyname (*name ? name:SOCK_NAME)) == NULL) {
			herror (PRGNAME": gethostbyname()");
			exit (1);
		}
		free (name);
		fprintf (stderr, "Connecting to %s:%d...\n", he->h_name, port);

		/* Connect to the destination */
		if ((sock_fd = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
			perror (PRGNAME": socket()");
			exit (1);
		}
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr = *((struct in_addr *)he->h_addr);
		memset (&(addr.sin_zero), '\0', 8);
		if (connect (sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror (PRGNAME": connect()");
			exit (1);
		}
	}

#ifdef WITH_DACS
	if (dacs_name) {
		char name[50];
		sprintf (name, "iwCtrl%d", getpid());
		if ( (dacs_entry = dacs_register (name)) == NULL) {
			fprintf (stderr, PRGNAME": Can't register as '%s'!", name);
			exit (1);
		}
	}
#endif

#ifdef WITH_XCF
	if (xcf_name) {
		try {
			xcf_server = XCF::RemoteServer::create (xcf_name);
		} catch (const XCF::GenericException& ex) {
			fprintf (stderr, PRGNAME": Could not create Server '%s'!\n"
					 "\tXCF::GenericException: %s\n", xcf_name, ex.reason.c_str());
			exit (1);
		}
	}
#endif

	if (setarg) {
		if (com_set (setarg) != 0)
			exit (1);
		free (setarg);
	}
	if (getarg) {
		if (com_get (NULL) != 0)
			exit (1);
	}
	if (!interactive) exit (0);

	/* Allow conditional parsing of the ~/.inputrc file. */
	rl_readline_name = PRGNAME;

	/* Tell the completer that we want a crack first. */
	rl_attempted_completion_function = iwctrl_completion;
	rl_completer_quote_characters = "\"";
#if RL_VERSION_MAJOR>=5
	rl_completion_word_break_hook = iwctrl_wordbreak;
#endif

	/* Loop reading and executing lines until the user quits. */
	while (!prg_exit) {
		char *line, *s;

		line = readline (PROMPT"> ");
		if (!line) break;

		/* Remove leading and trailing whitespace from the line.
		   Then, if there is anything left, add it to the history list
		   and execute it. */
		for (s = line; whitespace (*s); s++) /*empty*/;

		if (*s != '\0') {
			char *t = s + strlen (s) - 1;
			while (t > s && whitespace (*t))
				t--;
			*++t = '\0';
		}
		if (*s) {
			add_history (s);
			execute_line (s);
		}

		free (line);
	}
	return 0;
}
