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
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

#include "error.h"
#include "utils.h"
#include "debug.h"
#include "main.h"

#include "gui-window_main.h"

void cronos_status_error (const gchar *message) {
  if (WMain)
    gnome_appbar_push (GNOME_APPBAR (WMain->appbar), message);
  else
    printf ("%s\n", message);
}

void cronos_gui_warning (const gchar *message) {
  GtkWidget *window;
 
  window = gnome_warning_dialog (message);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  if (WMain)
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));
}

void cronos_gui_message (const gchar *message) {
  GtkWidget *window;
  
  window = gnome_ok_dialog (message);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  if (WMain)
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));
}

void cronos_error (int _errno, const char *description, unsigned short errtype) {
  char *errmsg=NULL;
  char *msg;

  errmsg = g_strerror (_errno);

  msg = g_strdup_printf ("%s: %s", description, errmsg ? errmsg : _("Unknown reason"));

  switch (errtype) {
    case ERROR_WARNING:
      cronos_status_error (msg);
      break;
    case ERROR_FATAL:
      cronos_gui_warning (msg);
      break;
    case ERROR_MESSAGE:
      cronos_gui_message (msg);
      break;
  }
}
