#include "../headers/plugin.h"
#include "../headers/init.h"
#include "../headers/utils.h"
#include <gnome.h>

/* Check if the local compilation supports plugins */

/* Required version */
#define REQUIRE_MAJOR_VERSION 0 /* <0>.2.0 */
#define REQUIRE_MINOR_VERSION 2 /* 0.<2>.0 */
#define REQUIRE_MICRO_VERSION 1 /* 0.2.<0> */

/* Plug In information */
enum {
  PLUGIN_NAME,
  PLUGIN_VERSION,
  PLUGIN_AUTHOR,
  PLUGIN_URL,
  PLUGIN_DESCRIPTION
};

char *information[] = {
  "Fortune",
  "1.0",
  "Pablo Fernández Navarro <cronosII@users.sourceforge.net>",
  "http://cronosII.sourceforge.net/plugins?fortune",
  "A plugin to use random signatures with Cronos II."
};

/* Function definitions */
static void
plugin_on_composer_insert_signature					(char **signature);

GtkWidget *
plugin_fortune_configure						(C2DynamicModule *module);

/* Global variables */
static char *cmnd = "fortune";

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
  module->configure	= plugin_fortune_configure;
  module->configfile	= NULL;

  /* Load the configuration */
  cmnd = gnome_config_get_string ("/plugins/fortune/cmnd=fortune");

  /* Connect the signals */
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_composer_insert_signature));

  return NULL;
}

GtkWidget *window;
GtkWidget *entry;

static void
on_plugin_fortune_configure_ok_btn_clicked (void) {
  cmnd = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
  gnome_config_set_string ("/plugins/fortune/cmnd", cmnd);
  gnome_config_sync ();
  gtk_widget_destroy (window);
}

static void
on_plugin_fortune_configure_cancel_btn_clicked (void) {
  gtk_widget_destroy (window);
}

GtkWidget *
plugin_fortune_configure (C2DynamicModule *module) {
  GtkWidget *hbox;
  GtkWidget *label;

  window = gnome_dialog_new ("Configuration", GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);
  
  label = gtk_label_new ("Command:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);
  gtk_entry_set_text (GTK_ENTRY (entry), cmnd);

  label = gtk_label_new ("You can use the commands just like in the terminal.\n"
  			 "\n"
			 "i.e. signature; cat $HOME/.signature");
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  gnome_dialog_button_connect (GNOME_DIALOG (window), 0,
  				GTK_SIGNAL_FUNC (on_plugin_fortune_configure_ok_btn_clicked), NULL);

  gnome_dialog_button_connect (GNOME_DIALOG (window), 1,
  				GTK_SIGNAL_FUNC (on_plugin_fortune_configure_cancel_btn_clicked), NULL);

  gtk_widget_show (window);
  
  return window;
}

void
module_cleanup (C2DynamicModule *module) {
  g_return_if_fail (module);

  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE);
}

static void
plugin_on_composer_insert_signature (char **signature) {
  char *out;
  if (!cmnd) return;

  out = cronos_system (cmnd);
  *signature = g_strdup_printf ("\n"
  				"--\n"
				"%s\n", out);
}
