/*  Cronos II
 *  Copyright (C) 2000-2001 Pablo Fernández Navarro
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE g_strconcat (getenv ("HOME"), ROOT, CONFIG, NULL)

#define CHAR(obj)		((char *) obj)

#define ALLOC(size)		g_malloc0(size)
#define CHARALLOC(size)		g_new0 (char, size+1)

#define PTHREAD2_NEW		g_new0 (Pthread2, 1)
#define PTHREAD3_NEW		g_new0 (Pthread3, 1)
#define PTHREAD2(obj)		((Pthread2 *) obj)
#define PTHREAD3(obj)		((Pthread3 *) obj)
#define PTHREAD4(obj)		((Pthread4 *) obj)

#define strne(x,y)		(!streq (x,y))
#define strnne(x,y,z)		(!strneq (x,y,z))
#define strcasene(x,y)		(!strcaseeq (x,y))
#define strncasene(x,y,z)	(!strncaseeq (x,y,z))

typedef void *(*PthreadFunc)	(void*);
#define PTHREAD_FUNC(f)		((PthreadFunc) f)

typedef guint32 gunichar;

typedef struct {
  gpointer v1;
  gpointer v2;
} Pthread2;

typedef struct {
  gpointer v1;
  gpointer v2;
  gpointer v3;
} Pthread3;

typedef struct {
  gpointer v1;
  gpointer v2;
  gpointer v3;
  gpointer v4;
} Pthread4;

char *
str_get_word						(guint8 word_n, const char *str, char ch);

char *
str_caps_lock_on					(const char *str);

char *
str_strip						(char *str, char ch);

char *
str_strip_enclosed					(const char *str, char open, char close);

gboolean
str_utf8_validate					(const gchar *str, gint max_len, const gchar **end);

char *
str_strip_non_utf8					(char *str);

gboolean
strcaseeq						(const char *fst, const char *snd);

gboolean
strncaseeq						(const char *fst, const char *snd, int length);

gboolean
streq							(const char *fst, const char *snd);

gboolean
strneq							(const char *fst, const char *snd, int length);

void
str_unquote						(char **str);

char *
str_get_line						(const char *str);

char *
str_replace						(const char *or_string, const char *se_string,
    							 const char *re_string);

const char *
strcasestr						(const char *_string, const char *_find);

char *
str_replace_all						(const char *or_string, const char *se_string,
    							 const char *re_string);

int
find_string						(const char *haystack, const char *needle);

char *
str_get_mail_address					(const char *to);

char *
strstrcase						(char *haystack, char *needle);

char *
strip_common_subject_prefixes				(char *string);

gboolean
c2_file_exists						(const char *file);

/* Returns the next word in the "fd" pointer */
char *
fd_get_word						(FILE *fd);

/* Returns the next line in the "fd" pointer */
char *
fd_get_line						(FILE *fd);

/* Returns true if path is a directory */
gboolean
fd_is_dir						(char *path);

/* 
 * Moves backward (!"forward") or "forward" ("forward") in the fd pointer "fp" searching for the character "c"
 * "cant" times and leaving the pointer before (!"next") or after ("next") "c"
 */
gboolean
fd_move_to						(FILE *fp, char c, guint8 cant,
    								 gboolean forward, gboolean next);

const char *
pixmap_get_icon_by_mime_type				(const char *mime_type);

/* Returns the path to a tmp file */
char *
cronos_tmpfile						(void);

int
fd_bin_cp						(const char *strfrm, const char *strdst);

/* Copy a file to other */
gboolean
fd_cp							(char *str_frm, char *str_dst);

/* Moves a file to other */
gboolean
fd_mv							(char *str_frm, char *str_dst);

typedef struct {
  gint day_of_week;
  gint day;
  gint month;
  gint year;
  gint hour;
  gint minute;
  gint second;
  gdouble original_tz_offset;
  gint bestguess;
} date_t;

date_t
parse_date						(char *datestr);

date_t
adjust_date_t_for_timezone				(date_t date);

date_t
parse_rfc822_date					(GList *tokens);

date_t
parse_bestguess_date					(GList *tokens);

gint
get_days_in_month					(gint month, gint year);

gint
weekday_toi						(gchar *string);

gint
month_toi						(gchar *string);

gdouble
rfc1123_timezone_tod					(gchar *timezonestring);

GList *
g_list_strtok						(char *string, char *delimeters);

char *
cronos_system						(const char *string);

#ifdef __cplusplus
}
#endif

#endif
