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
#ifndef WINDOW_COMPOSER_H
#define WINDOW_COMPOSER_H

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
#  include "gui-window_main.h"
#  include "mailbox.h"
#  include "message.h"
#else
#  include <cronosII.h>
#endif

#define C2_WIDGETS_COMPOSER_NEW ((WidgetsComposer *) g_new0 (WidgetsComposer, 1))

typedef enum {
  C2_COMPOSER_NEW,
  C2_COMPOSER_REPLY,
  C2_COMPOSER_REPLY_ALL,
  C2_COMPOSER_FORWARD,
  C2_COMPOSER_DRAFTS,
  C2_COMPOSER_LAST
} C2ComposerType;

typedef struct {
  Message *message;
  C2ComposerType type;
  GtkWidget *window;
  GtkWidget *toolbar;
  GtkWidget *menu_send;
  GtkWidget *tool_send;
  GtkWidget *tool_sendqueue;
  GtkWidget *menu_sendqueue;
  GtkWidget *tb_btn_close;
  GtkWidget *header_notebook;
  GtkWidget *header_table;
  GtkWidget *header_titles[HEADER_TITLES_LAST-2][3];
  GtkWidget *body;
  GtkWidget *icon_list; int selected_icon;
  GtkWidget *appbar;
  gboolean body_changed;
  gboolean sending;
  guint changed_signal;
  int drafts_mid;
  int queue_mid;
} WidgetsComposer;

void
c2_composer_new							(Message *message, C2ComposerType type);

#ifdef __cplusplus
}
#endif

#endif
