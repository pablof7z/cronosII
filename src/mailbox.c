/*  Cronos II     mailbox.c
 *  Copyright (C) 2000-2001 Pablo Fernández Navarro
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the c2_free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the c2_free Software
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
#include <string.h>
#include <errno.h>

#include "main.h"
#include "mailbox.h"
#include "search.h"
#include "utils.h"
#include "init.h"
#include "error.h"
#include "debug.h"
#include "index.h"

void
c2_mailbox_load (Mailbox **head, char *info) {
  Mailbox *new;
  Mailbox *search;
  Mailbox *child;

  new = g_new0 (Mailbox, 1);
 
  /* 
   * Decode the info:
   * "self_id\rmailbox_name\rparent_id"
   */
  new->self_id		= (guint16) atoi (str_get_word (0, info, '\r'));
  new->name		= str_get_word (1, info, '\r');
  new->parent_id	= (guint16) atoi (str_get_word (2, info, '\r'));
  new->child		= NULL;
  new->next		= NULL;
  new->widget		= NULL;
  new->higgest_mid	= -1;
  new->visible_row	= -1;

  if (new->self_id == new->parent_id) { /* I'm my parent */
    search = search_mailbox_last_element (*head);
    if (search != NULL) {
      search->next = new;
    } else {
      *head = new;
    }
  } else {
    /* Search for the parent and append the new element */
    search = search_mailbox_id (*head, new->parent_id);

    if (search != NULL) {
      /* Search for the last element */
      child = search_mailbox_last_element (search->child);
    
      if (child == NULL) {
        /* Then this is the first element */
        search->child = new;
      } else {
        child->next = new;
      }
    } else {
      *head = new;
    }
  }
}

/**
 * c2_mailbox_length
 * @mbox: A pointer to a Mailbox object.
 *
 * Counts how many mails there're in a mailbox.
 *
 * Return Value:
 * The number of mails in the mailbox.
 **/
int
c2_mailbox_length (const Mailbox *mbox) {
  char *path;
//  char *line;
  int character;
  FILE *fd;
  int mails = 0;
  
  g_return_val_if_fail (mbox, 0);
  
  /* Get the mailbox index path and open it */
  path = c2_mailbox_index_path (mbox->name);
  if ((fd = fopen (path, "r")) == NULL) return 0;
  
  for (;;) {
    /* Check if this line is deleted */
    if ((character = fgetc (fd)) == EOF) break;
    if (character != '?') mails++;
    fd_move_to (fd, '\n', 1, TRUE, TRUE);
  }
  
  return mails;
}

/**
 * c2_mailboxes_length
 * @head: A pointer to a Mailbox object.
 *
 * Counts how many mailbox there are.
 *
 * Return Value:
 * The number of mailboxes.
 **/
int
c2_mailboxes_length (const Mailbox *head) {
  int i;
  const Mailbox *search;

  g_return_val_if_fail (head, 0);
  
  for (search = head, i = 0; search; search = search->next, i++) {
    if (search->child) i += c2_mailboxes_length (search->child);
  }

  return i;
}

/**
 * mailbox_get_next_mid
 * @mbox: A pointer to a Mailbox object containing the mailbox to use.
 *
 * This function will register the next available
 * MID (Message ID) and return it working on the
 * mailbox @mbox.
 *
 * Return Value:
 * The new mid.
 **/
mid_t
c2_mailbox_get_next_mid (Mailbox *mbox) {
  g_return_val_if_fail (mbox, -1);
  
  if (mbox->higgest_mid < 0) {
    FILE *fd;
    char *path;
    int pos;

    /* Open the mailbox index file */
    path = c2_mailbox_index_path (mbox->name);
    if ((fd = fopen (path, "r")) == NULL) {
      cronos_error (errno, _("Failed to open the main DB file"), ERROR_WARNING);
      c2_free (path);
      return -1;
    }

    fseek (fd, -1, SEEK_END);
    pos = ftell (fd)+1;
    
    do {
      c2_free (path);
      fseek (fd, pos-1, SEEK_SET);
      if (!fd_move_to (fd, '\r', 1, FALSE, TRUE)) break;
      pos = ftell (fd)+1;
      path = fd_get_line (fd);
    } while (strneq (path, "?", 1));

    fclose (fd);
    if (path)
      mbox->higgest_mid = atoi (path);
    else
      mbox->higgest_mid = -1;
  }
  return ++(mbox->higgest_mid);
}

Mailbox *
c2_mailbox_copy_linked_list_node (const Mailbox *node) {
  Mailbox *cp_node;
  
  if (!node) return NULL;
  cp_node = g_new0 (Mailbox, 1);
  memcpy (cp_node, node, sizeof (Mailbox));

  return cp_node;
}

Mailbox *
c2_mailbox_copy_linked_list (const Mailbox *head) {
  Mailbox *cp_head=NULL;
  Mailbox *item;
  const Mailbox *current;
  
  if (!head) return NULL;
  
  /* Browse the hole linked list */
  for (current = head; current; current = current->next) {
    item = c2_mailbox_copy_linked_list_node (current);
    if (!cp_head) cp_head = item;
    item->child = c2_mailbox_copy_linked_list (current->child);
    item = item->next;
  }

  return cp_head;
}

guint16
c2_mailbox_next_avaible_id (const Mailbox *head) {
  const Mailbox *current;
  guint16 id=0;
  guint16 buf=0;

  if (!head) return 10;
  
  for (current = head; current; current = current->next) {
    if (current->self_id > id) id = current->self_id+1;
    if (current->child) if ((buf = c2_mailbox_next_avaible_id (current->child)) > id) id = buf + 1;
  }

  return id;
}

void expunge_mail (const char *strmbx, mid_t mid) {
  char *buf;
  char *frmf, *tmpf;
  FILE *fd, *tmp;
  mid_t rmid;

  if (!strmbx || mid < 0) return;

  /* Delete the mail from the frm mailbox */
  frmf = g_strdup_printf ("%s%s/%s.mbx/index", getenv ("HOME"), ROOT, strmbx);
  if ((fd = fopen (frmf, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (frmf);
    return;
  }

  tmpf = cronos_tmpfile ();
  if ((tmp = fopen (tmpf, "w")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (tmpf);
    c2_free (frmf);
    fclose (fd);
    return;
  }

  for (;;) {
    if ((buf = fd_get_line (fd)) == NULL) break;

    rmid = atoi (str_get_word (7, buf, '\r'));
    if (rmid == mid) {
      c2_free (buf);
    } else {
      fprintf (tmp, "%s\n", buf);
      c2_free (buf);
    }
  }

  fclose (tmp);
  fclose (fd);
  fd_mv (tmpf, frmf);
  frmf = c2_mailbox_mail_path (strmbx, mid);
  unlink (frmf);
  c2_free (frmf);

}

void move_mail (const char *strfrm, const char *strdst, mid_t mid) {
  Mailbox *dstmbx;
  char *buf;
  char *line_to_append=NULL;
  char *frmf, *tmpf;
  FILE *fd, *tmp;
  mid_t rmid, nmid;
  
  if (!strfrm || !strdst || mid < 0) return;

  /* Locate the mailbox in the Linked list */
  if ((dstmbx = search_mailbox_name (config->mailbox_head, (char *) strdst)) == NULL) {
    gdk_threads_enter ();
    cronos_error (ERROR_INTERNAL,"Could not find the mailbox in the Mailbox List", ERROR_WARNING);
    gdk_threads_leave ();
    return;
  }

  /* Delete the mail from the frm mailbox */
  frmf = g_strdup_printf ("%s%s/%s.mbx/index", getenv ("HOME"), ROOT, strfrm);
  if ((fd = fopen (frmf, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (frmf);
    return;
  }

  tmpf = cronos_tmpfile ();
  if ((tmp = fopen (tmpf, "w")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (tmpf);
    c2_free (frmf);
    fclose (fd);
    return;
  }

  for (;;) {
    if ((buf = fd_get_line (fd)) == NULL) {
      if (!line_to_append) {
	gdk_threads_enter ();
	cronos_error (ERROR_INTERNAL, "Couldn't found the requested mail for transfering in the selected mailbox", ERROR_WARNING);
	gdk_threads_leave ();
	return;
      }
      break;
    }

    rmid = atoi (str_get_word (7, buf, '\r'));
    if (rmid == mid) {
      line_to_append = buf;
    } else {
      fprintf (tmp, "%s\n", buf);
      c2_free (buf);
    }
  }

  fclose (tmp);
  fclose (fd);

  fd_mv (tmpf, frmf);

  /* Find a good mid */
  nmid = c2_mailbox_get_next_mid (dstmbx);

  /* Open the dst mailbox file */
  buf = g_strdup_printf ("%s%s/%s.mbx/index", getenv ("HOME"), ROOT, strdst);
  if ((fd = fopen (buf, "a")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (buf);
    return;
  }
  c2_free (buf);

  fprintf (fd, "%s\r%s\r%s\r%s\r%s\r%s\r%s\r%d\n",
      	str_get_word (0, line_to_append, '\r'), str_get_word (1, line_to_append, '\r'),
	str_get_word (2, line_to_append, '\r'), str_get_word (3, line_to_append, '\r'),
	str_get_word (4, line_to_append, '\r'), str_get_word (5, line_to_append, '\r'),
	str_get_word (6, line_to_append, '\r'), nmid);

  fclose (fd);

  c2_free (line_to_append);

  /* Move the mail */
  frmf = c2_mailbox_mail_path (strfrm, mid);
  tmpf = c2_mailbox_mail_path (strdst, nmid);

  fd_mv (frmf, tmpf);
  c2_free (frmf);
  c2_free (tmpf);
}

void copy_mail (const char *strfrm, const char *strdst, mid_t mid) {
  Mailbox *dstmbx;
  char *buf;
  char *line_to_append;
  FILE *fd;
  mid_t rmid, nmid;
  
  if (!strfrm || !strdst || mid < 0) return;

  if ((dstmbx = search_mailbox_name (config->mailbox_head, (char *) strdst)) == NULL) {
    gdk_threads_enter ();
    cronos_error (ERROR_INTERNAL,"Could not find the mailbox in the Mailbox List", ERROR_WARNING);
    gdk_threads_leave ();
    return;
  }

  buf = g_strdup_printf ("%s/%s/%s.mbx/index", getenv ("HOME"), ROOT, strfrm);
  if ((fd = fopen (buf, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (buf);
    return;
  }
  c2_free (buf);

  for (;;) {
    if ((buf = fd_get_line (fd)) == NULL) {
      gdk_threads_enter ();
      cronos_error (ERROR_INTERNAL, "Couldn't found the requested mail for transfering in the selected mailbox", ERROR_WARNING);
      gdk_threads_leave ();
      return;
    }

    rmid = atoi (str_get_word (7, buf, '\r'));
    if (rmid == mid) {
      line_to_append = buf;
      break;
    }
    c2_free (buf);
  }

  fclose (fd);

  /* Find a good mid */
  nmid = c2_mailbox_get_next_mid (dstmbx);

  /* Open the dst mailbox file */
  buf = g_strdup_printf ("%s/%s/%s.mbx/index", getenv ("HOME"), ROOT, strdst);
  if ((fd = fopen (buf, "a")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, "Opening the main DB file of a mailbox", ERROR_WARNING);
    gdk_threads_leave ();
    c2_free (buf);
    return;
  }
  c2_free (buf);

  fprintf (fd, "%s\r%s\r%s\r%s\r%s\r%s\r%s\r%d\n",
      	str_get_word (0, line_to_append, '\r'), str_get_word (1, line_to_append, '\r'),
	str_get_word (2, line_to_append, '\r'), str_get_word (3, line_to_append, '\r'),
	str_get_word (4, line_to_append, '\r'), str_get_word (5, line_to_append, '\r'),
	str_get_word (6, line_to_append, '\r'), nmid);

  fclose (fd);

  c2_free (line_to_append);

  line_to_append = g_strdup_printf ("%s/%s/%s.mbx/%d", getenv ("HOME"), ROOT, strfrm, mid);
  buf = g_strdup_printf ("%s/%s/%s.mbx/%d", getenv ("HOME"), ROOT, strdst, nmid);

  fd_cp (line_to_append, buf);
  c2_free (line_to_append);
  c2_free (buf);
}
