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

#  include "gui-composer.h"
#  include "gui-utils.h"
#  include "gui-decision_dialog.h"
#  include "gui-select_file.h"
#  include "gui-window_main.h"

#  include "main.h"
#  include "message.h"
#  include "rc.h"
#  include "account.h"
#  include "init.h"
#  include "mailbox.h"
#  include "utils.h"
#  include "error.h"
#  include "search.h"
#  include "index.h"
#  include "smtp.h"
#  if USE_PLUGINS
#    include "plugin.h"
#  endif
#  ifdef BUILD_ADDRESS_BOOK
#    include "addrbook.h"
#    include "autocompletition.h"
#  endif
#else
#  include <cronosII.h>
#endif
#include <gnome.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

/* This is for the generation of messages */
typedef enum {
  REQUIRE_NOTHING	= 0,
  REQUIRE_DRAFTS	= 1 << 1
} Requires;

/* Definitions */
static Message *
make_message						(WidgetsComposer *widget, Requires require);

static void
on_icon_list_drag_data_received				(GtkWidget *widget, GdkDragContext *context, int x,
    							 gint y, GtkSelectionData *data, guint info,
							 guint time);
static void
on_send_clicked						(GtkWidget *w, WidgetsComposer *widget);

static void
on_save_clicked						(GtkWidget *w, WidgetsComposer *widget);

static void
on_save_as_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_attachs_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_icon_list_select_icon				(GtkWidget *w, int num, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_icon_list_unselect_icon				(GtkWidget *w, int num, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_attachs_add_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_attachs_remove_clicked				(GtkWidget *w, WidgetsComposer *widget);

static void
on_close_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_delete_event						(GtkWidget *w, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_insert_file_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_select_all_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_clear_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_body_changed						(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_account_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_to_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_cc_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_bcc_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_subject_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_priority_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_insert_file_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_insert_signature_clicked				(GtkWidget *w, WidgetsComposer *widget);

/**
 * make_message
 * @widget: A pointer to a WidgetComposer object.
 * @requires: Flags of what this message needs.
 *
 *
 **/
static Message *
make_message (WidgetsComposer *widget, Requires require) {
  Message *message;
  char *buf;
  Account *account;
  char	*From = NULL,
  	*To = NULL,
  	*CC = NULL,
	*BCC = NULL,
	*In_Reply_To = NULL,
	*References = NULL,
	*Subject = NULL;
  char *boundary = NULL;
  char *body;
  char *filename;
  int i, account_n;
  int attachments = 0;
  GSList *parts = NULL;
  GSList *s;
  char *part;
  char *tmpfile;
  FILE *fd;
 
  /* Open the tmp file */
  tmpfile = cronos_tmpfile ();
  if ((fd = fopen (tmpfile, "w")) == NULL) {
    return NULL;
  }
  
  /* From */
  buf = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry));
  account_n = atoi (buf);
  if (!account_n) account_n = 1;
  
  for (i = 1, account = config->account_head; i < account_n && account; i++, account = account->next);
  if (!account) return NULL;
  From = g_strdup_printf ("%s <%s>", account->per_name, account->mail_addr);

  /* To */
  To = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_TO][1]));

  /* CC */
  CC = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_CC][1]));

  /* BCC */
  BCC = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_BCC][1]));

  /* Subject */
  Subject = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_SUBJECT][1]));

  if (widget->type == C2_COMPOSER_REPLY || widget->type == C2_COMPOSER_REPLY_ALL) {
    /* In-Reply-To */
    In_Reply_To = message_get_header_field (widget->message, NULL, "\nMessage-ID:");
    
    /* References */
    buf = message_get_header_field (widget->message, NULL, "\nReferences:");
    if (In_Reply_To && buf)
      References = g_strdup_printf ("%s %s", In_Reply_To, buf);
  else if (In_Reply_To)
    References = g_strdup_printf ("%s", In_Reply_To);
    c2_free (buf);
  }

  /* Attachments */
  attachments = GNOME_ICON_LIST (widget->icon_list)->icons;
  
  /* Body */
  body = gtk_editable_get_chars (GTK_EDITABLE (widget->body), 0, -1);

  if (attachments) {
    boundary = make_message_boundary ();
   
    for (i = 0; i <= attachments; i++) {
      /* Get the filename */
      if (i) filename = gnome_icon_list_get_icon_data (GNOME_ICON_LIST (widget->icon_list), i-1);
      else filename = NULL;

      part = make_message_part (filename, body, boundary);
      if (part) parts = g_slist_append (parts, part);
      if (body) c2_free (body);
    }
  }
  
  fprintf (fd, "From: %s\n"
      	       "To: %s\n"
		"Subject: %s\n"
		"X-Mailer: " XMAILER "\n"
		"X-CronosII-Account: %s\n"
		"MIME-Version: 1.0\n"
		"Content-Type: %s%s%s"
		"%s",
		From, To, Subject, account->acc_name,
		attachments ? "multipart/mixed; boundary=\"" : "text/plain",
		attachments ? boundary : "",
		attachments ? "\"\n": "\n",
		attachments ? "" : "Content-Transfer-Encoding: 8bit\n");
  
  if (strlen (CC)) {
    fprintf (fd, "CC: %s\n", CC);
  }

  if (strlen (BCC)) {
    fprintf (fd, "BCC: %s\n", BCC);
  }

  if (In_Reply_To) {
    fprintf (fd, "In-Reply-To: %s\n", In_Reply_To);
    c2_free (In_Reply_To);
  }

  if (References) {
    fprintf (fd, "References: %s\n", References);
    c2_free (References);
  }

  /* Fields required by Drafts */
  if (require & REQUIRE_DRAFTS) {
    if (widget->message) {
      fprintf (fd, "X-CronosII-Draft-Original-Mailbox: %s\n", widget->message->mbox);
      fprintf (fd, "X-CronosII-Draft-Original-MID: %d\n", widget->message->mid);
    }
    
    fprintf (fd, "X-CronosII-Draft-Action: %d\n", widget->type);
  }

  fprintf (fd, "\n");

  if (attachments) {
    fprintf (fd, _("This is a multipart message in MIME format.\n"
	  		    "The fact that you reached this text means that your\n"
			    "mail client does not understand MIME messages and you will\n"
			    "not be understand the attachments. You should consider moving\n"
			    "to another mail client or to a higher version.\n\n"));
    for (s = parts; s; s = s->next) {
      fprintf (fd, CHAR (s->data));
      c2_free (s->data);
    }
    fprintf (fd, "--%s--\n", boundary);
    g_slist_free (parts);
  } else fprintf (fd, "%s", body);
  c2_free (From);
  c2_free (In_Reply_To);
  c2_free (References);
  c2_free (boundary);

  fclose (fd);
  
  message = message_get_message_from_file (tmpfile);
  if (widget->message) {
    message->mbox = widget->message->mbox;
    message->mid = widget->message->mid;
  }
  else {
    message->mbox = NULL;
    message->mid = 0;
  }

  return message;
}

/**
 * make_message_part
 * @filename: A whole path to the file that is going to be attached.
 * @body: The body of the message.
 * @boundary: Boundary to use.
 *
 * Returns a whole part for appending to the message.
 * Note: filename can be NULL if body isn't NULL and viceversa.
 *
 * Return Value:
 * The generated part.
 **/
static char *
make_message_part (const char *filename, const char *body, const char *boundary) {
  char *part = NULL;

  g_return_val_if_fail (boundary, NULL);
  g_return_val_if_fail (!(!filename && !body), NULL);

  if (!filename) {
    part = g_strdup_printf ("--%s\n"
			    "Content-Type: text/plain\n" /* TODO Charset */
			    "Content-Transfer-Encoding: 8bit\n"
			    "Content-Disposition: inline\n"
			    "\n"
			    "%s\n",
			    boundary, body);
  } else {
    const char *content_type;
    char *content_disposition;
    char *buffer;
    char *enc;
    struct stat st;
    guint size;
    FILE *fd;

    /* File size */
    if (stat (filename, &st) < 0) return NULL;
    size = st.st_size;
    if (!size) return NULL;
    
    /* Content-Type */
    content_type = gnome_mime_type_or_default (filename, "application/octet-stream");
    if (streq (content_type, "application/octet-stream")) {
      content_type = g_strdup_printf ("application/octet-stream; name=\"%s\"", g_basename (filename));
    }

    /* Disposition */
    content_disposition = g_strdup_printf ("attachment; filename=\"%s\"", g_basename (filename));
    
    if ((fd = fopen (filename, "rt")) == NULL) return NULL;
    
    buffer = g_malloc0 (sizeof (char) * (size+1));
    fread (buffer, sizeof (char), size, fd);
    fclose (fd);
    chdir (getenv ("HOME"));
    buffer[size] = '\0';

    /* Encode */
    enc = encode_base64 (buffer, &size);
    c2_free (buffer);

    /* Part */
    part = g_strdup_printf ("--%s\n"
			    "Content-Type: %s\n"
			    "Content-Transfer-Encoding: base64\n"
			    "Content-Disposition: %s\n"
			    "\n"
			    "%s",
			    boundary, content_type, content_disposition, enc);
    c2_free (content_disposition);
    c2_free (enc);

  }

  return part;
}

/**
 * make_message_boundary
 *
 * This functions generates a boundary
 * for multipart messages.
 * 
 * Return Value:
 * The boundary.
 **/
static char *
make_message_boundary (void) {
  char *boundary = NULL;
  char *ptr;
  int i;

  srand (time (NULL));
  boundary = g_new0 (char, 50);
  sprintf (boundary, "Cronos-II=");
  ptr = boundary+10;
  for (i = 0; i < 39; i++) {
    *(ptr+i) = (rand () % 26)+97; /* From a to z */
  }
  if (*(ptr+i-1) == '-') *(ptr+i-1) = '.';
  *(ptr+i) = '\0';
  
  return boundary;
}
