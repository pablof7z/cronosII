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
#include <gdk/gdk.h>
#include "gui-utils.h"
#include "gui-window_main.h"

#include "smtp.h"
#include "error.h"
#include "rc.h"
#include "utils.h"
#include "init.h"

void cronos_gui_init (void) {
  extern GdkCursor *cursor_busy;
  extern GdkCursor *cursor_normal;

  cursor_busy = gdk_cursor_new (GDK_WATCH);
  cursor_normal = gdk_cursor_new (GDK_LEFT_PTR);
  font_body = gdk_font_load (rc->font_body);
  font_read = gdk_font_load (rc->font_read);
  font_unread = gdk_font_load (rc->font_unread);
  gdk_color_alloc (gdk_colormap_get_system (), &config->color_reply_original_message);
  gdk_color_alloc (gdk_colormap_get_system (), &config->color_misc_body);
  g_set_warning_handler (cronos_gui_warning);
  g_set_message_handler (cronos_gui_message);
}

static GSList *c2_progress_set_active_timeout_list = NULL;

static void c2_progress_set_active_timeout (Pthread3 *helper) {
  gfloat value;
  
  g_return_if_fail (helper);

  value = gtk_progress_get_value (GTK_PROGRESS (helper->v1));
  if (helper->v2 == GTK_PROGRESS_LEFT_TO_RIGHT) {
    value++;
    gtk_progress_set_value (GTK_PROGRESS (helper->v1), value);
  } else {
    value--;
    gtk_progress_set_value (GTK_PROGRESS (helper->v1), value);
  }
  if (value == 100) helper->v2 = (void *) GTK_PROGRESS_RIGHT_TO_LEFT;
  if (value == 0) helper->v2 = (void *) GTK_PROGRESS_LEFT_TO_RIGHT;
}

gint c2_progress_set_active (GtkWidget *progress) {
  Pthread3 *helper;
  gint timeout;
  
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);
  
  helper = PTHREAD3_NEW;
  helper->v1 = (void *) progress;
  helper->v2 = (void *) GTK_PROGRESS_LEFT_TO_RIGHT;
  gtk_progress_set_activity_mode (GTK_PROGRESS (progress), TRUE);
  gtk_progress_configure (GTK_PROGRESS (progress), 0, 0, 100);
  timeout = gtk_timeout_add (10, (GtkFunction) c2_progress_set_active_timeout, helper);
  helper->v3 = (void *) timeout;
  c2_progress_set_active_timeout_list = g_slist_append (c2_progress_set_active_timeout_list, (gpointer) helper);
  return timeout;
}

void c2_progress_set_active_remove (gint id) {
  GSList *node;
  Pthread3 *helper;

  gtk_timeout_remove (id);
 
  for (node = c2_progress_set_active_timeout_list; node; node = node->next)
    if (((Pthread3 *) node->data)->v3 == (void *) id) break;
  if (!node) return;
  helper = (Pthread3 *) node->data;
 
  gtk_progress_set_activity_mode (GTK_PROGRESS (helper->v1), FALSE);
  gtk_progress_set_value (GTK_PROGRESS (helper->v1), 0);
  g_slist_remove (c2_progress_set_active_timeout_list, node);
}

#if FALSE
void cronos_gui_update_clist_titles (void) {
  int i;
  int col;

  col = rc->sort_column;

  if (col > 2 && col < 7) col -= 2;
  for (i = 0; i < 5; i++) {
    if (i == col) {
      if (rc->sort_type == GTK_SORT_ASCENDING) {
	gtk_widget_show (WMain->clist_arrow_up[i]);
	gtk_widget_hide (WMain->clist_arrow_down[i]);
      } else {
	gtk_widget_show (WMain->clist_arrow_down[i]);
	gtk_widget_hide (WMain->clist_arrow_up[i]);
      }
    } else {
      gtk_widget_hide (WMain->clist_arrow_down[i]);
      gtk_widget_hide (WMain->clist_arrow_up[i]);
    }
  }
}
#endif

void cronos_gui_set_sensitive (void) {
  gboolean theres_a_mail_selected;

  if (GTK_CLIST (WMain->clist)->selection)
    theres_a_mail_selected = TRUE;
  else
    theres_a_mail_selected = FALSE;
  
  if (config->account_head) {
    gtk_widget_set_sensitive (WMain->mb_w.get_new_mail, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.get_new_mail, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.compose, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.compose, TRUE);
  } else {
    gtk_widget_set_sensitive (WMain->mb_w.get_new_mail, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.get_new_mail, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.compose, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.compose, FALSE);
  }

  if (theres_a_mail_selected) {
    gtk_widget_set_sensitive (WMain->mb_w.copy, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.move, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.save, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.save, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.print, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.print, TRUE);
#if SEARCH_IS_WORKING
    gtk_widget_set_sensitive (WMain->mb_w.search, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.search, TRUE);
#endif
    gtk_widget_set_sensitive (WMain->mb_w._delete, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w._delete, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.expunge, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.reply, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.reply, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.reply_all, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.reply_all, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.forward, TRUE);
    gtk_widget_set_sensitive (WMain->tb_w.forward, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.mark, TRUE);
    if ((int)GTK_CLIST (WMain->clist)->selection->data == 0) {
      gtk_widget_set_sensitive (WMain->mb_w.previous, FALSE);
      gtk_widget_set_sensitive (WMain->tb_w.previous, FALSE);
    } else {
      gtk_widget_set_sensitive (WMain->mb_w.previous, TRUE);
      gtk_widget_set_sensitive (WMain->tb_w.previous, TRUE);
    }
    if ((int)GTK_CLIST (WMain->clist)->selection->data+1 == GTK_CLIST (WMain->clist)->rows) {
      gtk_widget_set_sensitive (WMain->mb_w.next, FALSE);
      gtk_widget_set_sensitive (WMain->tb_w.next, FALSE);
    } else {
      gtk_widget_set_sensitive (WMain->mb_w.next, TRUE);
      gtk_widget_set_sensitive (WMain->tb_w.next, TRUE);
    }
  } else {
    gtk_widget_set_sensitive (WMain->mb_w.copy, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.move, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.save, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.save, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.print, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.print, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.search, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.search, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w._delete, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w._delete, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.expunge, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.reply, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.reply, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.reply_all, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.reply_all, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.forward, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.forward, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.previous, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.previous, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.next, FALSE);
    gtk_widget_set_sensitive (WMain->tb_w.next, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.mark, FALSE);
  }

  if (!config->use_persistent_smtp_connection) {
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_connect, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_disconnect, FALSE);
  } else {
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options, TRUE);
    if (smtp_persistent_sock_is_connected ()) {
      gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_connect, FALSE);
      gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_disconnect, TRUE);
    } else {
      gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_connect, TRUE);
      gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_disconnect, FALSE);
    }
  }
}
