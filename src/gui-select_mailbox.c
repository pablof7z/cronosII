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

#include "mailbox.h"
#include "gui-select_mailbox.h"
#include "gui-mailbox_tree.h"
#include "gui-window_main.h"
#include "init.h"
#include "main.h"

static void on_ok_clicked (GtkWidget *widget, gpointer data) {
  SelectMailbox *sm;

  sm = (SelectMailbox *) data;

  if (sm->mbox) {
      gtk_widget_destroy (sm->window);
      gtk_main_quit ();
  } else {
    sm->mbox = NULL;
    gtk_widget_destroy (sm->window);
    gtk_main_quit ();
  }
}

static void on_cancel_clicked (GtkWidget *widget, gpointer data) {
  SelectMailbox *sm;

  sm = (SelectMailbox *) data;

  sm->mbox = NULL;
  gtk_widget_destroy (sm->window);
  gtk_main_quit ();
}

static void on_sm_tree_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, gpointer data) {
  SelectMailbox *sm;
  GtkCTreeNode *node;

  sm = (SelectMailbox *) data;

  node = gtk_ctree_node_nth(ctree, row);
  sm->mbox = (char *) ((Mailbox *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node))->name;
}

char *gui_select_mailbox (char *title) {
  SelectMailbox *sm;
  GtkWidget *vbox;
  GtkWidget *scroll;
  char *rsp;

  sm = (SelectMailbox *) g_malloc (sizeof (SelectMailbox));
  sm->mbox = NULL;

  sm->window = gnome_dialog_new (title ? title : _("Select a mailbox"),
      			GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  gtk_widget_set_usize (sm->window, 240, 280);
  gtk_container_set_border_width (GTK_CONTAINER (sm->window), 5);
  gtk_window_set_modal (GTK_WINDOW (sm->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (sm->window), GTK_WINDOW (WMain->window));
  gtk_window_set_policy (GTK_WINDOW (sm->window), FALSE, FALSE, FALSE);
  gtk_widget_realize (sm->window);
  gnome_dialog_set_default (GNOME_DIALOG (sm->window), 0);
  gnome_dialog_set_parent (GNOME_DIALOG (sm->window), GTK_WINDOW (WMain->window));
  gnome_dialog_button_connect (GNOME_DIALOG (sm->window), 0, on_ok_clicked, (gpointer) sm);
  gnome_dialog_button_connect (GNOME_DIALOG (sm->window), 1, on_cancel_clicked, (gpointer) sm);

  vbox = GNOME_DIALOG (sm->window)->vbox;

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scroll);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  sm->tree = gtk_ctree_new (1, 0);
  gtk_widget_show (sm->tree);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), sm->tree);
  create_mailbox_tree (sm->window, sm->tree, NULL, config->mailbox_head);
  gtk_signal_connect (GTK_OBJECT (sm->tree), "select_row",
      			GTK_SIGNAL_FUNC (on_sm_tree_select_row), sm);

  gtk_widget_show (sm->window);

  gtk_main ();

  rsp = sm->mbox;

  c2_free (sm);

  return rsp;
}
