#include <cronosII.h>
#include <gnome.h>
#include <stdio.h>

/* Check if the local compilation supports plugins */
#ifndef USE_PLUGINS
#error "You can't compile a plugin for Cronos II since it was compiled without plugins support. You should recompile the Cronos II source if you want to use this plugin."
#endif

/* Required version */
#define REQUIRE_MAJOR_VERSION 0 /* <0>.2.0 */
#define REQUIRE_MINOR_VERSION 2 /* 0.<2>.0 */
#define REQUIRE_MICRO_VERSION 0 /* 0.2.<0> */

/* Plug In information */
enum {
  PLUGIN_NAME,
  PLUGIN_VERSION,
  PLUGIN_AUTHOR,
  PLUGIN_URL,
  PLUGIN_DESCRIPTION
};

char *information[] = {
  "Sample",
  "0.1",
  "Pablo Fernández Navarro <cronosII@users.sourceforge.net>",
  "http://cronosII.sourceforge.net",
  "This is a little plugin to show how easy is to write a plugin"
  " for Cronos II."
};

/* Function definitions */
static void
plugin_on_message_download_pop					(Message *message);

static void
plugin_on_message_download_spool				(Message *message);

static void
plugin_on_check_new_session					(int length);

static void
plugin_on_check_new_account					(Account *account);

static void
plugin_load_configuration					(const char *config);

static void
plugin_save_configuration					(const char *config);

GtkWidget *
plugin_sample_configure						(C2DynamicModule *module);

/* Global variables */
static char *watch_address = NULL;

/* Module Initializer */
char *
module_init (int major_version, int minor_version, int patch_version, C2DynamicModule *module) {
  /* Check if the version is correct */
  if (major_version < REQUIRE_MAJOR_VERSION)
    return g_strdup_printf ("The plugin %s requires at least Cronos II %d.%d.%d.", information[PLUGIN_NAME],
				REQUIRE_MAJOR_VERSION, REQUIRE_MINOR_VERSION, REQUIRE_MICRO_VERSION);
  if (major_version == REQUIRE_MAJOR_VERSION && minor_version < REQUIRE_MINOR_VERSION)
    return g_strdup_printf ("The plugin %s requires at least Cronos II %d.%d.%d.", information[PLUGIN_NAME],
				REQUIRE_MAJOR_VERSION, REQUIRE_MINOR_VERSION, REQUIRE_MICRO_VERSION);
  if (major_version == REQUIRE_MAJOR_VERSION &&
      minor_version == REQUIRE_MINOR_VERSION &&
      patch_version < REQUIRE_MICRO_VERSION)
    return g_strdup_printf ("The plugin %s requires at least Cronos II %d.%d.%d.", information[PLUGIN_NAME],
				REQUIRE_MAJOR_VERSION, REQUIRE_MINOR_VERSION, REQUIRE_MICRO_VERSION);

  /* Check if the module is already loaded */
  if (c2_dynamic_module_find (information[PLUGIN_NAME], config->module_head))
    return g_strdup_printf ("The plugin %s is already loaded.", information[PLUGIN_NAME]);

  /* Set up the module information */
  module->name		= information[PLUGIN_NAME];
  module->version	= information[PLUGIN_VERSION];
  module->author	= information[PLUGIN_AUTHOR];
  module->url		= information[PLUGIN_URL];
  module->description	= information[PLUGIN_DESCRIPTION];
  module->configure	= plugin_sample_configure;
  module->configfile	= c2_dynamic_module_get_config_file (module->name);

  /* Load the configuration */
  plugin_load_configuration (module->configfile);

  /* Connect the signals */
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_message_download_pop));
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_SPOOL,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_message_download_spool));
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_CHECK_NEW_SESSION,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_check_new_session));
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_CHECK_NEW_ACCOUNT,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_check_new_account));

  return NULL;
}

void
module_cleanup (C2DynamicModule *module) {
  g_return_if_fail (module);

  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP);
  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_SPOOL);
  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_CHECK_NEW_SESSION);
  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_CHECK_NEW_ACCOUNT);
}

static void
plugin_on_message_download_pop (Message *message) {
  char *from;
  char *addr;
  
  g_return_if_fail (message);

  if (!watch_address) return;

  from = message_get_header_field (message, NULL, "From:");
  addr = str_get_mail_address (from);
  if (!addr) addr = from;

  if (streq (addr, watch_address)) {
    GtkWidget *window;
    char *message;

    message = g_strdup_printf ("A message from %s has arrived from the POP module.", from);
    gdk_threads_enter ();
    window = gnome_ok_dialog (message);
    gtk_window_set_title (GTK_WINDOW (window), "Sample Plugin");
    gtk_widget_show (window);
    gdk_threads_leave ();
  }

  c2_free (from);
}

static void
plugin_on_message_download_spool (Message *message) {
  char *from;
  char *addr;
  
  g_return_if_fail (message);

  if (!watch_address) return;

  from = message_get_header_field (message, NULL, "From:");
  addr = str_get_mail_address (from);
  if (!addr) addr = from;

  if (streq (addr, watch_address)) {
    GtkWidget *window;
    char *message;

    message = g_strdup_printf ("A message from %s has arrived from SPOOL module.", from);
    gdk_threads_enter ();
    window = gnome_ok_dialog (message);
    gtk_window_set_title (GTK_WINDOW (window), "Sample Plugin");
    gtk_widget_show (window);
    gdk_threads_leave ();
  }

  c2_free (from);
}

static void
plugin_on_check_new_session (int length) {
  GtkWidget *window;
  char *message;

  message = g_strdup_printf ("Checking %d accounts\n", length);
  gdk_threads_enter ();
  window = gnome_ok_dialog (message);
  gtk_window_set_title (GTK_WINDOW (window), "Sample Plugin");
  gtk_widget_show (window);
  gdk_threads_leave ();
}

static void
plugin_on_check_new_account (Account *account) {
  char *message;

  g_return_if_fail (account);
  
  message = g_strdup_printf ("Checking account %s", account->acc_name);
  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (WMain->status), message);
  gdk_threads_leave ();
}

static void
plugin_load_configuration (const char *config) {
  char *line;
  char *key, *val;
  FILE *fd;
  
  g_return_if_fail (config);

  if ((fd = fopen (config, "r")) == NULL) return;
  
  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;

    if ((key = str_get_word (0, line, ' ')) == NULL) continue;
    if ((val = str_get_word (1, line, ' ')) == NULL) continue;

    if (streq (key, "watch_address")) {
      watch_address = val;
    }
    else {
      char *err = g_strdup_printf (_("There's an unknown command in the configuration file %s: %s"),
	  				config, key);
      gnome_dialog_run_and_close (GNOME_DIALOG (gnome_ok_dialog (err)));
      c2_free (err);
      continue;
    }
  }
  fclose (fd);
}

/* Note that keywords should be non-spaced, or, if
 * you want them spaced, you should quote them ("key").
 *
 * The value of the keyword should always be quoted ("val").
 */
static void
plugin_save_configuration (const char *config) {
  FILE *fd;
  
  g_return_if_fail (config);
  if (!watch_address) return;

  if ((fd = fopen (config, "w")) == NULL) return;

  fprintf (fd, "watch_address \"%s\"\n", watch_address);
  fclose (fd);
}

GtkWidget *
plugin_sample_configure (C2DynamicModule *module) {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;

  window = gnome_dialog_new ("Configuration", GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  gnome_dialog_set_default (GNOME_DIALOG (window), 0);
  vbox = GNOME_DIALOG (window)->vbox;

  label = gtk_label_new ("This module makes an alert when a mail from an specified address.\n"
      			 "Here you can choose which mail address to use.\n");
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
 
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);
  if (watch_address) gtk_entry_set_text (GTK_ENTRY (entry), watch_address);

  switch (gnome_dialog_run (GNOME_DIALOG (window))) {
    case 0:
      watch_address = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
      plugin_save_configuration (module->configfile);
      break;
  }
  gnome_dialog_close (GNOME_DIALOG (window));
  
  return NULL;
}
