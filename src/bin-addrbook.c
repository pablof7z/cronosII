/* This is the Cronos II independent Address Book */

#include <config.h>

#include "addrbook.h"
#include "init.h"

#include "gui-addrbook.h"

static void
on_close (void);

int
main (int argc, char **argv) {
  g_thread_init (NULL);
  gtk_set_locale ();
#ifdef ENABLE_NLS
#ifdef HAVE_SETLOCALE_H
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);
#endif
  gnome_init ("cronosII-address-book", VERSION, argc, argv);

  cronos_init ();
  addrbook = NULL;
  gaddrbook = NULL;
  gdk_threads_enter ();
  if (config->addrbook_init == INIT_ADDRESS_BOOK_INIT_START)
    c2_address_book_init ();
  gaddrbook = c2_address_book_new ("cronosII-address-book");
  gtk_signal_connect (GTK_OBJECT (gaddrbook->window), "delete_event",
      				GTK_SIGNAL_FUNC (on_close), NULL);
  gtk_widget_show (gaddrbook->window);
  gtk_main ();
  gdk_threads_leave ();
  return 0;
}

static void
on_close (void) {
  gtk_main_quit ();
}
