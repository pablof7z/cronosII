#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <glib.h>
#include <stdlib.h>
#include <pthread.h>

#include "main.h"
#include "init.h"
#include "error.h"
#include "debug.h"
#include "exit.h"
#include "check.h"
#include "index.h"
#include "utils.h"
#include "search.h"
#include "smtp.h"
#include "addrbook.h"
#include "mailbox.h"
#include "version.h"

#include "gui-window_main.h"
#include "gui-logo.h"
#include "gui-select_file.h"
#include "gui-window_checking.h"
#include "gui-utils.h"
#include "gui-addrbook.h"

static void
action_at_start							(void);

int main (int argc, char **argv) {
  pthread_t thread;
  Mailbox *queue=NULL;

  g_thread_init (NULL);
  gtk_set_locale ();
#ifdef ENABLE_NLS
#ifdef HAVE_SETLOCALE_H
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);
#endif
  gnome_init (PACKAGE, VERSION, argc, argv);
 
  gdk_threads_enter ();
  WMain = NULL;
  reload_mailbox_list_timeout = -1;
  persistent_sock = -1;
  window_checking = NULL;
  check_state = C2_CHECK_DEACTIVE;
  check_queue = NULL;
  tmp_files = NULL;
  last_path = NULL;
  addrbook = NULL;
  gaddrbook = NULL;

  cronos_init ();
  cronos_gui_init ();

  /* This should probably be done in the other thread */
  queue = search_mailbox_name (config->mailbox_head, MAILBOX_QUEUE);
  if (!queue) config->queue_state = FALSE;
  if (c2_mailbox_length (queue) == 0) config->queue_state = FALSE;
  else config->queue_state = TRUE;

  gui_window_main_new ();
  if (config->check_timeout)
    check_timeout = gtk_timeout_add (config->check_timeout*60000, (GtkFunction)check_timeout_check, NULL);
  else
    check_timeout = 0;
  gdk_threads_leave ();
  pthread_create (&thread, NULL, PTHREAD_FUNC (action_at_start), NULL);

  gdk_threads_enter ();
  gtk_main ();
  gdk_threads_leave ();

  return 0;
}

static void
action_at_start (void) {
  Mailbox *inbox=NULL;
  GtkCTreeNode *cnode=NULL;
  GtkCTreeNode *cnode_sele=NULL;
  int i;
 
  inbox = search_mailbox_name (config->mailbox_head, MAILBOX_INBOX);
  
  if (!inbox) goto after_selection_inbox;

  for (i=1, cnode = gtk_ctree_node_nth (GTK_CTREE (WMain->ctree), 0); cnode; i++) {
    if (i>1) cnode = gtk_ctree_node_nth (GTK_CTREE (WMain->ctree), i);
    cnode_sele = gtk_ctree_find_by_row_data (GTK_CTREE (WMain->ctree), cnode, inbox);
    
    if (cnode_sele) {
      gdk_threads_enter ();
      gtk_ctree_select (GTK_CTREE (WMain->ctree), cnode_sele);
      gdk_threads_leave ();
      selected_mbox = inbox->name;
      break;
    }
  }

  gdk_threads_enter ();
  cronos_gui_set_sensitive ();
  gdk_threads_leave ();
  selected_mbox = inbox->name;
  update_clist (inbox->name, FALSE, FALSE);

  if (config->addrbook_init == INIT_ADDRESS_BOOK_INIT_START)
    c2_address_book_init ();
  
  if (config->use_persistent_smtp_connection &&
      config->persistent_smtp_address) {
    Pthread3 *helper;
    pthread_t thread;

    helper = g_new0 (Pthread3, 1);
    helper->v1 = config->persistent_smtp_address;
    helper->v2 = (gpointer)config->persistent_smtp_port;
    helper->v3 = WMain->appbar;

    pthread_create (&thread, NULL, PTHREAD_FUNC (smtp_persistent_connect), helper);
  }
after_selection_inbox:
  if (config->check_at_start) {
    gdk_threads_enter ();
    if (!window_checking) gui_window_checking_new ();
    gtk_widget_show (window_checking->window);
    gdk_window_raise (window_checking->window->window);
    gdk_threads_leave ();
    check_account_all ();
  }
}
