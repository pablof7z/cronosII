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

#include "search.h"
#include "mailbox.h"
#include "account.h"
#include "debug.h"

Mailbox *search_mailbox_id (Mailbox *head, guint16 search_id) {
  Mailbox *buffer;
  Mailbox *tmp;

  if (head == NULL) return NULL;

  buffer = head;
  while (buffer != NULL) {
    if (buffer == NULL) break;
    if (buffer->self_id == search_id) return buffer;
    if (buffer->child != NULL) {
      tmp = search_mailbox_id (buffer->child, search_id);
      if (tmp != NULL) return tmp;
    }
    buffer = buffer->next;
  }

  return NULL;
}

Mailbox *search_mailbox_name (Mailbox *head, char *name) {
  Mailbox *buffer;
  Mailbox *tmp;

  if (head == NULL) return NULL;

  buffer = head;
  while (buffer != NULL) {
	if (buffer == NULL) break;
	if (!strcmp (buffer->name, name)) return buffer;
	if (buffer->child != NULL) {
	  tmp = search_mailbox_name (buffer->child, name);
	  if (tmp != NULL) return tmp;
	}
	buffer = buffer->next;
  }

  return NULL;
}

Mailbox *search_mailbox_last_element (Mailbox *head) {
  Mailbox *buffer;
  
  if (head == NULL) return NULL;

  buffer = head;

  while (buffer->next != NULL) buffer = buffer->next;

  return buffer;
}

Account *search_account_acc_name (Account *head, char *acc_name) {
  Account *buffer;
  
  if (head == NULL) return NULL;

  buffer = head;

  while (buffer) {
    if (!strcmp (buffer->acc_name, acc_name)) return buffer;
    buffer = buffer->next;
  }

  return NULL;
}

Account *search_account_mail_addr (Account *head, char *mail_addr) {
  Account *buffer;
  
  if (head == NULL) return NULL;

  buffer = head;

  while (buffer) {
    if (!strcmp (buffer->mail_addr, mail_addr)) return buffer;
    buffer = buffer->next;
  }

  return NULL;
}

Account *search_account_last_element (Account *head) {
  Account *buffer;

  if (head == NULL) return NULL;

  buffer = head;

  while (buffer->next != NULL) buffer = buffer->next;

  return buffer;
}
