/*  Cronos II /src/spool.c
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
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "spool.h"
#include "utils.h"
#include "debug.h"
#include "message.h"
#include "check.h"
#include "mailbox.h"
#include "account.h"
#include "pop.h"
#include "init.h"
#include "index.h"

#include "gui-window_main.h"
#include "gui-window_checking.h"
#include "gui-window_main_callbacks.h"
#include "gui-utils.h"

void check_spool_main (Account *account) {
  int messages;
  int i, _i;
  Message *message;
  FILE *fd;
  FILE *index;
  FILE *mail;
  char *subject, *from, *date, *msgid;
  char *buf;
  mid_t mid;
  char *row[8];
  GtkStyle *style, *style2;

  g_return_if_fail (account);

  if ((fd = fopen (account->protocol.spool.file, "r")) == NULL) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, _("Could not open the specified file."));
    gdk_threads_leave ();
    return;
  }

  messages = spool_get_messages_in_mailbox (fd);
  fseek (fd, 0, SEEK_SET);

  buf = c2_mailbox_index_path (account->mailbox->name);
  if ((index = fopen (buf, "a")) == NULL) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, _("Could not open the main DB file for appending the new messages"));
    gdk_threads_leave ();
    c2_free (buf);
    fclose (fd);
    return;
  }

  gdk_threads_enter ();
  gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), 0, 0, messages);
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      _("%p%% downloaded (%v of %u messages)"));
  gdk_threads_leave ();
  for (i = 1, _i = 0; i <= messages; i++, _i++) {
    message = spool_get_message_nth (fd, _i);
    if (!message) {
      messages--;
      gdk_threads_enter ();
      gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), i, 0, messages);
      gdk_threads_leave ();
      continue;
    }
    
    if (account->keep_copy) {
      msgid = message_get_header_field (message, NULL, "\nMessage-Id:");
      if (msgid) {
	if (uidl_check (msgid, account->acc_name)) {
	  c2_free (message->message);
	  c2_free (message->header);
	  c2_free (message);
	  c2_free (msgid);
	  i--; messages--;
	  if (i < 0) i = 0;
	  if (messages < 0) messages = 0;
	  gdk_threads_enter ();
	  gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), i, 0, messages);
	  gdk_threads_leave ();
	  continue;
	}
	uidl_register (msgid, account->acc_name);
	c2_free (msgid);
      }
    }

    gdk_threads_enter ();
    gtk_progress_set_value (GTK_PROGRESS (window_checking->mail_progress),
	  gtk_progress_get_value (GTK_PROGRESS (window_checking->mail_progress))+1);
    gdk_threads_leave ();

    mid = c2_mailbox_get_next_mid (account->mailbox);
    buf = c2_mailbox_mail_path (account->mailbox->name, mid);
    if ((mail = fopen (buf, "w")) == NULL) {
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name, _("Could not open the mail file for writing the new message"));
      gdk_threads_leave ();
      c2_free (buf);
      fclose (fd);
      fclose (index);
      return;
    }
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_SPOOL, message,
      				 	NULL, NULL, NULL, NULL);
#endif
    fprintf (mail, "%s", message->message);
    fclose (mail);
    subject = message_get_header_field (message, NULL, "\nSubject:");
    from = message_get_header_field (message, NULL, "\nFrom:");
    date = message_get_header_field (message, NULL, "\nDate:");
    c2_free (message->message);
    c2_free (message->header);
    c2_free (message);

    fprintf (index, "N\r\r\r%s\r%s\r%s\r%s\r%d\n",
			subject, from, date, account->acc_name, mid);
    fflush (index);

    if (streq (selected_mbox, account->mailbox->name)) {
      row[0] = "";
      row[1] = "";
      row[2] = "";
      row[3] = subject;
      row[4] = from;
      row[5] = date;
      row[6] = account->acc_name;
      row[7] = g_strdup_printf ("%d", mid);
      gdk_threads_enter ();
      gtk_clist_freeze (GTK_CLIST (WMain->clist));
      gtk_clist_append (GTK_CLIST (WMain->clist), row);
      style = gtk_widget_get_style (WMain->clist);
      style2 = gtk_style_copy (style);
      style2->font = font_unread;
      gtk_clist_set_row_style (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, style2);
      gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 0, pixmap_unread, mask_unread);
      new_messages++;
      gtk_clist_thaw (GTK_CLIST (WMain->clist));
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, (gpointer) "N");
      update_wm_title ();
      gdk_threads_leave ();
    }
  }

  fclose (fd);
  fclose (index);
  
  if (messages != 1)
    buf = g_strdup_printf (_("%d messages downloaded."), messages);
  else
    buf = g_strdup (_("1 message downloaded."));

  /* This is the deleting, will change when filters are activated */
  if (!account->keep_copy) {
    if ((fd = fopen (account->protocol.spool.file, "w")) == NULL) {
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name, _("Couldn't open the specified file."));
      gdk_threads_leave ();
      return;
    }
    fclose (fd);
  }
  
  gdk_threads_enter ();
  window_checking_report (C2_CHECK_OK, account->acc_name, buf);
  gdk_threads_leave ();
}

int spool_get_messages_in_mailbox (FILE *fd) {
  char *line;
  gboolean last_empty = TRUE;
  int messages = 0;

  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;
    if (!strlen (line)) last_empty = TRUE;
    if ((!strncmp (line, "From ", 5)) && last_empty) messages++;
    c2_free (line);
  }

  return messages;
}

Message *spool_get_message_nth (FILE *fd, int msg) {
  char *line;
  Message *message;
  GString *buf;
  GString *buf2;
  gboolean last_empty = TRUE;

  /* Check that this looks like the first line of a message */
  if ((line = fd_get_line (fd)) == NULL) return NULL;
  if (strncmp (line, "From ", 5)) {
    /* This is not the first line of a message */
    for (;;) {
      if ((line = fd_get_line (fd)) == NULL) return NULL;
      if (!strlen (line)) last_empty = TRUE;
      if (!strncmp (line, "From ", 5) && last_empty) {
	c2_free (line);
	break;
      }
    }
  }
  buf = g_string_new (NULL);
  buf2 = g_string_new (NULL);
  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;
    if (!strlen (line)) last_empty = TRUE;

    if (!strncmp (line, "From ", 5) && last_empty) {
      fseek (fd, 0-strlen (line)-1, SEEK_CUR);
      break;
    }

    if (last_empty && strlen (line)) last_empty = FALSE;
    
    g_string_sprintf (buf2, "%s\n", line);
    g_string_append (buf, buf2->str);
    c2_free (line);
  }

  c2_message_new (message);
  message->message = buf->str;
  message->header = NULL;
  message->body = NULL;
  g_string_free (buf2, TRUE);
  g_string_free (buf, FALSE);

  return message;
}
