/* nohtml.c
 *
 *  NoHtml 0.1.1 -- CronosII Plugin
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
 *  |  Basic Code Outline from CronosII Sample Plugin  |
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
  "0.1.0",
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
/* OPTIONS */
static char *preview_parse = "no";
static char *viewer_parse = "no";
static char *html_require = "no";
static char *symbol_replace = "no";

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
	does is to check if email contains "<html>" (unless "dont the require html" option
	is selected), and if so, to strip out all the text between < and >. 
	
	Also converts <br> into a \n... which I think is rather cool :)

	Also does a few &symbol; type replacements into "real" letters,
	please send me patches if you want more supported (or email me really nicely ;)).
*/
static void
plugin_on_message_open (Message *message, const char *type) {
	//char *text, *chr;
	//int i;
	gboolean in_tag = FALSE;
	gboolean in_sym = FALSE;
	char *current_char;
	char *dst;
	char *str = message->message;
	GString *tag=g_string_new (NULL);
	gboolean parsing = FALSE;
	int j=0;
	
	/* Check to see if I should not parse this message. */
	
	if ((strstr(type,"preview"))&&(strstr(preview_parse,"no"))) return;
	if ((strstr(type,"message"))&&(strstr(viewer_parse,"no"))) return;
	
	/* Check for <HTML>, or if it is not required */

	/* if ((strstr(str,"<html>")!=NULL)||(strstr(html_require,"no"))){ */
	if ((strcasestr(str,"<html>"))||(strstr(html_require,"no"))){
		for (current_char = dst = str; *current_char; current_char++) {

			if (parsing==FALSE){
				if ((*current_char =='\n')&&(*(current_char+1) == '\n')){
					parsing = TRUE;
					*dst++ = *current_char;
				}else{
					*dst++ = *current_char;
				}
				continue;
			}

//#define FAKEPARSE = 0

#ifdef FAKEPARSE
				*dst++ = 'a';
#else
			if (*current_char == '<'){
				in_tag = TRUE;
				g_string_assign(tag,"");
			}
			// open symbol tag
			if ((*current_char == '&')&&(strstr(symbol_replace,"yes"))){
				in_sym = TRUE;
				g_string_assign(tag,"");
			}
			// normal tag. print it.
			if ((in_tag == FALSE)&&(in_sym == FALSE)){
				g_string_assign(tag,"");
				*dst++ = *current_char;
			}
			// is within a tag.
			if ((in_tag == TRUE)||(in_sym == TRUE)){
				j = strlen(tag->str);
				g_string_append (tag, current_char);
				g_string_truncate(tag, j+1);
			}
			// close tag.
			if (*current_char == '>'){
				in_tag = FALSE;
				g_string_down(tag);
				if (strcasestr(tag->str,"<br>")){
					*dst++ = '\n';
				}
				g_string_assign(tag,"");
			}
			if ((*current_char == ';')&&(strstr(symbol_replace,"yes"))){
				in_sym = FALSE;
				if (strcasestr(tag->str,"&amp;")) *dst++ = '&';
				if (strcasestr(tag->str,"&nbsp;")) *dst++ = ' ';
				if (strcasestr(tag->str,"&lt;")) *dst++ = '<';
				if (strcasestr(tag->str,"&gt;")) *dst++ = '>';
				if (strcasestr(tag->str,"&quot;")) *dst++ = '"';

				g_string_assign(tag,"");
			}
#endif			
			
		} // for loop
		*dst = '\0';
	} // contains <html>
} // function

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

    /* OPTIONS */
    if (streq (key, "preview_parse")) {
      preview_parse = val;
    }
    if (streq (key, "viewer_parse")) {
      viewer_parse = val;
    }
    if (streq (key, "html_require")) {
      html_require = val;
    }
    if (streq (key, "symbol_replace")) {
      symbol_replace = val;
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
  //if ((!viewer_parse)&&(!preview_parse)) return;

  if ((fd = fopen (config, "w")) == NULL) return;
  /* OPTIONS */
  
  fprintf (fd, "preview_parse \"%s\"\n", preview_parse);
  fprintf (fd, "viewer_parse \"%s\"\n", viewer_parse);
  fprintf (fd, "html_require \"%s\"\n", html_require);
  fprintf (fd, "symbol_replace \"%s\"\n", symbol_replace);
  fclose (fd);
}

GtkWidget *
plugin_sample_configure (C2DynamicModule *module) {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *hsep1;
  GtkWidget *hsep2;
  GtkWidget *run_when_label;
  GtkWidget *parse_label;
  GtkWidget *pr_label;
  // Run When Options:
  GtkWidget *pr_check;
  GtkWidget *vi_check;
  // Html Parsing Options:
  GtkWidget *html_tag_check;
  GtkWidget *symbol_replace_check;

  // Now Actually Make the interface.
  
  window = gnome_dialog_new ("Configuration", GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
  gnome_dialog_set_default (GNOME_DIALOG (window), 0);
  vbox = GNOME_DIALOG (window)->vbox;

  label = gtk_label_new ("This plugin removes html from a message.");
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
 
  hsep1 = gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), hsep1, TRUE, TRUE, 0);
  gtk_widget_show (hsep1);
  
  // Run When Options:
 
  run_when_label = gtk_label_new ("When do you want the plugin to run?\n" \
  								"Note: If Preview is Parsed, then Viewer will be too. \n " \
  								"I think this is a cronos bug. Sorry.");
  gtk_box_pack_start (GTK_BOX (vbox), run_when_label, TRUE, TRUE, 0);
  gtk_widget_show (run_when_label);

  pr_check = gtk_check_button_new_with_label ("Preview Mode?");
  gtk_box_pack_start (GTK_BOX (vbox), pr_check, FALSE, FALSE, 0);
  gtk_widget_show (pr_check);
  
  vi_check = gtk_check_button_new_with_label ("Viewer Mode?");
  gtk_box_pack_start (GTK_BOX (vbox), vi_check, FALSE, FALSE, 0);
  gtk_widget_show (vi_check);

  hsep2 = gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), hsep2, TRUE, TRUE, 0);
  gtk_widget_show (hsep2);
  
  // HTML Parsing Options:

  parse_label = gtk_label_new ("A Few HTML Parsing Options:");
  gtk_box_pack_start (GTK_BOX (vbox), parse_label, TRUE, TRUE, 0);
  gtk_widget_show (parse_label);

  html_tag_check = gtk_check_button_new_with_label ("Require <html> Tag before Parsing?");
  gtk_box_pack_start (GTK_BOX (vbox), html_tag_check, FALSE, FALSE, 0);
  gtk_widget_show (html_tag_check);

  symbol_replace_check = gtk_check_button_new_with_label ("Replace &sym; type Symbols?");
  gtk_box_pack_start (GTK_BOX (vbox), symbol_replace_check, FALSE, FALSE, 0);
  gtk_widget_show (symbol_replace_check);

  // Load Options into the interface:


  /* OPTIONS */
  if (strstr(preview_parse,"yes")!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pr_check),TRUE);
  if (strstr(viewer_parse,"yes")!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vi_check),TRUE);
  if (strstr(html_require,"yes")!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (html_tag_check),TRUE);
  if (strstr(symbol_replace,"yes")!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (symbol_replace_check),TRUE);

  switch (gnome_dialog_run (GNOME_DIALOG (window))) {
    case 0:
      preview_parse = "no";
      viewer_parse = "no";
      html_require = "no";
      symbol_replace = "no";
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (pr_check))) preview_parse = "yes";
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (vi_check))) viewer_parse = "yes";
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (html_tag_check))) html_require = "yes";
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (symbol_replace_check))) symbol_replace = "yes";
      
      // preview_parse = g_strdup (gtk_entry_get_text (GTK_ENTRY (pr_entry)));
      plugin_save_configuration (module->configfile);
      break;
  }
  gnome_dialog_close (GNOME_DIALOG (window));
  
  return NULL;
}
