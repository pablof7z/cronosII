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
#ifndef SELECT_FILE_H
#define SELECT_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#if HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif

typedef struct {
  char *str_file;
  char *str_actual_dir;
  guint file_changed_signal_id;
  gboolean ok;

  GtkWidget *window;
  GtkWidget *top_vbox;
  GtkWidget *top_hbox;
  GtkWidget *up_btn;
  GtkWidget *home_btn;
  GtkWidget *clist_scroll;
  GtkWidget *clist;
  GtkWidget *file_hbox;
  GtkWidget *file;
  GtkWidget *button_hbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *help_btn;
} SelectFile;

char *general_path;
char *last_path;

void
update_select_file							(SelectFile *);

char *
gui_select_file_new						(const char *, const char *,
    								 const char *, const char *);

#ifdef __cplusplus
}
#endif

#endif
