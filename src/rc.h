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
#ifndef RC_H
#define RC_H

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
#  include "debug.h"
#else
#  include <cronosII.h>
#endif

typedef enum {
  SHOWABLE_HEADER_FIELD_TO	= 1 << 0,
  SHOWABLE_HEADER_FIELD_DATE	= 1 << 1,
  SHOWABLE_HEADER_FIELD_FROM	= 1 << 2,
  SHOWABLE_HEADER_FIELD_SUBJECT = 1 << 3,
  SHOWABLE_HEADER_FIELD_ACCOUNT	= 1 << 4,
  SHOWABLE_HEADER_FIELD_CC	= 1 << 5,
  SHOWABLE_HEADER_FIELD_BCC	= 1 << 6,
  SHOWABLE_HEADER_FIELD_PRIORITY= 1 << 7
} ShowableHeaderField;

enum {
  SHOWABLE_HEADERS_PREVIEW,
  SHOWABLE_HEADERS_MESSAGE,
  SHOWABLE_HEADERS_COMPOSE,
  SHOWABLE_HEADERS_SAVE,
  SHOWABLE_HEADERS_PRINT,
  SHOWABLE_HEADERS_LAST
};

typedef enum {
  MIME_WIN_STICKY,
  MIME_WIN_AUTOMATICALLY
} MimeWinMode;

typedef struct {
  int h_pan;
  int v_pan;
  MimeWinMode mime_win_mode;
  int clist_0;
  int clist_1;
  int clist_2;
  int clist_3;
  int clist_4;
  int clist_5;
  int clist_6;
  int clist_7;
  char *font_read;
  char *font_unread;
  char *font_body;
  char *font_print;
  char *title;
  int main_window_width;
  int main_window_height;
  GtkToolbarStyle toolbar;
  guint8 showable_headers[SHOWABLE_HEADERS_LAST];
#if FALSE
  int sort_column;
  GtkSortType sort_type;
#endif
} Rc;

Rc *rc;

void
rc_init									(void);

void
rc_save									(void);

#ifdef __cplusplus
}
#endif

#endif
