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

#include <stdio.h>
#include "gui-mailbox_tree.h"
#include "gui-window_main.h"
#include "mailbox.h"
#include "debug.h"
#include "xpm/drafts.xpm"
#include "xpm/inbox.xpm"
#include "xpm/outbox.xpm"
#include "xpm/trash.xpm"
#include "xpm/queue_mbox.xpm"
#include "xpm/folder.xpm"

void create_mailbox_tree (GtkWidget *window, GtkWidget *ctree, GtkCTreeNode *node, Mailbox *head) {
  Mailbox *current;
  GdkPixmap *xpm;
  GdkBitmap *msk;
  GtkCTreeNode *_node;
  char *buf;

  if (!head || !ctree) return;
  
  current = head;
  
  for (; current; current = current->next) {  
    if (!strcmp ((char *) current->name, MAILBOX_INBOX)) {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
  				&window->style->bg[GTK_STATE_NORMAL],
				inbox_xpm);
    } else if (!strcmp ((char *) current->name, MAILBOX_OUTBOX)) {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
  				&window->style->bg[GTK_STATE_NORMAL],
				outbox_xpm);
    } else if (!strcmp ((char *) current->name, MAILBOX_QUEUE)) {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
  				&window->style->bg[GTK_STATE_NORMAL],
				queue_mbox_xpm);
    } else if (!strcmp ((char *) current->name, MAILBOX_GARBAGE)) {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
  				&window->style->bg[GTK_STATE_NORMAL],
				trash_xpm);
    } else if (!strcmp ((char *) current->name, MAILBOX_DRAFTS)) {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
  				&window->style->bg[GTK_STATE_NORMAL],
				drafts_xpm);
    } else {
      xpm = gdk_pixmap_create_from_xpm_d (window->window, &msk,
      				&window->style->bg[GTK_STATE_NORMAL],
				folder_xpm);
    }
    
    buf = g_strdup ((char *) current->name);
    _node = gtk_ctree_insert_node (GTK_CTREE (ctree), node, NULL, (char **) &buf, 4, xpm, msk,
	xpm, msk, FALSE, TRUE);
    gtk_ctree_node_set_row_data(GTK_CTREE(ctree), _node, (gpointer) current);
    if (current->child) create_mailbox_tree (window, ctree, _node, current->child);
  }
}
