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
#ifndef ACCOUNT_H
#define ACCOUNT_H

#ifdef __cplusplus
extern "C" {
#endif
 
#include <gnome.h>
#include <glib.h>

#if HAVE_CONFIG_H
#  include <config.h>
#  include "mailbox.h"
#else
#  include <cronosII.h>
#endif

#define ACCOUNT(x) ((Account*)x)
#define ACCOUNT_NEW (ACCOUNT(g_new0(Account, 1)))

typedef enum {
  C2_ACCOUNT_POP,
  C2_ACCOUNT_SPOOL
} C2AccountType;

struct _Account {
  char *acc_name;
  char *per_name;
  char *mail_addr;

  C2AccountType type;
  union {
    struct {
      char *usr_name;
      char *pass;
      char *host; int host_port;
    } pop;
    struct {
      char *file;
    } spool;
  } protocol;

  char *smtp; int smtp_port;
  gboolean keep_copy;
  gboolean use_it;
  
  Mailbox *mailbox;

  gboolean always_append_signature;
  char *signature;

  struct _Account *next;
};

typedef struct _Account Account;

void
load_account							(Account **head, char *info);

Account *
account_copy							(Account *orig);

Account *
account_nth							(Account *head, guint nth);

gboolean
account_remove_nth						(Account **head, guint nth);

gboolean
account_insert							(Account **head, Account *elem, guint nth);

Account *
account_copy_linked_list					(Account *head);

void
account_free							(Account *head);

#ifdef __cplusplus
}
#endif

#endif
