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

#include "gui-import.h"
#include "gui-window_main.h"
#include "gui-mailbox_tree.h"
#include "gui-select_file.h"
#include "gui-utils.h"

#include "main.h"
#include "debug.h"
#include "search.h"
#include "spool.h"
#include "error.h"
#include "mailbox.h"
#include "message.h"
#include "account.h"
#include "utils.h"
#include "gui-export.h"

static void on_tree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, ImportWidgets *ew);
static void on_btn_clicked (GtkButton *btn, ImportWidgets *ew);
static void on_cancel_clicked (GtkWidget *widget, ImportWidgets *ew);

/* ---> Edit here for adding supported mailboxes <--- */
/* ---> Here just write the main function, that should be called by the OK button <--- */
int export_spool_main (const char *path, const Mailbox *mbox);

static void on_ok_clicked (GtkWidget *widget, ImportWidgets *ew) {
  char *mailbox;
  char *file;
  char *type;
  int messages;
  Mailbox *mbox;

  type = g_strdup (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (ew->type)->entry)));
  file = g_strdup (gtk_entry_get_text (GTK_ENTRY (ew->file)));
  mailbox = ew->mbox;

  if ((mbox = search_mailbox_name (config->mailbox_head, mailbox)) == NULL) {
    g_warning (_("The mailbox %s couldn't be found."), mailbox);
    goto out;
  }

  gtk_widget_destroy (ew->window);

  /* ---> Edit here for adding supported mailboxes <--- */
  gdk_window_set_cursor (WMain->window->window, cursor_busy);
  if (!strcmp (type, _("Spool"))) {
    messages = export_spool_main (file, mbox);
  } else {
    g_warning (_("The mailbox type %s is not supported."), type);
    gdk_window_set_cursor (WMain->window->window, cursor_busy);
    goto out;
  }
  gdk_window_set_cursor (WMain->window->window, cursor_normal);

  if (messages < 0)
    cronos_status_error (_("Unable to export mailbox."));
  else if(messages == 1)
    g_message (_("One message exported succesfully"));
  else
    g_message (_("%d messages exported succesfully"), messages);

out:
  c2_free (file);
  c2_free (type);
  c2_free (ew);
}

void gui_export_new (void) {
  ImportWidgets *ew;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *btn;
  GtkWidget *scroll;
  GList *list = NULL;

  ew = (ImportWidgets *) g_malloc0 (sizeof (ImportWidgets));

  /* ---> Edit here for adding supported mailbox format <--- */
  list = g_list_append (list, _("Spool"));
  
  ew->window = gnome_dialog_new (_("Export Mailbox"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  gtk_widget_realize (ew->window);
  gtk_window_set_modal (GTK_WINDOW (ew->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (ew->window), GTK_WINDOW (WMain->window));
  gnome_dialog_set_default (GNOME_DIALOG (ew->window), 0);
  gnome_dialog_set_parent (GNOME_DIALOG (ew->window), GTK_WINDOW (WMain->window));
  gnome_dialog_button_connect (GNOME_DIALOG (ew->window), 0, on_ok_clicked, (gpointer) ew);
  gnome_dialog_button_connect (GNOME_DIALOG (ew->window), 1, on_cancel_clicked, (gpointer) ew);
  
  frame = gtk_frame_new (_("Mailbox type"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ew->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  ew->type = gtk_combo_new ();
  gtk_container_add (GTK_CONTAINER (frame), ew->type);
  gtk_widget_show (ew->type);
  gtk_combo_set_popdown_strings (GTK_COMBO (ew->type), list);
  gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (ew->type)->entry), FALSE);

  frame = gtk_frame_new (_("From Mailbox"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ew->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  ew->mailbox = gtk_ctree_new (1, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), ew->mailbox);
  gtk_widget_show (ew->mailbox);
  create_mailbox_tree (ew->window, ew->mailbox, NULL, config->mailbox_head);
  gtk_signal_connect (GTK_OBJECT (ew->mailbox), "select_row",
      			GTK_SIGNAL_FUNC (on_tree_select_row), (gpointer) ew);
  {
    Mailbox *inbox=NULL;
    GtkCTreeNode *cnode=NULL;
    GtkCTreeNode *cnode_sele=NULL;
    int i;
   
    inbox = search_mailbox_name (config->mailbox_head, MAILBOX_INBOX);
    if (inbox) {
      for (i=1, cnode = gtk_ctree_node_nth (GTK_CTREE (ew->mailbox), 0); cnode; i++) {
	if (i>1) cnode = gtk_ctree_node_nth (GTK_CTREE (ew->mailbox), i);
	cnode_sele = gtk_ctree_find_by_row_data (GTK_CTREE (ew->mailbox), cnode, inbox);
	
	if (cnode_sele) {
	  gtk_ctree_select (GTK_CTREE (ew->mailbox), cnode_sele);
	  ew->mbox = inbox->name;
	  break;
	}
      }
    }
  }
	
  frame = gtk_frame_new (_("File"));
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (ew->window)->vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  ew->file = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), ew->file, TRUE, TRUE, 0);
  gtk_widget_show (ew->file);
  gtk_entry_set_editable (GTK_ENTRY (ew->file), FALSE);

  btn = gtk_button_new_with_label ("..");
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
  			GTK_SIGNAL_FUNC (on_btn_clicked), (gpointer) ew);

  gtk_widget_show (ew->window);
}

int export_spool_main (const char *path, const Mailbox *mbox) {
  char *buf;
  char *line;
  Message *message;
  char *to;
  char *mail_addr;
  char *date;
  int messages;
  int i;
  GList *list = NULL;
  GList *dlist;
  FILE *fd;
  FILE *idx;
  gboolean i_rull = TRUE;

  g_return_val_if_fail (path || mbox, -1); 

  /* Open the output file */
  if ((fd = fopen (path, "a")) == NULL) {
    cronos_error (errno, _("Couldn't open the selected file for exporting the mailbox"), ERROR_WARNING);
    return -1;
  }

  /* Make the list of mids */
  buf = c2_mailbox_index_path (mbox->name);
  if ((idx = fopen (buf, "r")) == NULL) {
    cronos_error (errno, _("Couln't open the main DB file for exporting the mailbox"), ERROR_WARNING);
    c2_free (buf);
    return -1;
  }
  c2_free (buf);

  for (;;) {
    if ((line = fd_get_line (idx)) == NULL) break;
    buf = str_get_word (7, line, '\r');
    list = g_list_append (list, (gpointer) atoi (buf));
    c2_free (line);
  }

  fclose (idx);

  messages = g_list_length (list);

  if (!status_is_busy) {
    gtk_progress_configure (GTK_PROGRESS (WMain->progress), 0, 0, (float) messages);
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Exporting..."));
    status_is_busy = TRUE;
  } else
    i_rull = FALSE;
  
  for (i = 0, dlist = list; i < messages && dlist; i++, dlist = dlist->next) {
    message = message_get_message (mbox->name, (int) dlist->data);

    date = message_get_header_field (message, NULL, "\nDate:");
    to = message_get_header_field (message, NULL, "\nTo:");
    mail_addr = str_get_mail_address (to);

    if (i > 0) fprintf (fd, "\n");
    fprintf (fd, "From %s  %s\n%s%s", mail_addr ? mail_addr : to, date, message->message,
		(*(message->message+strlen (message->message)-1) == '\n') ? "" : "\n");
    c2_free (date);
    c2_free (to);
    c2_free (mail_addr);
    c2_free (message);

    if (i_rull) {
      gtk_progress_set_value (GTK_PROGRESS (WMain->progress), i+1);
      while (gtk_events_pending ())
	gtk_main_iteration_do (TRUE);
    }
  }

  if (i_rull) {
    gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), _("Done."));
    status_is_busy = FALSE;
  }

  fclose (fd);

  g_list_free (list);

  return i;
}

static void on_btn_clicked (GtkButton *btn, ImportWidgets *ew) {
  char *file;
  
  file = gui_select_file_new (NULL, NULL, NULL, NULL);
  
  if (file) {
    gtk_entry_set_text (GTK_ENTRY (ew->file), file);
  }
}

static void on_cancel_clicked (GtkWidget *widget, ImportWidgets *ew) {
  gtk_widget_destroy (ew->window);
  c2_free (ew);
}

static void on_tree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, ImportWidgets *ew) {
  GtkCTreeNode *node;

  node = gtk_ctree_node_nth(ctree, row);
  ew->mbox = (char *) ((Mailbox *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node))->name;
}
