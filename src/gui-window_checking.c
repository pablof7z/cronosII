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
#include <pthread.h>

#include "gui-window_checking.h"
#include "gui-window_main.h"

#include "main.h"
#include "utils.h"

#include "xpm/mini_error.xpm"
#include "xpm/mini_about.xpm"

void window_checking_report (C2CheckStatus status, const char *acc_name, const char *info) {
  GdkPixmap *about_pixmap;
  GdkBitmap *about_mask;
  const char *append[3];

  append[0] = NULL;
  append[1] = acc_name;
  append[2] = info;

  gtk_clist_append (GTK_CLIST (window_checking->clist), (gchar **) append);

  if (status == C2_CHECK_OK) {
    about_pixmap = gdk_pixmap_create_from_xpm_d (window_checking->window->window, &about_mask,
  				&window_checking->window->style->bg[GTK_STATE_NORMAL],
				mini_about_xpm);
  } else {
    about_pixmap = gdk_pixmap_create_from_xpm_d (window_checking->window->window, &about_mask,
  				&window_checking->window->style->bg[GTK_STATE_NORMAL],
				mini_error_xpm);
  }

  gtk_clist_set_pixmap (GTK_CLIST (window_checking->clist), GTK_CLIST (window_checking->clist)->rows-1, 0,
      				about_pixmap, about_mask);
}

static void on_ok_btn_clicked (void) {
  gnome_dialog_close (GNOME_DIALOG (window_checking->window));
}

#if USE_PLUGINS
static void
on_focus_in_event_thread (void) {
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_WINDOW_FOCUS, "checker", NULL, NULL, NULL, NULL);
}

static void
on_focus_in_event (void) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_focus_in_event_thread), NULL);
}
#endif

void gui_window_checking_new (void) {
  GtkWidget *vbox, *scroll;

  window_checking = g_new0 (WWindowChecking, 1);

  window_checking->window = gnome_dialog_new (_("Cronos II: Get New Mail"), GNOME_STOCK_BUTTON_OK, NULL);
  gtk_widget_set_usize (window_checking->window, 400, 300);
  gnome_dialog_close_hides (GNOME_DIALOG (window_checking->window), TRUE);
#if USE_PLUGINS
  gtk_signal_connect (GTK_OBJECT (window_checking->window), "focus_in_event",
      			GTK_SIGNAL_FUNC (on_focus_in_event), NULL);
#endif

  vbox = GNOME_DIALOG (window_checking->window)->vbox;

  window_checking->acc_progress = gtk_progress_bar_new ();
  gtk_widget_show (window_checking->acc_progress);
  gtk_box_pack_start (GTK_BOX (vbox), window_checking->acc_progress, FALSE, FALSE, 0);
  gtk_progress_set_show_text (GTK_PROGRESS (window_checking->acc_progress), TRUE);

  window_checking->mail_progress = gtk_progress_bar_new ();
  gtk_widget_show (window_checking->mail_progress);
  gtk_box_pack_start (GTK_BOX (vbox), window_checking->mail_progress, FALSE, FALSE, 0);
  gtk_progress_set_show_text (GTK_PROGRESS (window_checking->mail_progress), TRUE);

  window_checking->bytes_progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), window_checking->bytes_progress, FALSE, FALSE, 0);
  gtk_progress_set_show_text (GTK_PROGRESS (window_checking->bytes_progress), TRUE);
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->bytes_progress), _("%p%% downloaded"));

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scroll);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 5);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
      					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  window_checking->clist = gtk_clist_new (3);
  gtk_widget_show (window_checking->clist);
  gtk_container_add (GTK_CONTAINER (scroll), window_checking->clist);
  gtk_clist_set_column_width (GTK_CLIST (window_checking->clist), 0, 23);
  gtk_clist_set_column_width (GTK_CLIST (window_checking->clist), 1, 80);
  gtk_clist_set_column_width (GTK_CLIST (window_checking->clist), 2, 80);
  gtk_clist_column_titles_hide (GTK_CLIST (window_checking->clist));

  gtk_signal_connect (GTK_OBJECT (window_checking->window), "delete_event",
      			GTK_SIGNAL_FUNC (on_ok_btn_clicked), NULL);
  gnome_dialog_button_connect (GNOME_DIALOG (window_checking->window), 0,
                      		GTK_SIGNAL_FUNC (on_ok_btn_clicked), NULL);
}
