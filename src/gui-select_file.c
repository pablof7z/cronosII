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

#include "gnomefilelist.h"

#include "gui-window_main.h"
#include "gui-select_file.h"

#include "main.h"

gboolean ok_clicked = FALSE;

static void on_ok_btn_clicked (GtkWidget *gnomefilelist, gpointer data) {
  ok_clicked = TRUE;
  gtk_main_quit ();
}

static void on_cancel_btn_clicked (GtkWidget *gnomefilelist, gpointer data) {
  ok_clicked = FALSE;
  gtk_main_quit ();
}

char *gui_select_file_new (const char *path, const char *title, const char *filter, const char *file_name) {
  GtkWidget *gnomefilelist;
  char *ret;

  if (!path) path = last_path;
  
  if (path)
    gnomefilelist = gnome_filelist_new_with_path (path);
  else
    gnomefilelist = gnome_filelist_new ();

  if (file_name) gnome_filelist_select_file (GNOME_FILELIST (gnomefilelist), file_name);

  gtk_window_set_transient_for (GTK_WINDOW (gnomefilelist), GTK_WINDOW (WMain->window));
  
  if (title)
    gnome_filelist_set_title (GNOME_FILELIST (gnomefilelist), title);
  else
    gnome_filelist_set_title (GNOME_FILELIST (gnomefilelist), _("Select a file"));
  
  gtk_signal_connect (GTK_OBJECT (GNOME_FILELIST (gnomefilelist)->ok_button), "clicked",
                      GTK_SIGNAL_FUNC (on_ok_btn_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (GNOME_FILELIST (gnomefilelist)->cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cancel_btn_clicked),
                      NULL);

  gtk_widget_show (gnomefilelist);

  gtk_main ();
  
  if (ok_clicked)
    ret = gnome_filelist_get_filename (GNOME_FILELIST (gnomefilelist));
  else
    ret = NULL;
  gtk_window_set_modal (GTK_WINDOW (gnomefilelist), FALSE);
  gtk_widget_destroy (gnomefilelist);

  if (ret) {
    c2_free (last_path);
    last_path = g_dirname (ret);
  }
  
  return ret;
}
