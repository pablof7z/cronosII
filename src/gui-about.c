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

#include "gui-about.h"
#include "gui-window_main.h"
#include "gui-logo.h"
#include "main.h"
#include "debug.h"
#include "main.h"

#if USE_GNOME
void gui_about_new (void) {
  GtkWidget *window;
  const char *authors[] = {    
    "Pablo Fernández Navarro <cronosII@users.sourceforge.net>",
    "Bosko Blagojevic <falling@users.sourceforge.net>",
    "Daniel Fairhead <ralphtheraccoon@uk2.net>",
    "André Casteliano <digitalcoder@users.sourceforge.net>",
    "Yves Mettier <ymettier@libertysurf.fr>",
    "Peter Gossner <gossner@arcom.com.au>",
	"Angel Ramos <seamus@debian.org>",
    NULL };
  GtkWidget *href;

  window = gnome_about_new ("CronosII",VERSION,"Debian/GNU edition (C) GPL 2000-2002 Pablo Fernández Navarro", authors, _("A fast flexible GNOME Mail Client."), DATADIR "/cronosII/pixmap/splash.png");
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));

  href= gnome_href_new ("http://www.cronosII.net",
      			_("Visit the Cronos II homepage"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), href, FALSE, TRUE, 0);
  gtk_widget_show (href);

  gtk_widget_show (window);
}
/*#else*/ /* Without GNOME */
/* this commented out for package builds  feb 2002 petergozz*/
/* void gui_about_new (void) { */
/*   GtkWidget *window; */
/*   GtkWidget *vbox; */
/*   GtkWidget *xpm; */
/*   GtkWidget *text; */
/*   GtkWidget *scroll; */
/*   GtkWidget *btn; */

/*   window = gtk_window_new (GTK_WINDOW_DIALOG); */
/*   gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window)); */
/* } */
#endif
