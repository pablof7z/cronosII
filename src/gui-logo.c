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

#include "gui-logo.h"
#include "gui-window_main.h"
#include "init.h"
#include "main.h"
#include "debug.h"
#include "main.h"

gint16 t_h;
GtkWidget *logo;

static void no_timeout (void) { 
  gtk_widget_destroy (logo);
  gtk_main_quit ();
  if (t_h != -1) gtk_timeout_remove ((guint16) t_h);
}

static gint timeout (gpointer data) {
  gtk_widget_destroy (logo);
  gtk_timeout_remove ((guint16) t_h);
  gtk_main_quit ();

  return 0;
}

void show_cronos_logo (guint16 time) {
  GtkWidget *pixmap;
  GtkWidget *vbox;
  GtkWidget *event;
  GtkWidget *href;

  t_h = -1;
  
  logo = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_realize (logo);
  gtk_window_set_modal (GTK_WINDOW (logo), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (logo), GTK_WINDOW (WMain->window));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (logo), vbox);
  gtk_widget_show (vbox);
  
  event = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox), event, TRUE, TRUE, 0);
  gtk_widget_show (event);
  gtk_signal_connect_object (GTK_OBJECT (event), "button_press_event",
		    	GTK_SIGNAL_FUNC (no_timeout), NULL);
  
  pixmap = gnome_pixmap_new_from_file (DATADIR "/cronosII/pixmap/splash.png");
  gtk_container_add (GTK_CONTAINER (event), pixmap);
  gtk_widget_show (pixmap);
  gtk_widget_shape_combine_mask (event, GNOME_PIXMAP (pixmap)->mask, 0, 0);

  href= gnome_href_new ("http://cronosII.sourceforge.net",
      			_("Visit the Cronos II homepage"));
  gtk_box_pack_start (GTK_BOX (vbox), href, FALSE, TRUE, 0);
  gtk_widget_show (href);

  gtk_window_set_position (GTK_WINDOW (logo), GTK_WIN_POS_CENTER);
  gtk_widget_show (logo);

  if (time != 0) {
    t_h = gtk_timeout_add (time, timeout, NULL);
  }

  gtk_main ();
}
