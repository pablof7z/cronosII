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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <gdk/gdk.h>
#include <pthread.h>
#include <gdk_imlib.h>
#include <pthread.h>

#include "gui-window_main_callbacks.h"
#include "gui-window_message.h"
#include "gui-select_file.h"
#include "gui-select_mailbox.h"
#include "gui-print.h"
#include "gui-window_main.h"
#include "gui-composer.h"
#include "gui-window_checking.h"
#include "gui-decision_dialog.h"
#include "gui-addrbook.h"

#include "account.h"
#include "addrbook.h"
#include "autocompletition.h"
#include "init.h"
#include "utils.h"
#include "error.h"
#include "debug.h"
#include "exit.h"
#include "rc.h"
#include "main.h"
#include "search.h"
#include "index.h"
#include "gui-utils.h"
#include "message.h"
#include "mailbox.h"
#include "check.h"
#include "smtp.h"

static void handle_message_mark (int row);

void on_wm_ctree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, gpointer data) {
  GtkCTreeNode *node;
  Mailbox *mbox;

  gdk_window_set_cursor (WMain->window->window, cursor_busy);

  cronos_gui_set_sensitive ();
  
  /* Before loading the new mailbox learn which is the mail selected */
  gdk_threads_leave ();
  if (selected_mbox) {
    Mailbox *mbox = NULL;

    mbox = search_mailbox_name (config->mailbox_head, selected_mbox);
    if (mbox) 
      if (GTK_CLIST (WMain->clist)->selection)
	mbox->visible_row = (int) GTK_CLIST (WMain->clist)->selection->data;
  }

  /* Get the name of the clicked mailbox */
  node = gtk_ctree_node_nth(ctree, row);
  mbox = gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node);

  if (!mbox) {
    gdk_threads_enter ();
    gdk_window_set_cursor (WMain->window->window, cursor_normal);
    gdk_threads_leave ();
    return;
  }

  selected_mbox = mbox->name;
  selected_row = -1;

  /* Update the clist titles */
  gdk_threads_enter ();
  if (streq (mbox->name, MAILBOX_DRAFTS) ||
      streq (mbox->name, MAILBOX_OUTBOX) ||
      streq (mbox->name, MAILBOX_QUEUE))
	  gtk_clist_set_column_title (GTK_CLIST (WMain->clist), 4, _("To"));
  else
	  gtk_clist_set_column_title (GTK_CLIST (WMain->clist), 4, _("From"));
  gdk_threads_leave ();

  update_clist (mbox->name, FALSE, FALSE);

  /* Change the cursor to a pointer */
  gdk_window_set_cursor (WMain->window->window, cursor_normal);
}

/**
 * on_wm_clist_select_row
 * 
 * This is the callback function that is called when a row in the GtkCList
 * of the main window is pressed.
 **/
void on_wm_clist_select_row (GtkWidget *widget, gint row, gint column,
		GdkEventButton *event, gpointer data) {
  char *_mid;
  mid_t mid;
  Message *message, *f_message;
  guint32 len;
  MimeHash *mime = NULL;
  MimeHash *tmp;
  GList *s;
  char *buf, *buf2, *path;
  char *content_type;
  char *parameter;
  char *from, *to, *cc, *bcc, *subject, *account, *date, *priority;
  GdkImlibImage *img;

  if (column == 1) {
    handle_message_mark (row);
  }
  
  selected_row = (int) GTK_CLIST (WMain->clist)->selection->data;
  if (g_list_length (GTK_CLIST (WMain->clist)->selection) > 1) return;
  if (reload_mailbox_list_timeout >= 0) {
    gtk_timeout_remove (reload_mailbox_list_timeout);
    reload_mailbox_list_timeout = -1;
  }
  
  /* Get the MID of the clicked row */
  gtk_clist_get_text (GTK_CLIST (WMain->clist), row, 7, &_mid);
  if (_mid == NULL) return;
  mid = atoi (_mid);

  gdk_window_set_cursor (WMain->window->window, cursor_busy);

  message = message_get_message (selected_mbox, mid);
  if (!message) {
    gdk_window_set_cursor (WMain->window->window, cursor_normal);
    gnome_appbar_push (GNOME_APPBAR (WMain->appbar), _("The message couldn't be found"));
    return;
  }

#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_OPEN, message, "preview", NULL, NULL, NULL);
#endif
  
  message_mime_parse (message, NULL);
  if (message->mime) {
    mime = message_mime_get_default_part (message->mime);
    message_mime_get_part (mime);
  } else mime = NULL;
  
  gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
  gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
  if (rc->mime_win_mode == MIME_WIN_AUTOMATICALLY) {
    if (g_list_length (message->mime) > 1) {
	on_wm_mime_left_btn_clicked ();
    } else {
      on_wm_mime_right_btn_clicked ();
    }
  }
  
  for (s = message->mime; s != NULL; s = s->next) {
    buf = buf2 = NULL;
    tmp = MIMEHASH (s->data);
    if (!tmp) continue;
    content_type = g_strdup_printf ("%s/%s", tmp->type, tmp->subtype);
    if (tmp->parameter) {
      parameter = message_mime_get_parameter_value (tmp->parameter, "name");
      if (!parameter) parameter = message_mime_get_parameter_value (tmp->parameter, "filename");
      if (!parameter) parameter = message_mime_get_parameter_value (tmp->disposition, "filename");
    }
    path = CHAR (pixmap_get_icon_by_mime_type (content_type));
    img = gdk_imlib_load_image (path);
    if (parameter && strlen (parameter)) {
      gnome_icon_list_append_imlib (GNOME_ICON_LIST (WMain->icon_list), img, parameter);
      c2_free (content_type);
    } else {
      gnome_icon_list_append_imlib (GNOME_ICON_LIST (WMain->icon_list), img, content_type);
    }
  }
  gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));

  gtk_text_freeze (GTK_TEXT (WMain->text));
  len = gtk_text_get_length (GTK_TEXT (WMain->text));
  gtk_text_set_point (GTK_TEXT (WMain->text), 0);
  gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
  if (mime)
    gtk_text_insert (GTK_TEXT (WMain->text), font_body, &config->color_misc_body, NULL, mime->part, -1);
  else {
    message_get_message_body (message, NULL);
    gtk_text_insert (GTK_TEXT (WMain->text), font_body, &config->color_misc_body, NULL, message->body, -1);
  }
  gtk_text_thaw (GTK_TEXT (WMain->text));

  /* Get the header fields */
  from		= message_get_header_field (message, NULL, "From:");
  to		= message_get_header_field (message, NULL, "To:");
  cc		= message_get_header_field (message, NULL, "CC:");
  bcc		= message_get_header_field (message, NULL, "BCC:");
  subject	= message_get_header_field (message, NULL, "Subject:");
  account	= message_get_header_field (message, NULL, "X-CronosII-Account:");
  date		= message_get_header_field (message, NULL, "Date:");
  priority	= message_get_header_field (message, NULL, "Priority:");
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_FROM][1]),	from);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_TO][1]),	to);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_CC][1]),	cc);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_BCC][1]),	bcc);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_SUBJECT][1]),subject);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_ACCOUNT][1]),account);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_DATE][1]),	date);
  gtk_label_set_text (GTK_LABEL (WMain->header_titles[HEADER_TITLES_PRIORITY][1]), priority);
  c2_free (from);
  c2_free (to);
  c2_free (cc);
  c2_free (bcc);
  c2_free (subject);
  c2_free (account);
  c2_free (date);
  c2_free (priority);
  
  /* Load the file */ 
  cronos_gui_set_sensitive ();

  f_message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
  if (f_message) message_free (f_message);
  gtk_object_set_data (GTK_OBJECT (WMain->text), "message", (gpointer) message);

  /* Check if the mail is marked as new */
  {
    char *mark;

    mark = (char *) gtk_clist_get_row_data (GTK_CLIST (WMain->clist), row);

    if (mark && *mark == 'N') {
      index_mark_as (selected_mbox, row, mid, C2_MARK_READ, config->mark_as_read*1000);
    }
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
}

void on_wm_clist_button_press_event (GtkWidget *clist, GdkEvent *event, gpointer data) {
  if (event->button.button == 1 &&
     (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)) {
    if (selected_row < 0) return;

    if (streq (selected_mbox, MAILBOX_DRAFTS)) {
      Message *message;

      message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
      if (!message) return;
      message = message_copy (message);
      c2_composer_new (message, C2_COMPOSER_DRAFTS);
    } else {
      int row, col;
      GdkEventButton *e;
      Message *message;

      message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
      if (!message) return;
      message = message_copy (message);
      
      e = (GdkEventButton *) event;
      gtk_clist_get_selection_info (GTK_CLIST (WMain->clist), e->x, e->y, &row, &col);
      if (row > GTK_CLIST (WMain->clist)->rows-1) return;
      gui_window_message_new_with_message (message);
    }
  }
  else if (event->button.button == 1 || event->button.button == 2) {
    int row, col;
    GdkEventButton *e;

    e = (GdkEventButton *) event;
    gtk_clist_get_selection_info (GTK_CLIST (WMain->clist), e->x, e->y, &row, &col);

    if (row > GTK_CLIST (WMain->clist)->rows-1) return;
    if ((event->button.button == 1 && col == 1) || event->button.button == 2)
      handle_message_mark (row);
  }
  else if (event->button.button == 3) {
    int row, col;
    GdkEventButton *e;

    if (!g_list_length (GTK_CLIST (WMain->clist)->selection)) return;

    e = (GdkEventButton *) event;
    gtk_clist_get_selection_info (GTK_CLIST (WMain->clist), e->x, e->y, &row, &col);
    
    if ((GTK_CLIST (WMain->clist)->selection) &&
	(g_list_length (GTK_CLIST (WMain->clist)->selection) < 2) &&
	(row < GTK_CLIST (WMain->clist)->rows))
      gtk_clist_unselect_row (GTK_CLIST (WMain->clist), (int)GTK_CLIST (WMain->clist)->selection->data, 0);

    gtk_clist_select_row (GTK_CLIST (WMain->clist), row, col);
  }
}

void
on_wm_mime_add_card_activate (GtkWidget *object, int num) {
  Message *message;
  GList *l;
  MimeHash *mime;
  C2VCard *card;

  message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
  if (!message) return;

  l = g_list_nth (message->mime, num);
  if (!l) return;
  mime = MIMEHASH (l->data);
 
  card = c2_address_book_get_card_from_str (mime->part);
  if (card) {
    c2_address_book_card_add (card);
  }
}

void
on_wm_mime_select_icon (GnomeIconList *gil, int num, GdkEvent *event) {
  Message *message;
  MimeHash *mime;
  GList *list;
  
  if (!GNOME_ICON_LIST (WMain->icon_list)->selection) return;
  message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
  if (!message) return;
  
  list = g_list_nth (message->mime, (int) GNOME_ICON_LIST (WMain->icon_list)->selection->data);
  if (!list) return;

  mime = MIMEHASH (list->data);
  if (!mime) return;

  if (streq (mime->type, "text")) on_wm_mime_view_clicked ();
  
  main_window_menu_attach_clear ();
  
  if (streq (mime->type, "text") && streq (mime->subtype, "x-vcard")) {
    main_window_menu_attach_add_item (_("Add Card"), DATADIR "/cronosII/pixmap/mcard.png",
					GTK_SIGNAL_FUNC (on_wm_mime_add_card_activate), num);
    main_window_menu_attach_add_item (NULL, NULL, NULL, NULL);
  }
}

void
on_wm_mime_button_pressed (GtkWidget *widget, GdkEvent *event, gpointer data) {
  if (event->button.button == 1 && (event->type == GDK_2BUTTON_PRESS ||
      				     event->type == GDK_3BUTTON_PRESS)) {
    on_wm_mime_open_clicked ();
  }
}

void
on_wm_mime_view_clicked (void) {
  Message *message;
  MimeHash *mime;
  GList *list;
  int len;
  
  if (!GNOME_ICON_LIST (WMain->icon_list)->selection) return;
  message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
  if (!message) return;
  
  list = g_list_nth (message->mime, (int) GNOME_ICON_LIST (WMain->icon_list)->selection->data);
  if (!list) return;

  mime = MIMEHASH (list->data);
  if (!mime) return;

  message_mime_get_part (mime);
  gtk_text_freeze (GTK_TEXT (WMain->text));
  len = gtk_text_get_length (GTK_TEXT (WMain->text));
  gtk_text_set_point (GTK_TEXT (WMain->text), 0);
  gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
  gtk_text_insert (GTK_TEXT (WMain->text), font_body, &config->color_misc_body, NULL, mime->part, -1);
  gtk_text_thaw (GTK_TEXT (WMain->text));
}

static void
on_wm_mime_open_clicked_thread (char *cmnd) {
  g_return_if_fail (cmnd);
  
  cronos_system (cmnd);
}

void
on_wm_mime_open_clicked (void) {
  MimeHash *mime;
  GList *list, *s;
  Message *message;
  const char *file_name;
  char *buf, *path;
  const char *program;
  char *content_type;
  char *command;
  FILE *fd;
  gboolean main_is_inline = FALSE;
  pthread_t thread;
  
  if (!GNOME_ICON_LIST (WMain->icon_list)->selection) return;
  message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");

  if (!message) return;
  
  list = g_list_nth (message->mime, (int) GNOME_ICON_LIST (WMain->icon_list)->selection->data);
  if (!list) return;
  
  mime = MIMEHASH (list->data);
  if (!mime) return;
  
  content_type = g_strdup_printf ("%s/%s", mime->type, mime->subtype);
  program = gnome_mime_program (content_type);
  if (!program && streq (content_type, "application/octet-stream")) program = "";
  if (!program) {
    buf = g_strdup_printf (_("No program associated to type %s"), content_type);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf);
    return;
  }
  c2_free (content_type);
  
  /* Check if this part is inline */
  if (strnne (mime->disposition, "attachment", 10)) main_is_inline = TRUE;
  else main_is_inline = FALSE;

  if (main_is_inline) {
    /* Go through the rest of the other parts saving all inline parts that have an ID */
    s = g_list_nth (message->mime, (int) GNOME_ICON_LIST (WMain->icon_list)->selection->data);
    for (s = s->next; s; s = s->next) {
      if (!MIMEHASH (s->data)->id) continue;

      path = g_strdup_printf ("/tmp/cid:%s", MIMEHASH (s->data)->id);
      message_mime_get_part (MIMEHASH (s->data));
     
      tmp_files = g_slist_append (tmp_files, path);
      if ((fd = fopen (path, "wb")) == NULL) {
	continue;
      }
      fwrite (MIMEHASH (s->data)->part, sizeof (char), MIMEHASH (s->data)->len, fd);
      fclose (fd);
    }
  }
  
  file_name = message_mime_get_parameter_value (mime->parameter, "name");
  if (!file_name) file_name = message_mime_get_parameter_value (mime->parameter, "filename");
  if (!file_name) file_name = message_mime_get_parameter_value (mime->disposition, "filename");
  
  if (file_name) {
    path = g_strdup_printf ("/tmp/%s", file_name);
  } else {
tmpnam:
    path = cronos_tmpfile ();
  }
  
  message_mime_get_part (mime);

  tmp_files = g_slist_append (tmp_files, path);
  if ((fd = fopen (path, "wb")) == NULL) {
    switch (errno) {
      case EEXIST:
      case EISDIR:
      case ETXTBSY:
	goto tmpnam;
    }
    buf = g_strdup_printf (_("Cannot write to temporary file: %s"), g_strerror (errno));
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf);
    return;
  }
  fwrite (mime->part, sizeof (char), mime->len, fd);
  fclose (fd);
  
  buf = g_strdup_printf ("\"%s\"", path);
  c2_free (path);
  path = buf;
  if (!strlen (program)) {
    command = path;
    chmod (path, 0775);
  } else command = str_replace (program, "%f", path);
  pthread_create (&thread, NULL, PTHREAD_FUNC (on_wm_mime_open_clicked_thread), command);
  pthread_detach (thread);
}

void
on_wm_mime_save_clicked (void) {
  Message *message;
  MimeHash *mime;
  GList *list;
  GList *s;
  char *buf;
  const char *file_name;
  char *path;
  FILE *fd;
  
  if (!GNOME_ICON_LIST (WMain->icon_list)->selection) return;

  message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
  if (!message) return;
  
  for (s = GNOME_ICON_LIST (WMain->icon_list)->selection; s != NULL; s = s->next) {
    list = g_list_nth (message->mime, (int) s->data);
    if (!list) return;
    
    mime = MIMEHASH (list->data);
    if (!mime) return;
    
    file_name = message_mime_get_parameter_value (mime->parameter, "name");
    if (!file_name) file_name = message_mime_get_parameter_value (mime->parameter, "filename");
    if (!file_name) file_name = message_mime_get_parameter_value (mime->disposition, "filename");
    
    path = gui_select_file_new (NULL, NULL, NULL, file_name);
    if (!path) continue;
    message_mime_get_part (mime);
    
    if ((fd = fopen (path, "wb")) == NULL) {
      buf = g_strdup_printf (_("Cannot write in selected file: %s"), g_strerror (errno));
      gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf);
      return;
    }
    fwrite (mime->part, sizeof (char), mime->len, fd);
    fclose (fd);
  }
  
  if (g_list_length (GNOME_ICON_LIST (WMain->icon_list)->selection) > 1)
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Parts saved succesfully"));
  else
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Part saved succesfully"));
}

void
on_wm_mime_stick_btn_clicked (GtkWidget *button) {
  if (GTK_TOGGLE_BUTTON (button)->active) rc->mime_win_mode = MIME_WIN_STICKY;
  else rc->mime_win_mode = MIME_WIN_AUTOMATICALLY;
}

void
on_wm_mime_left_btn_clicked (void) {
  gtk_widget_hide (WMain->mime_left);
  gtk_widget_show (WMain->mime_right);
  gtk_widget_show (WMain->mime_scroll);
}

void
on_wm_mime_right_btn_clicked (void) {
  gtk_widget_hide (WMain->mime_right);
  gtk_widget_show (WMain->mime_left);
  gtk_widget_hide (WMain->mime_scroll);
}

static void handle_message_mark (int row) {
  char *mid, *gmid;
  char *str_frm;
  char *str_dst;
  char *line;
  FILE *frm, *dst;
  char *content;

  gtk_clist_get_text (GTK_CLIST (WMain->clist), row, 1, &content);
 
  if (!strlen (content)) {
    /* Mark */
    gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_mark, mask_mark);
    gtk_clist_set_text (GTK_CLIST (WMain->clist), row, 1, "MARK");
  } else {
    /* Unmark */
    char *mark = (char *) gtk_clist_get_row_data (GTK_CLIST (WMain->clist), row);
    if (mark) {
      if (*mark == 'N')
	gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_unread, mask_unread);
      else if (*mark == 'F')
	gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_forward, mask_forward);
      else if (*mark == 'R')
	gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_reply, mask_reply);
      else
	gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_read, mask_read);
    } else
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), row, 0, pixmap_read, mask_read);
    gtk_clist_set_text (GTK_CLIST (WMain->clist), row, 1, "");
  }

  gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) row, 7, &mid);
  
  str_frm = c2_mailbox_index_path (selected_mbox);
  if ((frm = fopen (str_frm, "r")) == NULL) {
    cronos_error (errno, "Opening the mailbox's index file", ERROR_WARNING);
    c2_free (str_frm);
    return;
  }

  str_dst = cronos_tmpfile ();
  /* This file is going to use fd_mv, which use tmp_files, so it shouldn't
   * be used here. */
  if ((dst = fopen (str_dst, "w")) == NULL) {
    cronos_error (errno, "Opening the tmp file", ERROR_WARNING);
    c2_free (str_dst);
    c2_free (str_frm);
    return;
  }

  for (;;) {
    if ((line = fd_get_line (frm)) == NULL) break;
    
    gmid = str_get_word (7, line, '\r');
    
    if (streq (gmid, mid)) {
      fprintf (dst, "%s\r%s\r%s\r%s\r%s\r%s\r%s\r%s\n",
	  str_get_word (0, line, '\r'),
	  (!strlen (content)) ? "MARK" : "",
	  str_get_word (2, line, '\r'), str_get_word (3, line, '\r'), str_get_word (4, line, '\r'),
	  str_get_word (5, line, '\r'), str_get_word (6, line, '\r'), mid);
    } else {
      fprintf (dst, "%s\n", line);
    }
    
    c2_free (line);
    c2_free (gmid);
  }
  
  fclose (dst);
  fclose (frm);
  
  fd_mv (str_dst, str_frm);
}

static int
sort_a_menor_b (gconstpointer a, gconstpointer b) {
  return a < b;
}

typedef enum {
  DELETE_MOVES,
  DELETE_EXPUNGES
} DeleteAction;

/***************** Deleting *****************/
#ifndef USE_OLD_MBOX_HANDLERS
static void
on_delete_clicked_thread (void) {
  GList *rows_to_delete = NULL;
  GList *mids_to_delete = NULL;
  GList *mids_to_add = NULL;
  GList *s, *f;
  Mailbox *queue = NULL;

  int length, i = 1, len, a;
  gboolean have_status_ownership = TRUE;
  
  char *mid;

  DeleteAction action;

  char *tmp;
  char *index;

  char *mail1;
  char *mail2;

  char *line;
  char *read_mid;
  char *mark;
  
  FILE *fd, *fd_garbage;

  Mailbox *garbage;
  char *garbage_path = NULL;

  int first_delete_row;

  if (!g_list_length (GTK_CLIST (WMain->clist)->selection)) return;
  
  /* Make a list of the rows that are going to be deleted */
  first_delete_row = (int) GTK_CLIST (WMain->clist)->selection->data;
  for (s = GTK_CLIST (WMain->clist)->selection; s; s = s->next) {
    rows_to_delete = g_list_insert_sorted (rows_to_delete, s->data, sort_a_menor_b);
  }

  /* Make a list of the mids that are going to be deleted */
  for (s = rows_to_delete; s; s = s->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), GPOINTER_TO_INT (s->data), 7, &mid);
    if (!mid) continue;
    mids_to_delete = g_list_append (mids_to_delete, mid);
  }
  mids_to_delete = g_list_sort (mids_to_delete, sort_a_menor_b);

  length = g_list_length (mids_to_delete);

  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, length);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Deleting..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else {
    have_status_ownership = FALSE;
  }

  /* Check what action should be taken */
  if (streq (selected_mbox, MAILBOX_GARBAGE)) action = DELETE_EXPUNGES;
  else action = DELETE_MOVES;

  /* Open the index file */
  index = c2_mailbox_index_path (selected_mbox);
  if ((fd = fopen (index, "r+")) == NULL) {
    char *err, *error;
    err = g_strerror (errno);
    error = g_strdup_printf (_("Opening the file %s: %s\n"), tmp, err);
    c2_free (err);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), error);
    gdk_threads_leave ();
    return;
  }

  if (strne (selected_mbox, MAILBOX_GARBAGE)) {
    /* Open the garbage file */
    garbage_path = c2_mailbox_index_path (MAILBOX_GARBAGE);
    
    if ((fd_garbage = fopen (garbage_path, "a")) == NULL) {
      char *err, *error;
      err = g_strerror (errno);
      error = g_strdup_printf (_("Opening the %s index file %s: %s\n"), MAILBOX_GARBAGE, tmp, err);
      c2_free (err);
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), error);
      gdk_threads_leave ();
      return;
    }

    garbage = search_mailbox_name (config->mailbox_head, MAILBOX_GARBAGE);
  }

  /* Rebuild the index file */
  for (s = mids_to_delete; s; s = s->next) {
    for (;;) {
      if ((line = fd_get_line (fd)) == NULL) goto avoid_keep_writing;
      
      /* Check if this line contains one of the mids I'm interested */
      read_mid = str_get_word (7, line, '\r');
      if (streq (read_mid, CHAR (s->data))) {
	if (garbage_path) {
	  char *buf[7];
	  mid_t new_mid;
	  
	  buf[0] = str_get_word (0, line, '\r');
	  buf[1] = str_get_word (1, line, '\r');
	  buf[2] = str_get_word (2, line, '\r');
	  buf[3] = str_get_word (3, line, '\r');
	  buf[4] = str_get_word (4, line, '\r');
	  buf[5] = str_get_word (5, line, '\r');
	  buf[6] = str_get_word (6, line, '\r');
	  new_mid = c2_mailbox_get_next_mid (garbage);
	  
	  fprintf (fd_garbage, "%s\r%s\r%s\r%s\r%s\r%s\r%s\r%d\n",
	      buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], new_mid);
	  mids_to_add = g_list_append (mids_to_add, (void*) new_mid);
	  c2_free (buf[0]);
	  c2_free (buf[1]);
	  c2_free (buf[2]);
	  c2_free (buf[3]);
	  c2_free (buf[4]);
	  c2_free (buf[5]);
	  c2_free (buf[6]);
	}

	/* Check if this mail is unreaded */
	mark = str_get_word (0, line, '\r');
	if (*mark == 'N') new_messages--;
	c2_free (read_mid);

	/* Now lets remove it from the index file (actually is
	 * not really removing but transforming the field 0 to ?
	 * TODO Delete the line instead of replacing it
	 */
	len = strlen (line);
	fseek (fd, -(len+1), SEEK_CUR);

	for (a = 0; a < len; a++) {
	  fprintf (fd, "?");
	}

	fseek (fd, 1, SEEK_CUR);
	c2_free (line);
	break;
      }
      
      c2_free (line);
    }
  }
avoid_keep_writing:

  /* Move or Delete the mail files */
  if (strne (selected_mbox, MAILBOX_GARBAGE)) {
    for (s = mids_to_delete, f = mids_to_add; s && f; s = s->next, f = f->next) {
      mail1 = g_strdup_printf ("%s" ROOT "/%s.mbx/%s", getenv ("HOME"), selected_mbox, CHAR (s->data));
      mail2 = c2_mailbox_mail_path (MAILBOX_GARBAGE, GPOINTER_TO_INT (f->data));
      fd_mv (mail1, mail2);
      if (have_status_ownership) {
	gdk_threads_enter ();
	gtk_progress_set_value (GTK_PROGRESS (WMain->progress), i++);
	gdk_threads_leave ();
      }
      c2_free (mail1);
      c2_free (mail2);
    }
  } else {
    for (s = mids_to_delete; s; s = s->next) {
      mail1 = g_strdup_printf ("%s" ROOT "/%s.mbx/%s", getenv ("HOME"), selected_mbox, CHAR (s->data));
      if (unlink (mail1) < 0) perror ("unlink");
      c2_free (mail1);
    }
  }

  fclose (fd);

  if (garbage_path) {
    c2_free (garbage_path);
    fclose (fd_garbage);
  }
  
  c2_free (index);
  
  /* Remove from the GtkCList */
  gdk_threads_enter ();
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  for (s = rows_to_delete; s; s = s->next) {
    gtk_clist_remove (GTK_CLIST (WMain->clist), GPOINTER_TO_INT (s->data));
  }
  gtk_clist_thaw (GTK_CLIST (WMain->clist));
  gdk_threads_leave ();
  g_list_free (rows_to_delete);

  if (have_status_ownership) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    gdk_threads_leave ();
    status_is_busy = FALSE;
  }

  gdk_threads_enter ();
  if (first_delete_row < GTK_CLIST (WMain->clist)->rows && GTK_CLIST (WMain->clist)->rows)
    gtk_clist_select_row (GTK_CLIST (WMain->clist), first_delete_row, 3);
  else if (GTK_CLIST (WMain->clist)->rows) {
      gtk_clist_select_row (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 3);
  } else {
    Message *message;
    int len;
 
    gdk_threads_leave ();
    message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
    
    if (message) message_free (message);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (WMain->text));
    len = gtk_text_get_length (GTK_TEXT (WMain->text));
    gtk_text_set_point (GTK_TEXT (WMain->text), 0);
    gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
    gtk_text_thaw (GTK_TEXT (WMain->text));
    gtk_object_set_data (GTK_OBJECT (WMain->text), "message", NULL);
    gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  update_wm_title ();
  gdk_threads_leave ();
  
  /* verify for mails in queue mailbox */
  queue = search_mailbox_name (config->mailbox_head, MAILBOX_QUEUE);
  if (c2_mailbox_length (queue) == 0) {
	gdk_threads_enter ();
	gtk_widget_set_sensitive (WMain->mb_w.menu_sendqueue, FALSE);
	gtk_widget_set_sensitive (WMain->tb_w.sendqueue, FALSE);
	gdk_threads_leave ();
  }
}
#else
static void
on_delete_clicked_thread (void) {
  GList *list=NULL;
  GList *dlist=NULL;
  char *mid;
  gfloat max, act=1;
  gboolean i_rull = TRUE;
  int higger_row = 0;
  char *mark;
  Mailbox *queue = NULL;

  if (!g_list_length (GTK_CLIST (WMain->clist)->selection)) return NULL;

  gdk_threads_enter ();
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  gdk_threads_leave ();
 
  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    dlist = g_list_insert_sorted (dlist, list->data, sort_a_menor_b);
  }
  max = g_list_length (dlist);

  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, max);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Deleting..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else {
    i_rull = FALSE;
  }
  
  for (list = dlist; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    if (!mid) continue;
    mark = (char *) gtk_clist_get_row_data (GTK_CLIST (WMain->clist), (int) list->data);
    if (mark && *mark == 'N') new_messages--;
    if ((int) list->data > higger_row) higger_row = (int) list->data;
    
    if (i_rull) {
      gdk_threads_enter ();
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), act++);
      gdk_threads_leave ();
    }
    if (streq (selected_mbox, MAILBOX_GARBAGE))
      expunge_mail (selected_mbox, atoi (mid));
    else {
      move_mail (selected_mbox, MAILBOX_GARBAGE, atoi (mid));
    }
  }

  gdk_threads_enter ();
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  for (list = dlist; list; list = list->next) {
    gtk_clist_remove (GTK_CLIST (WMain->clist), (int) list->data);
  }
  gtk_clist_thaw (GTK_CLIST (WMain->clist));
  gdk_threads_leave ();

  if (i_rull) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    gdk_threads_leave ();
    status_is_busy = FALSE;
  }

  gdk_threads_enter ();
  if (higger_row < GTK_CLIST (WMain->clist)->rows && GTK_CLIST (WMain->clist)->rows)
    gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row, 3);
  else if (GTK_CLIST (WMain->clist)->rows) {
    if (higger_row-1 <= GTK_CLIST (WMain->clist)->rows)
      gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row-1, 3);
    else
      gtk_clist_select_row (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 3);
  } else {
    Message *message;
    int len;
 
    gdk_threads_leave ();
    message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
    
    if (message) message_free (message);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (WMain->text));
    len = gtk_text_get_length (GTK_TEXT (WMain->text));
    gtk_text_set_point (GTK_TEXT (WMain->text), 0);
    gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
    gtk_text_thaw (GTK_TEXT (WMain->text));
    gtk_object_set_data (GTK_OBJECT (WMain->text), "message", NULL);
    gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  gdk_threads_leave ();
  update_wm_title ();
  return NULL;
  
  /* verify for mails in queue mailbox */
  queue = search_mailbox_name (config->mailbox_head, MAILBOX_QUEUE);
  if (c2_mailbox_length (queue) == 0) {
	gtk_widget_set_sensitive (WMain->mb_w.menu_sendqueue, FALSE);
	gtk_widget_set_sensitive (WMain->tb_w.sendqueue, FALSE);
  }
}
#endif

void on_wm_delete_clicked (void) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_delete_clicked_thread), NULL);
}

static void *_on_expunge_clicked (void *dismiss) {
  GList *list=NULL;
  GList *dlist=NULL;
  char *mid;
  gfloat max, act=1;
  gboolean i_rull = TRUE;
  int higger_row = 0;
  char *mark;

  if (!g_list_length (GTK_CLIST (WMain->clist)->selection)) return NULL;
  
  /* Learn which are the mids to delete */
  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    dlist = g_list_insert_sorted (dlist, list->data, sort_a_menor_b);
  }
  max = g_list_length (dlist);

  gdk_threads_enter ();
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  gdk_threads_leave ();

  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, max);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Expunging..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else
    i_rull = FALSE;

  for (list = dlist; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    if (!mid) continue;
    mark = (char *) gtk_clist_get_row_data (GTK_CLIST (WMain->clist), (int) list->data);
    if (mark && *mark == 'N') new_messages--;
    if ((int) list->data > higger_row) higger_row = (int) list->data;
    if (i_rull) {
      gdk_threads_enter ();
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), act++);
      gdk_threads_leave ();
    }
    expunge_mail (selected_mbox, atoi (mid));
  }

  gdk_threads_enter ();
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  for (list = dlist; list; list = list->next) {
    gtk_clist_remove (GTK_CLIST (WMain->clist), (int) list->data);
  }
  gtk_clist_thaw (GTK_CLIST (WMain->clist));

  if (i_rull) {
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    status_is_busy = FALSE;
  }

  if (higger_row < GTK_CLIST (WMain->clist)->rows && GTK_CLIST (WMain->clist)->rows) {
    gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row, 3);
  } else if (GTK_CLIST (WMain->clist)->rows) {
    if (higger_row-1 <= GTK_CLIST (WMain->clist)->rows)
      gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row-1, 3);
    else
      gtk_clist_select_row (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 3);
  } else {
    Message *message;
    int len;
 
    gdk_threads_leave ();
    message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
    if (message) message_free (message);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (WMain->text));
    len = gtk_text_get_length (GTK_TEXT (WMain->text));
    gtk_text_set_point (GTK_TEXT (WMain->text), 0);
    gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
    gtk_text_thaw (GTK_TEXT (WMain->text));
    gtk_object_set_data (GTK_OBJECT (WMain->text), "message", NULL);
    gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  }
  
  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  gdk_threads_leave ();
  update_wm_title ();
  return NULL;
}

void on_wm_expunge_clicked (void) {
  pthread_t thread;
  int len;
  char *buf;
 
  len = g_list_length (GTK_CLIST (WMain->clist)->selection);
  if (!len) return;
  
  if (len > 1)
    buf = g_strdup_printf (_("Are you sure you want to expunge %d messages?"),
      			len);
  else
    buf = g_strdup_printf (_("Are you sure you want to expunge the selected message?"));
  if (!gui_question (_("Confirmation"), buf, WMain->window)) {
    c2_free (buf);
    return;
  }
  c2_free (buf);

  pthread_create (&thread, NULL, _on_expunge_clicked, NULL);
}

#ifndef USE_OLD_MBOX_HANDLERS
static void
on_move_mail_clicked_thread (char *str_mailbox) {
  GList *rows_to_delete = NULL;
  GList *mids_to_delete = NULL;
  GList *mids_to_add = NULL;
  GList *s, *f;

  int length, i = 1, len, a;
  gboolean have_status_ownership = TRUE;
  
  char *mid;

  char *tmp;
  char *index;

  char *mail1;
  char *mail2;

  char *line;
  char *read_mid;
  char *mark;
  
  FILE *fd, *fd_mailbox;

  Mailbox *mailbox;
  char *mailbox_path = NULL;

  int first_delete_row;

  if (!g_list_length (GTK_CLIST (WMain->clist)->selection)) return;
  
  /* Make a list of the rows that are going to be deleted */
  first_delete_row = (int) GTK_CLIST (WMain->clist)->selection->data;
  for (s = GTK_CLIST (WMain->clist)->selection; s; s = s->next) {
    rows_to_delete = g_list_insert_sorted (rows_to_delete, s->data, sort_a_menor_b);
  }

  /* Make a list of the mids that are going to be deleted */
  for (s = rows_to_delete; s; s = s->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), GPOINTER_TO_INT (s->data), 7, &mid);
    if (!mid) continue;
    mids_to_delete = g_list_append (mids_to_delete, mid);
  }
  mids_to_delete = g_list_sort (mids_to_delete, sort_a_menor_b);

  length = g_list_length (mids_to_delete);

  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, length);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Moving..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else {
    have_status_ownership = FALSE;
  }

  /* Open the index file */
  index = c2_mailbox_index_path (selected_mbox);
  if ((fd = fopen (index, "r+")) == NULL) {
    char *err, *error;
    err = g_strerror (errno);
    error = g_strdup_printf (_("Opening the file %s: %s\n"), tmp, err);
    c2_free (err);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), error);
    gdk_threads_leave ();
    return;
  }
  
  /* Open the mailbox file */
  mailbox_path = c2_mailbox_index_path (str_mailbox);
  
  if ((fd_mailbox = fopen (mailbox_path, "a")) == NULL) {
    char *err, *error;
    err = g_strerror (errno);
    error = g_strdup_printf (_("Opening the %s index file %s: %s\n"), str_mailbox, tmp, err);
    c2_free (err);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), error);
    gdk_threads_leave ();
    return;
  }
  
  mailbox = search_mailbox_name (config->mailbox_head, str_mailbox);

  /* Rebuild the index file */
  for (s = mids_to_delete; s; s = s->next) {
    for (;;) {
      if ((line = fd_get_line (fd)) == NULL) goto avoid_keep_writing;
      
      /* Check if this line contains one of the mids I'm interested */
      read_mid = str_get_word (7, line, '\r');
      if (streq (read_mid, CHAR (s->data))) {
	char *buf[7];
	mid_t new_mid;
	
	buf[0] = str_get_word (0, line, '\r');
	buf[1] = str_get_word (1, line, '\r');
	buf[2] = str_get_word (2, line, '\r');
	buf[3] = str_get_word (3, line, '\r');
	buf[4] = str_get_word (4, line, '\r');
	buf[5] = str_get_word (5, line, '\r');
	buf[6] = str_get_word (6, line, '\r');
	new_mid = c2_mailbox_get_next_mid (mailbox);
	
	fprintf (fd_mailbox, "%s\r%s\r%s\r%s\r%s\r%s\r%s\r%d\n",
	    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], new_mid);
	mids_to_add = g_list_append (mids_to_add, (void*) new_mid);
	c2_free (buf[0]);
	c2_free (buf[1]);
	c2_free (buf[2]);
	c2_free (buf[3]);
	c2_free (buf[4]);
	c2_free (buf[5]);
	c2_free (buf[6]);
	
	/* Check if this mail is unreaded */
	mark = str_get_word (0, line, '\r');
	if (*mark == 'N') new_messages--;
	c2_free (read_mid);
	
	/* Now lets remove it from the index file (actually is
	 * not really removing but transforming the field 0 to ?
	 * TODO Delete the line instead of replacing it
	 */
	len = strlen (line);
	fseek (fd, -(len+1), SEEK_CUR);
	
	for (a = 0; a < len; a++) {
	  fprintf (fd, "?");
	}
	
	fseek (fd, 1, SEEK_CUR);
	c2_free (line);
	break;
      }
      
      c2_free (line);
    }
  }
avoid_keep_writing:
  
  for (s = mids_to_delete, f = mids_to_add; s && f; s = s->next, f = f->next) {
    mail1 = g_strdup_printf ("%s" ROOT "/%s.mbx/%s", getenv ("HOME"), selected_mbox, CHAR (s->data));
    mail2 = c2_mailbox_mail_path (str_mailbox, GPOINTER_TO_INT (f->data));
    fd_mv (mail1, mail2);
    if (have_status_ownership) {
      gdk_threads_enter ();
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), i++);
      gdk_threads_leave ();
    }
    c2_free (mail1);
    c2_free (mail2);
  }

  fclose (fd);
  
  c2_free (mailbox_path);
  fclose (fd_mailbox);
  
  c2_free (index);
  
  /* Remove from the GtkCList */
  gdk_threads_enter ();
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  for (s = rows_to_delete; s; s = s->next) {
    gtk_clist_remove (GTK_CLIST (WMain->clist), GPOINTER_TO_INT (s->data));
  }
  gtk_clist_thaw (GTK_CLIST (WMain->clist));
  gdk_threads_leave ();
  g_list_free (rows_to_delete);

  if (have_status_ownership) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    gdk_threads_leave ();
    status_is_busy = FALSE;
  }

  gdk_threads_enter ();
  if (first_delete_row < GTK_CLIST (WMain->clist)->rows && GTK_CLIST (WMain->clist)->rows)
    gtk_clist_select_row (GTK_CLIST (WMain->clist), first_delete_row, 3);
  else if (GTK_CLIST (WMain->clist)->rows) {
      gtk_clist_select_row (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 3);
  } else {
    Message *message;
    int len;
 
    gdk_threads_leave ();
    message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
    
    if (message) message_free (message);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (WMain->text));
    len = gtk_text_get_length (GTK_TEXT (WMain->text));
    gtk_text_set_point (GTK_TEXT (WMain->text), 0);
    gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
    gtk_text_thaw (GTK_TEXT (WMain->text));
    gtk_object_set_data (GTK_OBJECT (WMain->text), "message", NULL);
    gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  update_wm_title ();
  gdk_threads_leave ();
}
#else
static void *
on_move_mail_clicked_thread (char *dst) {
  char *mid;
  GList *list, *dlist = NULL;
  int higger_row = 0;
  gfloat max, act=1;
  gboolean i_rull = TRUE;
  char *mark;
  
  gdk_threads_enter ();
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  gdk_threads_leave ();
 
  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    dlist = g_list_insert_sorted (dlist, list->data, sort_a_menor_b);
  }
  max = g_list_length (dlist);
  
  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, max);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Moving..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else {
    i_rull = FALSE;
  }
  
  for (list = dlist; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    mark = (char *) gtk_clist_get_row_data (GTK_CLIST (WMain->clist), (int) list->data);
    if (mark && *mark == 'N') new_messages--;
    if ((int) list->data > higger_row) higger_row = (int) list->data;

    if (i_rull) {
      gdk_threads_enter ();
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), act++);
      gdk_threads_leave ();
    }
     
    move_mail (selected_mbox, dst, atoi (mid));
  }

  gdk_threads_enter ();
  gtk_clist_freeze (GTK_CLIST (WMain->clist));
  for (list = dlist; list; list = list->next) {
    gtk_clist_remove (GTK_CLIST (WMain->clist), (int) list->data);
  }
  gtk_clist_thaw (GTK_CLIST (WMain->clist));
  if (i_rull) {
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    status_is_busy = FALSE;
  }
  
  if (higger_row < GTK_CLIST (WMain->clist)->rows && GTK_CLIST (WMain->clist)->rows) {
    gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row, 3);
  } else if (GTK_CLIST (WMain->clist)->rows) {
    if (higger_row-1 <= GTK_CLIST (WMain->clist)->rows)
      gtk_clist_select_row (GTK_CLIST (WMain->clist), higger_row-1, 3);
    else
      gtk_clist_select_row (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 3);
  } else {
    Message *message;
    int len;
 
    gdk_threads_leave ();
    message = (Message *) gtk_object_get_data (GTK_OBJECT (WMain->text), "message");
    if (message) message_free (message);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (WMain->text));
    len = gtk_text_get_length (GTK_TEXT (WMain->text));
    gtk_text_set_point (GTK_TEXT (WMain->text), 0);
    gtk_text_forward_delete (GTK_TEXT (WMain->text), len);
    gtk_text_thaw (GTK_TEXT (WMain->text));
    gtk_object_set_data (GTK_OBJECT (WMain->text), "message", NULL);
    gnome_icon_list_freeze (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_clear (GNOME_ICON_LIST (WMain->icon_list));
    gnome_icon_list_thaw (GNOME_ICON_LIST (WMain->icon_list));
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  gdk_threads_leave ();
  update_wm_title ();
  return NULL;
}
#endif

void on_wm_move_mail_clicked (void) {
  pthread_t thread;
  char *dst;
  
  if (g_list_length (GTK_CLIST (WMain->clist)->selection) < 1) return;
  if ((dst = gui_select_mailbox (NULL)) == NULL) return;
  
  pthread_create (&thread, NULL, PTHREAD_FUNC (on_move_mail_clicked_thread), dst);
}

static void *_on_copy_mail_clicked (void *dismiss) {
  char *dst;
  char *mid;
  int i;
  GList *list;
  gfloat max, act=1;
  gboolean i_rull = TRUE;

  dst = dismiss;
  
  if ((max = (gfloat) g_list_length (GTK_CLIST (WMain->clist)->selection)) < 1) return NULL;
  gdk_threads_enter ();
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  gdk_threads_leave ();
  
  if (!status_is_busy) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, max);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Copying..."));
    gdk_threads_leave ();
    status_is_busy = TRUE;
  } else
    i_rull = FALSE;

  for (list = GTK_CLIST (WMain->clist)->selection, i=0; i<g_list_length (GTK_CLIST (WMain->clist)->selection);
		  i++ , list = list->next) {
    if (i_rull) {
      gdk_threads_enter ();
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), act++);
      gdk_threads_leave ();
    }
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    copy_mail (selected_mbox, dst, atoi (mid));
  }

  if (i_rull) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    gdk_threads_leave ();
    status_is_busy = FALSE;
  }

  gdk_threads_enter ();
  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  gtk_widget_queue_draw (WMain->window);
  gdk_threads_leave ();
  return NULL;
}

void on_wm_copy_mail_clicked (void) {
  pthread_t thread;
  char *dst;
  
  if (g_list_length (GTK_CLIST (WMain->clist)->selection) < 1) return;
  if ((dst = gui_select_mailbox (NULL)) == NULL) return;

  pthread_create (&thread, NULL, _on_copy_mail_clicked, dst);
}

void on_wm_reply_clicked (void) {
  GList *list;
  char *mid;
  Message *message;

  if (!GTK_CLIST (WMain->clist)->selection) return;
  
  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    if (!mid) return;

    message = message_get_message (selected_mbox, atoi (mid));
    c2_composer_new (message, C2_COMPOSER_REPLY);
  }
}

void on_wm_reply_all_clicked (void) {
  GList *list;
  char *mid;
  Message *message;

  if (!GTK_CLIST (WMain->clist)->selection) return;
  
  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    if (!mid) return;

    message = message_get_message (selected_mbox, atoi (mid));
    c2_composer_new (message, C2_COMPOSER_REPLY_ALL);
  }
}

void on_wm_forward_clicked (void) {
  GList *list;
  char *mid;
  Message *message;

  if (!GTK_CLIST (WMain->clist)->selection) return;

  for (list = GTK_CLIST (WMain->clist)->selection; list; list = list->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) list->data, 7, &mid);
    if (!mid) return;

    message = message_get_message (selected_mbox, atoi (mid));
    c2_composer_new (message, C2_COMPOSER_FORWARD);
  }
}

void on_wm_save_clicked (void) {
  char *file;
  char *subject;
  char *from;
  char *date;
  char *account;
  char *body;
  FILE *fd;
  GList *l;

  for (l = GTK_CLIST (WMain->clist)->selection; l; l = l->next) {
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) l->data, 3, &subject);
    if (!subject || !strlen (subject)) subject = g_strdup ("No Subject");
    file = gui_select_file_new (NULL, NULL, NULL, subject);
    if (!file) continue;
    
    if ((fd = fopen (file, "w")) == NULL) {
      cronos_error (errno, _("Opening the selected file for writing"), ERROR_WARNING);
      continue;
    }
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) l->data, 4, &from);
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) l->data, 5, &date);
    gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) l->data, 6, &account);
    body = gtk_editable_get_chars (GTK_EDITABLE (WMain->text), 0, -1);
    fprintf (fd, _("From: %s\nSubject: %s\nDate: %s\nAccount: %s\n\n%s\n"), from, subject, date, account, body);
    c2_free (body);
    fclose (fd);
  }
}

void on_wm_print_clicked (void) {
  Message *message;
  char *msg;
  GList *ll;

  ll = GTK_CLIST (WMain->clist)->selection;
  
  for (ll = GTK_CLIST (WMain->clist)->selection; ll; ll = ll->next) {
    message = message_copy ((Message*)gtk_object_get_data (GTK_OBJECT (WMain->text), "message"));
    msg = gtk_editable_get_chars (GTK_EDITABLE (WMain->text), 0, -1);
    
    if (!message || !msg) continue;
    cronos_print (message, msg);
    c2_free (msg);
    message_free (message);
  }
}

void
on_wm_text_button_press_event (GtkWidget *text, GdkEvent *event) {
  /* mouse wheel scroll up */
  if (event->button.button == 5) {
    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_TEXT (WMain->text)->vadj),
	GTK_ADJUSTMENT (GTK_TEXT (WMain->text)->vadj)->value+120); 
  }
  
  /* mouse wheel scroll down */
  if (event->button.button == 4) {
    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_TEXT (WMain->text)->vadj),
	GTK_ADJUSTMENT (GTK_TEXT (WMain->text)->vadj)->value-120);
  }
}

#if FALSE
/* Sorting */
int on_wm_clist_msg_state_sort (GtkCList *clist, gconstpointer s1, gconstpointer s2) {
  char *d1, *d2;

  d1 = (char *) ((GtkCListRow *) s1)->data;
  d2 = (char *) ((GtkCListRow *) s2)->data;

  if (!d1 && !d2) return 0;
  else if (d1 && !d2) return 1;
  else if (!d1 && d2) return 0;
  else return g_strcasecmp (d1, d2);
}

int on_wm_clist_date_sort (GtkCList *clist, gconstpointer s1, gconstpointer s2) {
  /* Function by Jeffrey Stedfast */
  GtkCListRow *row1, *row2;
  gchar *text1, *text2;
  date_t date1, date2;
  
  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);
  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);
  
  row1 = (GtkCListRow *) s1;
  row2 = (GtkCListRow *) s2;
  
  text1 = GTK_CELL_TEXT (row1->cell[clist->sort_column])->text;
  text2 = GTK_CELL_TEXT (row2->cell[clist->sort_column])->text;
  
  if (text2 == NULL)
    return (text1 != NULL);
  if (text1 == NULL)
    return -1;
  
  date1 = parse_date(text1);
  date2 = parse_date(text2);
  
  if (date1.year < date2.year) return 1;
  if (date1.year > date2.year) return -1;
  
  if (date1.month != 0 && date2.month != 0) {
    if (date1.month < date2.month) return 1;
    if (date1.month > date2.month) return -1;
  } else {
    fprintf(stderr, "Invalid date format! (%s)\n", date1.month == 0 ? text1 : text2);
    fflush(stderr);
  }
  
  if (date1.day < date2.day) return 1;
  if (date1.day > date2.day) return -1;
  
  if (date1.hour < date2.hour) return 1;
  if (date1.hour > date2.hour) return -1;
  
  if (date1.minute < date2.minute) return 1;
  if (date1.minute > date2.minute) return -1;
  
  if (date1.second < date2.second) return 1;
  if (date1.second > date2.second) return -1;
  
  return 0;
}

int on_wm_clist_str_sort (GtkCList *clist, gconstpointer s1, gconstpointer s2) {
  /* Function by Jeffrey Stedfast */
  GtkCListRow *row1, *row2;
  gchar *text1, *text2;
  gint retval, text1length, text2length;
  
  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);
  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);
  
  row1 = (GtkCListRow *) s1;
  row2 = (GtkCListRow *) s2;
  
  text1 = strip_common_subject_prefixes (GTK_CELL_TEXT (row1->cell[clist->sort_column])->text);
  text2 = strip_common_subject_prefixes (GTK_CELL_TEXT (row2->cell[clist->sort_column])->text);
  /* Since we may be stripping a prefix, we may also need to compare
   * original text lengths, so the non-prefixed show up before the
   * prefixed (unless the responses/forwards don't match subjects, 
   * of course ;) */
  text1length = strlen (GTK_CELL_TEXT (row1->cell[clist->sort_column])->text);
  text2length = strlen (GTK_CELL_TEXT (row2->cell[clist->sort_column])->text);
  
  if (!text2) {
    retval = text1 != NULL;
  }
  else if (!text1) {
    retval = -1;
  } else {
    retval = g_strcasecmp (text1, text2);
    if (retval == 0) {
      if (text1length < text2length)
	retval = -1;
      else if (text1length > text2length)
	retval = 1;
    }
  }
  
  c2_free (text1);
  c2_free (text2);
  return retval;
}

void on_wm_clist_click_column (GtkWidget *clist, int column, gpointer user_data) {
  if (column == rc->sort_column) {
    if (rc->sort_type == GTK_SORT_ASCENDING)
      rc->sort_type = GTK_SORT_DESCENDING;
    else
      rc->sort_type = GTK_SORT_ASCENDING;
    gtk_clist_set_sort_type (GTK_CLIST (clist), rc->sort_type);
  } else {
    rc->sort_column = column;
    gtk_clist_set_sort_column (GTK_CLIST (clist), rc->sort_column);
    rc->sort_type = GTK_SORT_ASCENDING;
    gtk_clist_set_sort_type (GTK_CLIST (clist), rc->sort_type);
    switch (rc->sort_column) {
      case 0: /* Sort by state of message (new, forwarded, replied, readed) */
	gtk_clist_set_compare_func (GTK_CLIST (clist), on_clist_msg_state_sort);
	break;
      case 5: /* Sort by date */
	gtk_clist_set_compare_func (GTK_CLIST (clist), on_clist_date_sort);
	break;
      default: /* Sort by char */
	gtk_clist_set_compare_func (GTK_CLIST (clist), on_clist_str_sort);
    }
  }
 
  gtk_clist_freeze (GTK_CLIST (clist));
  gtk_clist_sort (GTK_CLIST (clist));
  if (GTK_CLIST (clist)->selection)
    gtk_clist_moveto (GTK_CLIST (WMain->clist), (int) GTK_CLIST (clist)->selection->data, 0, 0, 0);
  gtk_clist_thaw (GTK_CLIST (clist));
  cronos_gui_update_clist_titles ();
}
#endif

#ifndef USE_OLD_MBOX_HANDLERS
static void
on_wm_mailbox_speed_up_clicked_do_mailbox (Pthread4 *helper) {
  GtkWidget *progress;
  GtkWidget *garbage_bytes_label;
  GtkWidget *speed_up_label;
  const Mailbox *_mailbox;
  const Mailbox *mailbox;
  char *buf;
 
  char meter[7];
  int garbage_bytes;
  gboolean kbytes = FALSE;
  char *line;
  char *strindex, *strtmp;
  FILE *fdindex,  *fdtmp;
  
  int lines, bad_lines;
 
  g_return_if_fail (helper);
  
  progress = helper->v1;
  garbage_bytes_label = helper->v2;
  speed_up_label = helper->v3;
  _mailbox = helper->v4;
  
  mailbox = _mailbox ? _mailbox : config->mailbox_head;
  garbage_bytes = 0;
  
  if (!_mailbox) {
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (progress), 0, 0, c2_mailbox_length (config->mailbox_head));
    gdk_threads_leave ();
  }

  for (; mailbox; mailbox = mailbox->next) {
    lines = bad_lines = 0;
    gtk_label_get (GTK_LABEL (garbage_bytes_label), &buf);
    sscanf (buf, "%d %s", &garbage_bytes, meter);
    if (streq (meter, "Kbytes")) garbage_bytes *= 1024;
    buf = g_strdup_printf ("%s - %%P %%%%", mailbox->name);
    gdk_threads_enter ();
    gtk_progress_set_format_string (GTK_PROGRESS (progress), buf);
    
    gtk_progress_set_value (GTK_PROGRESS (progress),
		gtk_progress_get_value (GTK_PROGRESS (progress))+1);
    gdk_threads_leave ();
    c2_free (buf);

    /* Open the index file */
    strindex = c2_mailbox_index_path (mailbox->name);
    if ((fdindex = fopen (strindex, "r")) == NULL) {
      gdk_threads_enter ();
      cronos_error (errno, _("Opening the index file"), ERROR_WARNING);
      gdk_threads_leave ();
      c2_free (strindex);
      continue;
    }

    /* Open the tmp file */
    strtmp = cronos_tmpfile ();
    if ((fdtmp = fopen (strtmp, "w")) == NULL) {
      gdk_threads_enter ();
      cronos_error (errno, _("Opening the tmp file"), ERROR_WARNING);
      gdk_threads_leave ();
      c2_free (strindex);
      c2_free (strtmp);
      fclose (fdindex);
      continue;
    }

    for (;;) {
      if ((line = fd_get_line (fdindex)) == NULL) break;
      if (*line == '?') {
	garbage_bytes += strlen (line)+1;
	if (garbage_bytes >= 1024) kbytes = TRUE;
	if (kbytes)
	  buf = g_strdup_printf ("%d Kbytes", garbage_bytes/1024);
	else
	  buf = g_strdup_printf ("%d bytes", garbage_bytes);
	gdk_threads_enter ();
	gtk_label_set_text (GTK_LABEL (garbage_bytes_label), buf);
	gdk_threads_leave ();
	bad_lines++;
      } else {
	fprintf (fdtmp, "%s\n", line);
      }
      c2_free (line);
      lines++;
    }
    
    fclose (fdindex);
    fclose (fdtmp);

    fd_mv (strtmp, strindex);
    c2_free (strtmp);
    c2_free (strindex);

    {
      double local, total;

      local = (double)bad_lines/(double)lines;
      
      gtk_label_get (GTK_LABEL (speed_up_label), &buf);
      sscanf (buf, "%lf %%", &total);
      total /= 100;
      if (total) total = (total+local)/2;
      else total = local;
      total *= 100;

      buf = g_strdup_printf ("%f %%", total);
      gdk_threads_enter ();
      gtk_label_set_text (GTK_LABEL (speed_up_label), buf);
      gdk_threads_leave ();
    }
    
    if (mailbox->child) {
      helper->v4 = (gpointer) mailbox->child;
      on_wm_mailbox_speed_up_clicked_do_mailbox (helper);
    }
  }
}

static void
on_wm_mailbox_speed_up_clicked_ok_btn_clicked (GtkWidget *window, GtkWidget *dialog) {
  gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
  gnome_dialog_close (GNOME_DIALOG (dialog));
}

void
on_wm_mailbox_speed_up_clicked (void) {
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *viewport;
  GtkWidget *dialog;
  GtkWidget *progress;
  GtkWidget *label;
  GtkWidget *garbage_bytes_label;
  GtkWidget *speed_up_label;
  Pthread4 *helper;
  pthread_t thread;

  dialog = gnome_dialog_new (_("Speed Up"), GNOME_STOCK_BUTTON_OK, NULL);
  gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (WMain->window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);
  gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0,
      			GTK_SIGNAL_FUNC (on_wm_mailbox_speed_up_clicked_ok_btn_clicked), dialog);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), progress, FALSE, TRUE, 0);
  gtk_widget_show (progress);
  gtk_progress_set_show_text (GTK_PROGRESS (progress), TRUE);
  gtk_progress_set_format_string (GTK_PROGRESS (progress), "%P %%");
  
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), viewport, TRUE, TRUE, 0);
  gtk_widget_show (viewport);

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (viewport), table);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 1);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);

  label = gtk_label_new (_("Garbage:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = gtk_label_new (_("Speed up:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  garbage_bytes_label = gtk_label_new ("0 bytes");
  gtk_table_attach (GTK_TABLE (table), garbage_bytes_label, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (garbage_bytes_label);
  gtk_misc_set_alignment (GTK_MISC (garbage_bytes_label), 0, 0.5);

  speed_up_label = gtk_label_new ("0 %");
  gtk_table_attach (GTK_TABLE (table), speed_up_label, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (speed_up_label);
  gtk_misc_set_alignment (GTK_MISC (speed_up_label), 0, 0.5);

  gtk_widget_show (dialog);

  helper = g_new0 (Pthread4, 1);
  helper->v1 = progress;
  helper->v2 = garbage_bytes_label;
  helper->v3 = speed_up_label;
  helper->v4 = NULL;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_wm_mailbox_speed_up_clicked_do_mailbox), helper);
}
#endif

void on_wm_check_account (GtkWidget *widget, gpointer data) {
  pthread_t thread;
  Account *account;

  account = (Account*)data;
  if (!window_checking) gui_window_checking_new ();
  gtk_widget_show (window_checking->window);
  gdk_window_raise (window_checking->window->window);
  pthread_create (&thread, NULL, PTHREAD_FUNC (check_account_single), account);
  pthread_detach (thread);
}

void on_wm_check_clicked (void) {
  pthread_t thread;
  
  if (!window_checking) gui_window_checking_new ();
  gtk_widget_show (window_checking->window);
  gdk_window_raise (window_checking->window->window);
  pthread_create (&thread, NULL, PTHREAD_FUNC (check_account_all), NULL);
  pthread_detach (thread);
}

void wm_sendqueue_clicked_thread (void) {
  Message *message;
  Pthread2 *helper;
  Mailbox *queue;
  gint n_mails;
  FILE *fd;
  char *buf, *line;
  mid_t mid;
  
  /* get number of mails in queue mailbox */
  queue = search_mailbox_name (config->mailbox_head, MAILBOX_QUEUE);
  n_mails = c2_mailbox_length (queue);
  
  /* obvious */
  if (n_mails <= 0) return;

  /* Disable widgets */
  gdk_threads_enter ();
  gtk_widget_set_sensitive (WMain->tb_w.sendqueue, FALSE);
  gtk_widget_set_sensitive (WMain->mb_w.menu_sendqueue, FALSE);
  gdk_threads_leave ();
 
  buf = c2_mailbox_index_path (MAILBOX_QUEUE);
  if ((fd = fopen (buf, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, _("Opening the main DB file of the mailbox"), ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (buf);
    return;
  }
  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;
    buf = str_get_word (7, line, '\r');
    if (!buf) continue;
    mid = atoi (buf);
    c2_free (buf);
    message = message_get_message (MAILBOX_QUEUE, mid);
    if (!message) {
      /* oops */
      break;
    }
    helper = g_new0 (Pthread2, 1);
    helper->v1 = (gpointer) message;
    helper->v2 = NULL; /* Sending without open composer window (Thanks Pablo ;-)*/ 
    
    smtp_main (helper); /* just send the message */
    gdk_threads_enter ();
    expunge_mail (MAILBOX_QUEUE, mid); /* expunge sended message */
    gdk_threads_leave ();
    if (streq (selected_mbox, MAILBOX_QUEUE))
      update_clist (MAILBOX_QUEUE, FALSE, FALSE);
    c2_free (helper);
  }
}

void on_wm_sendqueue_clicked (void) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (wm_sendqueue_clicked_thread), NULL);
}

/**
 * on_wm_btn_show_check_clicked
 *
 * Callback for the Button that shows the
 * Window Checking.
 **/
void
on_wm_btn_show_check_clicked (void) {
  if (!window_checking) gui_window_checking_new ();
  gtk_widget_show (window_checking->window);
  gdk_window_raise (window_checking->window->window);
}

void on_wm_compose_clicked (void) {
  c2_composer_new (NULL, C2_COMPOSER_NEW);
}

void on_wm_previous_clicked (void) {
  int row = 0;

  if (!GTK_CLIST (WMain->clist)->selection) return;
  if (reload_mailbox_list_timeout >= 0) gtk_timeout_remove (reload_mailbox_list_timeout);
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  if (g_list_length (GTK_CLIST (WMain->clist)->selection) > 1) {
    gdk_window_set_cursor (WMain->window->window, cursor_normal);
    return;
  }
  row = (int) GTK_CLIST (WMain->clist)->selection->data;
  gtk_clist_unselect_row (GTK_CLIST (WMain->clist), row, 3);
  gtk_clist_select_row (GTK_CLIST (WMain->clist), row-1, 3);
  if (gtk_clist_row_is_visible (GTK_CLIST (WMain->clist), row-1) != GTK_VISIBILITY_FULL) {
    gtk_clist_moveto (GTK_CLIST (WMain->clist), row-1, 0, 0, 0);
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
}

void on_wm_next_clicked (void) {
  int row = 0;

  if (!GTK_CLIST (WMain->clist)->selection) return;
  if (reload_mailbox_list_timeout >= 0) gtk_timeout_remove (reload_mailbox_list_timeout);
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  if (g_list_length (GTK_CLIST (WMain->clist)->selection) > 1) {
    gdk_window_set_cursor (WMain->window->window, cursor_normal);
    return;
  }
  row = (int) GTK_CLIST (WMain->clist)->selection->data;
  gtk_clist_unselect_row (GTK_CLIST (WMain->clist), row, 3);
  gtk_clist_select_row (GTK_CLIST (WMain->clist), row+1, 3);
  if (gtk_clist_row_is_visible (GTK_CLIST (WMain->clist), row+1) != GTK_VISIBILITY_FULL) {
    gtk_clist_moveto (GTK_CLIST (WMain->clist), row+1, 0, 1, 0);
  }

  gdk_window_set_cursor (WMain->window->window, cursor_normal);
}

void on_wm_mark_as_unreaded_clicked (void) {
  char *mid, *gmid;
  GList *list, *dlist;
  char *str_frm;
  char *str_dst;
  char *line;
  FILE *frm, *dst;
  gboolean proc;

  if (!GTK_CLIST (WMain->clist)->selection || !g_list_length (GTK_CLIST (WMain->clist)->selection)) return;

  str_frm = c2_mailbox_index_path (selected_mbox);
  if ((frm = fopen (str_frm, "r")) == NULL) {
    cronos_error (errno, "Opening the mailbox's index file", ERROR_WARNING);
    c2_free (str_frm);
    return;
  }

  str_dst = cronos_tmpfile ();
  /* This file is going to use fd_mv, which use tmp_files, so it shouldn't
   * be used here. */
  if ((dst = fopen (str_dst, "w")) == NULL) {
    cronos_error (errno, "Opening the tmp file", ERROR_WARNING);
    c2_free (str_dst);
    c2_free (str_frm);
    return;
  }

  list = g_list_copy (GTK_CLIST (WMain->clist)->selection);
  
  for (;;) {
    if ((line = fd_get_line (frm)) == NULL) break;
    
    gmid = str_get_word (7, line, '\r');

    /* Check if this mid should marked */
    proc = TRUE;
    for (dlist = list; dlist; dlist = dlist->next) {
      gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) dlist->data, 7, &mid);
      if (!mid) continue;
      if (streq (gmid, mid)) {
	proc = FALSE;
	list = g_list_remove_link (list, dlist);
	fprintf (dst, "N\r%s\r%s\r%s\r%s\r%s\r%s\r%s\n",
      		     str_get_word (1, line, '\r'), str_get_word (2, line, '\r'),
		     str_get_word (3, line, '\r'), str_get_word (4, line, '\r'),
		     str_get_word (5, line, '\r'), str_get_word (6, line, '\r'), mid);
	break;
      }
    }
    if (proc) {
      fprintf (dst, "%s\n", line);
    }

    c2_free (line);
    c2_free (gmid);
  }

  fclose (dst);
  fclose (frm);
  
  fd_mv (str_dst, str_frm);

  update_clist (selected_mbox, TRUE, TRUE);
}

void on_wm_mark_as_readed_clicked (void) {
  char *mid, *gmid;
  GList *list, *dlist;
  char *str_frm;
  char *str_dst;
  char *line;
  FILE *frm, *dst;
  gboolean proc;

  if (!GTK_CLIST (WMain->clist)->selection || !g_list_length (GTK_CLIST (WMain->clist)->selection)) return;

  str_frm = c2_mailbox_index_path (selected_mbox);
  if ((frm = fopen (str_frm, "r")) == NULL) {
    cronos_error (errno, "Opening the mailbox's index file", ERROR_WARNING);
    c2_free (str_frm);
    return;
  }

  str_dst = cronos_tmpfile ();
  /* This file is going to use fd_mv, which use tmp_files, so it shouldn't
   * be used here. */
  if ((dst = fopen (str_dst, "w")) == NULL) {
    cronos_error (errno, "Opening the tmp file", ERROR_WARNING);
    c2_free (str_dst);
    c2_free (str_frm);
    return;
  }

  list = g_list_copy (GTK_CLIST (WMain->clist)->selection);
  
  for (;;) {
    if ((line = fd_get_line (frm)) == NULL) break;
    
    gmid = str_get_word (7, line, '\r');

    /* Check if this mid should marked */
    proc = TRUE;
    for (dlist = list; dlist; dlist = dlist->next) {
      gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) dlist->data, 7, &mid);
      if (!mid) continue;
      if (streq (gmid, mid)) {
	proc = FALSE;
	list = g_list_remove_link (list, dlist);
	fprintf (dst, " \r%s\r%s\r%s\r%s\r%s\r%s\r%s\n",
      		     str_get_word (1, line, '\r'), str_get_word (2, line, '\r'),
		     str_get_word (3, line, '\r'), str_get_word (4, line, '\r'),
		     str_get_word (5, line, '\r'), str_get_word (6, line, '\r'), mid);
	break;
      }
    }
    if (proc) {
      fprintf (dst, "%s\n", line);
    }

    c2_free (line);
    c2_free (gmid);
  }

  fclose (dst);
  fclose (frm);
  
  fd_mv (str_dst, str_frm);

  update_clist (selected_mbox, TRUE, TRUE);
}

void on_wm_mark_as_replied_clicked (void) {
  char *mid, *gmid;
  GList *list, *dlist;
  char *str_frm;
  char *str_dst;
  char *line;
  FILE *frm, *dst;
  gboolean proc;

  if (!GTK_CLIST (WMain->clist)->selection || !g_list_length (GTK_CLIST (WMain->clist)->selection)) return;

  str_frm = c2_mailbox_index_path (selected_mbox);
  if ((frm = fopen (str_frm, "r")) == NULL) {
    cronos_error (errno, "Opening the mailbox's index file", ERROR_WARNING);
    c2_free (str_frm);
    return;
  }

  str_dst = cronos_tmpfile ();
  /* This file is going to use fd_mv, which use tmp_files, so it shouldn't
   * be used here. */
  if ((dst = fopen (str_dst, "w")) == NULL) {
    cronos_error (errno, "Opening the tmp file", ERROR_WARNING);
    c2_free (str_dst);
    c2_free (str_frm);
    return;
  }

  list = g_list_copy (GTK_CLIST (WMain->clist)->selection);
  
  for (;;) {
    if ((line = fd_get_line (frm)) == NULL) break;
    
    gmid = str_get_word (7, line, '\r');

    /* Check if this mid should marked */
    proc = TRUE;
    for (dlist = list; dlist; dlist = dlist->next) {
      gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) dlist->data, 7, &mid);
      if (!mid) continue;
      if (streq (gmid, mid)) {
	proc = FALSE;
	list = g_list_remove_link (list, dlist);
	fprintf (dst, "R\r%s\r%s\r%s\r%s\r%s\r%s\r%s\n",
      		     str_get_word (1, line, '\r'), str_get_word (2, line, '\r'),
		     str_get_word (3, line, '\r'), str_get_word (4, line, '\r'),
		     str_get_word (5, line, '\r'), str_get_word (6, line, '\r'), mid);
	break;
      }
    }
    if (proc) {
      fprintf (dst, "%s\n", line);
    }

    c2_free (line);
    c2_free (gmid);
  }

  fclose (dst);
  fclose (frm);
  
  fd_mv (str_dst, str_frm);

  update_clist (selected_mbox, TRUE, TRUE);
}

void on_wm_mark_as_forwarded_clicked (void) {
  char *mid, *gmid;
  GList *list, *dlist;
  char *str_frm;
  char *str_dst;
  char *line;
  FILE *frm, *dst;
  gboolean proc;

  if (!GTK_CLIST (WMain->clist)->selection || !g_list_length (GTK_CLIST (WMain->clist)->selection)) return;

  str_frm = c2_mailbox_index_path (selected_mbox);
  if ((frm = fopen (str_frm, "r")) == NULL) {
    cronos_error (errno, "Opening the mailbox's index file", ERROR_WARNING);
    c2_free (str_frm);
    return;
  }

  str_dst = cronos_tmpfile ();
  /* This file is going to use fd_mv, which use tmp_files, so it shouldn't
   * be used here. */
  if ((dst = fopen (str_dst, "w")) == NULL) {
    cronos_error (errno, "Opening the tmp file", ERROR_WARNING);
    c2_free (str_dst);
    c2_free (str_frm);
    return;
  }

  list = g_list_copy (GTK_CLIST (WMain->clist)->selection);
  
  for (;;) {
    if ((line = fd_get_line (frm)) == NULL) break;
    
    gmid = str_get_word (7, line, '\r');

    /* Check if this mid should marked */
    proc = TRUE;
    for (dlist = list; dlist; dlist = dlist->next) {
      gtk_clist_get_text (GTK_CLIST (WMain->clist), (int) dlist->data, 7, &mid);
      if (!mid) continue;
      if (streq (gmid, mid)) {
	proc = FALSE;
	list = g_list_remove_link (list, dlist);
	fprintf (dst, "F\r%s\r%s\r%s\r%s\r%s\r%s\r%s\n",
      		     str_get_word (1, line, '\r'), str_get_word (2, line, '\r'),
		     str_get_word (3, line, '\r'), str_get_word (4, line, '\r'),
		     str_get_word (5, line, '\r'), str_get_word (6, line, '\r'), mid);
	break;
      }
    }
    if (proc) {
      fprintf (dst, "%s\n", line);
    }

    c2_free (line);
    c2_free (gmid);
  }

  fclose (dst);
  fclose (frm);
  
  fd_mv (str_dst, str_frm);

  update_clist (selected_mbox, TRUE, TRUE);
}

void
on_wm_view_menu_from_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 2;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 2) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_FROM][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_FROM][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_FROM][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_FROM][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_account_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 4;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 4) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_ACCOUNT][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_ACCOUNT][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_to_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 0;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 0) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_TO][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_TO][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_cc_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 5;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 5) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_CC][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_CC][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_bcc_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 6;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 6) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_BCC][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_BCC][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_date_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 1;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 1) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_DATE][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_DATE][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_DATE][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_DATE][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_subject_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 3;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 3) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_SUBJECT][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_SUBJECT][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_view_menu_priority_activate (GtkWidget *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] ^= 1 << 7;
  if (rc->showable_headers[SHOWABLE_HEADERS_PREVIEW] & 1 << 7) {
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_show (WMain->header_titles[HEADER_TITLES_PRIORITY][1]);
  } else {
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_hide (WMain->header_titles[HEADER_TITLES_PRIORITY][1]);
    gtk_widget_set_usize (WMain->header_table, -1, -1);
  }
}

void
on_wm_persistent_smtp_connect (void) {
  Pthread3 *helper;
  pthread_t thread;
  
  helper = g_new0 (Pthread3, 1);
  helper->v1 = config->persistent_smtp_address;
  helper->v2 = (gpointer) config->persistent_smtp_port;
  helper->v3 = WMain->appbar;
  
  pthread_create (&thread, NULL, PTHREAD_FUNC (smtp_persistent_connect), helper);
}

void
on_wm_persistent_smtp_disconnect (void) {
  smtp_do_persistent_disconnect (TRUE);
}

void
on_wm_address_book_clicked (void) {
  if (!gaddrbook) gaddrbook = c2_address_book_new ();
  else {
    gtk_widget_show (gaddrbook->window);
    gdk_window_raise (gaddrbook->window->window);
  }
}

void on_wm_bug_report_clicked (void) {
  gnome_url_show ("http://sourceforge.net/bugs/?group_id=7093");
}

void update_title (int mails, int new_mails) {
  char *title;
  char *buf;
  char *buf2;

  buf = g_strdup (rc->title);
  title = str_replace (buf, "%a", "Cronos II");
  c2_free (buf);
  buf = title;
  title = str_replace (buf, "%v", VERSION);
  c2_free (buf);
  buf = title;
  buf2 = g_strdup_printf ("%d", mails);
  title = str_replace (buf, "%m", buf2);
  c2_free (buf);
  c2_free (buf2);
  buf = title;
  buf2 = g_strdup_printf ("%d", new_mails);
  title = str_replace (buf, "%n", buf2);
  c2_free (buf);
  c2_free (buf2);
  buf = title;
  title = str_replace (buf, "%M", selected_mbox);
  c2_free (buf);

  gtk_window_set_title (GTK_WINDOW (WMain->window), title);
}

void on_wm_quit (void) {
  cronos_exit ();
}

void on_wm_size_allocate (GtkWidget *window, GtkAllocation *alloc) {
  rc->main_window_height = alloc->height;
  rc->main_window_width = alloc->width;
}

#if USE_PLUGINS
static void
on_wm_focus_in_event_thread (void) {
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_WINDOW_FOCUS, "main", NULL, NULL, NULL, NULL);
}

gboolean
on_wm_focus_in_event (void) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_wm_focus_in_event_thread), NULL);
  return FALSE;
}
#endif
