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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "main.h"
#include "rc.h"
#include "error.h"
#include "init.h"
#include "gui-window_main.h"
#include "utils.h"
#include "debug.h"

void rc_save (void) {
  char *buf;
  FILE *fd;
  
  buf = g_strconcat (getenv ("HOME"), ROOT, "/cronos.rc", NULL);

  if ((fd = fopen (buf, "w")) == NULL) {
    cronos_error (errno, "Opening rc file for writing", ERROR_WARNING);
    c2_free (buf);
    return;
  }
  c2_free (buf);

  fprintf (fd, "h_pan %d\n", GTK_PANED (WMain->hpaned)->child1_size);
  fprintf (fd, "v_pan %d\n", GTK_PANED (WMain->vpaned)->child1_size);
  fprintf (fd, "mime_win_mode %d\n", rc->mime_win_mode);
  fprintf (fd, "clist_0 %d\n", GTK_CLIST (WMain->clist)->column[0].width);
  fprintf (fd, "clist_1 %d\n", GTK_CLIST (WMain->clist)->column[1].width);
  fprintf (fd, "clist_2 %d\n", GTK_CLIST (WMain->clist)->column[2].width);
  fprintf (fd, "clist_3 %d\n", GTK_CLIST (WMain->clist)->column[3].width);
  fprintf (fd, "clist_4 %d\n", GTK_CLIST (WMain->clist)->column[4].width);
  fprintf (fd, "clist_5 %d\n", GTK_CLIST (WMain->clist)->column[5].width);
  fprintf (fd, "clist_6 %d\n", GTK_CLIST (WMain->clist)->column[6].width);
  fprintf (fd, "clist_7 %d\n", GTK_CLIST (WMain->clist)->column[7].width);
  fprintf (fd, "font_read \"%s\"\n", rc->font_read);
  fprintf (fd, "font_unread \"%s\"\n", rc->font_unread);
  fprintf (fd, "font_body \"%s\"\n", rc->font_body);
  fprintf (fd, "font_print \"%s\"\n", rc->font_print);
  fprintf (fd, "title \"%s\"\n", rc->title);
  fprintf (fd, "toolbar %d\n", rc->toolbar);
  fprintf (fd, "main_window_width %d\n", rc->main_window_width);
  fprintf (fd, "main_window_height %d\n", rc->main_window_height);
  fprintf (fd, "\"showable_headers:preview\" %d\n", rc->showable_headers[SHOWABLE_HEADERS_PREVIEW]);
  fprintf (fd, "\"showable_headers:message\" %d\n", rc->showable_headers[SHOWABLE_HEADERS_MESSAGE]);
  fprintf (fd, "\"showable_headers:compose\" %d\n", rc->showable_headers[SHOWABLE_HEADERS_COMPOSE]);
  fprintf (fd, "\"showable_headers:save\" %d\n", rc->showable_headers[SHOWABLE_HEADERS_SAVE]);
  fprintf (fd, "\"showable_headers:print\" %d\n", rc->showable_headers[SHOWABLE_HEADERS_PRINT]);
#if FALSE
  fprintf (fd, "sort_column %d\n", rc->sort_column);
  fprintf (fd, "sort_type %d\n", rc->sort_type);
#endif
  fclose (fd);
}

void rc_init (void) {
  char *key;
  char *val;
  char *buf;
  FILE *fd;

  buf = g_strconcat (getenv ("HOME"), ROOT, "/cronos.rc", NULL);
  rc = g_new0 (Rc, 1);

  rc->h_pan		= 120;
  rc->v_pan		= 120;
  rc->clist_0		= 20;
  rc->clist_1		= 10;
  rc->clist_2		= 10;
  rc->clist_3		= 150;
  rc->clist_4		= 150;
  rc->clist_5		= 100;
  rc->clist_6		= 65;
  rc->clist_7		= 15;
  rc->font_body		= g_strdup ("-adobe-times-medium-r-normal-*-*-140-*-*-p-*-iso8859-1");
  rc->font_unread	= g_strdup ("-b&h-lucida-bold-r-normal-*-*-100-*-*-p-*-iso8859-1");
  rc->font_read		= g_strdup ("-b&h-lucida-medium-r-normal-*-*-100-*-*-p-*-iso8859-1");
  rc->font_print	= g_strdup ("-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-iso8859-1");
/*  set window manager title here "Conversion chars: */
/* %a = App Name (Cronos II),%v = Version, */
/* %m = Messages in selected mailbox, */
/* %n = New messages in selected mailbox, */
/* %M = Selected mailbox. */
  rc->title		= g_strdup ("%a v.%v - %M - %m"); 
  rc->toolbar		= GTK_TOOLBAR_BOTH;
  rc->main_window_width	= 600;
  rc->main_window_height= 400;
  rc->mime_win_mode	= MIME_WIN_AUTOMATICALLY;
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] = SHOWABLE_HEADER_FIELD_TO | SHOWABLE_HEADER_FIELD_FROM | SHOWABLE_HEADER_FIELD_SUBJECT;
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] = SHOWABLE_HEADER_FIELD_TO | SHOWABLE_HEADER_FIELD_FROM | SHOWABLE_HEADER_FIELD_SUBJECT | SHOWABLE_HEADER_FIELD_DATE | SHOWABLE_HEADER_FIELD_ACCOUNT | SHOWABLE_HEADER_FIELD_CC;
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] = SHOWABLE_HEADER_FIELD_TO | SHOWABLE_HEADER_FIELD_FROM | SHOWABLE_HEADER_FIELD_ACCOUNT | SHOWABLE_HEADER_FIELD_SUBJECT | SHOWABLE_HEADER_FIELD_CC;
#if FALSE
  rc->sort_column	= 5;
  rc->sort_type		= GTK_SORT_ASCENDING;
#endif

  if ((fd = fopen (buf, "r")) == NULL) {
    cronos_error (errno, "Opening rc file for reading", ERROR_WARNING);
    c2_free (buf);
    return;
  }
  c2_free (buf);

  for (;;) {
    if ((key = fd_get_word (fd)) == NULL) break;
    if ((val = fd_get_word (fd)) == NULL) break;
    if (fd_move_to (fd, '\n', 1, TRUE, TRUE) == EOF) fseek (fd, -1, SEEK_CUR);

    if (!strcmp (key, "h_pan")) {
      rc->h_pan = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "v_pan")) {
      rc->v_pan = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "mime_win_mode")) {
      rc->mime_win_mode = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_0")) {
      rc->clist_0 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_1")) {
      rc->clist_1 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_2")) {
      rc->clist_2 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_3")) {
      rc->clist_3 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_4")) {
      rc->clist_4 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_5")) {
      rc->clist_5 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_6")) {
      rc->clist_6 = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "clist_7")) {
      rc->clist_7 = atoi (val);
      c2_free (val);
    } else
   if (!strcmp (key, "font_read")) {
      c2_free (rc->font_read);
      rc->font_read = val;
    } else
    if (!strcmp (key, "font_unread")) {
      c2_free (rc->font_unread);
      rc->font_unread = val;
    } else
    if (!strcmp (key, "font_body")) {
      c2_free (rc->font_body);
      rc->font_body = val;
    } else
    if (!strcmp (key, "font_print")) {
      c2_free (rc->font_print);
      rc->font_print = val;
    } else
    if (!strcmp (key, "title")) {
      c2_free (rc->title);
      rc->title = val;
    } else
    if (!strcmp (key, "toolbar")) {
      rc->toolbar = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "main_window_width")) {
      rc->main_window_width = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "main_window_height")) {
      rc->main_window_height = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "showable_headers:preview")) {
      rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "showable_headers:message")) {
      rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "showable_headers:compose")) {
      rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "showable_headers:print")) {
      rc->showable_headers[SHOWABLE_HEADERS_PRINT] = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "showable_headers:save")) {
      rc->showable_headers[SHOWABLE_HEADERS_SAVE] = atoi (val);
      c2_free (val);
    }
#if FALSE
    else
    if (!strcmp (key, "sort_column")) {
      rc->sort_column = atoi (val);
      c2_free (val);
    } else
    if (!strcmp (key, "sort_type")) {
      rc->sort_type = atoi (val);
      c2_free (val);
    }
#endif
    else {
      buf = g_strconcat ("Unknown command in rc file: ", key, NULL);
      cronos_error (ERROR_INTERNAL, buf, ERROR_WARNING);
      c2_free (val);
    }

    c2_free (key);
  }

  fclose (fd);
}
