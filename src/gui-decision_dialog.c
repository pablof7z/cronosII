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
#include <stdarg.h>

#include "gui-window_main.h"

#include "debug.h"
#include "utils.h"

static int dec = 0;

static void on_btn_clicked (GtkWidget *widget, gpointer data) {
  dec = (int) data;
  gtk_main_quit ();
}

static void gui_question_cb (int reply, char **answer) {
  *answer = (char *) reply;
  gtk_main_quit ();
}

gboolean gui_question (char *title, char *leyend, GtkWidget *parent) {
  GtkWidget *window;
  char *answer = "0";

  window = gnome_question_dialog (leyend, (GnomeReplyCallback) gui_question_cb, &answer);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  if (GTK_IS_WINDOW (parent))
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (parent));
  gtk_widget_show (window);
  gtk_main ();
  gtk_widget_destroy (window);
  return ((gboolean) (!(int)answer));
}

int gui_decision_dialog (char *title, char *leyend, int argc, ...) {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *hsep;
  GtkWidget *btn;
  GtkWidget *label;
  int i;
  va_list arg;
  char *btn_label;

  window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_widget_set_usize (GTK_WIDGET (window), 325, 125);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);
  
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  label = gtk_label_new (leyend);
  gtk_box_pack_start (GTK_BOX (vbox), label , TRUE, TRUE, 0);
  gtk_widget_show (label);

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);
  
  va_start (arg, argc);
  
  for (i = 0; i < argc; i++) {
    btn_label = (char *) va_arg (arg, char *);
    if (streq (btn_label, _("OK")))
      btn = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
    else if (streq (btn_label, _("Yes")))
      btn = gnome_stock_button (GNOME_STOCK_BUTTON_YES);
    else if (streq (btn_label, _("No")))
      btn = gnome_stock_button (GNOME_STOCK_BUTTON_NO);
    else if (streq (btn_label, _("Save"))) {
      GtkWidget *tmp_box;
      GtkWidget *label;
      GtkWidget *xpm;

      tmp_box = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

      xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_SAVE, 22, 22);
      label = gtk_label_new (_("Save"));

      gtk_box_pack_start (GTK_BOX (tmp_box), xpm, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (tmp_box), label, TRUE, TRUE, 0);

      gtk_widget_show (xpm);
      gtk_widget_show (label);
      gtk_widget_show (tmp_box);
      
      btn = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (btn), tmp_box);
      gtk_widget_show (btn);
    } else
      btn = gtk_button_new_with_label (btn_label);
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
    gtk_widget_show (btn);
    gtk_signal_connect (GTK_OBJECT (btn), "clicked",
    			GTK_SIGNAL_FUNC (on_btn_clicked), (gpointer) i);
  }

  va_end (arg);
  
  gtk_widget_show (window);
  gtk_main ();
  gtk_widget_destroy (window);

  return dec;
}
