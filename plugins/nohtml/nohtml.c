/* nohtml.c
 *
 *  NoHtml CronosII Plugin
 * 
 *  Copyright (C) 2002 Daniel Fairhead 
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
 *
 *  +--------------------------------------------------+
 *  |   Most of this code is taken from the CronosII   |
 *  |   sample plugin.                                 |
 *  +--------------------------------------------------+
*/

/*#include <cronosII.h>*/
#include "../headers/main.h"
#include "../headers/plugin.h"
#include "../headers/init.h"
#include "../headers/message.h"
#include <gnome.h>
#include <stdio.h>
/*#ifndef USE_PLUGINS
#error "You can't compile a plugin for Cronos II since it was compiled"
#error "without plugins support. Recompile with:"
#error "   ./configure --enable-plugins (and whatever else you want) ."
#endif*/

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
  "No HTML",
  "0.0.1",
  "Daniel Fairhead <madprof@madprof.net>",
  "http://www.madprof.net",
  "I hate HTML emails."
};

static void
plugin_on_message_open (Message *message, const char *type);

static void
plugin_load_configuration (const char *config);

static void
plugin_save_configuration (const char *config);

GtkWidget *
plugin_sample_configure	(C2DynamicModule *module);

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
  c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_MESSAGE_OPEN,
  				C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_message_open));
  return NULL;
}

void
module_cleanup (C2DynamicModule *module) {
  g_return_if_fail (module);
  c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_MESSAGE_OPEN);
}

/*
	Here is where we actually remove all the HTML. In fact... All this
	does is to check if email contains "<html>", and if so, to strip out
	all the text between < and >.
*/
static void
plugin_on_message_open (Message *message, const char *type) {
	char *text, *chr;
	int i;
	gboolean in_tag = FALSE;
	char *frm;
	char *dst;
	char *str = message->message;
	char *str_pointer;
	str_pointer = strstr(str,"<html>");
	if (strstr(str,"<html>")!=NULL){
		for (frm = dst = str; *frm; frm++) {
			if (*frm == '<') in_tag = TRUE;
			if (in_tag == FALSE) *dst++ = *frm;
			if (*frm == '>') in_tag = FALSE;
		}
		*dst = '\0';
	}
}

static void
plugin_load_configuration (const char *config) {
  char *line;
  char *key;
  char *val;
  FILE *fd; 

  /*char *fd;*/
  
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

  label = gtk_label_new ("This module makes an alert when mail arrives from an specified address.\n"
      			 "Enter a mail address to use.\n");
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
