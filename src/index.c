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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "search.h"
#include "index.h"
#include "init.h"
#include "mailbox.h"
#include "debug.h"
#include "error.h"
#include "utils.h"
#include "rc.h"

#include "gui-window_main_callbacks.h"
#include "gui-window_main.h"
#include "gui-utils.h"

static void _index_mark_as (C2MarkMail *mark);

void index_mark_as (char *mbox, int row, mid_t mid, C2MarkType type, guint16 timeout) {
  C2MarkMail *mark;

  if (!mbox) return;

  mark = g_new0 (C2MarkMail, 1);

  mark->mbox = mbox;
  mark->row = row;
  mark->mid = mid;
  mark->type = type;

  if (timeout)
    reload_mailbox_list_timeout = gtk_timeout_add (timeout, (GtkFunction) _index_mark_as, mark);
  else {
    reload_mailbox_list_timeout = -1;
    _index_mark_as (mark);
  }
}

static void
_index_mark_as (C2MarkMail *mark) {
  char *path;
  char *line;
  char *mid;
  FILE *fd;

  gboolean was_unread;
  GtkStyle *style;
  
  g_return_if_fail (mark);
  
  /* Remove the timeout */
  if (reload_mailbox_list_timeout >= 0) {
    gtk_timeout_remove (reload_mailbox_list_timeout);
    reload_mailbox_list_timeout = -1;
  }

  /* Open the index file */
  path = c2_mailbox_index_path (mark->mbox);
  if ((fd = fopen (path, "r+")) == NULL) {
    cronos_error (errno, _("Opening the mailbox main DB file"), ERROR_WARNING);
    c2_free (path);
    c2_free (mark);
    return;
  }

  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;

    mid = str_get_word (7, line, '\r');
    if (atoi (mid) == mark->mid) {
      char log;
      fseek (fd, -(strlen (line)+1), SEEK_CUR);
      if ((log = fgetc (fd)) == C2_MARK_NEW) was_unread = TRUE;
      else was_unread = FALSE;
      fseek (fd, -1, SEEK_CUR);
      fputc ((int)mark->type, fd);
      c2_free (line);
      c2_free (mid);
      break;
    }
    c2_free (line);
    c2_free (mid);
  }

  fclose (fd);

  /* Update the GtkClist */
  if (mark->row >= 0) {
    if (mark->type == C2_MARK_READ) {
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), mark->row, 0, pixmap_read, mask_read);
      if (was_unread) new_messages--;
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), mark->row, (gpointer) " ");
      style = gtk_style_copy (gtk_clist_get_row_style (GTK_CLIST (WMain->clist), mark->row));
      style->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), mark->row, style);
    }
    else if (mark->type == C2_MARK_NEW) {
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), mark->row, 0, pixmap_unread, mask_unread);
      if (!was_unread) new_messages++;
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), mark->row, (gpointer) "N");
      style = gtk_style_copy (gtk_clist_get_row_style (GTK_CLIST (WMain->clist), mark->row));
      style->font = font_unread;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), mark->row, style);
    }
    else if (mark->type == C2_MARK_REPLY) {
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), mark->row, 0, pixmap_reply, mask_reply);
      if (was_unread) new_messages--;
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), mark->row, (gpointer) "R");
      style = gtk_style_copy (gtk_clist_get_row_style (GTK_CLIST (WMain->clist), mark->row));
      style->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), mark->row, style);
    }
    else if (mark->type == C2_MARK_FORWARD) {
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), mark->row, 0, pixmap_forward, mask_forward);
      if (was_unread) new_messages--;
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), mark->row, (gpointer) "F");
      style = gtk_style_copy (gtk_clist_get_row_style (GTK_CLIST (WMain->clist), mark->row));
      style->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), mark->row, style);
    }
  } else update_clist (mark->mbox, TRUE, FALSE);
  
  c2_free (mark);
  update_wm_title ();
}

void
update_clist (char *mbox, gboolean updating, gboolean in_gdk_thread) {
  char *buffer;
  char *title[8];
  char character;
  int showing_row = 0;
  int selected_row = 0;
  int row;
  gboolean marked;
  GtkStyle *style, *style2;
  Mailbox *search_mbox = NULL;
  FILE *fd;
 
  new_messages = 0;
  if (updating) {
    /* Learn which is the actual row being show and which is the one being selected */
    if (!in_gdk_thread) gdk_threads_enter ();
    for (;;showing_row++)
      if (gtk_clist_row_is_visible (GTK_CLIST (WMain->clist), showing_row) == GTK_VISIBILITY_FULL) break;
    if (!in_gdk_thread) gdk_threads_leave ();
     if (GTK_CLIST (WMain->clist)->selection)
       selected_row = (int) GTK_CLIST (WMain->clist)->selection->data;
     else
       selected_row = -1;
  } else {
    search_mbox = search_mailbox_name (config->mailbox_head, mbox);
  }

  /* Open the index file */
  buffer = c2_mailbox_index_path (mbox);

  if ((fd = fopen (buffer, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, buffer, ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (buffer);
    return;
  }
  c2_free (buffer);
  style = gtk_widget_get_default_style ();

  /* Clear the body */
  if (!in_gdk_thread) gdk_threads_enter ();
  gtk_text_freeze (GTK_TEXT (WMain->text));
  gtk_text_set_point (GTK_TEXT (WMain->text), 0);
  gtk_text_forward_delete (GTK_TEXT (WMain->text), gtk_text_get_length (GTK_TEXT (WMain->text)));
  gtk_text_thaw (GTK_TEXT (WMain->text));
  
  /* Clear the clist */
  gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
  gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
  gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  gtk_clist_clear (GTK_CLIST (WMain->clist));
  if (!in_gdk_thread) gdk_threads_leave ();

  /* Get the headers and put them in the clist */
  for (row = 0;;) {
    /* First ask for the first character of this line
     * to check if it is a ?
     */
    if ((character = fgetc (fd)) == EOF) break;
    if (character == '?') {
      fd_move_to (fd, '\n', 1, TRUE, TRUE);
      continue;
    }
    fseek (fd, -1, SEEK_CUR);
    buffer = fd_get_line (fd);
    if (!strlen (buffer)) continue;

    title[0] = str_get_word (0, buffer, '\r');
    title[1] = str_get_word (1, buffer, '\r');
    title[2] = str_get_word (2, buffer, '\r');
    title[3] = str_get_word (3, buffer, '\r');
    title[4] = str_get_word (4, buffer, '\r');
    title[5] = str_get_word (5, buffer, '\r');
    title[6] = str_get_word (6, buffer, '\r');
    title[7] = str_get_word (7, buffer, '\r');

    if (!title[2]) title[2] = "";
    if (!title[3]) title[3] = "";
    if (!title[4]) title[4] = "";
    if (!title[5]) title[5] = "";
    if (!title[6]) title[6] = "";
    if (!title[7]) title[7] = "";
    if (!in_gdk_thread) gdk_threads_enter ();
    gtk_clist_append (GTK_CLIST (WMain->clist), title);
    if (streq (title[1], "MARK")) {
      marked = TRUE;
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_mark, mask_mark);
    } else marked = FALSE;
    if (streq (title[2], "1")) {
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist),
	  GTK_CLIST (WMain->clist)->rows-1, 2, pixmap_attach, mask_attach);
    }

    style = gtk_widget_get_style (WMain->clist);
    style2 = gtk_style_copy (style);

    if (streq (title[0], "N")) {
      style2->font = font_unread;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), row, style2);
      if (!marked) gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_unread, mask_unread);
      new_messages++;
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), row, (gpointer) "N");
    } else
    if (streq (title[0], "R")) {
      style2->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), row, style2);
      if (!marked) gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_reply, mask_reply);
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), row, (gpointer) "R");
    } else
    if (streq (title[0], "F")) {
      style2->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), row, style2);
      if (!marked) gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_forward, mask_forward);
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), row, (gpointer) "F");
    } else {
      style2->font = font_read;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), row, style2);
      if (!marked) gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_read, mask_read);
    }
    if (!in_gdk_thread) gdk_threads_leave ();

    c2_free (buffer);
    c2_free (title[1]);
    c2_free (title[2]);
    c2_free (title[3]);
    c2_free (title[4]);
    c2_free (title[5]);
    c2_free (title[6]);
    c2_free (title[7]);
    row++;
  }

#if FALSE
  gtk_clist_sort (GTK_CLIST (WMain->clist));
#endif
  if (!in_gdk_thread) gdk_threads_enter ();
  gtk_clist_thaw (GTK_CLIST (WMain->clist));
  if (!in_gdk_thread) gdk_threads_leave ();
  if (selected_row == -1) selected_row = GTK_CLIST (WMain->clist)->rows-1;
  
  if (updating) {
    /* Now scroll to the row that was visible */
    if (!in_gdk_thread) gdk_threads_enter ();
    gtk_clist_select_row (GTK_CLIST (WMain->clist), selected_row, 3);
    gtk_clist_moveto (GTK_CLIST (WMain->clist), showing_row, 0, 0, 0);
    if (!in_gdk_thread) gdk_threads_leave ();
  } else {
    if (search_mbox) {
      if (search_mbox->visible_row < 0) search_mbox->visible_row = GTK_CLIST (WMain->clist)->rows-1;
      if (!in_gdk_thread) gdk_threads_enter ();
      gtk_clist_select_row (GTK_CLIST (WMain->clist), search_mbox->visible_row, 3);
      gtk_clist_moveto (GTK_CLIST (WMain->clist), search_mbox->visible_row, 0, 0.5, 0);
      if (!in_gdk_thread) gdk_threads_leave ();
    }
  }

  if (!in_gdk_thread) gdk_threads_enter ();
  update_wm_title ();
  cronos_gui_set_sensitive ();
  if (!in_gdk_thread) gdk_threads_leave ();

  fclose (fd);
}
