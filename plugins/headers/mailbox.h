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
#ifndef MAILBOX_H
#define MAILBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gnome.h>
#include <stdlib.h>
#include <stdio.h>

#define MAILBOX_INBOX		N_("Inbox")
#define MAILBOX_OUTBOX		N_("Outbox")
#define MAILBOX_QUEUE		N_("Queue")
#define MAILBOX_GARBAGE		N_("Garbage")
#define MAILBOX_DRAFTS		N_("Drafts")

#define c2_mailbox_mail_path(mbx, mid)	g_strdup_printf ("%s" ROOT "/%s.mbx/%d", getenv ("HOME"), mbx, mid)
#define c2_mailbox_index_path(mbx)	g_strconcat (getenv ("HOME"), ROOT, "/", mbx, ".mbx/index", NULL)

typedef short mid_t;

typedef struct _Mailbox {
  char *name;				/* Mailbox name */
  guint16 self_id;			/* Id of this mailbox */
  guint16 parent_id;		/* Id of its parent (== to self_id if its in the top level of the tree) */
  mid_t higgest_mid;			/* Mailbox's higher MID */
  GtkWidget *widget;		/* Widget in the mailbox tree in the main window */
  int visible_row;		/* The row beeing visible in the clist */
  struct _Mailbox *next;	/* Pointer to the next mailbox in the same level */
  struct _Mailbox *child;	/* Pointer to the first element of the subtree that depends on this mailbox */
} Mailbox;

void
c2_mailbox_load							(Mailbox **head, char *info);

int
c2_mailbox_length						(const Mailbox *mbox);

int
c2_mailboxes_length						(const Mailbox *head);

mid_t
c2_mailbox_get_next_mid						(Mailbox *mbox);

Mailbox *
c2_mailbox_copy_linked_list_node				(const Mailbox *node);

Mailbox *
c2_mailbox_copy_linked_list					(const Mailbox *head);

guint16
c2_mailbox_next_avaible_id					(const Mailbox *head);

void
expunge_mail							(const char *strmbx, mid_t mid);

void
move_mail							(const char *, const char *, mid_t);

void
copy_mail							(const char *, const char *, mid_t);

#ifdef __cplusplus
}
#endif
  
#endif
