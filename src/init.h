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
#ifndef INIT_H
#define INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#if HAVE_CONFIG_H
#  include <config.h>
#  include "mailbox.h"
#  include "account.h"
#  if USE_PLUGINS
#    include "plugin.h"
#  endif
#else
#  include <cronosII.h>
#endif

#define ROOT	"/.CronosII"
#define CONFIG	"/cronos.conf"
#define TMP	"/tmp"

typedef enum {
  INIT_DEFAULT_MIME_PART_PLAIN,
  INIT_DEFAULT_MIME_PART_HTML
} InitDefaultMimePart;

#ifdef BUILD_ADDRESS_BOOK
typedef enum {
  INIT_ADDRESS_BOOK_INIT_START,
  INIT_ADDRESS_BOOK_INIT_REQ,
  INIT_ADDRESS_BOOK_INIT_OPEN,
} InitAddressBookInitializationMode;
#endif

typedef struct {
  Account	*account_head;		/* Pointer to the first element of the account's linked list	*/
  Mailbox	*mailbox_head;		/* Pointer to the first element of the mailbox's linked list	*/
#if USE_PLUGINS
  C2DynamicModule *module_head;		/* Pointer to the first element of the module's  linked list	*/
#endif
  gboolean	empty_garbage;		/* Should delete the Garbage mailbox when exiting		*/
  guint8	check_timeout;		/* Will auto check for mails every $check_timeout minutes	*/
  gboolean	check_at_start;		/* Check for mail at start					*/
  gboolean	use_outbox;		/* Keep a copy in the Outbox mailbox when sending a mail	*/
  char		*prepend_char_on_re;	/* Prepend this char to each line when Replying or Forwarding	*/
  guint16	message_bigger;		/* Do not download message bigger than				*/
  int 		timeout;		/* Configurable timeout for net related process			*/
  guint8	mark_as_read;		/* Seconds before marking a mail as readed			*/
  gboolean	use_persistent_smtp_connection;
  char		*persistent_smtp_address;
  int		persistent_smtp_port;
  InitDefaultMimePart default_mime_part;/* Default MIME Part (plain or html)				*/
#ifdef BUILD_ADDRESS_BOOK
  InitAddressBookInitializationMode addrbook_init; /* When to initializate the address book		*/
#endif

  GdkColor	color_reply_original_message; /* Replying and forwarding: Original message color	*/
  GdkColor	color_misc_body;	/* Message Text Color						*/

  gboolean	queue_state;
} Conf;

Conf *config;

void
cronos_init						(void);

#ifdef __cplusplus
}
#endif

#endif
