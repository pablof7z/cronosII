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
#include <errno.h>
#include <unistd.h>

#include "mailbox.h"
#include "init.h"
#include "exit.h"
#include "rc.h"
#include "error.h"
#include "debug.h"
#include "utils.h"
#include "main.h"
#include "smtp.h"

#include "gui-window_main.h"

void free_mailboxes (Mailbox *head, gboolean free_name, gboolean free_widg) {
  Mailbox *curr;
  Mailbox *buf;
  
  for (curr = head; curr;) {
    buf = curr->next;
    if (free_name) c2_free (curr->name);
    if (free_widg) c2_free (curr->widget);
    if (curr->child) free_mailboxes (curr->child, free_name, free_widg);
    c2_free (curr);
    curr = buf;
    if (!curr) return;
  }
}

static void empty_garbage (void) {
  char *path;
  char *tpath;
  char *line;
  mid_t mid;
  FILE *fd;

  path = g_strdup_printf ("%s%s/%s.mbx/index", getenv ("HOME"), ROOT, MAILBOX_GARBAGE);
  if ((fd = fopen (path, "r")) == NULL) {
    cronos_error (errno, _("Opening the main DB file of the Garbage Mailbox for deleting all mails"),
				ERROR_WARNING);
    c2_free (path);
    return;
  }

  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;
    mid = atoi (str_get_word (7, line, '\r'));
    
    tpath = c2_mailbox_mail_path (MAILBOX_GARBAGE, mid);
    unlink (tpath);
    
    c2_free (tpath);
  }

  fclose (fd);

  if ((fd = fopen (path, "w")) == NULL) {
    cronos_error (errno, _("Opening the main DB file of the Garbage Mailbox for deleting all mails"),
				ERROR_WARNING);
    c2_free (path);
    return;
  }
  fclose (fd);

  c2_free (path);
}

static void
delete_tmp_files (void) {
  char *nam;
  GSList *list;

  for (list = tmp_files; list != NULL; list = list->next) {
    nam = CHAR (list->data);
    if (!nam) continue;
    unlink (nam);
  }
}

void cronos_exit (void) {
  if (config->use_persistent_smtp_connection) smtp_do_persistent_disconnect (TRUE);
  rc_save ();
  gtk_main_quit ();
  if (config->empty_garbage) empty_garbage ();
  if (tmp_files) delete_tmp_files ();
}
