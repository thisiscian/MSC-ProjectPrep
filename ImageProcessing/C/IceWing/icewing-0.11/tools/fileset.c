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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include "main/image.h"
#include "fileset.h"

typedef struct fsetEntry {
	char *name;
	int frame;					/* Single image: -1, otherwise: frame number */
	struct timeval time;		/* %T%t-timestamp or 0,0 */
	BOOL rgb:1;
	BOOL frameadv:1;
} fsetEntry;

typedef struct _fsetList {
	int act;					/* Current position in fileset */
	int cnt;					/* Number of entries in fileset */
	fsetEntry *fileset;
	char avi_name[PATH_MAX];
	iwMovie *avi_file;
	iwImage *image;

	BOOL rgb;					/* Command line: RGB/YUV */
	BOOL use_ext;				/* Command line: check only extension? */
	BOOL frameadv;
} _fsetList;

/*********************************************************************
  Change the entry which will be returned by fset_get().
  relative==TRUE: change relative to current position.
  Returns new position.
*********************************************************************/
int iw_fset_set_pos (fsetList *fset, int step, BOOL relative)
{
	if (relative)
		fset->act += step;
	else
		fset->act = step;
	if (fset->act < 0) fset->act = fset->cnt - 1;

	return fset->act;
}

/*********************************************************************
  Return length of file set.
*********************************************************************/
int iw_fset_get_length (fsetList *fset)
{
	return fset->cnt;
}

static BOOL file_exist (char *fname)
{
	struct stat sb;
	return (stat(fname, &sb) == -1) ? FALSE : TRUE;
}

/*********************************************************************
  Append frame new entries (or 1 if frame < 0) to the array
  (*fset)->fileset.
*********************************************************************/
static void fileset_add (fsetList *f, char *name,
						 int frame, struct timeval *time)
{
	int i, num_new;

	if (frame < 0)
		num_new = 1;
	else
		num_new = frame;
	f->fileset = realloc (f->fileset, (num_new+f->cnt)*sizeof(fsetEntry));
	for (i = 0; i < num_new; i++) {
		if (frame < 0 || i == 0)
			f->fileset[f->cnt].name = strdup (name);
		else
			f->fileset[f->cnt].name = f->fileset[f->cnt - i].name;
		if (frame < 0)
			f->fileset[f->cnt].frame = frame;
		else
			f->fileset[f->cnt].frame = i;
		f->fileset[f->cnt].rgb = f->rgb;
		f->fileset[f->cnt].frameadv = f->frameadv;
		if (time) {
			f->fileset[f->cnt].time = *time;
		} else {
			f->fileset[f->cnt].time.tv_sec = 0;
			f->fileset[f->cnt].time.tv_usec = 0;
		}
		f->cnt++;
	}
}

/*********************************************************************
  Return TRUE, if fname should be opened to check for the number of
  frames (if fname is a movie file).
*********************************************************************/
static BOOL fileset_open_file (char *fname, BOOL use_ext)
{
	static char *ext[] = {
		".asf", ".avi", ".bin", ".divx", ".flc", ".fli", ".m1v",
		".m4v",".mjpeg", ".mjpg", ".mkv", ".mov", ".mpeg", ".mpg",
		".nuv", ".ogg", ".ogm", ".qt", ".rm", ".viv", ".vob", ".wmv",
		NULL};
	char *fext;
	int i;

	if (!use_ext) return TRUE;

	fext = strrchr (fname, '.');
	for (i=0; fext && ext[i]; i++)
		if (!g_strcasecmp (ext[i], fext))
			return TRUE;
	return FALSE;
}

static void fset_add_file (fsetList *fset, char *name,
						   struct timeval *time)
{
	iwMovie *avi;
	int cnt = -1;

	avi = NULL;
	if (fileset_open_file (name, fset->use_ext))
		avi = iw_movie_open (name, NULL);
	if (avi) {
		cnt = iw_movie_get_framecnt (avi);
		if (cnt <= 1)
			cnt = -1;
		iw_movie_close (avi);
	}
	fileset_add (fset, name, cnt, time);
}

typedef struct fsetDirEntry {
	char *name;
	int sec, msec, seq;
} fsetDirEntry;

/*********************************************************************
  qsort() compare function for directory entries.
*********************************************************************/
static int fset_entry_cmp (fsetDirEntry *small, fsetDirEntry *big)
{
	int ret = 0;

	if (big->msec >= 0 && big->msec != small->msec)
		ret = small->msec - big->msec;
	if (big->sec >= 0 && big->sec != small->sec)
		ret = small->sec - big->sec;
	if (big->seq >= 0 && big->seq != small->seq)
		ret = small->seq - big->seq;

	return ret;
}

/*********************************************************************
  Add files to the file set. If
    1. file 'name' exists it is added, otherwise
    2. 'name' contains '%t' or '%T', maching files from the directory
       are added, otherwise
    3. the files {'sprintf (file, name, 0++)'} are checked.
  File names r R y Y e E f F are interpreted as flags for
    RGB|YUV|use_ext|frameadv.
  r | y: All folowing files are RGB|YUV images.
  E    : Files are only opened to check the frame count if
         a known extension for movie files is used.
  f | F: Return frames according to the FPS value |
         Return only the frames stored in the movie
  Returns number of entries currently in the file set.
*********************************************************************/
int iw_fset_add (fsetList **fset, char *name)
{
	char *p;
	fsetList *f;

	if (!*fset) {
		*fset = calloc (1, sizeof(fsetList));
		(*fset)->rgb = TRUE;
	}
	f = *fset;

	if (name[1] == '\0') {
		if (toupper((int)name[0]) == 'R') {
			f->rgb = TRUE;
			return f->cnt;
		} else if (toupper((int)name[0]) == 'Y') {
			f->rgb = FALSE;
			return f->cnt;
		} else if (name[0] == 'e') {
			f->use_ext = FALSE;
			return f->cnt;
		} else if (name[0] == 'E') {
			f->use_ext = TRUE;
			return f->cnt;
		} else if (name[0] == 'f') {
			f->frameadv = FALSE;
			return f->cnt;
		} else if (name[0] == 'F') {
			f->frameadv = TRUE;
			return f->cnt;
		}
	}

	if (file_exist (name)) {
		/* Add single file */
		fset_add_file (f, name, NULL);
	} else if (((p = strstr(name, "%t")) && (p == name || *(p-1) != '%')) ||
			   ((p = strstr(name, "%T")) && (p == name || *(p-1) != '%'))) {
		/* Scan directory for matching files */
		char *dname, *dend, *format, *p2, *p3;
		struct stat statbuf;
		struct dirent *entry;
		DIR *dir;
		int len, val_cnt, val_sec, val_msec, val_seq, val_read;
		int *vals[3];
		fsetDirEntry *dir_entry;
		int dir_cnt, dir_max;

		dir_max = dir_cnt = 0;
		dir_entry = NULL;

		/* Get directory part from name */
		p = strrchr (name, '/');
		if (p) {
			len = p-name+1;
			dname = malloc (len+1+PATH_MAX);
			strncpy (dname, name, len);
			dname[len] = '\0';
		} else {
			len = 2;
			dname = malloc (len+1+PATH_MAX);
			strcpy (dname, "./");
		}
		dname[len+PATH_MAX] = '\0';
		dend = dname+len;

		/* Get file part from name for a pattern during scanning of the directory */
		if (p)
			p++;
		else
			p = name;
		format = malloc (strlen(p)+3);
		strcpy (format, p);
		strcat (format, "%n");

		/* Get order of the three conversion specifications.
		   First get their positions and their number... */
		val_cnt = 0;
		val_sec = val_msec = val_seq = -1;
		if ((p3 = strstr(format, "%d"))) {	/* == val_seq */
			val_cnt++;
		} else
			p3 = format+99999;
		if ((p2 = strstr(format, "%t"))) {	/* == val_msec */
			*(p2+1) = 'd';
			val_cnt++;
		} else
			p2 = format+99999;
		if ((p = strstr(format, "%T"))) {	/* == val_sec */
			*(p+1) = 'd';
			val_cnt++;
		} else
			p = format+99999;

		/* ...and then sort the positions. */
		if (p < p2) {
			if (p < p3) {
				vals[0] = &val_sec;
				if (p2 < p3) {
					vals[1] = &val_msec;
					vals[2] = &val_seq;
				} else {
					vals[1] = &val_seq;
					vals[2] = &val_msec;
				}
			} else {
				vals[0] = &val_seq;
				vals[1] = &val_sec;
				vals[2] = &val_msec;
			}
		} else {
			if (p2 < p3) {
				vals[0] = &val_msec;
				if (p < p3) {
					vals[1] = &val_sec;
					vals[2] = &val_seq;
				} else {
					vals[1] = &val_seq;
					vals[2] = &val_sec;
				}
			} else {
				vals[0] = &val_seq;
				vals[1] = &val_msec;
				vals[2] = &val_sec;
			}
		}

		/* Get all directory entries which
		   a) are files and b) fit to the name format specification */
		if ((dir = opendir (dname))) {
			while ((entry = readdir (dir))) {
				strncpy (dend, entry->d_name, PATH_MAX);
				if (!stat(dname, &statbuf) && S_ISREG(statbuf.st_mode)) {
					if ( (val_cnt == 3 &&
						  sscanf (dend, format, vals[0], vals[1], vals[2], &val_read) == 3) ||
						 (val_cnt == 2 &&
						  sscanf (dend, format, vals[0], vals[1], &val_read) == 2) ||
						 (val_cnt == 1 &&
						  sscanf (dend, format, vals[0], &val_read) == 1)) {
						if (val_read != strlen(dend))
							continue;
						if (dir_cnt >= dir_max) {
							dir_max += 50;
							dir_entry = realloc (dir_entry, sizeof(fsetDirEntry) * dir_max);
						}
						if (val_msec > 1000) {
							val_sec += val_msec/1000;
							val_msec = val_msec%1000;
						}
						dir_entry[dir_cnt].name = strdup (dname);
						dir_entry[dir_cnt].sec = val_sec;
						dir_entry[dir_cnt].msec = val_msec;
						dir_entry[dir_cnt].seq = val_seq;
						dir_cnt++;
						/*
						  printf ("|%s| sec %d msec %d seq %d\n",
								  dname, val_sec, val_msec, val_seq);
						*/
					}
				}
			}
			closedir (dir);
		}

		/* Sort directory entries and append them to the file set */
		if (dir_entry) {
			qsort (dir_entry, dir_cnt, sizeof(fsetDirEntry), (int(*)())fset_entry_cmp);
			for (len=0; len < dir_cnt; len++) {
				/*
				  printf ("Sort |%s| sec %d msec %d seq %d\n",
						  dir_entry[len].name,
						  dir_entry[len].sec, dir_entry[len].msec, dir_entry[len].seq);
				*/
				if (dir_entry[len].sec >= 0 || dir_entry[len].msec >= 0) {
					struct timeval time = {0, 0};
					if (dir_entry[len].sec >= 0)
						time.tv_sec = dir_entry[len].sec;
					if (dir_entry[len].msec >= 0)
						time.tv_usec = 1000*dir_entry[len].msec;
					fset_add_file (f, dir_entry[len].name, &time);
				} else
					fset_add_file (f, dir_entry[len].name, NULL);
				free (dir_entry[len].name);
			}
			free (dir_entry);
		}
		free (dname);
		free (format);
	} else {
		/* Scan for consecutively numbered list of files */
		char fname[PATH_MAX];
		int act = 0;
		sprintf (fname, name, act++);
		while (file_exist (fname)) {
			fset_add_file (f, fname, NULL);
			sprintf (fname, name, act++);
		}
	}

	return f->cnt;
}

/*********************************************************************
  Open the avi-file name and return its frame 'frame'.
  timestep!=NULL: If an animation is read return #ms before
				  showing the new image.
*********************************************************************/
static iwImage* avi_read (fsetList *fset, char *name,
						  int frame, double *framerate, iwImgStatus *status)
{
	iwImage *img;

	/* Check if a new file should be opened */
	if (fset->avi_file && strcmp (fset->avi_name, name)) {
		iw_movie_close (fset->avi_file);
		fset->avi_file = NULL;
	}

	if (!fset->avi_file) {
		strncpy (fset->avi_name, name, PATH_MAX-1);
		fset->avi_name[PATH_MAX-1] = '\0';
	}

	img = iw_movie_read (&fset->avi_file, fset->avi_name, frame, status);

	if (framerate) {
		if (fset->avi_file)
			*framerate = iw_movie_get_framerate (fset->avi_file);
		else
			*framerate = 0;
	}

	return img;
}

/*********************************************************************
  Read current image and return it, including different meta
  information about the image.
  direction: 0 | IW_MOVIE_NEXT_FRAME | IW_MOVIE_PREV_FRAME
*********************************************************************/
iwImgStatus iw_fset_get (fsetList *fset, fsetImage *i, int direction)
{
	int frame;
	iwImgStatus status;

	if (fset->act >= fset->cnt) {
		iw_debug (4, "end of stream set reached before %d", fset->act);
		fset->act = 0;
		return IW_IMG_STATUS_EOS;
	}

	strcpy (i->fname, fset->fileset[fset->act].name);
	frame = fset->fileset[fset->act].frame;
	i->frame_num = frame + 1;
	i->rgb = fset->fileset[fset->act].rgb;
	i->img = NULL;

	/* Try reading an avi file */
	if (frame >= 0) {
		double framerate;
		if (direction && fset->fileset[fset->act].frameadv) {
			i->img = avi_read (fset, i->fname, direction, &framerate, &status);
			fset->act += iw_movie_get_framepos(fset->avi_file) - frame;
		} else
			i->img = avi_read (fset, i->fname, frame, &framerate, &status);
		if (i->img) {
			i->time = fset->fileset[fset->act].time;
			if (i->time.tv_sec == 0 && i->time.tv_usec == 0) {
				i->time.tv_usec = 1000*1000 / framerate;
			} else {
				if (frame > framerate) {
					int f = (int)(frame/framerate);
					i->time.tv_sec += f;
					frame -= f*framerate;
				}
				i->time.tv_usec += frame*1000*1000 / framerate;
				if (i->time.tv_usec >= 1000*1000) {
					i->time.tv_sec++;
					i->time.tv_usec -= 1000*1000;
				}
			}
		}
	}
	/* Try reading a bitmap image */
	if (!i->img) {
		if (fset->image)
			iw_img_free (fset->image, IW_IMG_FREE_ALL);
		i->img = fset->image = iw_img_load (i->fname, &status);
		i->time = fset->fileset[fset->act].time;

		/* Try the movie reader if not already done */
		if (!i->img && frame < 0)
			i->img = avi_read (fset, i->fname, 0, NULL, &status);
	}
	i->fset_pos = fset->act;

	return status;
}
