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
#ifndef GUI__WINDOW_MAIN_H
#define GUI__WINDOW_MAIN_H

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
#  include "init.h"
#  include "debug.h"
#  include "gui-window_main_callbacks.h"
#else
#  include <cronosII.h>
#endif

#define update_wm_title()	update_title ((GTK_CLIST (WMain->clist)->selection) ? GTK_CLIST (WMain->clist)->rows : 0, new_messages)

#define SEARCH_IS_WORKING FALSE

enum {
  HEADER_TITLES_FROM,
  HEADER_TITLES_TO,
  HEADER_TITLES_CC,
  HEADER_TITLES_BCC,
  HEADER_TITLES_SUBJECT,
  HEADER_TITLES_ACCOUNT,
  HEADER_TITLES_DATE,
  HEADER_TITLES_PRIORITY,
  HEADER_TITLES_LAST
};

typedef struct {
  GtkWidget *get_new_mail;
  GtkWidget *sendqueue;
  GtkWidget *compose;
  GtkWidget *save;
  GtkWidget *print;
  GtkWidget *search;
  GtkWidget *_delete;
  GtkWidget *reply;
  GtkWidget *reply_all;
  GtkWidget *forward;
  GtkWidget *previous;
  GtkWidget *next;
#if DEBUG
  GtkWidget *debug;
#endif
  GtkWidget *quit;
} WindowMainToolbar;

typedef struct {
  GtkWidget *file_menu;
    GtkWidget *get_new_mail;
      GtkWidget *get_new_mail_sep;
    GtkWidget *menu_sendqueue;
    GtkWidget *persistent_smtp_options;
      GtkWidget *persistent_smtp_options_connect;
      GtkWidget *persistent_smtp_options_disconnect;
    GtkWidget *quit;
  GtkWidget *edit_menu;
    GtkWidget *search;
  GtkWidget *message_menu;
    GtkWidget *compose;
    GtkWidget *save;
    GtkWidget *print;
    GtkWidget *reply;
    GtkWidget *reply_all;
    GtkWidget *forward;
    GtkWidget *copy;
    GtkWidget *move;
    GtkWidget *_delete;
    GtkWidget *expunge;
    GtkWidget *previous;
    GtkWidget *next;
    GtkWidget *mark;
  GtkWidget *settings;
    GtkWidget *preferences;
  GtkWidget *help;
    GtkWidget *about;
  GtkWidget *attach_menu;
    GtkWidget *attach_menu_sep;
} WindowMainMenubar;

typedef struct {
  GtkWidget *window;
  GtkWidget *hpaned;
  GtkWidget *toolbar;
  GtkWidget *vpaned;
  GtkWidget *ctree;
  GtkWidget *clist;
  GtkWidget *mime_left, *mime_right;
#if FALSE
  GtkWidget *clist_arrow_up[5];
  GtkWidget *clist_arrow_down[5];
#endif
  GtkWidget *menu_clist;
  GtkWidget *header_table;
  GtkWidget *header_titles[HEADER_TITLES_LAST][2];
  GtkWidget *text;
  GtkWidget *mime_scroll;
  GtkWidget *icon_list;
  GtkWidget *appbar;
  GtkWidget *progress;

  GtkTooltips *tips;

  WindowMainToolbar tb_w;
  WindowMainMenubar mb_w;
} WindowMain;

WindowMain *WMain;
char *selected_mbox;
mid_t selected_mid;
unsigned int selected_row;
unsigned int new_messages;
gboolean status_is_busy;
guint32 check_timeout;

GSList *tmp_files;

void
gui_window_main_new								(void);

void
main_window_menubar								(void);

GtkWidget *
main_window_menu_attach_add_item						(const char *label,
    										 const char *pixmap,
										 GtkSignalFunc func,
										 gpointer data);
void
main_window_menu_attach_clear							(void);

void
main_window_make_account_menu							(void);

void
main_window_toolbar								(void);

#ifdef __cplusplus
}
#endif

#endif
