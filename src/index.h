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
#ifndef INDEX_H
#define INDEX_H

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
#  include "init.h"
#else
#  include <cronosII.h>
#endif

typedef enum {
  C2_MARK_READ = ' ',
  C2_MARK_NEW = 'N',
  C2_MARK_FORWARD = 'F',
  C2_MARK_REPLY = 'R'
} C2MarkType;

#define index_mark_as_read(mbox, mid) index_mark_as (mbox, mid, C2_MARK_READ, config->mark_as_read)
#define index_mark_as_new(mbox, mid) index_mark_as (mbox, mid, C2_MARK_NEW, config->mark_as_read)
#define index_mark_as_reply(mbox, mid) index_mark_as (mbox, mid, C2_MARK_REPLY, config->mark_as_read)
#define index_mark_as_forward(mbox, mid) index_mark_as (mbox, mid, C2_MARK_FORWARD, config->mark_as_read)
#define mark_as_read(mbox, mid) index_mark_as_read(mbox, mid)
#define mark_as_new(mbox, mid) index_mark_as_new(mbox, mid)
#define mark_as_reply(mbox, mid) index_mark_as_reply(mbox, mid)
#define mark_as_forward(mbox, mid) index_mark_as_forward(mbox, mid)

typedef struct {
  char *mbox;
  int row;
  mid_t mid;
  C2MarkType type;
} C2MarkMail;

gint16 reload_mailbox_list_timeout;

void
index_mark_as							(char *mbox, int row, mid_t mid,
    								 C2MarkType type, guint16 timeout);

void
update_clist							(char *mbox, gboolean updating, gboolean in_gdk_thread);

#ifdef __cplusplus
}
#endif

#endif
