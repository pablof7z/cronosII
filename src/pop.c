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

#include "main.h"
#include "pop.h"
#include "net.h"
#include "debug.h"
#include "utils.h"
#include "error.h"
#include "init.h"
#include "mailbox.h"
#include "message.h"
#include "search.h"

#include "gui-window_checking.h"
#include "gui-window_main.h"
#include "gui-utils.h"

enum {
  DOWNLOAD_LIST_TOTAL,
  DOWNLOAD_LIST_UIDL,
  DOWNLOAD_LIST_TOP,
  DOWNLOAD_LIST_LAST
};

enum {
  HEADER_SUBJECT,
  HEADER_FROM,
  HEADER_DATE,
  HEADER_LAST
};

static int
pop_check_answer						(const char *buf, const Account *account,
    								 int timedout);

static gboolean
gui_message_big_new						(const char *from, const char *subject,
    								 const char *date, const  char *account,
								 const  char *kbytes);

static gboolean
gui_ask_password						(Account *account);

void *
check_pop_main (Account *account) {
  char *buf = NULL, *buf2, *buf3;
  C2ResolveNode *resolve;
  int sock;
  int timedout = FALSE;
  struct sockaddr_in server;
  
  int messages = 0, bytes = 0, downloaded_bytes = 0, i = 0, password_errors = 3;
  
  GList *download[DOWNLOAD_LIST_LAST], *uidl_search = NULL, *top_search = NULL;
  GList *list;
  gboolean supports_uidl = FALSE;
  
  mid_t mid;
  
  FILE *index;
  FILE *mail;

  Message message;
  char *mailbox;
  Mailbox *mbox;
  GString *strmsg;
  char *header[HEADER_LAST];
  gboolean reading_header = TRUE;
  gboolean with_attachs = FALSE;
  char *content_type;

  char *row[8];
  GtkStyle *style, *style2;
  gboolean clisted = FALSE;
  
  g_return_val_if_fail (account, NULL);
  g_return_val_if_fail (account->type == C2_ACCOUNT_POP, NULL);

  download[DOWNLOAD_LIST_TOTAL] = NULL;
  download[DOWNLOAD_LIST_UIDL] = NULL;
  download[DOWNLOAD_LIST_TOP] = NULL;  

  resolve = c2_resolve (account->protocol.pop.host, &buf);
  if (buf) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    return NULL;
  }

  sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, _("Failed to create socket"));
    gdk_threads_leave ();
    return NULL;
  }

  server.sin_family	= AF_INET;
  server.sin_port	= htons (account->protocol.pop.host_port);
  server.sin_addr.s_addr= inet_addr (resolve->ip);

  if (connect (sock, (struct sockaddr *)&server, sizeof (server)) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    return NULL;
  }

  /* Guten Morgen, Herr Server! */
  buf = sock_read (sock, &timedout);
  if (pop_check_answer (buf, account, timedout) < 0) {
    if (timedout) goto run_for_your_life;
    goto bye_bye_server;
  }
  c2_free (buf);

  /* Log In */
  gdk_threads_enter ();
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
     		_("Logging in..."));
  gdk_threads_leave ();

retry_login:
  if (sock_printf (sock, "USER %s\r\n", account->protocol.pop.usr_name) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = sock_read (sock, &timedout);
  if (pop_check_answer (buf, account, timedout) < 0) {
    if (timedout) goto run_for_your_life;
    goto bye_bye_server;
  }
  c2_free (buf);

  if (sock_printf (sock, "PASS %s\r\n", account->protocol.pop.pass) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = sock_read (sock, &timedout);
  if (strnne (buf, "+OK", 3)) {
    if (--password_errors < 0) {
      if (pop_check_answer (buf, account, timedout) < 0) {
	if (timedout) goto run_for_your_life;
	goto bye_bye_server;
      }
    }
    gdk_threads_enter ();
    if (!gui_ask_password (account)) {
      gdk_threads_leave ();
      if (pop_check_answer (buf, account, timedout) < 0) {
	if (timedout) goto run_for_your_life;
	goto bye_bye_server;
      }
    } else {
      gdk_threads_leave ();
      goto retry_login;
    }
  }
  c2_free (buf);

  /* STAT */
  gdk_threads_enter ();
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      		_("Checking for number of mails in server..."));
  gdk_threads_leave ();

  if (sock_printf (sock, "STAT\r\n") < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = sock_read (sock, &timedout);
  if (pop_check_answer (buf, account, timedout) < 0) {
    if (timedout) goto run_for_your_life;
    goto bye_bye_server;
  }
  sscanf (buf, "+OK %d ", &messages);
  c2_free (buf);
 
  if (!messages) {
    gdk_threads_enter ();
    gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      		_("No messages in server"));
    window_checking_report (C2_CHECK_OK, account->acc_name, _("No messages to download"));
    gdk_threads_leave ();
    clisted = TRUE;
    goto bye_bye_server;
  }
  else if (messages != 1)
    buf = g_strdup_printf (_("%d messages in server"), messages);
  else
    buf = g_strdup_printf (_("1 message in server"));
  gdk_threads_enter ();
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      		buf);
  gdk_threads_leave ();
  c2_free (buf);

  /* UIDL */
  if (!account->keep_copy) {
dont_use_uidl:
    /* Without UIDL*/
    for (i = 1; i <= messages; i++) {
      buf = g_strdup_printf ("%d", i);
      download[DOWNLOAD_LIST_UIDL] = g_list_append (download[DOWNLOAD_LIST_UIDL], (gpointer) buf);
    }
  } else {
    /* With UIDL */
    if (sock_printf (sock, "UIDL\r\n") < 0) {
      buf = g_strerror (errno);
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
      gdk_threads_leave ();
      goto bye_bye_server;
    }
    
    for (i = 0;; i++) {
      buf = sock_read (sock, &timedout);
      if (!i && strnne (buf, "+OK", 3)) {
	/* UIDL is optional for POP servers,
	 * so I won't complain if server doesn't like it */
	buf2 = g_strdup_printf (_("The POP server of the account %s doesn't support UIDL."),
	    	account->acc_name);
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf2);
	gdk_threads_leave ();
	supports_uidl = FALSE;
	goto dont_use_uidl;
      }
      supports_uidl = TRUE;
      if (!i) continue;
      if (streq (buf, ".\r\n")) break;

      buf2 = str_get_word (1, buf, ' ');
      buf3 = str_strip (buf2, '\r');
      buf2 = str_strip (buf3, '\n');
      if (!uidl_check (buf2, account->acc_name)) {
	download[DOWNLOAD_LIST_UIDL] = g_list_append (download[DOWNLOAD_LIST_UIDL], buf);
      }
    }
  }
 
  /* TOP */
  if (!config->message_bigger) {
    /* Without TOP */
dont_use_list:
dont_use_top:
    for (i = 1; i <= messages; i++)
      	download[DOWNLOAD_LIST_TOP] = g_list_append (download[DOWNLOAD_LIST_TOP], (gpointer) i);
  } else {
    /* With TOP */
    char *subject, *from, *date, *kbytes;
    
    if (sock_printf (sock, "LIST\r\n") < 0) {
      buf = g_strerror (errno);
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
      gdk_threads_leave ();
      goto bye_bye_server;
    }
    
    for (i = 0;; i++) {
      buf = sock_read (sock, &timedout);
      if (!i && strnne (buf, "+OK", 3)) {
	buf2 = g_strdup_printf (_("The POP server of the account %s doesn't support LIST."),
	    	account->acc_name);
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf2);
	gdk_threads_leave ();
	goto dont_use_list;
      }
      if (!i) continue;
      if (streq (buf, ".\r\n")) break;
      buf2 = str_get_word (1, buf, ' ');
      str_strip (buf2, '\r');
      str_strip (buf2, '\n');
      download[DOWNLOAD_LIST_TOP] = g_list_append (download[DOWNLOAD_LIST_TOP], buf);
      c2_free (buf2);
    }

    for (list = download[DOWNLOAD_LIST_TOP]; list; list = list->next) {
      if (sock_printf (sock, "TOP %d 0\r\n", atoi (CHAR (list->data))) < 0) {
	buf = g_strerror (errno);
	gdk_threads_enter ();
	window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
	gdk_threads_leave ();
	goto bye_bye_server;
      }

      strmsg = g_string_new (NULL);
      for (i = 0;; i++) {
	buf = sock_read (sock, &timedout);
	if (!i && strnne (buf, "+OK", 3)) {
	  buf2 = g_strdup_printf (_("The POP server of the account %s doesn't support TOP."),
	      account->acc_name);
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf2);
	  gdk_threads_leave ();
	  goto dont_use_top;
	}
	if (!i) continue;
	if (streq (buf, ".\r\n")) break;
	g_string_append (strmsg, buf);
	c2_free (buf);
      }
      subject = message_get_header_field (NULL, strmsg->str, "\nSubject:");
      from = message_get_header_field (NULL, strmsg->str, "From:");
      date = message_get_header_field (NULL, strmsg->str, "\nDate:");
      kbytes = str_get_word (1, CHAR (list->data), ' '); str_strip (kbytes, '\r');str_strip (kbytes, '\n');
      gdk_threads_enter ();
      if ((atoi (kbytes) >= config->message_bigger*1024) &&
	  (!gui_message_big_new (from, subject, date, account->acc_name, kbytes))) {
	gdk_threads_leave ();
	c2_free (list->data);
	list->data = NULL;
	download[DOWNLOAD_LIST_TOP] = g_list_remove_link (download[DOWNLOAD_LIST_TOP], list);
      } else gdk_threads_leave ();
    }
  }

  /* Learn messages to download */
  if (!account->keep_copy && !config->message_bigger) {		/* !UIDL AND !TOP */
    download[DOWNLOAD_LIST_TOTAL] = download[DOWNLOAD_LIST_UIDL];
  }
  else if (account->keep_copy && !config->message_bigger) {	/*  UIDL AND !TOP */
    for (list = download[DOWNLOAD_LIST_UIDL]; list; list = list->next) {
      download[DOWNLOAD_LIST_TOTAL] = g_list_append (download[DOWNLOAD_LIST_TOTAL], 
	 					str_get_word (0, CHAR (list->data), ' '));
    }
  }
  else if (!account->keep_copy && config->message_bigger) {	/* !UIDL AND  TOP */
    download[DOWNLOAD_LIST_TOTAL] = download[DOWNLOAD_LIST_TOP];
  }
  else if (account->keep_copy && config->message_bigger) {	/*  UIDL AND  TOP */
    for (uidl_search = download[DOWNLOAD_LIST_UIDL]; !uidl_search; uidl_search = uidl_search->next) {
      for (top_search = download[DOWNLOAD_LIST_TOP]; !top_search; top_search = top_search->next) {
	printf ("%d %d\n", (int) uidl_search->data, (int) top_search->data); /* TODO */
      }
    }
  }

  messages = g_list_length (download[DOWNLOAD_LIST_TOTAL]);
  gdk_threads_enter ();
  gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), 0, 0, messages);
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      				_("%p%% downloaded (%v of %u messages)"));
  gdk_threads_leave ();

  strmsg = g_string_new (NULL);
  message.message = message.header = NULL;
  for (list = download[DOWNLOAD_LIST_TOTAL]; list; list = list->next) {
    buf = str_get_word (0, CHAR (list->data), ' ');
    i = atoi (buf);
    c2_free (buf);
    /* Ask for the mail */
    if (sock_printf (sock, "RETR %d\r\n", i) < 0) {
      buf = g_strerror (errno);
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
      gdk_threads_leave ();
      goto bye_bye_server;
    }

    /* Read the first line */
    buf = sock_read (sock, &timedout);
    if (pop_check_answer (buf, account, timedout) < 0) {
      if (timedout) goto run_for_your_life;
      goto bye_bye_server;
    }
    /* Learn bytes in the messages */
    sscanf (buf, "+OK %d octets\r\n", &bytes);
    if (bytes) {
      gdk_threads_enter ();
      gtk_progress_configure (GTK_PROGRESS (window_checking->bytes_progress), 0, 0, bytes);
      gtk_widget_show (window_checking->bytes_progress);
      gdk_threads_leave ();
    } else {
      gdk_threads_enter ();
      gtk_widget_hide (window_checking->bytes_progress);
      gdk_threads_leave ();
    }
    c2_free (buf);
    
    /* Get the mail */
    reading_header = TRUE;
    for (;;) {
      buf = sock_read (sock, &timedout);
      if (bytes) {
	downloaded_bytes += strlen (buf);
	gdk_threads_enter ();
	gtk_progress_set_value (GTK_PROGRESS (window_checking->bytes_progress), downloaded_bytes);
	gdk_threads_leave ();
      }
      if (streq (buf, ".\r\n")) {
	message.message = g_strdup (strmsg->str);
	g_string_assign (strmsg, "");
	str_strip (message.message, '\r');
	break;
      }
      if (reading_header && strlen (buf) > 2) {
	char *buf2;
	buf2 = decode_8bit (buf);
	c2_free (buf);
	buf = buf2;
      }
      if (reading_header && strlen (buf) == 2) { /* Still reading header and is an empty line */
	buf2 = g_strdup_printf ("X-CronosII-Account: %s\r\n", account->acc_name);
	g_string_append (strmsg, buf2);
	c2_free (buf2);
	reading_header = FALSE;
      }
      g_string_append (strmsg, buf);
    }
    gtk_progress_set_percentage (GTK_PROGRESS (window_checking->bytes_progress), 1);

    /* Write to the mail file */
    mailbox = account->mailbox->name;
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP, &message,
      				 	&mailbox, NULL, NULL, NULL);
#endif
    mbox = search_mailbox_name (config->mailbox_head, mailbox);
    if (!mbox) {
      /* Mailbox couldn't be found, going with the default */
      mbox = account->mailbox;
    }
    mid = c2_mailbox_get_next_mid (mbox);
    buf = c2_mailbox_mail_path (mailbox, mid);
    if ((mail = fopen (buf, "w")) == NULL) {
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name,
	  _("Error opening the file where to store the new mail"));
      cronos_error (errno, _("Opening the mail file"), ERROR_WARNING);
      gdk_threads_leave ();
      c2_free (buf);
      continue;
    }
    c2_free (buf);
    fprintf (mail, "%s", message.message);
    fclose (mail);

    /* Write to the index file */
    buf = c2_mailbox_index_path (mailbox);
    if ((index = fopen (buf, "a")) == NULL) {
      gdk_threads_enter ();
      window_checking_report (C2_CHECK_ERR, account->acc_name,
	  _("Error opening the main DB file to store the new mail"));
      cronos_error (errno, _("Opening the main DB file"), ERROR_WARNING);
      gdk_threads_leave ();
      c2_free (buf);
      goto bye_bye_server;
    }
    header[HEADER_SUBJECT]	= message_get_header_field (&message, NULL, "\nSubject:");
    header[HEADER_FROM]		= message_get_header_field (&message, NULL, "\nFrom:");
    header[HEADER_DATE]		= message_get_header_field (&message, NULL, "\nDate:");
    content_type		= message_get_header_field (&message, NULL, "\nContent-Type:");
    with_attachs		= FALSE;
/*    if (content_type) {
      message_mime_parse_content_type (content_type, &type, &subtype, &parameter);
      if (streq (type, "multipart")) {
	GList *s;
	MimeHash *mime;
	message_mime_parse (&message, NULL);
	for (s = message.mime; s != NULL; s = s->next) {
	  mime = MIMEHASH (s->data);
	  if (!mime) continue;
	  if (strneq (mime->disposition, "attachment", 10)) with_attachs = TRUE;
	}
      }
    }*/

    if (!header[HEADER_SUBJECT]) header[HEADER_SUBJECT] = "";
    if (!header[HEADER_FROM]) header[HEADER_FROM] = "";
    if (!header[HEADER_DATE]) header[HEADER_DATE] = "";
    fprintf (index, "N\r\r%s\r%s\r%s\r%s\r%s\r%d\n",
		with_attachs ? "1" : "", header[HEADER_SUBJECT], header[HEADER_FROM], header[HEADER_DATE],
		account->acc_name, mid);
    fclose (index);
    c2_free (message.message);
    c2_free (message.header);
    message.message = message.header = NULL;

    if (!account->keep_copy) {
      /* Delete the message */
      if (sock_printf (sock, "DELE %d\r\n", i) < 0) {
	buf = g_strerror (errno);
	gdk_threads_enter ();
	window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
	gdk_threads_leave ();
	goto bye_bye_server;
      }
      buf = sock_read (sock, &timedout);
      if (pop_check_answer (buf, account, timedout) < 0) {
	if (timedout) goto run_for_your_life;
	goto bye_bye_server;
      }
    }
    
    if (streq (selected_mbox, account->mailbox->name)) {
      row[0] = "";
      row[1] = "";
      row[2] = "";
      row[3] = header[HEADER_SUBJECT];
      row[4] = header[HEADER_FROM];
      row[5] = header[HEADER_DATE];
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
      if (with_attachs) gtk_clist_set_pixmap (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, 2, pixmap_attach, mask_attach);
      new_messages++;
      gtk_clist_thaw (GTK_CLIST (WMain->clist));
      gtk_clist_set_row_data (GTK_CLIST (WMain->clist), GTK_CLIST (WMain->clist)->rows-1, (gpointer) "N");
      update_wm_title ();
      gtk_progress_set_value (GTK_PROGRESS (window_checking->mail_progress), i);
      gdk_threads_leave ();
      clisted = TRUE;
    }
    gdk_threads_enter ();
    gtk_progress_set_value (GTK_PROGRESS (window_checking->mail_progress),
		gtk_progress_get_value (GTK_PROGRESS (window_checking->mail_progress))+1);
    gdk_threads_leave ();
  }
  if (supports_uidl) {
    GList *llist;
    for (llist = download[DOWNLOAD_LIST_UIDL]; llist != NULL; llist = llist->next) {
      char *uidl;
      uidl = CHAR (llist->data);
      buf2 = str_get_word (1, uidl, ' ');
      buf3 = str_strip (buf2, '\r');
      buf2 = str_strip (buf3, '\n');
      if (buf2) {
	uidl_register (buf2, account->acc_name);
      }
    }
  }
        
  if (messages != 1)
    buf = g_strdup_printf (_("%d messages downloaded."), messages);
  else
    buf = g_strdup_printf (_("1 message downloaded."));
  gdk_threads_enter ();
  gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), messages, 0, messages);
  gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress),
      				buf);
  window_checking_report (C2_CHECK_OK, account->acc_name, buf);
  gdk_threads_leave ();
  
bye_bye_server:
  if (sock_printf (sock, "QUIT\r\n") < 0) {
    buf = g_strerror (errno);
  }
  
  buf = sock_read (sock, &timedout);
  if (pop_check_answer (buf, account, timedout) < 0) {
    if (timedout) goto run_for_your_life;
  }
run_for_your_life:
  close (sock);
  return NULL;
}

char *
decode_8bit (const char *text) {
  const char *ptr;
  char *buf;
  GString *str;
  int words = 0, i;
  
  g_return_val_if_fail (text, NULL);

  str = g_string_new (NULL);
  
  /* Count the words */
  for (ptr = text; *ptr != '\0'; ptr++) {
    if (*ptr == ' ' && *(ptr+1) != ' ') words++;
  }

  for (i = 0; i <= words; i++) {
    if ((buf = str_get_word (i, text, ' ')) == NULL) break;
    if (i) g_string_append_c (str, ' ');

    /* Check if the word is wrong */
    if (strneq (buf, "=?iso-8859-1?Q?", 15)) {
      for (ptr = buf+15; *ptr != '\0'; ptr++) {
	if (strneq (ptr, "?=", 2)) break;
	if (*ptr == '=') {
	  int num;
	  ptr += sscanf (ptr, "=%X", &num)+1;
	  g_string_append_c (str, (char)num);
	} else g_string_append_c (str, *ptr);
      }
      c2_free (buf);
    } else {
      g_string_append (str, buf);
      c2_free (buf);
    }
  }

  if (*(str->str+strlen (str->str)-1) != '\n') g_string_append_c (str, '\n');
  buf = str->str;
  g_string_free (str, FALSE);
  return buf;
}

static int
pop_check_answer (const char *buf, const Account *account, int timedout) {
  g_return_val_if_fail (account, -1);
  g_return_val_if_fail (buf, -1);

  if (timedout) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name,
			_("Operation timeout."));
    gdk_threads_leave ();
    return -1;
  }
  if (strneq (buf, "+OK", 3))
    return 0;
  else if (strneq (buf, "-ERR", 4)) {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf+5);
    gdk_threads_leave ();
    return -1;
  } else {
    gdk_threads_enter ();
    window_checking_report (C2_CHECK_ERR, account->acc_name, buf);
    gdk_threads_leave ();
    return -1;
  }

  return -1;
}

gboolean uidl_check (const char *uidl, const char *account) {
  char *db_uidl;
  char *path;
  FILE *fd;

  g_return_val_if_fail (uidl, FALSE);
  g_return_val_if_fail (account, FALSE);

  path = g_strconcat (getenv ("HOME"), ROOT, "/", account, ".uidl", NULL);
  if ((fd = fopen (path, "r")) == NULL) {
    if (errno == ENOENT) {
      if ((fd = fopen (path, "w")) == NULL) {
	cronos_error (errno, _("Creating the UIDL database"), ERROR_WARNING);
	return FALSE;
      }
      fclose (fd);
      fd = fopen (path, "r");
    } else {
      cronos_error (errno, _("Opening the UIDL database"), ERROR_WARNING);
      return FALSE;
    }
  }
  c2_free (path);

  for (;;) {
    if ((db_uidl = fd_get_line (fd)) == NULL) {
      fclose (fd);
      return FALSE;
    }

    if (streq (db_uidl, uidl)) {
      fclose (fd);
      return TRUE;
    }
  }

  return FALSE;
}

void uidl_register (const char *uidl, const char *account) {
  char *path;
  FILE *fd;

  g_return_if_fail (uidl);
  g_return_if_fail (account);
  
  path = g_strconcat (getenv ("HOME"), ROOT, "/", account, ".uidl", NULL);
  if ((fd = fopen (path, "a")) == NULL) {
    cronos_error (errno, _("Opening the UIDL database"), ERROR_WARNING);
    return;
  }
  c2_free (path);

  fprintf (fd, "%s\n", uidl);
  fclose (fd);
}

GtkWidget *window;
gboolean gbool = TRUE;
gboolean go = FALSE;

static void cb (GtkWidget *widget, gpointer data) {
  if (*((char *) data) == 'Y') gbool = TRUE;
  else gbool = FALSE;
  gtk_widget_destroy (window);
  go = TRUE;
}

static gboolean gui_message_big_new (const char *from, const char *subject, const char *date, const  char *account, const  char *kbytes) {
  GtkWidget *vbox, *hbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *hsep;
  GtkStyle *style1;
  GtkStyle *style2;
  char *buf;
  GtkWidget *xpm;

  go = FALSE;
  gbool = TRUE;

  window = gnome_dialog_new (_("Confirm message downloading"),
      			GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO, NULL);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));
  gnome_dialog_set_default (GNOME_DIALOG (window), 0);

  vbox = GNOME_DIALOG (window)->vbox;

  label = gtk_label_new (_("There's a message bigger than what you allowed to automatically download."));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  table = gtk_table_new (5, 2, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);

  if (from && subject && date) {
    label = gtk_label_new (_("From:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
	(GtkAttachOptions) (GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
    gtk_widget_set_style (label, style2);
    label = gtk_label_new (from);
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1,
	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
    gtk_widget_set_style (label, style2);
    
    label = gtk_label_new (_("Date:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
	(GtkAttachOptions) (GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
    gtk_widget_set_style (label, style2);
    label = gtk_label_new (date);
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2,
	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
    gtk_widget_set_style (label, style2);
  } else {
    label = gtk_label_new (_("Since the server doesn't support advanced POP commands very little information about this message could be extracted."));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
	(GtkAttachOptions) (GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
  }
  
  label = gtk_label_new (_("Account:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
      (GtkAttachOptions) (GTK_FILL),
      (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  style1 = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style1);
  style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
  gtk_widget_set_style (label, style2);

  if (from && subject && date) {
    label = gtk_label_new (_("Subject:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
	(GtkAttachOptions) (GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
    gtk_widget_set_style (label, style2);
    label = gtk_label_new (subject);
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 4, 5,
	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
	(GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
    style1 = gtk_widget_get_style (label);
    style2 = gtk_style_copy (style1);
    style2->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
    gtk_widget_set_style (label, style2);
  }
  
  label = gtk_label_new (_("Size:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  style1 = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style1);
  style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
  gtk_widget_set_style (label, style2);

  buf = g_strdup_printf ("%s (%d Kb)", kbytes, atoi (kbytes)/1024);
  label = gtk_label_new (buf);
  c2_free (buf);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  style1 = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style1);
  style2->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
  gtk_widget_set_style (label, style2); 

  label= gtk_label_new (account);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  style1 = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style1);
  style2->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
  gtk_widget_set_style (label, style2); 

  hsep = gtk_hseparator_new ();
  gtk_widget_show (hsep);
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  
  buf = gnome_unconditional_pixmap_file("gnome-question.png");
  if (buf) {
    xpm = gnome_pixmap_new_from_file(buf);
    c2_free(buf);
    gtk_widget_show (xpm);
    gtk_box_pack_start (GTK_BOX (hbox), xpm, FALSE, FALSE, 0);
  }
  
  label = gtk_label_new (_("Do you want to download it?"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  gnome_dialog_button_connect (GNOME_DIALOG (window), 0, GTK_SIGNAL_FUNC (cb), (gpointer) "Y");
  gnome_dialog_set_accelerator (GNOME_DIALOG (window), 0, GDK_Y, 0);
  gnome_dialog_button_connect (GNOME_DIALOG (window), 1, GTK_SIGNAL_FUNC (cb), (gpointer) "N");
  gnome_dialog_set_accelerator (GNOME_DIALOG (window), 1, GDK_N, 0);

  gtk_widget_show (window);

  gdk_threads_leave ();
  while (!go) usleep (500);
  gdk_threads_enter ();
  return gbool;
}

GtkWidget *password_window;
GtkWidget *password_entry;
gboolean password_go = FALSE;
gboolean password_gbool = TRUE;

static void password_cb (GtkWidget *widget, gpointer data) {
  if (*((char *) data) == 'Y') password_gbool = TRUE;
  else password_gbool = FALSE;
  gtk_widget_hide (password_window);
  password_go = TRUE;
}

static gboolean
gui_ask_password (Account *account) {
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *vbox;
  char *buf;
 
  password_go = FALSE;
  password_gbool = TRUE;
  password_window = gnome_dialog_new (_("Incorrect Password"),
      			GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  vbox = GNOME_DIALOG (password_window)->vbox;
  buf = g_strdup_printf (_("The password of the account %s is wrong"), account->acc_name);
  label = gtk_label_new (buf);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  c2_free (buf);
  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  label = gtk_label_new (_("Username:"));
  gtk_table_attach_defaults (GTK_TABLE (table), label,
      			0, 1, 0, 1);
  gtk_widget_show (label);
  label = gtk_label_new (_("Password:"));
  gtk_table_attach_defaults (GTK_TABLE (table), label,
      			0, 1, 1, 2);
  gtk_widget_show (label);
  label = gtk_label_new (account->protocol.pop.usr_name);
  gtk_table_attach_defaults (GTK_TABLE (table), label,
      			1, 2, 0, 1);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  password_entry = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (password_entry), FALSE);
  gtk_table_attach_defaults (GTK_TABLE (table), password_entry,
      			1, 2, 1, 2);
  gtk_widget_show (password_entry);

  gtk_widget_show (password_window);

  gnome_dialog_button_connect (GNOME_DIALOG (password_window), 0, GTK_SIGNAL_FUNC (password_cb), (gpointer) "Y");
  gnome_dialog_button_connect (GNOME_DIALOG (password_window), 1, GTK_SIGNAL_FUNC (password_cb), (gpointer) "N");
  
  gdk_threads_leave ();
  while (!password_go) usleep (500);
  if (!password_gbool) return FALSE;
  buf = gtk_entry_get_text (GTK_ENTRY (password_entry));
  gdk_threads_enter ();
  if (!buf) return FALSE;
  c2_free (account->protocol.pop.pass);
  account->protocol.pop.pass = g_strdup (buf);
  gtk_widget_destroy (password_window);
  return TRUE;
}
