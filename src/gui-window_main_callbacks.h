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
#ifndef WINDOW_MAIN_CALLBACKS_H
#define WINDOW_MAIN_CALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#ifdef HAVE_CONFIG_H
#  include <config.h>
#  include "account.h"
#  include "debug.h"
#else
#  include <cronosII.h>
#endif

void
update_title							(int, int);

void
on_wm_mime_select_icon						(GnomeIconList *gil, int num, GdkEvent *event);

void
on_wm_mime_open_clicked						(void);

void
on_wm_mime_button_pressed					(GtkWidget *widget,
    								 GdkEvent *event, gpointer data);

void
on_wm_mime_view_clicked						(void);

void
on_wm_mime_save_clicked						(void);

void
on_wm_mime_stick_btn_clicked					(GtkWidget *button);

void
on_wm_mime_left_btn_clicked					(void);

void
on_wm_mime_right_btn_clicked					(void);

void
on_wm_delete_clicked						(void);

void
on_wm_expunge_clicked						(void);

void
on_wm_copy_mail_clicked						(void);

void
on_wm_move_mail_clicked						(void);

void
on_wm_reply_clicked						(void);

void
on_wm_reply_all_clicked						(void);

void
on_wm_forward_clicked						(void);

void
on_wm_quit							(void);

void
on_wm_ctree_select_row						(GtkCTree *tree, gint row, gint column,
    								 GdkEvent *event, gpointer data);

void
on_wm_clist_select_row						(GtkWidget *, gint, gint, GdkEventButton *,
    								 gpointer);

void
on_wm_clist_button_press_event					(GtkWidget *, GdkEvent *, gpointer);

#ifndef USE_OLD_MBOX_HANDLERS
void
on_wm_mailbox_speed_up_clicked					(void);
#endif

void
on_wm_check_account						(GtkWidget *widget, gpointer data);

void
on_wm_check_clicked						(void);

void
wm_sendqueue_clicked_thread 					(void);

void
on_wm_sendqueue_clicked						(void);

void
on_wm_btn_show_check_clicked					(void);

void
on_wm_compose_clicked						(void);

void
on_wm_previous_clicked						(void);

void
on_wm_next_clicked						(void);

void
on_wm_mark_as_unreaded_clicked					(void);

void
on_wm_mark_as_readed_clicked					(void);

void
on_wm_mark_as_replied_clicked					(void);

void
on_wm_mark_as_forwarded_clicked					(void);

void
on_wm_view_menu_from_activate					(GtkWidget *widget);

void
on_wm_view_menu_account_activate				(GtkWidget *widget);

void
on_wm_view_menu_to_activate					(GtkWidget *widget);

void
on_wm_view_menu_cc_activate					(GtkWidget *widget);

void
on_wm_view_menu_bcc_activate					(GtkWidget *widget);

void
on_wm_view_menu_date_activate					(GtkWidget *widget);

void
on_wm_view_menu_subject_activate				(GtkWidget *widget);

void
on_wm_view_menu_priority_activate				(GtkWidget *widget);

void
on_wm_save_clicked						(void);

void
on_wm_print_clicked						(void);

void
on_wm_text_button_press_event					(GtkWidget *text, GdkEvent *event);

#if FALSE
int
on_wm_clist_msg_state_sort					(GtkCList *clist, gconstpointer s1,
    								 gconstpointer s2);

int
on_wm_clist_date_sort						(GtkCList *clist, gconstpointer s1,										gconstpointer s2);

int
on_wm_clist_str_sort						(GtkCList *clist, gconstpointer s1,
    								 gconstpointer s2);

void
on_wm_clist_click_column					(GtkWidget *clist, int column,
    								 gpointer user_data);
#endif

void
on_wm_persistent_smtp_connect					(void);

void
on_wm_persistent_smtp_disconnect				(void);

void
on_wm_address_book_clicked					(void);

void
on_wm_bug_report_clicked					(void);

void
on_wm_help_clicked						(void);

void
on_wm_size_allocate						(GtkWidget *window, GtkAllocation *alloc);

#if USE_PLUGINS
gboolean on_wm_focus_in_event					(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
