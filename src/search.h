/*  Cronos II /src/search.h
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
#ifndef SEARCH_H
#define SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#ifdef HAVE_CONFIG_H
#  include <config.h>
#  include "mailbox.h"
#  include "account.h"
#else
#  include <cronosII.h>
#endif

/* Returns the mailbox that has the id "search_id" or NULL if it couldn't find it searching in "head" */
Mailbox *
search_mailbox_id						(Mailbox *, guint16);

Mailbox *
search_mailbox_name						(Mailbox *, char *);

/* Returns the last element of the "head" linked list  */
Mailbox *
search_mailbox_last_element					(Mailbox *);

Account *
search_account_acc_name						(Account *, char *);

Account *
search_account_mail_addr					(Account *head, char *mail_addr);

/* Returns the last element of the "head" linked list  */
Account *
search_account_last_element					(Account *);

#ifdef __cplusplus
}
#endif
  
#endif
