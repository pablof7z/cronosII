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
/* This is the front end to the account checking, all callbacks functions relay in here, as well
 * as the main loop for account checking.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <glib.h>
#include <pthread.h>

#include "main.h"
#include "check.h"
#include "pop.h"
#include "init.h"
#include "account.h"
#include "spool.h"
#include "search.h"
#include "debug.h"
#include "plugin.h"
#include "utils.h"

#include "gui-window_checking.h"
#include "gui-window_main.h"

int
check_timeout_check (void) {
  pthread_t thread;
  
  if (!window_checking) gui_window_checking_new ();
  pthread_create (&thread, NULL, PTHREAD_FUNC (check_account_all), NULL);

  return TRUE;
}

int check_account_all (void) {
  Account *account;

  if (check_state == C2_CHECK_ACTIVE) {
    gdk_threads_enter ();
    gtk_widget_show (window_checking->window);
    gdk_threads_leave ();
  }

  /* Add all of the accounts to the queue */
  for (account = config->account_head; account; account = account->next) {
    if (account->use_it)
      check_add_queue (account);
  }

  /* Start the process or reinit the UI */
  if (check_state == C2_CHECK_DEACTIVE) check_main ();
  else check_init ();

  return TRUE;
}

void check_account_single (Account *account) {
  g_return_if_fail (account);

  gdk_threads_enter ();
  if (check_state == C2_CHECK_ACTIVE) gtk_widget_show (window_checking->window);
  gdk_threads_leave ();
  check_add_queue (account);

  if (check_state == C2_CHECK_DEACTIVE) check_main ();
  else check_init ();
}

void check_init (void) {
  gfloat offset;

  /* Window Checking */
  gdk_threads_enter ();
  if (!window_checking) gui_window_checking_new ();
  else {
    if (check_state == C2_CHECK_DEACTIVE) {
      gtk_clist_freeze (GTK_CLIST (window_checking->clist));
      gtk_clist_clear (GTK_CLIST (window_checking->clist));
      gtk_clist_thaw (GTK_CLIST (window_checking->clist));
    }
  }

  offset = gtk_progress_get_value (GTK_PROGRESS (window_checking->acc_progress));
  gtk_progress_configure (GTK_PROGRESS (window_checking->acc_progress), offset, 0, check_queue_length ());
  gdk_threads_leave ();
#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_CHECK_NEW_SESSION, (gpointer)check_queue_length (),
      				 NULL, NULL, NULL, NULL);
#endif
}

#if TRUE
void check_main (void) {
  Account *current;
  char *buf = NULL, *buf2 = NULL;
  int i;

  g_return_if_fail (check_state == C2_CHECK_DEACTIVE);
  check_init ();
  check_state = C2_CHECK_ACTIVE;
  
  for (current = check_queue, i = 1; current; current = current->next, i++) {
    buf = g_strdup_printf (_("Checking Account: %s"), current->acc_name);
    gdk_threads_enter ();
      gtk_window_set_title (GTK_WINDOW (window_checking->window), buf);
      gtk_progress_set_format_string (GTK_PROGRESS (window_checking->acc_progress),
	  		_("Checking account %v of %u"));
    gdk_threads_leave ();
    if (buf2) c2_free (buf2);
    buf2 = buf;
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_CHECK_NEW_ACCOUNT, current,
      				 NULL, NULL, NULL, NULL);

    /* Process */
    if (current->type == C2_ACCOUNT_POP) {
      check_pop_main (current);
    }
    else if (current->type == C2_ACCOUNT_SPOOL) {
      gdk_threads_enter ();
      gtk_widget_hide (window_checking->bytes_progress);
      gdk_threads_leave ();
      check_spool_main (current);
    }
    
    gdk_threads_enter ();
      gtk_window_set_title (GTK_WINDOW (window_checking->window), buf);
      gtk_progress_set_format_string (GTK_PROGRESS (window_checking->acc_progress),
	  		_("Checked account %v of %u"));
      gtk_progress_set_value (GTK_PROGRESS (window_checking->acc_progress), i);
    gdk_threads_leave ();
  }

  account_free (check_queue);
  check_queue = NULL;
  check_state = C2_CHECK_DEACTIVE;
}
#else
void check_main (void) {
  static int offset = 0;
  static int length = 0;
  Account *current, *prev = NULL;
  char *buf;

  if (check_state == C2_CHECK_DEACTIVE) {
    check_init ();
    check_state = C2_CHECK_ACTIVE;
    length = check_queue_length ();
  }
  else {
    gdk_threads_enter ();
    buf = g_strdup_printf (_("Checking account %s [%%v of %%u]"), current->acc_name);
    gtk_progress_set_format_string (GTK_PROGRESS (window_checking->acc_progress), buf);
    gtk_progress_set_value (GTK_PROGRESS (window_checking->acc_progress),
		gtk_progress_get_value (GTK_PROGRESS (window_checking->acc_progress))+1);
    length = check_queue_length ();
    gdk_threads_leave ();
  }

  current = check_queue_nth (offset);
  if (offset > 0) prev = check_queue_nth (offset-1);
  if (prev) c2_free (prev);
  offset++;
  if (offset > length) return;

  if (!current) {
    check_queue = NULL;
    check_state = C2_CHECK_DEACTIVE;
    gdk_threads_enter ();
    gtk_progress_configure (GTK_PROGRESS (window_checking->acc_progress), 0, 0, 0);
    gtk_progress_configure (GTK_PROGRESS (window_checking->mail_progress), 0, 0, 0);
    gtk_progress_configure (GTK_PROGRESS (window_checking->bytes_progress), 0, 0, 0);
    gtk_progress_set_format_string (GTK_PROGRESS (window_checking->acc_progress), NULL);
    gtk_progress_set_format_string (GTK_PROGRESS (window_checking->mail_progress), NULL);
    gtk_progress_set_format_string (GTK_PROGRESS (window_checking->bytes_progress), NULL);
    gdk_threads_leave ();
    return;
  }

  if (current->type == C2_ACCOUNT_POP) {
    gdk_threads_enter ();
    gtk_widget_show (window_checking->bytes_progress);
    gdk_threads_leave ();
    check_pop_main (current);
  }
  else if (current->type == C2_ACCOUNT_SPOOL) {
    gdk_threads_enter ();
    gtk_widget_hide (window_checking->bytes_progress);
    gdk_threads_leave ();
    check_spool_main (current);
  }
  check_main ();
}
#endif

void check_add_queue (Account *account) {
  Account *last;
  Account *new;
  
  g_return_if_fail (account);
  new = account_copy (account);
  
  if (check_queue) {
    last = search_account_last_element (check_queue);
    last->next = new;
  } else {
    check_queue = new;
  }
}

Account *check_queue_nth (guint nth) {
  Account *acc;
  int i;

  for (acc = check_queue, i=0; i < nth && acc; i++, acc = acc->next) if (!acc) return NULL;
  if (!acc) return NULL;
  if (i != nth) return NULL;
  return acc;
}

void check_queue_remove_nth (guint nth) {
  Account *acc;

  acc = check_queue_nth (nth);
  g_return_if_fail (acc);
}

int check_queue_length (void) {
  int i;
  Account *acc;

  for (i=0, acc = check_queue; acc; acc = acc->next, i++) if (!acc) return i;
  return i;
}
