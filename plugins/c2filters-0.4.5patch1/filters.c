/*  Cronos II Message Filters Plugin
 *  Copyright (C) 2001 Bosko Blagojevic
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
 **/

/* filters.c 
 * 
 * version 0.4.5-pre1
 * 
 * Filters plugin for CronosII
 *
 * By Bosko Blagojevic <falling@users.sourceforge.net>
 * Spring 2001
 * 
 * UNDER CONSTRUCTION!!
 * this is a hard hat area :)
 * 
 * Check README for a TODO list and more info
 **/

#include "../headers/message.h"
#include "../headers/plugin.h"
#include "../headers/init.h"
#include "../headers/utils.h"
#include "../headers/search.h"
#include <gnome.h>
#include <stdio.h>
#include <errno.h>
#include <fnmatch.h>
#include "../headers/main.h"

#ifndef FNM_CASEFOLD
	#define FNM_CASEFOLD 1<<4
#endif

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
	"CronosII Message Filters",
	"0.4.5-pre1",
	"Bosko Blagojevic <falling@users.sourceforge.net>",
	"http://cronosII.sourceforge.net/",
	"A plugin to implement filters support under CronosII."
};

typedef struct _C2Filter {
	gchar *notes;
	gchar *match;
	gchar *action;
} C2Filter;

typedef struct _C2FilterNewFields {
	gint edit;
	GtkWidget *window;
	GtkWidget *NameEntry;
	GtkWidget *DescriptionEntry;
	GtkWidget *MatchAllRadio;
	GtkWidget *MatchAnyRadio;
	GtkWidget *MatchTextEntry;
	GtkWidget *MatchWhereCombo;
	GtkWidget *MatchList;
	GtkWidget *CaseButton;
	GtkWidget *CopyButton;
	GtkWidget *CopyMailboxCombo;
	GtkWidget *MoveButton;
	GtkWidget *MoveMailboxCombo;
	GtkWidget *DeleteButton;
	GtkWidget *BeepButton;
} C2FilterNewFields;

/* format of the config file, (for upgrading) */
#define C2_PLUGIN_FILTERS_CONFIG_FORMAT 1

/* Function prototypes */
static void plugin_filters_load_config(const gchar *filename);
static void plugin_filters_save_config(const gchar *filename);
static gint plugin_filters_upgrade_config(const gchar *filename);
static void plugin_on_download_message(Message *message, gchar **mailbox);
static GtkWidget *plugin_filters_configure(C2DynamicModule *module);
static gboolean match_message(Message *message, const gchar *match);
static void act_upon_message(Message *message, gchar **mailbox, const gchar *action);
static gchar *filters_get_word(gint num, const gchar *str);
/* GUI Functions */
static gboolean plugin_filters_on_configure_close(GtkWidget *widget, GdkEvent *event, C2DynamicModule *module);
static void plugin_filters_fill_clist(void);
static void plugin_filters_on_configure_exit(GtkButton *widget, C2DynamicModule *module);
static gboolean plugin_filters_on_double_click_filter(GtkWidget *widget, GdkEventButton *event);
static void plugin_filters_on_filter_up_down(GtkButton *button, gchar *dir);
static void plugin_filters_on_new_filter(GtkButton *button, GtkWidget *widget);
static void plugin_filters_on_edit_filter(GtkButton *button, GtkWidget *widget);
static void plugin_filters_draw_edit_filter_window(/*GtkButton *button, GtkWidget *widget,*/ gint edit);
static void plugin_filters_edit_filter_on_cancel(GtkButton *button, GtkWidget *window);
static GtkWidget *plugin_filters_gui_build_mailbox_combo(void);
static void plugin_filters_edit_filter(GtkButton *button, C2FilterNewFields *fields);
static void plugin_filters_new_on_add_match_rule(GtkButton *button, C2FilterNewFields *fields);
static void plugin_filters_new_on_remove_match_rule(GtkButton *button, C2FilterNewFields *fields);
static void plugin_filters_new_on_move_button(GtkButton *button, C2FilterNewFields *fields);
static void plugin_filters_new_on_delete_button(GtkButton *button, C2FilterNewFields *fields);
static void plugin_filters_on_delete_filter(void);

/* Global variables */
static GList *filters = NULL;
static GtkWidget *filters_clist;
static GtkWidget *main_window;

/* Module Initializer */
char *
module_init (int major_version, int minor_version, int patch_version, C2DynamicModule *module) 
{
	C2Filter *temp;
	
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
	module->configure	= plugin_filters_configure;
	module->configfile	= c2_dynamic_module_get_config_file (module->name);

	/* Load the configuration */
	if(plugin_filters_upgrade_config(module->configfile))
	{
		cronos_gui_message(_("Your CronosII Filters configuration file has been\n"
			"upgraded to version 1! Please check the\n"
			"CronosII Filters Changlog for details on the\nimprovments."));
	}
	plugin_filters_load_config(module->configfile);
 
	/* Connect the signals */
	c2_dynamic_module_signal_connect (information[PLUGIN_NAME], C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP,
																		C2_DYNAMIC_MODULE_SIGNAL_FUNC (plugin_on_download_message));


	/* ~~~~~~~~~~~~ */
	
	return NULL;
}

static void 
plugin_filters_load_config(const gchar *filename)
{
	FILE *file;
	gchar *temp;
	C2Filter *filter;
	
	if(!(file = fopen(filename, "r"))) 
		return;
	
	/* read the config file format version lines */
	if(!(temp = fd_get_line(file)))
	{
		fclose(file);
		return;
	}
	
	for(;;) 
	{
		if(!(temp = fd_get_line(file))) 
			break;
		filter = g_new0(C2Filter, 1);
		filter->notes  = temp;
		filter->match  = fd_get_line(file);
		filter->action = fd_get_line(file);
		filters = g_list_append(filters, filter);
	}
	
	fclose(file);
}

static void 
plugin_filters_save_config(const gchar *filename)
{
	FILE *file;
	gint i;
	C2Filter *filter;

	if(!(file = fopen(filename, "w")))
		return;

	fwrite("~~~CronosII Filters Config File; Format 1\n", 42, 1, file);

	for(i=0; i < g_list_length(filters); i++) {
		filter = g_list_nth_data(filters, i);
		fwrite(filter->notes, strlen(filter->notes), 1, file);
		fwrite("\n", 1, 1, file);
		fwrite(filter->match, strlen(filter->match), 1, file);
		fwrite("\n", 1, 1, file);
		fwrite(filter->action, strlen(filter->action), 1, file);
		fwrite("\n", 1, 1, file);
	}
	
	fclose(file);
}

/* plguin_filters_upgrade_config
 * 
 * @filename: the filename of the config file to upgrade
 * 
 * This function will read the config file into memory with 
 * backwards compatibility for old config files, and then
 * recreate a new config file following the latest
 * correct format for CronosII Filters Config Files
 *
 * Return Value: 
 * 1 if the config file was upgraded, 0 otherwise
 */
static gint
plugin_filters_upgrade_config(const gchar *filename)
{
	FILE *file;
	gchar *temp = NULL;
	C2Filter *filter;
	gint i;
	gint format = 0;
	
	if(!(file = fopen(filename, "r"))) 
		return 0;
	
  if(!(temp = fd_get_line(file)))
	{
		fclose(file);
		return 0;
	}
	
	if(!strneq(temp, "~~~CronosII Filters Config File; Format ", 40))
	{
		fclose(file);
		if(!(file = fopen(filename, "r")))
			return 0;
	}
	else {
		temp += 40;
		format = atoi(temp);
		/*printf("format == %i\n", format);*/
		temp -= 40;
	}

	/* it it's up to date, don't waste our time on it */
	c2_free(temp);
	if(format == C2_PLUGIN_FILTERS_CONFIG_FORMAT) return 0;
	
	for(;;) 
	{
		if(!(temp = fd_get_line(file))) 
			break;
		filter = g_new0(C2Filter, 1);
		filter->notes  = temp;
		filter->match  = fd_get_line(file);
		filter->action = fd_get_line(file);
		filters = g_list_append(filters, filter);
	}
	fclose(file);
	
	/* now we go through checking which upgrades are necessary 
	 * depending on format # */

	if(format < 1) /* upgrade "match" rules, for case matching */
	{
		gint x;
		GString *gstr;

		for(i=0; i < g_list_length(filters); i++)
	  {
			gstr = g_string_new(NULL);
			
			filter = g_list_nth_data(filters, i);
			for(x=0; ;x++)
			{
				temp = filters_get_word(x, filter->match);
				if(!temp)
				{	
					c2_free(filter->match);
					c2_free(temp);
					filter->match = gstr->str;
					g_string_free(gstr, FALSE);
					break;
				}
				if(streq("AND", temp) || streq("OR", temp))
				{
					gstr = g_string_append(gstr, "~");
					gstr = g_string_append(gstr, temp);
					gstr = g_string_append(gstr, "~match");
				}
				else
				{
					if(x > 0) gstr = g_string_append(gstr, "~");
					gstr = g_string_append(gstr, temp);
				}
				c2_free(temp);
			}
		}
	}
	
	/* save our *upgraded* configuration! */
	plugin_filters_save_config(filename);
	
	/* clean up mah' shit */
	for(i=0; i < g_list_length(filters); i++)
	{
		filter = g_list_nth_data(filters, i);
		c2_free(filter->notes);
		c2_free(filter->match);
		c2_free(filter->action);
	}
	if(filters)
		g_list_free(filters);
	filters = NULL;
	return 1;
}


void 
module_cleanup (C2DynamicModule *module) 
{
	C2Filter *temp;
	gint i;
	g_return_if_fail (module);

	plugin_filters_save_config(module->configfile);
		
	for(i=0; i < g_list_length(filters); i++) 
	{
		temp = g_list_nth_data(filters, i);
		c2_free(temp->notes);
		c2_free(temp->match);
		c2_free(temp->action);
	}

	if(filters != NULL)
		g_list_free(filters);
	filters = NULL;
	c2_dynamic_module_signal_disconnect (module->name, C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP);
}

/* go trough each of the filtering rules, and if it finds a match,
 * apply the actions for that rule, and stop searching through the rules. */
static void 
plugin_on_download_message(Message *message, gchar **mailbox)
{
	C2Filter *filter;
	int i;
	
	for(i=0; i < g_list_length(filters); i++) 
	{
		filter = g_list_nth_data(filters, i);
		if(match_message(message, filter->match) == TRUE) {
			act_upon_message(message, mailbox, filter->action);
			break;
		}
	}
}

static gboolean 
match_message(Message *message, const gchar *match) 
{
	gchar *matchcommand, *header, *str, *mheader;
	gint i = 0;
	gboolean return_val = FALSE;
	
	for(i = 0; ;i += 4) 
	{
		matchcommand = filters_get_word(i, match);
		header = filters_get_word(i+1, match);
		str = filters_get_word(i+2, match);
		if(streq(header, _("Message Body"))) 
		{
			message->body = NULL;
			message_get_message_body(message, NULL);
			if(strneq(matchcommand, "matchcase", 9))
				if(message->body && fnmatch(str, message->body, 0) == 0)
					return_val = TRUE;
				else
					return_val = FALSE;
			else
				if(message->body && fnmatch(str, message->body, FNM_CASEFOLD))
					return_val = TRUE;
				else
					return_val = FALSE;
		}
		else if(streq(header, _("Entire Message")))
		{
			if(strneq(matchcommand, "matchcase", 9))
				if(fnmatch(str, message->message, 0) == 0)
					return_val = TRUE;
				else
					return_val = FALSE;
			else
				if(fnmatch(str, message->message, FNM_CASEFOLD) == 0)
					return_val = TRUE;
				else
					return_val = FALSE;
		}
		else {
			message_get_message_header(message, NULL);
			mheader = message_get_header_field(message, NULL, header);		
			/*printf("we are trying to match %s with %s\n", str, mheader);*/
			if(str && mheader) {
				if(strneq(matchcommand, "matchcase", 9)) {
					/*printf("matching with case sensitivity...");*/
					if(fnmatch(str, mheader, 0) == 0) {
						return_val = TRUE;
						/*printf("we have a match!\n");*/
					}
					/*else*/
						/*printf("no match\n");*/
				}
				else {
					/*printf("matching with case INsensitivity...");*/
					if(fnmatch(str, mheader, FNM_CASEFOLD) == 0) {
						/*printf("we have a match!\n");*/
						return_val = TRUE;
					}
					/*else*/
						/*printf("no match\n");*/
				}
			}
			c2_free(mheader);
		}
		c2_free(matchcommand);
		c2_free(header);
		c2_free(str);
		str = filters_get_word(i+3, match);
		if(strneq(str, "and", 3) && return_val == TRUE) {
			return_val = FALSE;
			c2_free(str);
		}
		else if(strneq(str, "or", 2))
		{
			c2_free(str);
			if(return_val == TRUE)
				break;
		}
		else
		{
			c2_free(str);
			break;
		}
	}
	
	return return_val;
}

/* function below still needs some more actions */
static void 
act_upon_message(Message *message, gchar **mailbox, const gchar *action) 
{
	gint i;
	gchar *act;
	
	for(i=1; ; i+=2) 
	{
		act =filters_get_word(i, action);
		if(strneq(act, "move", 4)) /* move the message */
		{
			gchar *to = filters_get_word(i+1, action);
			*mailbox = g_strdup(to);
			c2_free(to);
			i++;
		}
		else if(strneq(act, "delete", 6)) /* move message to garbage... */
			*mailbox = g_strdup("Garbage");
		else if(strneq(act, "beep", 4)) /* beep the message */
		{
			/* TODO: add sound functionality..? */
			printf("\a");
			fflush(NULL);
		}
		else if(strneq(act, "copy", 4)) /* copy message to another mailbox */
		{
			gchar *to = filters_get_word(i+1, action);
			gchar *buf;
			mid_t mid;
			FILE *file;
			Mailbox *mbox;
			
			i++;
			
			mbox = search_mailbox_name(config->mailbox_head, to);
			if(mbox)
			{
				mid = c2_mailbox_get_next_mid(mbox);
				buf = c2_mailbox_mail_path(to, mid);
				if((file = fopen(buf, "w")) != NULL)
				{
					c2_free(buf);
					fprintf(file, "%s", message->message);
					fclose(file);
					buf = c2_mailbox_index_path(to);
					if((file = fopen(buf, "a")) != NULL) 
					{
						gchar *header[3];
						gchar *content_type;
						gboolean with_attachs = FALSE;

						header[0] = message_get_header_field(message, NULL, "\nSubject:");
						header[1] = message_get_header_field(message, NULL, "\nFrom:");
						header[2] = message_get_header_field(message, NULL, "\nDate:");
						content_type = message_get_header_field(message, NULL, "\nContent-Type:");
						if(!header[0]) header[0] = "";
						if(!header[1]) header[1] = "";
						if(!header[2]) header[2] = "";
						if(!content_type) content_type = "";
						fprintf (file, "N\r\r%s\r%s\r%s\r%s\r%s\r%d\n",
								 with_attachs ? "1" : "", header[0], header[1], header[2],
								 "", mid);
						if(header[0] != "") c2_free(header[0]);
						if(header[1] != "") c2_free(header[1]);
						if(header[2] != "") c2_free(header[2]);
						if(content_type != "") c2_free(content_type);
						fclose(file);
					}
					else
						c2_free(buf);
				}
				else
					c2_free(buf);
			}
		}
		c2_free(act);
		act = filters_get_word(i+1, action);
		if(!strneq(act, "and", 3)) 
		{
			c2_free(act);
			break;
		}
		c2_free(act);
	}
}

/**
 * filters_get_word
 * @num: Position of the desired word.
 * @str: A pointer to a string containing the target.
 *
 * Finds the word in position @num. This function is
 * meant to evaluate filters descriptions, match, or action
 * strings. Fields are seperated by the character '~'
 * except when escaped with a '/'. '/' itself may be
 * escaped with an additional '/' placed directly 
 * before it.
 *
 * Return Value:
 * A freeable string containing the desired word or
 * NULL in case it doesn't exists.
 **/
gchar *
filters_get_word(gint num, const gchar *str)
{
	guint i;
	GString *gstr = NULL;
	gchar *return_val;
	gboolean escape = FALSE;
	
	if(!str)
		return NULL;
	
	gstr = g_string_new(NULL);
	
	for(i = 0; i < strlen(str); i++)
	{
		if(str[i] == '/' && !escape) escape = TRUE;
		else if(str[i] == '/' && escape) {
			escape = FALSE;
			if(num == 0)
				g_string_append_c(gstr, str[i]);
		}
		else if(str[i] == '~' && !escape) num--;
		else if(str[i] == '~' && escape) {
			escape = FALSE;
			if(num == 0)
				g_string_append_c(gstr, str[i]);
		}
		else if(num == 0) gstr = g_string_append_c(gstr, str[i]);
		else if(num < 0) break;
	}
	
	return_val = gstr->str;
	if(gstr->len > 0)
	{	
		g_string_free(gstr, FALSE);
		return return_val;
	}
	else
	{
		g_string_free(gstr, TRUE);
		return NULL;
	}
}

/* ------------- */
/* GUI Functions */
/* ------------- */
static GtkWidget *
plugin_filters_configure(C2DynamicModule *module)
{
	GtkWidget *window;
	GtkWidget *dock1;
	GtkWidget *toolbar1;
	GtkWidget *tmp_toolbar_icon;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	GtkWidget *button4;
	GtkWidget *button5;
	GtkWidget *button6;
	GtkWidget *scrolledwindow1;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *appbar1;
	
	window = gnome_app_new ("CronosII Message Filters", _("CronosII Message Filters"));
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
										 GTK_SIGNAL_FUNC(plugin_filters_on_configure_close), module);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
	gtk_widget_show(window);
	
	dock1 = GNOME_APP (window)->dock;
	gtk_widget_show (dock1);

	toolbar1 = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gtk_widget_show (toolbar1);
	gnome_app_add_toolbar(GNOME_APP (window), GTK_TOOLBAR (toolbar1), "toolbar1",
												GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
												GNOME_DOCK_TOP, 1, 0, 0);
	gtk_container_set_border_width (GTK_CONTAINER (toolbar1), 1);
	gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar1), 16);
	gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar1), GTK_TOOLBAR_SPACE_LINE);
	gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar1), GTK_RELIEF_NONE);

	tmp_toolbar_icon = gnome_stock_pixmap_widget (window, GNOME_STOCK_PIXMAP_UP);
	button5 = gtk_toolbar_append_element (GTK_TOOLBAR(toolbar1),
																				GTK_TOOLBAR_CHILD_BUTTON,
																				NULL,
																				_("Up"),
																				_("Move rule up in rule checking order"), NULL,
																				tmp_toolbar_icon, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT (button5), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_filter_up_down), "up");
	gtk_widget_show(button5);
	
	tmp_toolbar_icon = gnome_stock_pixmap_widget (window, GNOME_STOCK_PIXMAP_DOWN);
	button6 = gtk_toolbar_append_element (GTK_TOOLBAR(toolbar1),
																				GTK_TOOLBAR_CHILD_BUTTON,
																				NULL,
																				_("Down"),
																				_("Move rule down in rule checking order"), NULL,
																				tmp_toolbar_icon, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT (button6), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_filter_up_down), "down");
	gtk_widget_show(button6);
	
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar1));
	
	tmp_toolbar_icon = gnome_stock_pixmap_widget (window, GNOME_STOCK_PIXMAP_NEW);
	button1 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
																				GTK_TOOLBAR_CHILD_BUTTON,
																				NULL,
																				_("New"),
																				_("New Filtering Rule"), NULL,
																				tmp_toolbar_icon, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(button1), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_new_filter), window);
	gtk_widget_show(button1);

	tmp_toolbar_icon = gnome_stock_pixmap_widget (window, GNOME_STOCK_PIXMAP_PROPERTIES);
	button2 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
																				GTK_TOOLBAR_CHILD_BUTTON,
																				NULL,
																				_("Edit"),
																				_("Edit Filtering Rule"), NULL,
																				tmp_toolbar_icon, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(button2), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_edit_filter), window);
	gtk_widget_show(button2);
	
	tmp_toolbar_icon = gnome_stock_pixmap_widget (window, GNOME_STOCK_PIXMAP_CLOSE);
	button3 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
																				GTK_TOOLBAR_CHILD_BUTTON,
																				NULL,
																				_("Delete"),
																				_("Delete Filtering Rule"), NULL,
																				tmp_toolbar_icon, NULL, NULL);
	gtk_widget_show(button3);
	
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar1));
	
	tmp_toolbar_icon = gnome_stock_pixmap_widget(window, GNOME_STOCK_PIXMAP_EXIT);
	button4 = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
																			 GTK_TOOLBAR_CHILD_BUTTON,
																			 NULL,
																			 _("Exit"),
																			 _("Exit Filters Configuration"), NULL,
																			 tmp_toolbar_icon, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(button4), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_configure_exit), module);
	gtk_widget_show(button4);
	
	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gnome_app_set_contents (GNOME_APP (window), scrolledwindow1);
	
	filters_clist = gtk_clist_new(2);
	gtk_widget_show (filters_clist);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), filters_clist);
	gtk_clist_set_column_width (GTK_CLIST (filters_clist), 0, 110);
	gtk_clist_set_column_width (GTK_CLIST (filters_clist), 1, 450);
	gtk_clist_column_titles_show (GTK_CLIST (filters_clist));
	plugin_filters_fill_clist();
	gtk_signal_connect(GTK_OBJECT(button3), "clicked",
										 GTK_SIGNAL_FUNC(plugin_filters_on_delete_filter), NULL);
	
	label1 = gtk_label_new (_("Name"));
	gtk_widget_show (label1);
	gtk_clist_set_column_widget (GTK_CLIST (filters_clist), 0, label1);
	
	label2 = gtk_label_new (_("Description"));
	gtk_widget_show (label2);
	gtk_clist_set_column_widget (GTK_CLIST (filters_clist), 1, label2);
	
	appbar1 = gnome_appbar_new (TRUE, TRUE, GNOME_PREFERENCES_NEVER);
	gtk_widget_show (appbar1);
	gnome_app_set_statusbar (GNOME_APP (window), appbar1);
	
	gtk_signal_connect(GTK_OBJECT(filters_clist), "button_press_event", 
										 GTK_SIGNAL_FUNC(plugin_filters_on_double_click_filter), 
										 NULL);
	gtk_signal_connect(GTK_OBJECT(filters_clist), "button_release_event",
										 GTK_SIGNAL_FUNC(plugin_filters_on_double_click_filter), 
                     NULL);
	
	main_window = window;
	return window;
}

static 
gboolean plugin_filters_on_configure_close(GtkWidget *widget, GdkEvent *event,
																					 C2DynamicModule *module)
{
	plugin_filters_save_config(module->configfile);
	gtk_widget_destroy(widget);
	return TRUE;
}

static void 
plugin_filters_on_configure_exit(GtkButton *widget, C2DynamicModule *module)
{
	plugin_filters_save_config(module->configfile);
	gtk_widget_destroy(main_window);
}

static gboolean
plugin_filters_on_double_click_filter(GtkWidget *widget, GdkEventButton *event)
{
	if ((event->type==GDK_2BUTTON_PRESS) &&
			g_list_length(GTK_CLIST(widget)->selection) > 0)
	{
		gint i = GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(filters_clist)->selection, 0));
		plugin_filters_draw_edit_filter_window(i);
	}
	
	return FALSE;
}

static void 
plugin_filters_fill_clist(void)
{
	gint i;
	C2Filter *filter;
	gchar *notes[2];
	
	gtk_clist_freeze(GTK_CLIST(filters_clist));
	gtk_clist_clear(GTK_CLIST(filters_clist));
	
	for(i=0; i < g_list_length(filters); i++) 
	{
		filter = g_list_nth_data(filters, i);
		notes[0] = filters_get_word(0, filter->notes);
		notes[1] = filters_get_word(1, filter->notes);
		gtk_clist_append(GTK_CLIST(filters_clist), notes);
		c2_free(notes[0]);
		c2_free(notes[1]);
	}
	
	gtk_clist_thaw(GTK_CLIST(filters_clist));
}

static void 
plugin_filters_on_filter_up_down(GtkButton *button, gchar *dir)
{
	GList *node;
  C2Filter *filter;
	gint i = GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(filters_clist)->selection, 0));
	
	if(g_list_length(GTK_CLIST(filters_clist)->selection) != 1)
		return;
	
	if((streq(dir, "up")) && i == 0)
		return;
	else if((streq(dir, "down")) && ((i + 1) == g_list_length(filters)))
		return;
	
	node = g_list_nth(filters, i);
 	filter = node->data;
	filters = g_list_remove_link(filters, node);
	if(streq(dir, "up"))
		filters = g_list_insert(filters, filter, i - 1);
	else
		filters = g_list_insert(filters, filter, i + 1);
	plugin_filters_fill_clist();
	if(streq(dir, "up"))
		gtk_clist_select_row(GTK_CLIST(filters_clist), i - 1, 0);
	else
		gtk_clist_select_row(GTK_CLIST(filters_clist), i + 1, 0);
}

static void 
plugin_filters_on_new_filter(GtkButton *button, GtkWidget *widget)
{
	plugin_filters_draw_edit_filter_window(-1);
}

static void 
plugin_filters_on_edit_filter(GtkButton *button, GtkWidget *widget)
{
	if (!(GTK_CLIST(filters_clist)->selection) || 
			g_list_length(GTK_CLIST(filters_clist)->selection) != 1)  
		return;
	else
	{
		gint i;
		i = GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(filters_clist)->selection, 0));
		plugin_filters_draw_edit_filter_window(i);
	}
}

static void 
plugin_filters_draw_edit_filter_window(gint edit)
{
	GtkWidget *notebook1;
	GtkWidget *page1;
	GtkWidget *page1_label;
	GtkWidget *table1;
	GtkWidget *page2;
	GtkWidget *page2_label;
	GtkWidget *scrolledwindow1;
 	GtkWidget *vbox1;
	GtkWidget *hbox1;
 	GtkWidget *hbox2;
	GtkWidget *hbox3;
	GtkWidget *hbox4;
	GtkWidget *hbox5;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *combo1;
	GtkWidget *AddButton;
	GtkWidget *RemoveButton;
	GtkWidget *frame1;
	GtkWidget *framevbox;
	GtkWidget *framehbox1;
	GtkWidget *framehbox2;
	GtkWidget *OkButton;
	GtkWidget *CancelButton;

	C2FilterNewFields *filter_fields;
	filter_fields = g_new0(C2FilterNewFields, 1);
	filter_fields->edit = edit;

	filter_fields->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (filter_fields->window), 5);
	gtk_window_set_default_size(GTK_WINDOW(filter_fields->window), 425, 350);
	gtk_window_set_title (GTK_WINDOW (filter_fields->window), _("New Filter"));

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (filter_fields->window), vbox1);
	
	table1 = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 5);
	gtk_box_pack_start (GTK_BOX(vbox1), table1, FALSE, FALSE, 0);
	gtk_widget_show(table1);

	label1 = gtk_label_new (_("Filter Name"));
	gtk_table_attach_defaults(GTK_TABLE(table1), label1, 0, 1, 0, 1);
	gtk_widget_show (label1);

	filter_fields->NameEntry = gtk_entry_new_with_max_length (256);
	gtk_table_attach_defaults(GTK_TABLE(table1), filter_fields->NameEntry, 1, 2, 0, 1);
	gtk_widget_show (filter_fields->NameEntry);

	label2 = gtk_label_new (_("Description"));
	gtk_table_attach_defaults(GTK_TABLE(table1), label2, 0, 1, 1, 2);
	gtk_widget_show (label2);

	filter_fields->DescriptionEntry = gtk_entry_new ();
	gtk_table_attach_defaults(GTK_TABLE(table1), filter_fields->DescriptionEntry, 1, 2, 1, 2);
	gtk_widget_show (filter_fields->DescriptionEntry);
	
	notebook1 = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook1), GTK_POS_BOTTOM);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook1, TRUE, TRUE, 0);
	gtk_widget_show(notebook1);
	
	page1 = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(page1), 5);
	gtk_widget_show (page1);
	
	page1_label = gtk_label_new(_("Match Rules"));
	gtk_widget_show(page1_label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), page1, page1_label);
	
	page2 = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(page2), 5);
	gtk_widget_show(page2);
	
	frame1 = gtk_frame_new (_("Actions"));
	gtk_widget_show (frame1);
	gtk_box_pack_start (GTK_BOX (page2), frame1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);
	
	framevbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(frame1), framevbox);
	gtk_widget_show(framevbox);
	
	framehbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX(framevbox), framehbox1,
											FALSE, FALSE, 0);
	gtk_widget_show(framehbox1);
	
	filter_fields->CopyButton = gtk_check_button_new_with_label(_("Copy message to: "));
	gtk_box_pack_start (GTK_BOX(framehbox1), filter_fields->CopyButton,
											FALSE, FALSE, 0);
	gtk_widget_show(filter_fields->CopyButton);
	
	filter_fields->CopyMailboxCombo = plugin_filters_gui_build_mailbox_combo();
	gtk_box_pack_start (GTK_BOX(framehbox1), filter_fields->CopyMailboxCombo,
											TRUE, TRUE, 5);
	gtk_widget_show(filter_fields->CopyMailboxCombo);
	
	framehbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX(framevbox), framehbox2,
											FALSE, FALSE, 0);
	gtk_widget_show(framehbox2);
	
	filter_fields->MoveButton = gtk_check_button_new_with_label(_("Move message to: "));
	gtk_signal_connect (GTK_OBJECT(filter_fields->MoveButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_new_on_move_button), filter_fields);
	gtk_box_pack_start (GTK_BOX(framehbox2), filter_fields->MoveButton,
											FALSE, FALSE, 0);
	gtk_widget_show(filter_fields->MoveButton);
	
	filter_fields->MoveMailboxCombo = plugin_filters_gui_build_mailbox_combo();
	gtk_box_pack_start (GTK_BOX(framehbox2), filter_fields->MoveMailboxCombo,
											TRUE, TRUE, 5);
	gtk_widget_show(filter_fields->MoveMailboxCombo);
	
	filter_fields->DeleteButton = gtk_check_button_new_with_label(_("Delete Message"));
	gtk_signal_connect (GTK_OBJECT(filter_fields->DeleteButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_new_on_delete_button), filter_fields);
	gtk_box_pack_start (GTK_BOX(framevbox), filter_fields->DeleteButton,
											FALSE, FALSE, 0);
	gtk_widget_show(filter_fields->DeleteButton);
	
	filter_fields->BeepButton = gtk_check_button_new_with_label(_("Produce a System Beep"));
	gtk_box_pack_start (GTK_BOX(framevbox), filter_fields->BeepButton,
											FALSE, FALSE, 0);
	gtk_widget_show(filter_fields->BeepButton);
	
	page2_label = gtk_label_new(_("Actions"));
	gtk_widget_show(page2_label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), page2, page2_label);
	
	/* matchhhh*/
	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX (page1), hbox1, FALSE, FALSE, 0);

	label3 = gtk_label_new(_("Match:"));
	gtk_widget_show(label3);
	gtk_box_pack_start(GTK_BOX (hbox1), label3, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC (label3), 5, 0);

	filter_fields->MatchTextEntry = gtk_entry_new_with_max_length(256);
	gtk_widget_show(filter_fields->MatchTextEntry);
	gtk_box_pack_start(GTK_BOX (hbox1), filter_fields->MatchTextEntry, TRUE, TRUE, 0);

	label4 = gtk_label_new(_("in"));
	gtk_widget_show (label4);
	gtk_box_pack_start (GTK_BOX (hbox1), label4, FALSE, FALSE, 0);
	gtk_misc_set_padding (GTK_MISC (label4), 5, 0);

	{	
		GList *headers = NULL;
		headers = g_list_append(headers, "To:");
		headers = g_list_append(headers, "CC:");
		headers = g_list_append(headers, "From:");
		headers = g_list_append(headers, "Reply-To:");
		headers = g_list_append(headers, "Date:");
		headers = g_list_append(headers, "Subject:");
		headers = g_list_append(headers, _("Message Body"));
		headers = g_list_append(headers, _("Entire Message"));

		combo1 = gtk_combo_new ();
		gtk_combo_set_popdown_strings(GTK_COMBO(combo1), headers);
		gtk_combo_set_value_in_list(GTK_COMBO(combo1), TRUE, FALSE);		
		gtk_widget_show (combo1);
		gtk_box_pack_start (GTK_BOX (hbox1), combo1, TRUE, TRUE, 0);
		
		g_list_free(headers);
	}
		
	filter_fields->MatchWhereCombo = GTK_COMBO (combo1)->entry;
	gtk_widget_show (filter_fields->MatchWhereCombo);
	
	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page1), hbox2, FALSE, FALSE, 0);
	gtk_widget_show(hbox2);

	filter_fields->CaseButton = gtk_check_button_new_with_label(_("Case Sensitive"));
	gtk_container_set_border_width(GTK_CONTAINER(filter_fields->CaseButton), 3);
	gtk_box_pack_start (GTK_BOX (hbox2), filter_fields->CaseButton, FALSE, FALSE, 0);
	gtk_widget_show(filter_fields->CaseButton);
	
	AddButton = gtk_button_new_with_label (_("Add"));
	gtk_container_set_border_width(GTK_CONTAINER(AddButton), 10);
	gtk_signal_connect (GTK_OBJECT(AddButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_new_on_add_match_rule), filter_fields);
	gtk_box_pack_start (GTK_BOX (hbox2), AddButton, TRUE, TRUE, 0);
	gtk_widget_show(AddButton);

	hbox3 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page1), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);
		
	filter_fields->MatchAllRadio = gtk_radio_button_new_with_label(NULL, _("Match All Rules"));
	gtk_box_pack_start(GTK_BOX(hbox3), filter_fields->MatchAllRadio, TRUE, TRUE, 0);
	gtk_widget_show(filter_fields->MatchAllRadio);
	
	filter_fields->MatchAnyRadio = 
		gtk_radio_button_new_with_label(gtk_radio_button_group
																		(GTK_RADIO_BUTTON(filter_fields->MatchAllRadio)),
																		_("Match Any Rules"));
	gtk_box_pack_start(GTK_BOX(hbox3), filter_fields->MatchAnyRadio, TRUE, TRUE, 0);
	gtk_widget_show(filter_fields->MatchAnyRadio);
	
	
	hbox4 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (page1), hbox4, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(hbox4), scrolledwindow1, TRUE, TRUE, 0);
	gtk_widget_show(scrolledwindow1);
	
	{
		char *MatchListTitles[] = { _("Case"), _("Field"), _("Match String") };
		
		filter_fields->MatchList = gtk_clist_new_with_titles(3, MatchListTitles);
		gtk_clist_set_column_width(GTK_CLIST(filter_fields->MatchList), 0, 30);
		gtk_clist_set_column_width(GTK_CLIST(filter_fields->MatchList), 1, 150);
		gtk_clist_set_column_width(GTK_CLIST(filter_fields->MatchList), 2, 50);
		gtk_widget_show (filter_fields->MatchList);
		gtk_container_add(GTK_CONTAINER(scrolledwindow1), filter_fields->MatchList);
	}
  
	RemoveButton = gtk_button_new_with_label (_("Remove Match"));
	gtk_container_set_border_width(GTK_CONTAINER(RemoveButton), 3);
	gtk_box_pack_start (GTK_BOX (page1), RemoveButton, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT(RemoveButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_new_on_remove_match_rule), filter_fields);
	gtk_widget_show (RemoveButton);
	
	hbox5 = gtk_hbox_new (TRUE, 10);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox5, FALSE, FALSE, 3);
	gtk_widget_show (hbox5);
	
	OkButton = gnome_stock_button(GNOME_STOCK_BUTTON_OK);
	gtk_box_pack_start (GTK_BOX(hbox5), OkButton,
											TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT(OkButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_edit_filter), filter_fields);
	gtk_widget_show(OkButton);
	
	CancelButton = gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
	gtk_box_pack_start (GTK_BOX(hbox5), CancelButton,
											TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT(CancelButton), "clicked",
											GTK_SIGNAL_FUNC(plugin_filters_edit_filter_on_cancel), 
											filter_fields->window);
	gtk_widget_show (CancelButton);
	
	gtk_window_set_transient_for (GTK_WINDOW (main_window), GTK_WINDOW (filter_fields->window));
	gtk_window_set_modal (GTK_WINDOW (main_window), FALSE);
	gtk_window_set_modal (GTK_WINDOW (filter_fields->window), TRUE);
	
	/* if this is a new filter... we're done */
	if(edit < 0)
	{
			gtk_widget_show(filter_fields->window);
			return;
	}
	/* otherwise keep going to fill in the fields */
	else
	{
		C2Filter *filter;
		gchar *temp;
		gchar *list[3] = { NULL, NULL, NULL };
		gint i;
			
		filter = g_list_nth_data(filters, edit);
		temp = filters_get_word(0, filter->notes);
		if(temp)
			gtk_entry_set_text(GTK_ENTRY(filter_fields->NameEntry), temp);
		c2_free(temp);
		temp = filters_get_word(1, filter->notes);
		if(temp)
			gtk_entry_set_text(GTK_ENTRY(filter_fields->DescriptionEntry), temp);
		c2_free(temp);
		temp = filters_get_word(3, filter->match);
		if(temp && streq(temp, "or"))
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->MatchAnyRadio), TRUE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->MatchAllRadio), TRUE);
		
		/* fill in the match list */
		for(i=0; ;i+=4)
		{
			temp = filters_get_word(i, filter->match);
			if(temp && streq(temp, "matchcase"))
				list[0] = g_strdup("case");
			else
				list[0] = g_strdup(" ");
			c2_free(temp);
			temp = filters_get_word(i+1, filter->match);
			if(temp && strlen(temp) > 0)
				list[1] = temp;
			temp = filters_get_word(i+2, filter->match);
			if(temp && strlen(temp) > 0)
				list[2] = temp;
			if(list[0] && list[1] && list[2])
				gtk_clist_append(GTK_CLIST(filter_fields->MatchList), list);
			/*else
				printf("YO! no good drawing match list\n");*/
			c2_free(list[0]);
			c2_free(list[1]);
			c2_free(list[2]);
			temp = filters_get_word(i+3, filter->match);
			if(temp && (streq(temp, "and") || streq(temp, "or")))
			{
				c2_free(temp); /* carry on.. */
			}
			else
			{
				c2_free(temp); /* bust out... */
				break;
			}
		}
		
		/* check off the right action buttons */
		for(i=1; ; i+=2)
		{
			temp =filters_get_word(i, filter->action);
			if(strneq(temp, "copy", 4))
			{
				gchar *to = filters_get_word(i+1, filter->action);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->CopyButton), TRUE);
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(filter_fields->CopyMailboxCombo)->entry), to);
				c2_free(to);
				i++;
			}
			else if(strneq(temp, "move", 4)) 
			{
				gchar *to = filters_get_word(i+1, filter->action);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->MoveButton), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->DeleteButton), FALSE);
				gtk_widget_set_sensitive(filter_fields->DeleteButton, FALSE);
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(filter_fields->MoveMailboxCombo)->entry), to);
				c2_free(to);
				i++;
			}
			else if(strneq(temp, "delete", 6))
			{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->DeleteButton), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->MoveButton), FALSE);
				gtk_widget_set_sensitive(filter_fields->MoveButton, FALSE);
				gtk_widget_set_sensitive(filter_fields->MoveMailboxCombo, FALSE);
			}
			else if(strneq(temp, "beep", 4))
			{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filter_fields->BeepButton), TRUE);
			}
			c2_free(temp);
			temp = filters_get_word(i+1, filter->action);
			if(!strneq(temp, "and", 3))
			{
				c2_free(temp);
				break;
			}
			c2_free(temp);
		}
		
		gtk_widget_show(filter_fields->window);
	}
}

static void
plugin_filters_edit_filter_on_cancel(GtkButton *button, GtkWidget *window)
{
	gtk_widget_destroy(window);
}

static GtkWidget *
plugin_filters_gui_build_mailbox_combo(void)
{
	GtkWidget *combo;
	GList *mailbox_list = NULL;
	Mailbox *box = config->mailbox_head;
	
	while(box)
	{
		mailbox_list = g_list_append(mailbox_list, box->name);
		box = box->next;
	}
		 
	combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), mailbox_list);
	gtk_combo_set_value_in_list(GTK_COMBO(combo), TRUE, FALSE);
	g_list_free(mailbox_list);
	
	return combo;
}

static void 
plugin_filters_new_on_add_match_rule (GtkButton *button, 
																			C2FilterNewFields *fields)
{
	gchar *temp[3];
	if(GTK_TOGGLE_BUTTON(fields->CaseButton)->active == TRUE)
		temp[0] = g_strdup("case");
	else
		temp[0] = g_strdup(" ");
	temp[1] = gtk_editable_get_chars(GTK_EDITABLE(fields->MatchWhereCombo), 0, -1);
	temp[2] = gtk_editable_get_chars(GTK_EDITABLE(fields->MatchTextEntry), 0, -1);
	
	gtk_clist_append(GTK_CLIST(fields->MatchList), temp);
	c2_free(temp[0]);
	c2_free(temp[1]);
	c2_free(temp[2]);
}

static void 
plugin_filters_new_on_remove_match_rule(GtkButton *button, 
																				C2FilterNewFields *fields)
{
	gint i;
	i = GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(fields->MatchList)->selection, 0));
	if(GTK_CLIST(fields->MatchList)->selection)
		gtk_clist_remove(GTK_CLIST(fields->MatchList), i);
}

static void 
plugin_filters_new_on_delete_button(GtkButton *button,
																		C2FilterNewFields *fields)
{
	if(GTK_TOGGLE_BUTTON(fields->DeleteButton)->active == TRUE)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fields->MoveButton), FALSE);
		gtk_widget_set_sensitive(fields->MoveButton, FALSE);
		gtk_widget_set_sensitive(fields->MoveMailboxCombo, FALSE);
	}
	else
  {
		gtk_widget_set_sensitive(fields->MoveButton, TRUE);
		gtk_widget_set_sensitive(fields->MoveMailboxCombo, TRUE);
	}
}

static void 
plugin_filters_new_on_move_button(GtkButton *button,
																	C2FilterNewFields *fields)
{
	if(GTK_TOGGLE_BUTTON(fields->MoveButton)->active == TRUE)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fields->DeleteButton), FALSE);
		gtk_widget_set_sensitive(fields->DeleteButton, FALSE);
	}
	else
		gtk_widget_set_sensitive(fields->DeleteButton, TRUE);
}

static void 
plugin_filters_edit_filter(GtkButton *button, C2FilterNewFields *fields)
{
	gchar *str1, *str2, *str3, *tempstr;
	gint i;
	C2Filter *filter;
	GString *gstr;
	
	filter = g_new0(C2Filter, 1);
	
	/* the name + description */
	str1 = gtk_editable_get_chars(GTK_EDITABLE(fields->NameEntry), 0, -1);
	str2 = gtk_editable_get_chars(GTK_EDITABLE(fields->DescriptionEntry), 0, -1);
	str1 = str_replace_all(str1, "/", "//");
	tempstr = str1;
	str1 = str_replace_all(str1, "~", "/~");
	c2_free(tempstr);
	str2 = str_replace_all(str2, "/", "//");
	tempstr = str2;
	str2 = str_replace_all(str2, "~", "/~");
	c2_free(tempstr);
	filter->notes = g_strconcat(str1, "~", str2, NULL);
	c2_free(str1);
	c2_free(str2);
	
	/* the matching rules */
	gstr = g_string_new(NULL);
	
	for(i=0; i < GTK_CLIST(fields->MatchList)->rows; i++)
	{
		gtk_clist_get_text(GTK_CLIST(fields->MatchList), i, 0, &str3);
		gtk_clist_get_text(GTK_CLIST(fields->MatchList), i, 1, &str2);
		gtk_clist_get_text(GTK_CLIST(fields->MatchList), i, 2, &str1);
		tempstr = str2;
		str2 = str_replace_all(str2, "/", "//");
		c2_free(tempstr);
		tempstr = str2;
		str2 = str_replace_all(str2, "~", "/~");
		c2_free(tempstr);
		tempstr = str1;
		str1 = str_replace_all(str1, "/", "//");
		c2_free(tempstr);
		tempstr = str1;
		str1 = str_replace_all(str1, "~", "/~");
		c2_free(tempstr);
		/*printf("and str3 == %s\n", str3);*/
		if(streq(str3, "case"))
			gstr = g_string_append(gstr, "matchcase~");
		else
			gstr = g_string_append(gstr, "match~");
		gstr = g_string_append(gstr, str2);
		gstr = g_string_append(gstr, "~");
		gstr = g_string_append(gstr, str1);
		c2_free(str1);
		c2_free(str2);
		/* if(str3)c2_free(str3);  don't know why, but this shouldn't be freed? */
		if(i+1 < GTK_CLIST(fields->MatchList)->rows)
		{
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fields->MatchAllRadio)))
				gstr = g_string_append(gstr, "~AND~");
			else
				gstr = g_string_append(gstr, "~OR~");
		}
	}

	filter->match = gstr->str;
	g_string_free(gstr, FALSE);
	
	gstr = g_string_new("action~");

	/* the actions to take on messages */
	if(GTK_TOGGLE_BUTTON(fields->CopyButton)->active == TRUE)
	{
		gchar *mbox = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(fields->CopyMailboxCombo)->entry), 0, -1);
		gstr = g_string_append(gstr, "copy~");
		gstr = g_string_append(gstr, mbox);
	}
	if(GTK_TOGGLE_BUTTON(fields->MoveButton)->active == TRUE)
	{
		gchar *mbox = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(fields->MoveMailboxCombo)->entry), 0, -1);
		if(gstr->len > 7)
			gstr = g_string_append(gstr, "~AND~");
		gstr = g_string_append(gstr, "move~");
		gstr = g_string_append(gstr, mbox);
	}
	if(GTK_TOGGLE_BUTTON(fields->DeleteButton)->active == TRUE)
	{
		if(gstr->len > 7)
			gstr = g_string_append(gstr, "~AND~");
		gstr = g_string_append(gstr, "delete");
	}
	if(GTK_TOGGLE_BUTTON(fields->BeepButton)->active == TRUE)
	{
		if(gstr->len > 7) 
			gstr = g_string_append(gstr, "~AND~");
		gstr = g_string_append(gstr, "beep");
	}
  
	filter->action = gstr->str;
	g_string_free(gstr, FALSE);

	gtk_widget_destroy(fields->window);
	
	if(fields->edit < 0)
		filters = g_list_append(filters, filter);
	else
	{
		GList *link;
		C2Filter *old_filter;
		
		link = g_list_nth(filters, fields->edit);
		filters = g_list_remove_link(filters, link);
		
		old_filter = link->data;
		if(old_filter->notes) c2_free(old_filter->notes);
		if(old_filter->match) c2_free(old_filter->match);
		if(old_filter->action) c2_free(old_filter->action);
		g_list_free_1(link);
		
		filters = g_list_insert(filters, filter, fields->edit);
	}
	
	gtk_widget_destroy(fields->window);
	plugin_filters_fill_clist();
}

static void 
plugin_filters_on_delete_filter(void)
{
	GList *temp;
	gint i;
	
	if(!(GTK_CLIST(filters_clist)->selection) || g_list_length(GTK_CLIST(filters_clist)->selection) != 1)
		return;
	else
	{
		i = GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(filters_clist)->selection, 0));
		temp = g_list_nth(filters, i);
		filters = g_list_remove_link(filters, temp);
		c2_free(temp->data);
		g_list_free(temp);
		gtk_clist_remove(GTK_CLIST(filters_clist), i);
	}
}
