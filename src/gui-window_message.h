/* Cronos II
 * Copyright (C) 2000-2001 Pablo Fernández Navarro
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef GUI_WINDOW_MESSAGE_H
#define GUI_WINDOW_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#  include <config.h>
#  include "gui-window_main.h"
#  include "message.h"
#else
#  include <cronosII.h>
#endif

typedef struct {
  GtkWidget *window;
  GtkWidget *text;
  Message *message;
  gint destroy_signal;
} WindowMessageProperties;

typedef struct {
  GtkWidget *window;
  GtkWidget *header_titles[HEADER_TITLES_LAST][2];
  GtkWidget *header_table;
  GtkWidget *text;
  GtkWidget *icon_list;
  Message *message;
  gint destroy_signal;
} WindowMessage;

WindowMessage *
gui_window_message_new						(void);

WindowMessage *
gui_window_message_new_with_message				(Message *message);

void
gui_window_message_set_message					(WindowMessage *wm, Message *message);

WindowMessageProperties *
gui_window_message_properties_new				(void);

WindowMessageProperties *
gui_window_message_properties_new_with_message			(Message *message);

void
gui_window_message_properties_set_message			(WindowMessageProperties *wmp,
    								 Message *message);

#ifdef __cplusplus
}
#endif

#endif
