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
#else
#  define XMAILER "Cronos II" 
#endif
#if USE_GNOME
#include <gnome.h>
#else
#include <gtk/gtk.h>
#endif
#include <errno.h>
#include <time.h>

#include "main.h"
#include "message.h"
#include "net.h"
#include "search.h"
#include "utils.h"
#include "init.h"
#include "debug.h"
#include "error.h"
#include "index.h"
#include "smtp.h"

#include "gui-composer.h"
#include "gui-utils.h"
#include "gui-window_main.h"

static char *
make_8bit								(const guchar *text);

static GList *
get_mail_addresses							(const char *string);

static gboolean sending_message = FALSE;

void *
smtp_main (void *data) {
  Pthread2 *helper = NULL;
  Message *message = NULL;
  WidgetsComposer *widget = NULL;
  char *buf = NULL;
  char *ptr, *ptr2;
  char *to = NULL, *cc = NULL, *bcc = NULL, *all_to = NULL;
  int timedout = FALSE;
  Account *account = NULL;
  int length = 0, sent_length = 0;
  gboolean success = FALSE;
  gboolean sending_header;
  GList *list = NULL, *s;
  int sock = -1;
  GtkWidget *appbar;
 
  helper = PTHREAD2 (data);
  g_return_val_if_fail (helper, NULL);
  g_return_val_if_fail (helper->v1, NULL);

  message = helper->v1;
  widget = helper->v2;
  appbar = widget ? widget->appbar : WMain->appbar;
  sending_message = TRUE;

  if (config->use_persistent_smtp_connection) sock = persistent_sock;

  buf = message_get_header_field (message, NULL, "\nX-CronosII-Account:");
  if (!buf) account = config->account_head;
  else {
    account = search_account_acc_name (config->account_head, buf);
    if (!account) account = config->account_head;
    c2_free (buf);
  }
  buf = NULL;

  if (sock < 0) {
    sock = smtp_do_connect (account->smtp, account->smtp_port, appbar);
  }
  
  if (config->use_persistent_smtp_connection) persistent_sock = sock;

  if (sock < 0) return NULL;

  if (sock_printf (sock, "MAIL FROM: <%s>\r\n", account->mail_addr) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
    /* <Check for widget> */
    if (widget) {
      gtk_widget_show (widget->window);
      gdk_window_raise (widget->window->window);
      widget->sending = FALSE;
    }
    /* </Check for widget> */
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = NULL;
  do {
    c2_free (buf);
    buf = sock_read (sock, &timedout);
    if (timedout) {
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar),
	  _("Timeout while waiting for a response of the server"));
      gdk_threads_leave ();
      goto just_leave;
    }
    if (strnne (buf, "250", 3)) {
      if (strneq (buf, "552", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because the storage "
	      "location has been exceed."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "451", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("Error in the delivery of the message because it has ocurred an error "
	      "in the SMTP server."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "452", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because there's not "
	      "enough system storage."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "500", 3) || strneq (buf, "501", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server isn't RFC821 complaiment, please send me an E-Mail "
	      "with the hostname address."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "421", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server is not available."));
	gdk_threads_leave ();
      }
      else {
	gdk_threads_enter ();
	str_strip (buf, '\n');
	gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	gdk_threads_leave ();
      }
      /* <Check for widget> */
      if (widget) {
	gdk_threads_enter ();
	gtk_widget_show (widget->window);
	gdk_window_raise (widget->window->window);
	widget->sending = FALSE;
	gdk_threads_leave ();
      }
      /* </Check for widget> */
      goto bye_bye_server;
    }
  } while (*(buf+3) == '-');
  c2_free (buf); 

  to = message_get_header_field (message, NULL, "To:");
  cc = message_get_header_field (message, NULL, "CC:");
  bcc = message_get_header_field (message, NULL, "BCC:");

  if (cc && !bcc) all_to = g_strdup_printf ("%s; %s", to, cc);
  else if (!cc && bcc) all_to = g_strdup_printf ("%s; %s", to, bcc);
  else if (cc && bcc) all_to = g_strdup_printf ("%s; %s; %s", to, cc, bcc);
  else all_to = g_strdup (to);

  c2_free (to);
  c2_free (cc);
  c2_free (bcc);
  
  for (list = s = get_mail_addresses (all_to); s; s = s->next) {
    if (sock_printf (sock, "RCPT TO: <%s>\r\n", CHAR (s->data)) < 0) {
      buf = g_strerror (errno);
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
      /* <Check for widget> */
      if (widget) {
	gtk_widget_show (widget->window);
	gdk_window_raise (widget->window->window);
	widget->sending = FALSE;
      }
      /* </Check for widget> */
      gdk_threads_leave ();
      goto bye_bye_server;
    }
    
    buf = NULL;
    do {
      c2_free (buf);
      buf = sock_read (sock, &timedout);
      if (timedout) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("Timeout while waiting for a response of the server"));
	gdk_threads_leave ();
	goto just_leave;
      }
      if (strnne (buf, "250", 3) && strnne (buf, "251", 3)) {
	if (strneq (buf, "550", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because the mailbox "
		"is unavailable."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "551", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because the user "
		"is not local."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "552", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because the storage "
		"location has been exceed."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "553", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because the mailbox "
		"name is not allowed."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "450", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because the mailbox "
		"is unavailable."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "451", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("Error in the delivery of the message because it has ocurred an error "
		"in the SMTP server."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "452", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server aborted the delivery of the message because there's not "
		"enough system storage."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "500", 3) || strneq (buf, "501", 3) || strneq (buf, "503", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server isn't RFC821 complaiment, please send me an E-Mail "
		"with the hostname address."));
	  gdk_threads_leave ();
	}
	else if (strneq (buf, "421", 3)) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("The SMTP server is not available."));
	  gdk_threads_leave ();
	}
	else {
	  gdk_threads_enter ();
	  str_strip (buf, '\n');
	  gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	  gdk_threads_leave ();
	}
	/* <Check for widget> */
	if (widget) {
	  gdk_threads_enter ();
	  gtk_widget_show (widget->window);
	  gdk_window_raise (widget->window->window);
	  widget->sending = FALSE;
	  gdk_threads_leave ();
	}
	/* </Check for widget> */
	goto bye_bye_server;
      }
    } while (*(buf+3) == '-');
    c2_free (buf);
    c2_free (s->data);
  }
  g_list_free (list);
  c2_free (all_to);
  
  if (sock_printf (sock, "DATA\r\n") < 0) {
    c2_free (buf);
    buf = g_strerror (errno);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
    /* <Check for widget> */
    if (widget) {
      gtk_widget_show (widget->window);
      gdk_window_raise (widget->window->window);
      widget->sending = FALSE;
    }
    /* </Check for widget> */
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = NULL;
  do {
    c2_free (buf);
    buf = sock_read (sock, &timedout);
    if (timedout) {
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar),
	  _("Timeout while waiting for a response of the server"));
      gdk_threads_leave ();
      goto just_leave;
    }
    if (strnne (buf, "354", 3)) {
      if (strneq (buf, "451", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("Error in the delivery of the message because it has ocurred an error "
	      "in the SMTP server."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "552", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because the storage "
	      "location has been exceed."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "452", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because there's not "
	      "enough system storage."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "500", 3) || strneq (buf, "501", 3) || strneq (buf, "503", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server isn't RFC821 complaiment, please send me an E-Mail "
	      "with the hostname address."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "421", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server is not available."));
	gdk_threads_leave ();
      }
      else {
	gdk_threads_enter ();
	str_strip (buf, '\n');
	gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	gdk_threads_leave ();
      }
      /* <Check for widget> */
      if (widget) {
	gdk_threads_enter ();
	gtk_widget_show (widget->window);
	gdk_window_raise (widget->window->window);
	widget->sending = FALSE;
	gdk_threads_leave ();
      }
      /* </Check for widget> */
      goto bye_bye_server;
    }
  } while (*(buf+3) == '-');
  c2_free (buf);

  length = strlen (message->message);
  gdk_threads_enter ();
  /* <Check for widget> */
  if (widget)
    gtk_widget_show (widget->window);
  /* </Check for widget> */
  gtk_progress_configure (GTK_PROGRESS (GNOME_APPBAR (appbar)->progress), 0, 0, length);
  gdk_threads_leave ();
  sending_header = TRUE;

  for (ptr = message->message, sent_length = 0;;) {
    if ((ptr2 = str_get_line (ptr)) == NULL) break;
    ptr += strlen (ptr2);
    /* Delete the last char (\n) */
    if (ptr2[strlen (ptr2)-1] == '\n')
      ptr2[strlen (ptr2)-1] = '\0';

    /* Check if this line is a header of c2 */
    if (sending_header) {
      char *ptr3;
      if (strneq (ptr2, "X-CronosII", 10)) goto skip_sending;
      if (!strlen (ptr2)) sending_header = FALSE;
      /* Some files shouldn't be 8bited */
      if (strnne (ptr2, "Content-Type:", 13)) {
	ptr3 = make_8bit (ptr2);
	c2_free (ptr2);
	ptr2 = ptr3;
      }
    }
    sock_printf (sock, "%s\r\n", ptr2);
skip_sending:
    sent_length += strlen (ptr2)+1;
    c2_free (ptr2);
    gdk_threads_enter ();
    gtk_progress_set_value (GTK_PROGRESS (GNOME_APPBAR (appbar)->progress),
	  	(gfloat) sent_length);
    gdk_threads_leave ();
  }

  if (send (sock, ".\r\n", 3, 0) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
    gtk_widget_show (widget->window);
    gdk_window_raise (widget->window->window);
    widget->sending = FALSE;
    gdk_threads_leave ();
    goto bye_bye_server;
  }

  buf = NULL;
  do {
    c2_free (buf);
    buf = sock_read (sock, &timedout);
    if (timedout) {
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar),
	  _("Timeout while waiting for a response of the server"));
      gdk_threads_leave ();
      goto just_leave;
    }
    if (strnne (buf, "250", 3)) {
      if (strneq (buf, "552", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because the storage "
	      "location has been exceed."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "554", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because the transaction "
	      "failed."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "451", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("Error in the delivery of the message because it has ocurred an error "
	      "in the SMTP server."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "452", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server aborted the delivery of the message because there's not "
	      "enough system storage."));
	gdk_threads_leave ();
      }
      else {
	gdk_threads_enter ();
	str_strip (buf, '\n');
	gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	gdk_threads_leave ();
      }
      /* <Check for widget> */
      if (widget) {
	gdk_threads_enter ();
	gtk_widget_show (widget->window);
	gdk_window_raise (widget->window->window);
	widget->sending = FALSE;
	gdk_threads_leave ();
      }
      /* </Check for a widget> */
      goto bye_bye_server;
    }
  } while (*(buf+3) == '-');
  c2_free (buf);
  success = TRUE;
  /* <Check for a widget> */
  if (widget) {
    gdk_threads_enter ();
    gtk_widget_hide (widget->window);
    gdk_threads_leave ();
  }
  /* </Check for a widget> */
  
bye_bye_server:
  if (!config->use_persistent_smtp_connection) {
    if (send (sock, "QUIT\r\n", 6, 0) < 0) {
      buf = g_strerror (errno);
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
      /* <Check for a widget> */
      if (widget) {
	gtk_widget_show (widget->window);
	gdk_window_raise (widget->window->window);
	widget->sending = FALSE;
      }
      /* </Check for a widget> */
      gdk_threads_leave ();
      goto just_leave;
    }
    
    /* I don't mind what the server says now, I'm leaving anyway */
    buf = NULL;
    do {
      c2_free (buf);
      buf = sock_read (sock, &timedout);
      if (timedout) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("Timeout while waiting for a response of the server"));
	/* <Check for a widget> */
	if (widget) {
	  gdk_window_raise (widget->window->window);
	  widget->sending = FALSE;
	}
	/* </Check for a widget> */
	gdk_threads_leave ();
	goto just_leave;
      }
    } while (*(buf+3) == '-');
    c2_free (buf);
  }

just_leave:
  if (!config->use_persistent_smtp_connection) {
    close (sock);
    sock = -1;
  }
  if (success) {
    if (config->use_outbox) {
      FILE *fd;
      mid_t mid;
      Mailbox *mbox;
      char *subject, *to, date[40], *_account;
      time_t now;
      struct tm *ptr;
      
      mbox = search_mailbox_name (config->mailbox_head, MAILBOX_OUTBOX);
      if (mbox) {
	mid = c2_mailbox_get_next_mid (mbox);
	buf = c2_mailbox_mail_path (MAILBOX_OUTBOX, mid);
	if ((fd = fopen (buf, "w")) != NULL) {
	  fprintf (fd, "%s", message->message);
	  fclose (fd);
	  c2_free (buf);
	  
	  buf = c2_mailbox_index_path (MAILBOX_OUTBOX);
	  if ((fd = fopen (buf, "a")) != NULL) {
	    c2_free (buf);
	    subject = message_get_header_field (message, NULL, "\nSubject:");
	    to = message_get_header_field (message, NULL, "\nTo:");
	    _account = message_get_header_field (message, NULL, "\nX-CronosII-Account:");
	    time (&now);
	    ptr = localtime (&now);
	    strftime (date, sizeof (date), "%a, %d %b %Y %H:%M:%S %z", ptr);
	    if (!subject) subject = "";
	    fprintf (fd, "N\r\r\r%s\r%s\r%s\r%s\r%d\n",
		subject, to, date, _account, mid);
	    fclose (fd);
	    c2_free (subject);
	    c2_free (to);
	    c2_free (_account);
	  } else c2_free (buf);
	} else {
	  c2_free (buf);
	}
      }
    }
    
    /* <Check for widget> */
    if (widget) {
      gdk_threads_enter ();
      gtk_widget_destroy (widget->window);
      gdk_threads_leave ();
    }
    /* </Check for widget> */

    /* Marks a message (if corresponds) as replied || replied all || forwarded when continue
     * editing a message of the Drafts mailbox */
    /* <Check for a widget> */
    if (widget && widget->type == C2_COMPOSER_DRAFTS) {
      char *mbox;
      mid_t mid;
      char *buf;
      C2ComposerType action;
      mbox = message_get_header_field (message, NULL, "\nX-CronosII-Original-Mailbox:");
      buf = message_get_header_field (message, NULL, "\nX-CronosII-Original-MID:");
      mid = atoi (buf);
      c2_free (buf);
      buf = message_get_header_field (message, NULL, "\nX-CronosII-Action:");
      action = (C2ComposerType) atoi (buf);
      c2_free (buf);
      if (action == C2_COMPOSER_REPLY || action == C2_COMPOSER_REPLY_ALL) {
	index_mark_as (mbox, -1, mid, C2_MARK_REPLY, 0);
      }
      else if (action == C2_COMPOSER_FORWARD) {
	index_mark_as (mbox, -1, mid, C2_MARK_FORWARD, 0);
      }
      expunge_mail (MAILBOX_DRAFTS, widget->drafts_mid);
      if (streq (selected_mbox, message->mbox) || streq (selected_mbox, MAILBOX_DRAFTS)) {
	update_clist (selected_mbox, TRUE, FALSE);
      }
    }

    /* If there's no window asociated with this message we
     * should still look for some X-CronosII headers to mark
     * messages. */
    if (!widget) {
      char *mbox;
      mid_t mid;
      char *buf;
      char *buf2;
      C2ComposerType action;

      mbox = message_get_header_field (message, NULL, "\nX-CronosII-Original-Mailbox:");
      buf = message_get_header_field (message, NULL, "\nX-CronosII-Original-MID:");
      buf2 = message_get_header_field (message, NULL, "\nX-CronosII-Message-Action:");
      printf ("%s\n%s\n%s\n", mbox, buf, buf2);
      if (mbox && buf && buf2) {
L	mid = atoi (buf);
L	c2_free (buf);
L	action = (C2ComposerType) atoi (buf2);
c2_free (buf2);
printf ("%s %d\n", buf, action);
L	if (action == C2_COMPOSER_REPLY || action == C2_COMPOSER_REPLY_ALL) {
L	  index_mark_as (mbox, -1, mid, C2_MARK_REPLY, 0);
L	}
	else if (action == C2_COMPOSER_FORWARD) {
L	  index_mark_as (mbox, -1, mid, C2_MARK_FORWARD, 0);
L	}
L      }
L    }
    /* </Check for a widget> */
     
    /* Marks a message (if corresponds) as replied || replied all || forwarded */
    /* <Check for a widget> */
    if (widget) {
      if (widget->type != C2_COMPOSER_NEW && widget->type != C2_COMPOSER_DRAFTS && widget->message) {
	if (widget->type == C2_COMPOSER_REPLY || widget->type == C2_COMPOSER_REPLY_ALL) {
	  index_mark_as (widget->message->mbox, -1, widget->message->mid, C2_MARK_REPLY, 0);
	}
	else if (widget->type == C2_COMPOSER_FORWARD) {
	  index_mark_as (widget->message->mbox, -1, widget->message->mid, C2_MARK_FORWARD, 0);
	}
	if (streq (selected_mbox, widget->message->mbox)) {
	  update_clist (selected_mbox, TRUE, FALSE);
	}
      } 
      if (widget->message) {
	message_free (widget->message);
	widget->message = NULL;
      } else {
	message_free (message);
	message = NULL;
	c2_free (helper);
      }
      c2_free (widget);
    }
    /* </Check for a widget> */
  }

  sending_message = FALSE;
  return NULL;
}

static char *
make_8bit (const guchar *text) {
  GString *str;
  GString *gbuf;
  char *buf, *buf2;
  const guchar *ptr;
  int words = 0;
  gboolean word_is_ok = FALSE;
  int i;

  str = g_string_new (NULL);

  /* Count the words */
  for (ptr = text; *ptr != '\0'; ptr++) {
    if (*ptr == ' ' && *(ptr+1) != ' ') words++;
  }
  
  for (i = 0; i <= words; i++) {
    if ((buf = str_get_word (i, text, ' ')) == NULL) break;
    if (i) g_string_append_c (str, ' ');
    
    /* Check if this word is wrong */
    word_is_ok = TRUE;
    for (ptr = buf; *ptr != '\0'; ptr++) {
      if (*ptr > 127 || *ptr == '=') {
	word_is_ok = FALSE;
	break;
      }
    }
    if (word_is_ok) {
      g_string_append (str, buf);
      c2_free (buf);
    } else {
      gbuf = g_string_new ("=?iso-8859-1?Q?");
      for (ptr = buf; *ptr != '\0'; ptr++) {
	if (*ptr > 127 || *ptr == '=') {
	  g_string_append_c (gbuf, '=');
	  buf2 = g_strdup_printf ("%X", *ptr);
	  g_string_append (gbuf, buf2);
	  c2_free (buf2);
	} else {
	  g_string_append_c (gbuf, *ptr);
	}
      }
      g_string_append (gbuf, "?=");
      g_string_append (str, gbuf->str);
      g_string_free (gbuf, TRUE);
      c2_free (buf);
    }
  }

  buf = str->str;
  g_string_free (str, FALSE);
  return buf;
}

static GList *
get_mail_addresses (const char *string) {
  const char *ptr, *ptr2;
  char *buf;
  int len;
  GList *list = NULL;

  g_return_val_if_fail (string, NULL);

  for (ptr = ptr2 = string;; ptr2++) {
    if (*ptr2 == ';' || *ptr2 == ',' || *ptr2 == '\0') {
      if (*ptr2 != '\0') {
	len = ptr2-ptr;
	buf = g_new0 (char, len+1);
	strncpy (buf, ptr, len);
	buf[len] = 0;
	ptr=ptr2+1;
	for (; *ptr == ' '; ptr++) if (*ptr == '\0') goto ignore_this;
	ptr2++;
	list = g_list_append (list, str_get_mail_address (buf));
	c2_free (buf);
ignore_this:
      } else {
	len = strlen (ptr);
	buf = g_new0 (char, len+1);
	strncpy (buf, ptr, len);
	buf[len] = 0;
	list = g_list_append (list, str_get_mail_address (buf));
	c2_free (buf);
	break;
      }
    }
  }
  
  return list;
}

void
smtp_persistent_connect (Pthread3 *helper) {
  g_return_if_fail (helper);

  persistent_sock = smtp_do_connect (helper->v1, GPOINTER_TO_INT (helper->v2), helper->v3);
}

/**
 * smtp_do_connect
 * @addr: Host where to connect.
 * @port: Port where to connect.
 * @appbar: Appbar where to report activities.
 *
 * Connects to a host using the SMTP protocol
 *
 * Return Value:
 * The socket created.
 **/
int
smtp_do_connect (const char *addr, int port, GtkWidget *appbar) {
  C2ResolveNode *resolve = NULL;
  struct sockaddr_in server;
  char *buf = NULL;
  guint32 addrlen = 0;
  gboolean is_esmtp = FALSE;
  int timedout = FALSE;
  guint progress_id = 0;
  struct sockaddr_in localaddr;
  struct hostent *host = NULL;
  char *localhostname = NULL;
  int sock;

  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (appbar), _("Resolving SMTP host..."));
  progress_id = c2_progress_set_active (GTK_WIDGET (GNOME_APPBAR (appbar)->progress));
  gdk_threads_leave ();

  resolve = c2_resolve (addr, &buf);
  if (buf) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
    c2_progress_set_active_remove (progress_id);
    gdk_threads_leave ();
    return -1;
  }

  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (appbar), "");
  c2_progress_set_active_remove (progress_id);
  gdk_threads_leave ();

  sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), _("Failed to create socket"));
    gdk_threads_leave ();
    return -1;
  }

  server.sin_family	= AF_INET;
  server.sin_port	= htons (port);
  server.sin_addr.s_addr= inet_addr (resolve->ip);

  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (appbar), _("Connecting to the SMTP host..."));
  progress_id = c2_progress_set_active (GTK_WIDGET (GNOME_APPBAR (appbar)->progress));
  gdk_threads_leave ();

  if (connect (sock, (struct sockaddr *)&server, sizeof (server)) < 0) {
    buf = g_strerror (errno);
    gdk_threads_enter ();
    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
    c2_progress_set_active_remove (progress_id);
    gdk_threads_leave ();
    return -1;
  }

  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (appbar), "");
  c2_progress_set_active_remove (progress_id);
  gdk_threads_leave ();

  /* Guten Morgen, Herr Server! */
  buf = NULL;
  do {
    c2_free (buf);
    buf = sock_read (sock, &timedout);
    if (timedout) {
      gdk_threads_enter ();
      gnome_appbar_set_status (GNOME_APPBAR (appbar),
	  _("Timeout while waiting for a response of the server"));
      gdk_threads_leave ();
      goto connect_just_leave;
    }
    if (strnne (buf, "220", 3)) {
      if (strneq (buf, "221", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server has closed the transmission channel."));
	gdk_threads_leave ();
      }
      else if (strneq (buf, "421", 3)) {
	gdk_threads_enter ();
	gnome_appbar_set_status (GNOME_APPBAR (appbar),
	    _("The SMTP server is not available."));
	gdk_threads_leave ();
      }
      else {
	gdk_threads_enter ();
	str_strip (buf, '\n');
	gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	gdk_threads_leave ();
      }
      goto connect_bye_bye_server;
    }
  } while (*(buf+3) == '-');
  c2_free (buf);

  if (find_string (buf, "ESMTP")) is_esmtp = TRUE;

  addrlen = sizeof (localaddr);
  getsockname (sock, (struct sockaddr*)&localaddr, &addrlen);
  host = gethostbyaddr ((char *)&localaddr.sin_addr, sizeof (localaddr.sin_addr), AF_INET);
  if (host && host->h_name)
    localhostname = g_strdup(host->h_name);
  else {
    localhostname = g_new0(gchar, 32);
    if(gethostname(localhostname, 31) < 0)
      localhostname = "localhost";
  }

  if (localhostname) {
    if (sock_printf (sock, "%s %s\r\n", is_esmtp ? "EHLO" : "HELO", localhostname) < 0) {
			c2_free(localhostname);
      buf = g_strerror (errno);
      if (appbar) {
        gdk_threads_enter ();
        gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
        gdk_threads_leave ();
      }
      goto connect_bye_bye_server;
    }
    
    buf = NULL;
    do {
      c2_free (buf);
      buf = sock_read (sock, &timedout);
      if (timedout) {
	if (appbar) {
	  gdk_threads_enter ();
	  gnome_appbar_set_status (GNOME_APPBAR (appbar),
	      _("Timeout while waiting for a response of the server"));
	  gdk_threads_leave ();
	}
	goto connect_just_leave;
      }
      if (strnne (buf, "250", 3)) {
	if (strneq (buf, "500", 3) || strneq (buf, "501", 3)) {
	  if (appbar) {
	    gdk_threads_enter ();
	    gnome_appbar_set_status (GNOME_APPBAR (appbar),
	        _("The SMTP server isn't RFC821 complaiment, please send me an E-Mail "
		  "with the hostname address."));
	    gdk_threads_leave ();
	  }
	}
	else if (strneq (buf, "421", 3)) {
	  if (appbar) {
	    gdk_threads_enter ();
	    gnome_appbar_set_status (GNOME_APPBAR (appbar),
	        _("The SMTP server is not available."));
	    gdk_threads_leave ();
	  }
	}
	else {
	  if (appbar) {
	    gdk_threads_enter ();
	    str_strip (buf, '\n');
	    gnome_appbar_set_status (GNOME_APPBAR (appbar), buf);
	    gdk_threads_leave ();
	  }
	}
	goto connect_bye_bye_server;
      }
    } while (*(buf+3) == '-');
    c2_free (buf);
  }

  if (config->use_persistent_smtp_connection) {
    gdk_threads_enter ();
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_connect, FALSE);
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_disconnect, TRUE);
    gdk_threads_leave ();
  }
  return sock;
connect_bye_bye_server:
connect_just_leave:
  /* If we reach here, it means that the connection failed, the socket
   * will be reasigned with a value of -1 and if it isn't equal to
   * -1 it will be closed
   */
  if (sock != -1) close (sock);
  sock = -1;
  return -1;
}

/**
 * smtp_do_persistent_disconnect
 * @in_gui_thread: If the calling function is outside of
 * 		   the Gdk thread, this should be FALSE.
 *
 * Disconnects the persistent socket.
 **/
void
smtp_do_persistent_disconnect (gboolean in_gui_thread) {
  if (persistent_sock < 0) return;
  close (persistent_sock);
  persistent_sock = -1;

  if (in_gui_thread && config->use_persistent_smtp_connection) {
    if (!in_gui_thread) gdk_threads_enter ();
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_connect, TRUE);
    gtk_widget_set_sensitive (WMain->mb_w.persistent_smtp_options_disconnect, FALSE);
    if (!in_gui_thread) gdk_threads_leave ();
  }
}

/**
 * smtp_persistent_sock_is_connected
 *
 * Checks if the persistent smtp socket is connected.
 *
 * Return Value:
 * TRUE if the socket is connected or FALSE.
 **/
gboolean
smtp_persistent_sock_is_connected (void) {
  if (persistent_sock < 0) return FALSE;
  return TRUE;
}

gboolean
smtp_is_busy (void) {
  return sending_message;
}
