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
#  define SPOOL_MAIL_DIR "/var"
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif

#include <errno.h>
#include "main.h"
#include "gui-import.h"
#include "gui-window_main.h"
#include "gui-mailbox_tree.h"
#include "gui-select_file.h"
#include "gui-utils.h"
#include "debug.h"
#include "search.h"
#include "spool.h"
#include "error.h"
#include "mailbox.h"
#include "message.h"
#include "account.h"
#include "utils.h"

static void on_tree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, ImportWidgets *iw);
static void on_btn_clicked (GtkButton *btn, ImportWidgets *iw);
static void on_cancel_clicked (GtkWidget *widget, ImportWidgets *iw);

/* ---> Edit here for adding supported mailboxes <--- */
/* ---> Here just write the main function, that should be called by the OK button <--- */
int import_spool_main (const char *path, const Mailbox *mbox);

static void on_ok_clicked (GtkWidget *widget, ImportWidgets *iw) {
  char *mailbox;
  char *file;
  char *type;
  int messages;
  Mailbox *mbox;

  type = g_strdup (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (iw->type)->entry)));
  file = g_strdup (gtk_entry_get_text (GTK_ENTRY (iw->file)));
  mailbox = iw->mbox;

  if ((mbox = search_mailbox_name (config->mailbox_head, mailbox)) == NULL) {
    g_warning (_("The mailbox %s could not be found."), mailbox);
    goto out;
  }

  gtk_widget_destroy (iw->window);

  /* ---> Edit here for adding supported mailboxes <--- */
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  if (!strcmp (type, _("Spool"))) {
    messages = import_spool_main (file, mbox);
  } else {
    g_warning (_("The mailbox type %s is not supported."), type);
    goto out;
  }
  gdk_window_set_cursor (WMain->window->window, cursor_normal);

  if (messages < 0)
    cronos_status_error (_("Unable to import mailbox."));
  else if(messages == 1)
    g_message (_("One message imported succesfully"));
  else
    g_message (_("%d messages imported succesfully"), messages);

out:
  gdk_window_set_cursor (WMain->window->window, cursor_normal);
  c2_free (file);
  c2_free (type);
  c2_free (iw);
}

void gui_import_new (void) {
  ImportWidgets *iw;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *btn;
  GtkWidget *scroll;
  GList *list = NULL;

  iw = (ImportWidgets *) g_malloc0 (sizeof (ImportWidgets));

  /* ---> Edit here for adding supported mailbox format <--- */
  list = g_list_append (list, _("Spool"));
  
  iw->window = gnome_dialog_new (_("Import Mailbox"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  gtk_widget_realize (iw->window);
  gtk_window_set_modal (GTK_WINDOW (iw->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (iw->window), GTK_WINDOW (WMain->window));
  gnome_dialog_set_default (GNOME_DIALOG (iw->window), 0);
  gnome_dialog_set_parent (GNOME_DIALOG (iw->window), GTK_WINDOW (WMain->window));
  gnome_dialog_button_connect (GNOME_DIALOG (iw->window), 0, on_ok_clicked, (gpointer) iw);
  gnome_dialog_button_connect (GNOME_DIALOG (iw->window), 1, on_cancel_clicked, (gpointer) iw);
  
  frame = gtk_frame_new (_("Mailbox type"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (iw->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  iw->type = gtk_combo_new ();
  gtk_container_add (GTK_CONTAINER (frame), iw->type);
  gtk_widget_show (iw->type);
  gtk_combo_set_popdown_strings (GTK_COMBO (iw->type), list);
  gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (iw->type)->entry), FALSE);

  frame = gtk_frame_new (_("Destination Mailbox"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (iw->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  iw->mailbox = gtk_ctree_new (1, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), iw->mailbox);
  gtk_widget_show (iw->mailbox);
  create_mailbox_tree (iw->window, iw->mailbox, NULL, config->mailbox_head);
  gtk_signal_connect (GTK_OBJECT (iw->mailbox), "select_row",
      			GTK_SIGNAL_FUNC (on_tree_select_row), (gpointer) iw);
  {
    Mailbox *inbox=NULL;
    GtkCTreeNode *cnode=NULL;
    GtkCTreeNode *cnode_sele=NULL;
    int i;
   
    inbox = search_mailbox_name (config->mailbox_head, MAILBOX_INBOX);
    if (!inbox) goto out;
    
    for (i=1, cnode = gtk_ctree_node_nth (GTK_CTREE (iw->mailbox), 0); cnode; i++) {
      if (i>1) cnode = gtk_ctree_node_nth (GTK_CTREE (iw->mailbox), i);
      cnode_sele = gtk_ctree_find_by_row_data (GTK_CTREE (iw->mailbox), cnode, inbox);
      
      if (cnode_sele) {
	gtk_ctree_select (GTK_CTREE (iw->mailbox), cnode_sele);
	iw->mbox = inbox->name;
	break;
      }
    }
out:
  }
	
  frame = gtk_frame_new (_("File"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (iw->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  iw->file = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), iw->file, TRUE, TRUE, 0);
  gtk_widget_show (iw->file);
  gtk_entry_set_editable (GTK_ENTRY (iw->file), FALSE);

  btn = gtk_button_new_with_label ("..");
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
  			GTK_SIGNAL_FUNC (on_btn_clicked), (gpointer) iw);

  gtk_widget_show (iw->window);
}

/* TODO Detect which is the account that download the importing mail using the To
 * TODO field. Get the mail addr from that field and search in the Linked List for that address.
 * TODO If it's not found, search for the same name (if its in format that gives the name ("name" <addr>
 * TODO or something...)).
 * TODO If it can't find that either, we can ask which account should be asigned (for all mails, not mail
 * TODO per mail) or we can leave the field blank (mmmm, not recomended).
 */
int import_spool_main (const char *path, const Mailbox *mbox) {
  int messages;
  int i;
  Message *message;
  FILE *fd;
  gboolean i_rull = TRUE;
  char *buf;
  FILE *index;
  FILE *mail;
  char *subject;
  char *from;
  char *date;
  char *account;
  char *mail_addr;
  Account *acc;
  mid_t mid;
  
  g_return_val_if_fail (path, -1);
  g_return_val_if_fail (mbox, -1);

  if ((fd = fopen (path, "r")) == NULL) {
    cronos_error (errno, _("Couldn't open the specified file for importing messages"),
    			ERROR_WARNING);
    return -1;
  }

  messages = spool_get_messages_in_mailbox (fd);
  fseek (fd, 0, SEEK_SET);

  if (!status_is_busy) {
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, (float) messages);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Importing..."));
    status_is_busy = TRUE;
  } else
    i_rull = FALSE;

  /* Open the mailbox index file */
  buf = c2_mailbox_index_path (mbox->name);
  if ((index = fopen (buf, "a")) == NULL) {
    cronos_error (errno, _("Couldn't open the main DB file for appending the imported message"),
				ERROR_WARNING);
    c2_free (buf);
    return -1;
  }

  for (i = 0; i < messages; i++) {
    message = spool_get_message_nth (fd, i);
    if (!message) continue;

    if (i_rull) {
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), i+1);
      while (gtk_events_pending ())
	gtk_main_iteration_do (TRUE);
    }
    
    /* Open the mail file */
    mid = c2_mailbox_get_next_mid ((Mailbox *) mbox);
    buf = c2_mailbox_mail_path (mbox->name, mid);
    if ((mail = fopen (buf, "w")) == NULL) {
      cronos_error (errno, _("Couldn't open the mail file for writing the imported message"),
				ERROR_WARNING);
      c2_free (buf);
      return -1;
    }
    fprintf (mail, "%s", message->message);
    fclose (mail);
    subject = message_get_header_field (message, NULL, "\nSubject:");
    from = message_get_header_field (message, NULL, "\nFrom:");
    date = message_get_header_field (message, NULL, "\nDate:");
    account = message_get_header_field (message, NULL, "\nTo:");
    mail_addr = str_get_mail_address ((const char *) account);
    c2_free (account);
    if (!mail_addr) {
      /* Use the default account */
      acc = config->account_head;
    } else {
      /* Found the mail address, check if there's this mail address in the Linked List */
      acc = search_account_mail_addr (config->account_head, mail_addr);
      if (!acc) {
	/* Use the default account */
	acc = config->account_head;
      }
    }
    c2_free (mail_addr);
    c2_free (message->message);
    c2_free (message->header);
    c2_free (message);

    fprintf (index, "N\r\r\r%s\r%s\r%s\r%s\r%d\n",
			subject, from, date, acc ? acc->acc_name : "", mid);
    fflush (index);
  }

  if (i_rull) {
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    status_is_busy = FALSE;
  }

  fclose (fd);
  fclose (index);

  return messages;
}

static void on_btn_clicked (GtkButton *btn, ImportWidgets *iw) {
  char *file;
  
  file = gui_select_file_new (SPOOL_MAIL_DIR, NULL, NULL, NULL);
  
  if (file) {
    gtk_entry_set_text (GTK_ENTRY (iw->file), file);
  }
}

static void on_cancel_clicked (GtkWidget *widget, ImportWidgets *iw) {
  gtk_widget_destroy (iw->window);
  c2_free (iw);
}

static void on_tree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, ImportWidgets *iw) {
  GtkCTreeNode *node;

  node = gtk_ctree_node_nth(ctree, row);
  iw->mbox = (char *) ((Mailbox *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node))->name;
}
