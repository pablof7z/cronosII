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

#include <stdlib.h>
#include "account.h"
#include "mailbox.h"
#include "search.h"
#include "utils.h"
#include "init.h"
#include "main.h"
#include "debug.h"

/*
 * Format:
 * $fmt = "0.acc_name 1.per_name 2.mail_addr 3.type 4.smtp 5.smtp_port 6.keep_copy 7. use_it 8.mailbox"
 * if (type == POP)
 *   $fmt = "$fmt 9.usr_name 10.pass 11.host 12.host_port 13.always_append_signature 14.signature"
 * else if (type == SPOOL)
 *   $fmt = "$fmt 9.file 10.always_append_signature 11.signature"
 */
void load_account (Account **head, char *info) {
  Account *new;
  Account *search;
  Mailbox *mbox_search;
  guint16 mailbox;
  char *buf;

  g_return_if_fail (head);
  g_return_if_fail (info);
  
  /*
   * Decode info and save it
   */
  new = g_new0 (Account, 1);
  new->acc_name		= str_get_word (0, info, '\r');
  new->per_name		= str_get_word (1, info, '\r');
  new->mail_addr	= str_get_word (2, info, '\r');
  
  buf = str_get_word (3, info, '\r');
  if (streq (buf, "POP"))
    new->type = C2_ACCOUNT_POP;
  else if (streq (buf, "SPOOL"))
    new->type = C2_ACCOUNT_SPOOL;
  else {
    g_warning (_("An unknown protocol has been set, not supported by the current version of Cronos II: %s\n"),
	  	buf);
    return;
  }

  new->smtp		= str_get_word (4, info, '\r');
  new->smtp_port	= atoi (str_get_word (5, info, '\r'));
  new->keep_copy	= atoi (str_get_word (6, info, '\r'));
  new->use_it		= atoi (str_get_word (7, info, '\r'));
  mailbox		= atoi (str_get_word (8, info, '\r'));
  new->next		= NULL;

  mbox_search = search_mailbox_id (config->mailbox_head, mailbox);

  if (mbox_search == NULL) {
	g_warning (_("Could not find mailbox with ID %d, specified in your configuration file in the mailbox linked list.\nThis might cause serious bugs in the future.\n"), mailbox);
  } else {
     new->mailbox = mbox_search;
   }

  if (new->type == C2_ACCOUNT_POP) {
    new->protocol.pop.usr_name		= str_get_word (9, info, '\r');
    new->protocol.pop.pass		= str_get_word (10, info, '\r');
    new->protocol.pop.host		= str_get_word (11, info, '\r');
    new->protocol.pop.host_port		= (int) atoi (str_get_word (12, info, '\r'));
    new->always_append_signature	= (int) atoi (str_get_word (13, info, '\r'));
    new->signature			= str_get_word (14, info, '\r');
  } else if (new->type == C2_ACCOUNT_SPOOL) {
    new->protocol.spool.file		= str_get_word (9, info, '\r');
    new->always_append_signature	= (int) atoi (str_get_word (10, info, '\r'));
    new->signature			= str_get_word (11, info, '\r');
  }
  
  search = search_account_last_element (*head);

  if (search == NULL) {
    *head = new;
  } else {
    search->next = new;
  }
}

Account *account_copy (Account *orig) {
  Account *new;

  new = ACCOUNT_NEW;

  new->acc_name		= g_strdup (orig->acc_name);
  new->per_name		= g_strdup (orig->per_name);
  new->mail_addr	= g_strdup (orig->mail_addr);
  new->type		= orig->type;
  new->smtp		= g_strdup (orig->smtp);
  new->smtp_port	= orig->smtp_port;
  new->keep_copy	= orig->keep_copy;
  new->use_it		= orig->use_it;
  new->mailbox		= orig->mailbox;
  new->always_append_signature = orig->always_append_signature;
  new->signature	= g_strdup (orig->signature);
  new->next		= NULL;

  if (new->type == C2_ACCOUNT_POP) {
    new->protocol.pop.usr_name		= g_strdup (orig->protocol.pop.usr_name);
    new->protocol.pop.pass		= g_strdup (orig->protocol.pop.pass);
    new->protocol.pop.host		= g_strdup (orig->protocol.pop.host);
    new->protocol.pop.host_port		= orig->protocol.pop.host_port;
  } else if (new->type == C2_ACCOUNT_SPOOL) {
    new->protocol.spool.file		= g_strdup (orig->protocol.spool.file);
  }

  return new;
}

Account *account_nth (Account *head, guint nth) {
  Account *s;
  int i;

  g_return_val_if_fail (head, NULL);
  
  for (s = head, i = 0; s && i < nth; s = s->next, i++);
  return s;
}

gboolean account_remove_nth (Account **head, guint nth) {
  Account *prev = NULL, *next = NULL, *s;
  int i;

  g_return_val_if_fail (*head, FALSE);

  for (i = 0, s = *head; i < nth && s; i++, s = s->next) {
    if (i == nth-1) prev = s;
  }
  if (!s) return FALSE;
  next = s->next;

  if (!prev) *head = next;
  else prev->next = next;

  return TRUE;
}

gboolean account_insert (Account **head, Account *elem, guint nth) {
  Account *prev = NULL, *s;
  int i;
  gboolean is_the_last_element = FALSE;
  
  g_return_val_if_fail (*head, FALSE);
  g_return_val_if_fail (elem, FALSE);

  if (!elem->next) is_the_last_element = TRUE;

  for (s = *head, i = 0; s && i < nth; s = s->next, i++) {
    if (i == nth-1) prev = s;
  }
  if (!s) return FALSE;

  if (!prev) *head = elem;
  else prev->next = elem;
  elem->next = s;

  return TRUE;
}

Account *account_copy_linked_list (Account *head) {
  Account *cp_head=NULL;
  Account *item;
  Account *current;
  Account *prox;
  
  if (!head) return NULL;
  
  /* Browse the hole linked list */
  for (current = head; current; current = current->next) {
    item = (Account *) g_malloc0 (sizeof (Account));
    item->acc_name = g_strdup (current->acc_name);
    item->per_name = g_strdup (current->per_name);
    item->mail_addr = g_strdup (current->mail_addr);
    item->type = current->type;
    if (current->type == C2_ACCOUNT_POP) {
      item->protocol.pop.usr_name = g_strdup (current->protocol.pop.usr_name);
      item->protocol.pop.pass = g_strdup (current->protocol.pop.pass);
      item->protocol.pop.host = g_strdup (current->protocol.pop.host);
      item->protocol.pop.host_port = current->protocol.pop.host_port;
    } else {
      item->protocol.spool.file = g_strdup (current->protocol.spool.file);
    }
    item->smtp = g_strdup (current->smtp);
    item->smtp_port = current->smtp_port;
    item->mailbox = current->mailbox;
    item->keep_copy = current->keep_copy;
    item->use_it = current->use_it;
    item->always_append_signature = current->always_append_signature;
    item->signature = g_strdup (current->signature);
    if (!cp_head) cp_head = prox = item;
    else {
      prox->next = item;
      prox = prox->next;
    }
    item = item->next;
  }

  return cp_head;
}

void account_free (Account *head) {
  Account *s;
  Account *next;

  for (s = head; s;) {
    next = s->next;
    c2_free (s->acc_name);
    c2_free (s->per_name);
    c2_free (s->mail_addr);
    if (s->type == C2_ACCOUNT_POP) {
      c2_free (s->protocol.pop.usr_name);
      c2_free (s->protocol.pop.host);
      c2_free (s->protocol.pop.pass);
    } else if (s->type == C2_ACCOUNT_SPOOL) {
      c2_free (s->protocol.spool.file);
    }
    c2_free (s->smtp);
    c2_free (s->signature);
    c2_free (s);
    s = next;
  }
}

int account_length (Account *head) {
  Account *s;
  int i;

  g_return_val_if_fail (head, 0);
  
  for (i = 1, s = head; s; s = s->next, i++);
  return i;
}
