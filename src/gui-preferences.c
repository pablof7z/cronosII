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
#else
#  define SPOOL_MAIL_DIR "/var"
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "gui-preferences.h"
#include "gui-window_main.h"
#include "gui-window_main_callbacks.h"
#include "gui-mailbox_tree.h"
#include "gui-utils.h"
#include "gui-select_file.h"

#include "account.h"
#include "mailbox.h"
#include "init.h"
#include "rc.h"
#include "debug.h"
#include "search.h"
#include "utils.h"
#include "error.h"
#include "init.h"
#include "main.h"
#include "utils.h"
#include "exit.h"
#include "check.h"
#if USE_PLUGINS
#  include "plugin.h"
#endif

#define prefs_append_page(box, text, node) \
{ \
  char *label[2] = { g_strdup (text), NULL }; \
  gtk_ctree_insert_node (GTK_CTREE (sections_ctree), node, NULL, label, 4, \
      	NULL, NULL, NULL, NULL, FALSE, TRUE);\
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), box, gtk_label_new (text));\
}

#define QUICK_HELP(txt, cnst) gtk_text_insert (GTK_TEXT (txt), NULL, NULL, NULL, cnst, -1)
#define QUICK_HELP_NEW_ACCOUNT	_("Cronos II has support for\n" \
				"many protocols. This is the\n" \
				"moment for you to decide\n" \
				"which type of account you're\n" \
				"going to use with Cronos II.\n" \
				"No matter what account you\n" \
				"choose to use right now, you\n" \
				"will be able to configure\n" \
				"other accounts of different\n" \
				"type later.\n" \
				"Clicking in the items of the\n" \
				"list will show you some help\n" \
				"on each protocol.\nEnjoy.")
#define QUICK_HELP_SPOOL	_("Spool is the most common\n" \
				"account type in UNIX boxes.\n" \
				"It format resides in a file\n" \
				"commonly located under the\n" \
				"/var directory.\n" \
				"Cronos II has full support\n" \
				"for this protocol.")
#define QUICK_HELP_POP		_("POP is the most common\n" \
				"account type in the Internet.\n" \
				"It format resides in the\n" \
				"authentication on a server.\n" \
				"Cronos II has full suport\n" \
				"for this protocol.")

static void on_prefs_sections_clist_select_row (GtkWidget *widget, gint row);
static void on_prefs_ok_btn_clicked (GtkWidget *, gpointer);
static void on_prefs_previous_btn_clicked (void);
static void on_prefs_next_btn_clicked (void);
static void on_prefs_cancel_btn_clicked (void);
static void gui_destroy_data (GtkWidget *widget, gpointer data);

static void prefs_add_page_logo (void);
static void prefs_add_page_accounts (void);
/*static void prefs_add_page_filters (void);*/
static void prefs_add_page_options (void);
static void prefs_add_page_interface (void);
static void prefs_add_page_fonts (void);
static void prefs_add_page_colors (void);
static void prefs_add_page_mailboxes (void);
static void prefs_add_page_plugins (void);
static void prefs_add_page_advanced (void);

static GtkWidget *window;
static GtkWidget *sections_ctree;
static GtkCTreeNode *ctree_general;
static GtkCTreeNode *ctree_mail;
static GtkCTreeNode *ctree_interface;
static GtkWidget *notebook;
static GtkWidget *browse_prev_btn;
static GtkWidget *browse_next_btn;
static GtkAccelGroup *accel_group;
static GtkTooltips *tooltips;

static Account *account_list;
static GtkWidget *account_clist;
static GtkWidget *account_edit_btn;
static GtkWidget *account_delete_btn;
#if FALSE /* TODO */
static GtkWidget *account_up_btn;
static GtkWidget *account_down_btn;
#endif
#if FALSE /* TODO */
static GtkWidget *filter_clist;
#endif

static GtkAdjustment *options_mail_check;
static GtkAdjustment *options_mark_as_read;
static GtkWidget *options_message_bigger;
static GtkWidget *options_prepend_character_entry;
static GtkWidget *options_empty_garbage;
static GtkWidget *options_keep_copy;
static GtkWidget *options_check_at_start;

static GtkWidget *appareance_font_message_body;
static GtkWidget *appareance_font_unread_message_list;
static GtkWidget *appareance_font_read_message_list;
static GtkWidget *appareance_font_print;
static GtkWidget *appareance_title;
static GtkWidget *appareance_toolbar_both;
static GtkWidget *appareance_toolbar_xpm;
static GtkWidget *appareance_toolbar_text;
static GtkWidget *appareance_colors_reply_original_message;
static GtkWidget *appareance_colors_misc_body;

static Mailbox   *mailboxes_list;
static GtkWidget *mailboxes_tree;
static GtkWidget *mailboxes_new_mailbox_entry;

#if USE_PLUGINS
static GtkWidget *plugins_loaded_clist;
static GtkWidget *plugins_loaded_btn_configure;
static GtkWidget *plugins_unload_btn;
static GtkWidget *plugins_label_name;
static GtkWidget *plugins_label_version;
static GtkWidget *plugins_label_author;
static GtkWidget *plugins_label_url;
static GtkWidget *plugins_label_description;
#endif

static GtkWidget *advanced_timeout;
static GtkWidget *advanced_use_persistent_smtp_connection;
static GtkWidget *advanced_persistent_smtp_address;
static GtkWidget *advanced_persistent_smtp_port;
#if BUILD_ADDRESS_BOOK
static GtkWidget *advanced_addrbook_init;
#endif

void gui_preferences_new (void) {
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *sections_vbox;
  GtkWidget *scroll;
  char *label[2];
 
  window = gnome_dialog_new (_("Preferences"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_APPLY,
      	GNOME_STOCK_BUTTON_CANCEL, NULL);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (WMain->window));
  
  vbox = GNOME_DIALOG (window)->vbox;
  accel_group = GNOME_DIALOG (window)->accelerators;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /** Tooltips **/
  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_delay (tooltips, 150);

  /*******************
   ** Sections VBox **
   *******************/
  sections_vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), sections_vbox, FALSE, TRUE, 0);
  gtk_widget_show (sections_vbox);

  /************
   ** Scroll **
   ************/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (sections_vbox), scroll, TRUE, TRUE, 0);
  gtk_widget_set_usize (scroll, 125, 220);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /********************
   ** Sections CTree **
   ********************/
  sections_ctree = gtk_ctree_new (1, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), sections_ctree);
  gtk_widget_show (sections_ctree);
  gtk_signal_connect (GTK_OBJECT (sections_ctree), "select_row",
		  	GTK_SIGNAL_FUNC (on_prefs_sections_clist_select_row), NULL); 
  
  label[0] = g_strdup (_("General"));
  label[1] = "";
  ctree_general = gtk_ctree_insert_node (GTK_CTREE (sections_ctree), NULL, NULL, label, 4,
      			NULL, NULL, NULL, NULL, FALSE, TRUE);
  
  label[0] = g_strdup (_("Mail"));
  ctree_mail = gtk_ctree_insert_node (GTK_CTREE (sections_ctree), NULL, NULL, label, 4,
      			NULL, NULL, NULL, NULL, FALSE, TRUE);

  /**************
   ** Prev Btn **
   **************/
  browse_prev_btn = gnome_stock_button (GNOME_STOCK_BUTTON_PREV);
  gtk_box_pack_start (GTK_BOX (sections_vbox), browse_prev_btn, FALSE, TRUE, 0);
  gtk_widget_show (browse_prev_btn);
  gtk_widget_set_usize (GTK_WIDGET (browse_prev_btn), -2, 18);
  gtk_widget_set_events (browse_prev_btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (browse_prev_btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (browse_prev_btn, "clicked", accel_group,
                              GDK_Z, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (browse_prev_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_prefs_previous_btn_clicked), NULL);

  /**************
   ** Next Btn **
   **************/
  browse_next_btn = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
  gtk_box_pack_start (GTK_BOX (sections_vbox), browse_next_btn, FALSE, TRUE, 0);
  gtk_widget_show (browse_next_btn);
  gtk_widget_set_usize (GTK_WIDGET (browse_next_btn), -2, 18);
 gtk_widget_set_events (browse_next_btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (browse_next_btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (browse_next_btn, "clicked", accel_group,
                              GDK_X, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (browse_next_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_prefs_next_btn_clicked), NULL);

  /**************
   ** Notebook **
   **************/
  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_widget_realize (window);
  prefs_add_page_logo ();
  prefs_add_page_options ();
  prefs_add_page_logo ();
  prefs_add_page_accounts ();
  prefs_add_page_mailboxes ();
/*  prefs_add_page_filters (); TODO */
  prefs_add_page_interface ();
  prefs_add_page_fonts ();
  prefs_add_page_colors ();
#if USE_PLUGINS
  prefs_add_page_plugins ();
#endif
  prefs_add_page_advanced ();
#if USE_PLUGINS
#  define PAGES_IN_NOTEBOOK 9
#else
#  define PAGES_IN_NOTEBOOK 8
#endif
  gtk_clist_select_row (GTK_CLIST (sections_ctree), 1, 0);
  gtk_clist_set_selectable (GTK_CLIST (sections_ctree), 0, FALSE);
  gtk_clist_set_selectable (GTK_CLIST (sections_ctree), 2, FALSE);

  gnome_dialog_button_connect (GNOME_DIALOG (window), 2, on_prefs_cancel_btn_clicked, NULL);
  gnome_dialog_button_connect (GNOME_DIALOG (window), 1, on_prefs_ok_btn_clicked, NULL);
  gnome_dialog_button_connect (GNOME_DIALOG (window), 0, on_prefs_ok_btn_clicked, (gpointer) TRUE);

  gtk_widget_show (window);
}

static void on_prefs_ok_btn_clicked_write_mailboxes (Mailbox *head, FILE *fd);
static void on_prefs_ok_btn_clicked_write_accounts (Account *head, FILE *fd);

static void on_prefs_ok_btn_clicked (GtkWidget *widget, gpointer accepting) {
  GtkWidget *menu;
  char *buf;
  FILE *fd;

  buf = CONFIG_FILE;
  if ((fd = fopen (buf, "w")) == NULL) {
    cronos_error (errno, "Opening config file to write configuration", ERROR_WARNING);
    c2_free (buf);
    return;
  }
  c2_free (buf);
 
  fprintf (fd,    "* Configuration file for Cronos II\n"
		  "cronosII v." VERSION);
 
  fprintf (fd,	  "\n"
		  "*************\n"
		  "* Mailboxes *\n"
		  "*************\n");
  
  config->mailbox_head = mailboxes_list;
  on_prefs_ok_btn_clicked_write_mailboxes (mailboxes_list, fd);
  gtk_clist_freeze (GTK_CLIST (WMain->ctree));
  gtk_clist_clear (GTK_CLIST (WMain->ctree));
  create_mailbox_tree (WMain->window, WMain->ctree, NULL, config->mailbox_head);
  gtk_clist_thaw (GTK_CLIST (WMain->ctree));

  fprintf (fd,    "\n"
		  "************\n"
		  "* Accounts *\n"
		  "************\n");
  account_free (config->account_head);
  config->account_head = account_copy_linked_list (account_list);
  on_prefs_ok_btn_clicked_write_accounts (config->account_head, fd);
  main_window_make_account_menu ();

  /*********
   * Check *
   *********/
  if (config->check_timeout != (int) options_mail_check->value) {
    if (check_timeout) gtk_timeout_remove (check_timeout);
    config->check_timeout = (int) options_mail_check->value;
    if (config->check_timeout)
      check_timeout = gtk_timeout_add ((config->check_timeout*60)*1000, (GtkFunction) check_timeout_check, NULL);
  }
  fprintf (fd,	"\n\n"
		"* Check mail every X minutes\n"
		"check_timeout %d\n",
		config->check_timeout);

  /** Check at start */
  config->check_at_start = GTK_TOGGLE_BUTTON (options_check_at_start)->active ? TRUE : FALSE;
  fprintf (fd, "\n"
      	       "* Check at start\n"
	       "check_at_start %d\n", config->check_at_start);

  /*****************
   * Empty Garbage *
   *****************/
  config->empty_garbage = GTK_TOGGLE_BUTTON (options_empty_garbage)->active ? 1 : 0;
  fprintf (fd,	"\n"
		"* Empty the Garbage Mailbox on exit?\n"
		"empty_garbage %d\n", config->empty_garbage);

  /****************
   * Mark as read *
   ****************/
  config->mark_as_read = (int) options_mark_as_read->value;
  fprintf (fd,  "\n"
      		"* Mark as read timeout (secs)\n"
		"mark_as_read %d\n", config->mark_as_read);
  
  /***********************
   * Keep copy in Outbox *
   ***********************/
  config->use_outbox = GTK_TOGGLE_BUTTON (options_keep_copy)->active ? 1 : 0;
  fprintf (fd,	"\n"
		"* Keep a copy in the Outbox Mailbox every time you send an E-Mail\n"
		"use_outbox %d\n", config->use_outbox);

  /*****************************
   * Prepend char on re or fwd *
   *****************************/
  config->prepend_char_on_re = g_strdup (gtk_entry_get_text (GTK_ENTRY (options_prepend_character_entry)));
  if (!config->prepend_char_on_re) config->prepend_char_on_re = g_strdup ("> ");
  fprintf (fd,	"\n"
		"* Prepend this character to each line when Replying or Forward a message\n"
		"prepend_char_on_re \"%s\"\n", config->prepend_char_on_re);

  /*********************************
   * Timeout for net related stuff *
   *********************************/
  config->timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (advanced_timeout));
  fprintf (fd,	"\n"
		"* Timeout for net related stuff\n"
		"timeout %d\n",
		config->timeout);

  /** Persistent SMTP Connection **/
  config->use_persistent_smtp_connection =
    			GTK_TOGGLE_BUTTON (advanced_use_persistent_smtp_connection)->active ? 1 : 0;
  fprintf (fd,	"\n"
		"* Use the persistent SMTP connection\n"
		"use_persistent_smtp_connection %d\n", config->use_persistent_smtp_connection);
  
  /** Persistent SMTP Address **/
  config->persistent_smtp_address = g_strdup (gtk_entry_get_text (
					GTK_ENTRY (advanced_persistent_smtp_address)));
  fprintf (fd,	"\n"
		"* Persistent SMTP address\n"
		"persistent_smtp_address \"%s\"\n", config->persistent_smtp_address);

  /** Persistent SMTP port **/
  config->persistent_smtp_port = gtk_spin_button_get_value_as_int (
      				GTK_SPIN_BUTTON (advanced_persistent_smtp_port));
  fprintf (fd,	"\n"
		"* Persistent SMTP port\n"
		"persistent_smtp_port %d\n", config->persistent_smtp_port);

#ifdef BUILD_ADDRESS_BOOK
  /** Address Book Initialization **/
  menu = GTK_BIN (advanced_addrbook_init)->child;
  gtk_label_get (GTK_LABEL (menu), &buf);
  if (streq (buf, _("when required."))) config->addrbook_init = INIT_ADDRESS_BOOK_INIT_REQ;
  else if (streq (buf, _("when it is opened."))) config->addrbook_init = INIT_ADDRESS_BOOK_INIT_OPEN;
  else config->addrbook_init = INIT_ADDRESS_BOOK_INIT_START;
  fprintf (fd,	"\n"
		"* Address Book Initialization\n"
		"addrbook_init %d\n", config->addrbook_init);
#endif

  /***************************************
   * Do not download message bigger than *
   ***************************************/
  config->message_bigger = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (options_message_bigger));
  fprintf (fd,	"\n"
		"* Do not download messages bigger than\n"
		"message_bigger %d\n",
		config->message_bigger);

  /**********
   * COLORS *
   **********/
  gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (appareance_colors_reply_original_message),
		&config->color_reply_original_message.red,
		&config->color_reply_original_message.green,
		&config->color_reply_original_message.blue,
		NULL);
  gdk_color_alloc (gdk_colormap_get_system (), &config->color_reply_original_message);
  fprintf (fd,	"\n* Colors\n"
		"color_reply_original_message %dx%dx%d\n",
		config->color_reply_original_message.red,
		config->color_reply_original_message.green,
		config->color_reply_original_message.blue);

  gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (appareance_colors_misc_body),
		&config->color_misc_body.red,
		&config->color_misc_body.green,
		&config->color_misc_body.blue,
		NULL);
  gdk_color_alloc (gdk_colormap_get_system (), &config->color_misc_body);
  fprintf (fd,	"\n* Colors\n"
		"color_misc_body %dx%dx%d\n",
		config->color_misc_body.red,
		config->color_misc_body.green,
		config->color_misc_body.blue);

  fclose (fd);

  /*********************
   *********************
   	   - RC -
   *********************
   *********************/
  c2_free (rc->title);
  rc->title = g_strdup (gtk_entry_get_text (GTK_ENTRY (appareance_title)));
  update_wm_title ();
  
  if (GTK_TOGGLE_BUTTON (appareance_toolbar_both)->active) rc->toolbar = GTK_TOOLBAR_BOTH;
  else if (GTK_TOGGLE_BUTTON (appareance_toolbar_xpm)->active) rc->toolbar = GTK_TOOLBAR_ICONS;
  else if (GTK_TOGGLE_BUTTON (appareance_toolbar_text)->active) rc->toolbar = GTK_TOOLBAR_TEXT;
  gtk_toolbar_set_style (GTK_TOOLBAR (WMain->toolbar), rc->toolbar);
  gtk_widget_queue_resize (WMain->window);
  
  c2_free (rc->font_body);
  c2_free (rc->font_unread);
  c2_free (rc->font_read);
  c2_free (rc->font_print);

  rc->font_body = g_strdup (gtk_entry_get_text (GTK_ENTRY (appareance_font_message_body)));
  rc->font_unread = g_strdup (gtk_entry_get_text (GTK_ENTRY (appareance_font_unread_message_list)));
  rc->font_read = g_strdup (gtk_entry_get_text (GTK_ENTRY (appareance_font_read_message_list)));
  rc->font_print = g_strdup (gtk_entry_get_text (GTK_ENTRY (appareance_font_print)));
 
  cronos_gui_init ();
  cronos_gui_set_sensitive ();
  if (accepting)
    gtk_widget_destroy (window);
}

static void on_prefs_ok_btn_clicked_write_mailboxes (Mailbox *head, FILE *fd) {
  Mailbox *mailbox;
  char *buf;
  DIR *dir;
  FILE *file;

  for (mailbox = head; mailbox; mailbox = mailbox->next) {
    fprintf (fd, "mailbox \"%d\r%s\r%d\"\n", mailbox->self_id, mailbox->name, mailbox->parent_id);
    if (mailbox->child != NULL) on_prefs_ok_btn_clicked_write_mailboxes (mailbox->child, fd);

    /* Check that the directory exists */
    buf = g_strconcat (getenv ("HOME"), ROOT, "/", mailbox->name, ".mbx", NULL);
    if ((dir = opendir (buf)) == NULL) {
      if (errno != ENOENT) {
	cronos_error (errno, _("Checking if the directory of the mailbox exists"), ERROR_WARNING);
	c2_free (buf);
	continue;
      }
      mkdir (buf, 0700);
   } else {
    closedir (dir);
    c2_free (buf);
    continue;
   }
   
    c2_free (buf);
    buf = g_strconcat (getenv ("HOME"), ROOT, "/", mailbox->name, ".mbx/index", NULL);
    if ((file = fopen (buf, "w")) == NULL) {
      cronos_error (errno, _("Creating the index directory for the nex mailbox"), ERROR_WARNING);
      c2_free (buf);
      continue;
    }
    fclose (file);
    c2_free (buf);
  }
}

static void on_prefs_ok_btn_clicked_write_accounts (Account *head, FILE *fd) {
  Account *account;
  char *buf;
  
  for (account = head; account; account = account->next) {
    if (account->type == C2_ACCOUNT_POP) {
      buf = g_strdup ("POP");
    } else if (account->type == C2_ACCOUNT_SPOOL) {
      buf = g_strdup ("SPOOL");}
    
    fprintf (fd, "account \"%s\r%s\r%s\r%s\r%s\r%d\r%d\r%d\r%d\r",
		account->acc_name, account->per_name, account->mail_addr,
		buf, account->smtp, account->smtp_port, account->keep_copy,
		account->use_it, account->mailbox->self_id);
    if (account->type == C2_ACCOUNT_POP)
      fprintf (fd, "%s\r%s\r%s\r%d\r%d\r%s\"\n",
	  	account->protocol.pop.usr_name, account->protocol.pop.pass, account->protocol.pop.host,
		account->protocol.pop.host_port, account->always_append_signature, account->signature);
    else if (account->type == C2_ACCOUNT_SPOOL)
      fprintf (fd, "%s\r%d\r%s\"\n",
	  	account->protocol.spool.file, account->always_append_signature, account->signature);
  }
}

/* Functions to browse through the differents menues */
static void on_prefs_sections_clist_select_row (GtkWidget *widget, gint row) {
  int page;
  
  if (row == 0 || row == 2) return;
  gtk_notebook_set_page (GTK_NOTEBOOK (notebook), row);

  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  if (page == 1) {
    gtk_widget_set_sensitive (browse_prev_btn, FALSE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  } else
  if (page == PAGES_IN_NOTEBOOK) {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  }
}

static void on_prefs_previous_btn_clicked (void) {
  int current_page;
  
  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  if (current_page == 1) return;
  if (current_page == 3) {
    gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
    current_page--;
  }
  gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
  current_page--;
  
  gtk_clist_unselect_row (GTK_CLIST (sections_ctree), current_page-1, 0);
  gtk_clist_select_row (GTK_CLIST (sections_ctree), current_page, 0);

  if (current_page == 1) {
    gtk_widget_set_sensitive (browse_prev_btn, FALSE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  } else
  if (current_page == PAGES_IN_NOTEBOOK) {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  }
}

static void on_prefs_next_btn_clicked (void) {
  int current_page;

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  if (current_page == 1) {
    gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
    current_page++;
  }
  gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
  current_page++;
  
  gtk_clist_unselect_row (GTK_CLIST (sections_ctree), current_page+1, 0);
  gtk_clist_select_row (GTK_CLIST (sections_ctree), current_page, 0);
  
  if (current_page == 1) {
    gtk_widget_set_sensitive (browse_prev_btn, FALSE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  } else
  if (current_page == PAGES_IN_NOTEBOOK) {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (browse_prev_btn, TRUE);
    gtk_widget_set_sensitive (browse_next_btn, TRUE);
  }
}

static void on_prefs_cancel_btn_clicked (void) {
  gtk_widget_destroy (window);
}

static void gui_destroy_data (GtkWidget *widget, gpointer data) {
  gtk_widget_destroy (GTK_WIDGET (data));
}

/*******************************************************************
 *******************************************************************
 ****			      LOGO				****
 *******************************************************************
 *******************************************************************/
static void prefs_add_page_logo (void) {
  GtkWidget *vbox;
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, NULL);
  gtk_widget_show (vbox);
}

/*******************************************************************
 *******************************************************************
 ****			     ACCOUNT				****
 *******************************************************************
 *******************************************************************/
static char *mbox_name = NULL;
static char *new_account_type = NULL;

void account_update_clist (void) {
  Account *account;
  char *append[4];

  gtk_clist_freeze (GTK_CLIST (account_clist));
  gtk_clist_clear (GTK_CLIST (account_clist));
  for (account = account_list; account; account = account->next) {
      append[0] = account->acc_name;
      if (account->type == C2_ACCOUNT_POP)
	append[1] = g_strdup ("POP");
      else if (account->type == C2_ACCOUNT_SPOOL)
	append[1] = g_strdup ("Spool");
      else
	append[1] = g_strdup (_("Unknown"));
      append[2] = account->per_name;
      append[3] = account->mail_addr;

      gtk_clist_append (GTK_CLIST (account_clist), append);
  }
  gtk_clist_thaw (GTK_CLIST (account_clist));
}

static void on_account_new_ok_clicked (GtkWidget *widget, gpointer data) {
  PreferencesAccount *_account;
  Account *account;
  Account *last_node;
  Mailbox *search;

  _account = (PreferencesAccount *) data;
  if (_account == NULL) return;
  if (!mbox_name) {
    g_warning (_("You must select a mailbox where to store messages downloaded from this account."));
    return;
  }
  account = (Account *) g_malloc0 (sizeof (Account));

  /* Load the data */
  account->acc_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->acc_name)));
  account->per_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->per_name)));
  account->mail_addr = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->mail_addr)));
  account->type = _account->type;
  if (streq (new_account_type, "POP")) {
    account->type = C2_ACCOUNT_POP;
    account->protocol.pop.usr_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.usr_name)));
    account->protocol.pop.pass = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.pass)));
    account->protocol.pop.host = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.host)));
    account->protocol.pop.host_port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (_account->protocol.pop.host_port));
  } else if (streq (new_account_type, "SPOOL")) {
    account->type = C2_ACCOUNT_SPOOL;
    account->protocol.spool.file = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.spool.file)));
  }
  account->smtp = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->smtp)));
  account->smtp_port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (_account->smtp_port));
  if (GTK_TOGGLE_BUTTON (_account->keep_copy)->active) account->keep_copy = TRUE;
  else account->keep_copy = FALSE;
  if (GTK_TOGGLE_BUTTON (_account->use_it)->active) account->use_it = TRUE;
  else account->use_it = FALSE;
  account->always_append_signature = GTK_TOGGLE_BUTTON (_account->always_append_signature)->active ? TRUE : FALSE;
  account->signature = g_strdup (gtk_editable_get_chars (GTK_EDITABLE (_account->signature), 0, -1));

  search = search_mailbox_name (config->mailbox_head, mbox_name);
  if (search == NULL) return;
  account->mailbox = search;

  /* Delete the window */
  gtk_widget_destroy (_account->window);

  /* Append to the linked list */
  last_node = search_account_last_element (account_list);
  if (!last_node) {
    account_list = account;
  } else {
    last_node->next = account;
  }
  account->next = NULL;

  account_update_clist ();
  c2_free (_account);
}

static void on_account_edit_ok_clicked (GtkWidget *widget, gpointer data) {
  PreferencesAccount *_account;
  Account *account;
  Account *acc_ll;
  Mailbox *search;

  _account = (PreferencesAccount *) data;
  if (_account == NULL) return;
  account = g_new0 (Account, 1);

  /* Load the data */
  account->acc_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->acc_name)));
  account->per_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->per_name)));
  account->mail_addr = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->mail_addr)));
  account->type = _account->type;
  if (account->type == C2_ACCOUNT_POP) {
    account->protocol.pop.usr_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.usr_name)));
    account->protocol.pop.pass = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.pass)));
    account->protocol.pop.host = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.pop.host)));
    account->protocol.pop.host_port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (_account->protocol.pop.host_port));
  } else if (account->type == C2_ACCOUNT_SPOOL) {
    account->protocol.spool.file = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->protocol.spool.file)));
  }
  account->smtp = g_strdup (gtk_entry_get_text (GTK_ENTRY (_account->smtp)));
  account->smtp_port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (_account->smtp_port));
  if (GTK_TOGGLE_BUTTON (_account->keep_copy)->active) account->keep_copy = TRUE;
  else account->keep_copy = FALSE;
  if (GTK_TOGGLE_BUTTON (_account->use_it)->active) account->use_it = TRUE;
  else account->use_it = FALSE;
  account->always_append_signature = GTK_TOGGLE_BUTTON (_account->always_append_signature)->active ? TRUE : FALSE;
  account->signature = g_strdup (gtk_editable_get_chars (GTK_EDITABLE (_account->signature), 0, -1));

  if (!mbox_name) return;
  search = search_mailbox_name (config->mailbox_head, mbox_name);
  if (search == NULL) return;
  account->mailbox = search;

  /* Delete the window */
  gtk_widget_destroy (_account->window);

  /* Append to the linked list */
  acc_ll = search_account_acc_name (account_list, account->acc_name);
  if (!acc_ll) {
    return;
  }

  c2_free (acc_ll->acc_name); acc_ll->acc_name = account->acc_name;
  c2_free (acc_ll->per_name); acc_ll->per_name = account->per_name;
  c2_free (acc_ll->mail_addr); acc_ll->mail_addr = account->mail_addr;
  if (account->type == C2_ACCOUNT_POP) {
    c2_free (acc_ll->protocol.pop.usr_name); acc_ll->protocol.pop.usr_name = account->protocol.pop.usr_name;
    c2_free (acc_ll->protocol.pop.pass); acc_ll->protocol.pop.pass = account->protocol.pop.pass;
    c2_free (acc_ll->protocol.pop.host); acc_ll->protocol.pop.host = account->protocol.pop.host;
    acc_ll->protocol.pop.host_port = account->protocol.pop.host_port;
  } else if (account->type == C2_ACCOUNT_SPOOL) {
    c2_free (acc_ll->protocol.spool.file); acc_ll->protocol.spool.file = account->protocol.spool.file;
  }
  c2_free (acc_ll->smtp); acc_ll->smtp = account->smtp;
  acc_ll->smtp_port = account->smtp_port;
  acc_ll->mailbox = account->mailbox;
  acc_ll->keep_copy = account->keep_copy;
  acc_ll->use_it = account->use_it;
  acc_ll->always_append_signature = account->always_append_signature;
  c2_free (acc_ll->signature); acc_ll->signature = account->signature;

  /* Update the clist */
  account_update_clist ();
  c2_free (_account);
}

static void account_new_or_edit_mailbox_select_row (GtkCTree *ctree, gint row, gint col, GdkEvent *event, gpointer data) {
  GtkCTreeNode *node;
  node = gtk_ctree_node_nth(ctree, row);
  mbox_name = ((Mailbox*)gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node))->name;
}

static void on_account_new_or_edit_mail_addr_changed (GtkWidget *widget, gpointer acc) {
  PreferencesAccount *account;
  char *mail=NULL;
  char *mail_=NULL;
  char *user=NULL;
  char *host=NULL;
  int at_pos;
  gboolean has_at=TRUE;

  g_return_if_fail (acc);

  account = (PreferencesAccount *) acc;
  mail = g_strdup (gtk_entry_get_text (GTK_ENTRY (account->mail_addr)));

  if (account->type == C2_ACCOUNT_SPOOL) return;

  for (mail_ = mail, at_pos=0; *mail_ != '@'; mail_++, at_pos++) {
    if (*mail_ == '\0') {
      has_at = FALSE;
      break;
    }
  }

  user = CHARALLOC (at_pos);
  strncpy (user, mail, at_pos);
  if (has_at) {
    host = CHARALLOC (strlen (mail)-at_pos);
    strcpy (host, mail+at_pos+1);
  }

  gtk_entry_set_text (GTK_ENTRY (account->protocol.pop.usr_name), user);
  if (has_at) gtk_entry_set_text (GTK_ENTRY (account->protocol.pop.host), host);

  c2_free (mail);
  c2_free (user);
  c2_free (host);
}

static void on_account_new_or_edit_select_file_btn_clicked (GtkWidget *btn, GtkWidget *entry) {
  char *file = gui_select_file_new (SPOOL_MAIL_DIR, NULL, NULL, NULL);

  if (file)
    gtk_entry_set_text (GTK_ENTRY (entry), file);
}

static char *account_new_or_edit (Account *_account) {
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *hsep;
  GtkWidget *btn;
  GtkWidget *label;
  GtkAdjustment *adj=NULL;
  GtkWidget *scroll;
  GtkWidget *hbox;
  GtkWidget *nb;
  GtkStyle *style, *style2;
  PreferencesAccount *account;
  GtkAccelGroup *accel;

  accel = gtk_accel_group_new ();

  account = g_new0 (PreferencesAccount, 1);
  if (!_account) {
    if (streq (new_account_type, "SPOOL")) account->type = C2_ACCOUNT_SPOOL;
    else if (streq (new_account_type, "POP")) account->type = C2_ACCOUNT_POP;
    else account->type = C2_ACCOUNT_POP;
  } else account->type = _account->type;

  account->window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_window_set_title (GTK_WINDOW (account->window), _account ? _("Edit Account") : _("New Account"));
  gtk_window_set_modal (GTK_WINDOW (account->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (account->window), GTK_WINDOW (window));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (account->window), vbox);
  gtk_widget_show (vbox);

  nb = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox), nb, TRUE, TRUE, 0);
  gtk_widget_show (nb); 

  /* Account Page */
  table = gtk_table_new (3, 2, FALSE);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  label = gtk_label_new (_("Account Name"));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
  gtk_widget_show (label);
  label = gtk_label_new (_("Name: "));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
  gtk_widget_show (label);
  label = gtk_label_new (_("E-Mail: "));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
  gtk_widget_show (label);

  account->acc_name = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), account->acc_name, 1, 2, 0, 1);
  gtk_widget_show (account->acc_name);
  if (_account != NULL) {
    gtk_entry_set_text (GTK_ENTRY (account->acc_name), _account->acc_name);
    gtk_widget_set_sensitive (GTK_WIDGET (account->acc_name), FALSE);
  }

  account->per_name = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), account->per_name, 1, 2, 1, 2);
  gtk_widget_show (account->per_name);
  if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->per_name), _account->per_name);

  account->mail_addr = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), account->mail_addr, 1, 2, 2, 3);
  gtk_widget_show (account->mail_addr);
  if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->mail_addr), _account->mail_addr);
  gtk_signal_connect (GTK_OBJECT (account->mail_addr), "changed",
      				GTK_SIGNAL_FUNC (on_account_new_or_edit_mail_addr_changed), account);

  label = gtk_label_new (_("Account"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nb), table, label);
  /* /Account Page */

  /* Protocol Page */
  if (account->type == C2_ACCOUNT_POP) {
    table = gtk_table_new (4, 2, FALSE);
    gtk_widget_show (table);
    gtk_table_set_row_spacings (GTK_TABLE (table), 2);
    gtk_table_set_col_spacings (GTK_TABLE (table), 2);
    
    label = gtk_label_new (_("User Name: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
    gtk_widget_show (label);
    label = gtk_label_new (_("Password: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
    gtk_widget_show (label);
    label = gtk_label_new (_("POP3 Server: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
    gtk_widget_show (label);
    label = gtk_label_new (_("SMTP Server: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
    gtk_widget_show (label);
    
    account->protocol.pop.usr_name = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (table), account->protocol.pop.usr_name, 1, 2, 0, 1);
    gtk_widget_show (account->protocol.pop.usr_name);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->protocol.pop.usr_name), _account->protocol.pop.usr_name);
    
    account->protocol.pop.pass = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (table), account->protocol.pop.pass, 1, 2, 1, 2);
    gtk_widget_show (account->protocol.pop.pass);
    gtk_entry_set_visibility (GTK_ENTRY (account->protocol.pop.pass), FALSE);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->protocol.pop.pass), _account->protocol.pop.pass);
    
    hbox = gtk_hbox_new (FALSE, 2);
    gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
    gtk_widget_show (hbox);
    
    account->protocol.pop.host = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), account->protocol.pop.host, TRUE, TRUE, 0);
    gtk_widget_show (account->protocol.pop.host);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->protocol.pop.host), _account->protocol.pop.host);
    
    label = gtk_label_new (_("Port: "));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);
    
    adj = (GtkAdjustment *) gtk_adjustment_new ((_account != NULL) ? _account->protocol.pop.host_port : 110,
	1, 99999, 1, 10, 0);
    account->protocol.pop.host_port = gtk_spin_button_new (adj, 1, 0);
    gtk_box_pack_start (GTK_BOX (hbox), account->protocol.pop.host_port, TRUE, TRUE, 0);
    gtk_widget_show (account->protocol.pop.host_port);
    
    hbox = gtk_hbox_new (FALSE, 2);
    gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 3, 4);
    gtk_widget_show (hbox);
    
    account->smtp = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), account->smtp, TRUE, TRUE, 0);
    gtk_widget_show (account->smtp);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->smtp), _account->smtp);
    if (config->use_persistent_smtp_connection) gtk_widget_set_sensitive (account->smtp, FALSE);
    
    label = gtk_label_new (_("Port: "));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);
    
    adj = (GtkAdjustment *) gtk_adjustment_new ((_account != NULL) ? _account->smtp_port : 25,
	1, 99999, 1, 10, 0);
    account->smtp_port = gtk_spin_button_new (adj, 1, 0);
    gtk_box_pack_start (GTK_BOX (hbox), account->smtp_port, TRUE, TRUE, 0);
    gtk_widget_show (account->smtp_port);
    if (config->use_persistent_smtp_connection) gtk_widget_set_sensitive (account->smtp_port, FALSE);
  } else if (account->type == C2_ACCOUNT_SPOOL) {
    table = gtk_table_new (2, 2, FALSE);
    gtk_widget_show (table);
    gtk_table_set_row_spacings (GTK_TABLE (table), 2);
    gtk_table_set_col_spacings (GTK_TABLE (table), 2);
    
    label = gtk_label_new (_("File: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
    gtk_widget_show (label);
    label = gtk_label_new (_("SMTP Server: "));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
    gtk_widget_show (label);
    
    hbox = gtk_hbox_new (FALSE, 2);
    gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 0, 1);
    gtk_widget_show (hbox);
    
    account->protocol.spool.file = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), account->protocol.spool.file, TRUE, TRUE, 0);
    gtk_widget_show (account->protocol.spool.file);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->protocol.spool.file), _account->protocol.spool.file);
    gtk_entry_set_editable (GTK_ENTRY (account->protocol.spool.file), FALSE);
    
    btn = gtk_button_new_with_label (_(".."));
    gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
    gtk_widget_show (btn);
    gtk_signal_connect (GTK_OBJECT (btn), "clicked",
			GTK_SIGNAL_FUNC (on_account_new_or_edit_select_file_btn_clicked),
				(gpointer) account->protocol.spool.file);
    
    hbox = gtk_hbox_new (FALSE, 2);
    gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);
    gtk_widget_show (hbox);
    
    account->smtp = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), account->smtp, TRUE, TRUE, 0);
    gtk_widget_show (account->smtp);
    if (_account != NULL) gtk_entry_set_text (GTK_ENTRY (account->smtp), _account->smtp);
    
    label = gtk_label_new (_("Port: "));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);
    
    adj = (GtkAdjustment *) gtk_adjustment_new ((_account != NULL) ? _account->smtp_port : 25,
	1, 99999, 1, 10, 0);
    account->smtp_port = gtk_spin_button_new (adj, 1, 0);
    gtk_box_pack_start (GTK_BOX (hbox), account->smtp_port, TRUE, TRUE, 0);
    gtk_widget_show (account->smtp_port);
  }
    
    label = gtk_label_new (_("Protocol"));
    gtk_notebook_append_page (GTK_NOTEBOOK (nb), table, label);
    /* /Protocol Page */
  
  /* Mailbox Page */
  table = gtk_frame_new (_("Mailbox"));
  gtk_widget_show (table);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (table), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  account->mailbox = gtk_ctree_new (1, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), account->mailbox);
  gtk_widget_show (account->mailbox);
  create_mailbox_tree (window, account->mailbox, NULL, config->mailbox_head);
  gtk_signal_connect (GTK_OBJECT (account->mailbox), "select_row",
      				GTK_SIGNAL_FUNC (account_new_or_edit_mailbox_select_row), NULL);
  if (_account) {
    GtkCTreeNode *cnode=NULL;
    GtkCTreeNode *cnode_sele=NULL;
    int i;

    for (i=1, cnode = gtk_ctree_node_nth (GTK_CTREE (account->mailbox), 0); cnode; i++) {
      if (i>1) cnode = gtk_ctree_node_nth (GTK_CTREE (account->mailbox), i);
      cnode_sele = gtk_ctree_find_by_row_data (GTK_CTREE (account->mailbox), cnode, _account->mailbox);

      if (cnode_sele) {
	gtk_ctree_select (GTK_CTREE (account->mailbox), cnode_sele);
	mbox_name = _account->mailbox->name;
	break;
      }
    }

    if (!cnode_sele) {
      goto select_inbox;
    }
  } else {
select_inbox:
    {
      Mailbox *inbox=NULL;
      GtkCTreeNode *cnode=NULL;
      GtkCTreeNode *cnode_sele=NULL;
      int i;
      
      inbox = search_mailbox_name (config->mailbox_head, MAILBOX_INBOX);

      if (!inbox) goto out;

      for (i=1, cnode = gtk_ctree_node_nth (GTK_CTREE (account->mailbox), 0); cnode; i++) {
	if (i>1) cnode = gtk_ctree_node_nth (GTK_CTREE (account->mailbox), i);
	cnode_sele = gtk_ctree_find_by_row_data (GTK_CTREE (account->mailbox), cnode, inbox);
	
	if (cnode_sele) {
	  gtk_ctree_select (GTK_CTREE (account->mailbox), cnode_sele);
	  mbox_name = inbox->name;
	  break;
	}
      }
    }
  }
out:
  label = gtk_label_new (_("Mailbox"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nb), table, label);
  /* /Mailbox Page */

  /* Signature Page */
  table = gtk_vbox_new (FALSE, 1);
  gtk_widget_show (table);
  
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (table), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  account->signature = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), account->signature);
  gtk_widget_show (account->signature);
  style = gtk_widget_get_style (account->signature);
  style2 = gtk_style_copy (style);
  style2->font = font_body;
  gtk_widget_set_style (account->signature, style2);
  gtk_text_set_editable (GTK_TEXT (account->signature), TRUE);
  if (_account && _account->signature) {
    gtk_text_insert (GTK_TEXT (account->signature), NULL, NULL, NULL, _account->signature, -1);
  } else if (!_account) {
    FILE *fd;
    char *path;
    char *buf;
    GString *sign;

    path = g_strdup_printf ("%s/.signature", getenv ("HOME"));
    if ((fd = fopen (path, "r")) == NULL) {
      c2_free (path);
      goto doesnt_have_signature_file;
    }
    c2_free (path);

    sign = g_string_new (NULL);
    for (;;) {
      if ((buf = fd_get_line (fd)) == NULL) break;
      path = g_strdup_printf ("%s\n", buf);
      g_string_append (sign, path);
      c2_free (path);
      c2_free (buf);
    }
    fclose (fd);
    gtk_text_insert (GTK_TEXT (account->signature), NULL, NULL, NULL, sign->str, -1);
    g_string_free (sign, TRUE);
  }
doesnt_have_signature_file:

  account->always_append_signature = gtk_check_button_new_with_label (_("Automatically append signature"));
  gtk_box_pack_start (GTK_BOX (table), account->always_append_signature, FALSE, FALSE, 0);
  gtk_widget_show (account->always_append_signature);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (account->always_append_signature), TRUE);
  if (_account) {
    if (!_account->always_append_signature) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (account->always_append_signature), FALSE);
    }
  }

  label = gtk_label_new (_("Signature"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nb), table, label);
  /* /Signature Page */
  
  /* Advanced Page */
  table = gtk_table_new (2, 1, FALSE);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  
  account->keep_copy = gtk_check_button_new_with_label (_("Keep messages on the server."));
  gtk_table_attach_defaults (GTK_TABLE (table), account->keep_copy, 0, 1, 0, 1);
  gtk_widget_show (account->keep_copy);
  if (_account != NULL) {
    if (_account->keep_copy) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (account->keep_copy), TRUE);
    }
  }

  account->use_it = gtk_check_button_new_with_label (_("Activate this account"));
  gtk_table_attach_defaults (GTK_TABLE (table), account->use_it, 0, 1, 1, 2);
  gtk_widget_show (account->use_it);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (account->use_it), TRUE);
  if (_account != NULL) {
    if (!_account->use_it) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (account->use_it), FALSE);
    }
  }
  label = gtk_label_new (_("Advanced"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nb), table, label);
  /* /Advanced Page */

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (btn, "clicked", accel,
                              GDK_Return, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (btn);
  if (_account == NULL) {
    gtk_signal_connect (GTK_OBJECT (btn), "clicked",
 		  	GTK_SIGNAL_FUNC (on_account_new_ok_clicked), account);
  } else {
    gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		    	GTK_SIGNAL_FUNC (on_account_edit_ok_clicked), account);
  }

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (btn, "clicked", accel,
                              GDK_Escape, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (gui_destroy_data), account->window);

  gtk_window_add_accel_group (GTK_WINDOW (account->window), accel);

  gtk_widget_show (account->window);

  return NULL;
}

static void on_account_ask_account_type_ok_btn_clicked (GtkWidget *widget, PreferencesAccountType *actype) {
  g_return_if_fail (actype);

  if (GTK_TOGGLE_BUTTON (actype->spool_btn)->active) {
    new_account_type = g_strdup ("SPOOL");
  } else {
    new_account_type = g_strdup ("POP");
  }

  gtk_widget_destroy (actype->window);
  c2_free (actype);
  account_new_or_edit (NULL);
}

static void on_account_ask_account_type_cancel_btn_clicked (GtkWidget *widget, PreferencesAccountType *actype) {
  new_account_type = NULL;
  gtk_widget_destroy (actype->window);
  c2_free (actype);
}

static void on_account_new_or_edit_btn_toggled (GtkWidget *btn, PreferencesAccountType *actype) {
  if (GTK_TOGGLE_BUTTON (actype->spool_btn)->active) {
    gtk_text_freeze (GTK_TEXT (actype->text));
    gtk_text_set_point (GTK_TEXT (actype->text), 0);
    gtk_text_forward_delete (GTK_TEXT (actype->text), gtk_text_get_length (GTK_TEXT (actype->text)));
    gtk_text_backward_delete (GTK_TEXT (actype->text), -1);
    QUICK_HELP (actype->text, QUICK_HELP_SPOOL); 
    gtk_text_thaw (GTK_TEXT (actype->text));
  } else {
    gtk_text_freeze (GTK_TEXT (actype->text));
    gtk_text_set_point (GTK_TEXT (actype->text), 0);
    gtk_text_forward_delete (GTK_TEXT (actype->text), gtk_text_get_length (GTK_TEXT (actype->text)));
    QUICK_HELP (actype->text, QUICK_HELP_POP);
    gtk_text_thaw (GTK_TEXT (actype->text));
  }
}

static void account_ask_account_type (void) {
  PreferencesAccountType *actype;
  GtkWidget *hbox, *scroll, *vbox, *label, *hsep, *hbox2, *btn;
  GtkStyle *style, *style2;
  GSList *grp = NULL;
  GtkAccelGroup *accel;

  accel = gtk_accel_group_new ();
  
  actype = (PreferencesAccountType *) g_malloc0 (sizeof (PreferencesAccountType));
  actype->window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_window_set_title (GTK_WINDOW (actype->window), _("Account Type"));
  gtk_widget_set_usize (GTK_WIDGET (actype->window), 350, 130);
  gtk_window_set_modal (GTK_WINDOW (actype->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (actype->window), GTK_WINDOW (window));
  gtk_window_set_position (GTK_WINDOW (actype->window), GTK_WIN_POS_CENTER);
  gtk_window_set_policy (GTK_WINDOW (actype->window), TRUE, TRUE, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (actype->window), 3); 

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_container_add (GTK_CONTAINER (actype->window), hbox);
  gtk_widget_show (hbox);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  actype->text = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), actype->text);
  gtk_widget_show (actype->text);
  gtk_editable_set_editable (GTK_EDITABLE (actype->text), FALSE);
  style = gtk_widget_get_style (actype->text);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load ("-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-iso8859-1");
  gtk_widget_set_style (actype->text, style2);
  QUICK_HELP (actype->text, QUICK_HELP_NEW_ACCOUNT);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
  gtk_widget_show (vbox);

  label = gtk_label_new (_("Account Type"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);
  style = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-180-*-*-p-*-iso8859-1");
  gtk_widget_set_style (label, style2);

  actype->pop_btn = gtk_radio_button_new_with_label (grp, _("POP3 Account"));
  grp = gtk_radio_button_group (GTK_RADIO_BUTTON (actype->pop_btn));
  gtk_box_pack_start (GTK_BOX (vbox), actype->pop_btn, FALSE, FALSE, 0);
  gtk_widget_show (actype->pop_btn);
  gtk_signal_connect (GTK_OBJECT (actype->pop_btn), "clicked",
      			GTK_SIGNAL_FUNC (on_account_new_or_edit_btn_toggled), actype);

  actype->spool_btn = gtk_radio_button_new_with_label (grp, _("Spool Account"));
  grp = gtk_radio_button_group (GTK_RADIO_BUTTON (actype->spool_btn));
  gtk_box_pack_start (GTK_BOX (vbox), actype->spool_btn, FALSE, FALSE, 0);
  gtk_widget_show (actype->spool_btn);
  gtk_signal_connect (GTK_OBJECT (actype->spool_btn), "toggled",
      			GTK_SIGNAL_FUNC (on_account_new_or_edit_btn_toggled), actype);

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, FALSE, 3);
  gtk_widget_show (hsep);

  hbox2 = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, TRUE, 0);
  gtk_widget_show (hbox2);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
  gtk_box_pack_end (GTK_BOX (hbox2), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (btn, "clicked", accel,
                              GDK_Escape, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_account_ask_account_type_cancel_btn_clicked), actype);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
  gtk_box_pack_end (GTK_BOX (hbox2), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (btn, "clicked", accel,
                              GDK_Return, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_account_ask_account_type_ok_btn_clicked), actype);

  gtk_window_add_accel_group (GTK_WINDOW (actype->window), accel);

  gtk_widget_show (actype->window);
}

static void on_account_clist_select_row_or_unselect_row (GtkWidget *clist) {
  if (GTK_CLIST (clist)->selection) {
    gtk_widget_set_sensitive (account_edit_btn, TRUE);
    gtk_widget_set_sensitive (account_delete_btn, TRUE);
#if FALSE /* TODO */
    if (((int) GTK_CLIST (clist)->selection->data) == GTK_CLIST (clist)->rows-1)
      gtk_widget_set_sensitive (account_down_btn, FALSE);
    else
      gtk_widget_set_sensitive (account_down_btn, TRUE);
    if (((int) GTK_CLIST (clist)->selection->data) == 0)
      gtk_widget_set_sensitive (account_up_btn, FALSE);
    else
      gtk_widget_set_sensitive (account_up_btn, TRUE);
#endif
  } else {
    gtk_widget_set_sensitive (account_edit_btn, FALSE);
    gtk_widget_set_sensitive (account_delete_btn, FALSE);
#if FALSE /* TODO */
    gtk_widget_set_sensitive (account_up_btn, FALSE);
    gtk_widget_set_sensitive (account_down_btn, FALSE);
#endif
  }
}

static void on_account_edit (void) {
  char *name;
  Account *account;

  if (!g_list_length (GTK_CLIST (account_clist)->selection)) return;
  gtk_clist_get_text (GTK_CLIST (account_clist), (gint) GTK_CLIST (account_clist)->selection->data,
		  	0, &name);
  g_return_if_fail (name);
  
  account = search_account_acc_name (account_list, name);
  g_return_if_fail (account);

  account_new_or_edit (account);
}

static void on_account_delete (void) {
  Account *prev=NULL;
  Account *curr=NULL;
  Account *next=NULL;
  char *name;
  int iteration;
  
  if (!g_list_length (GTK_CLIST (account_clist)->selection)) return;
  gtk_clist_get_text (GTK_CLIST (account_clist), (gint) GTK_CLIST (account_clist)->selection->data,
		  	0, &name);
  if (!name) return;
  
  for (iteration = 0, curr = account_list; curr; curr = curr->next, iteration++) {
    if (iteration != 0) {
      if (!prev) {
	 prev = account_list;
       } else {
	 prev = prev->next;
       }
    }
    if (!strcmp (curr->acc_name, name)) break;
  }

  if (!curr) return;
  
  next = curr->next;

  /* Delete from the linked list */
  if (prev == NULL) account_list = next;
  else prev->next = next;

  if (curr->acc_name) c2_free (curr->acc_name);
  if (curr->per_name) c2_free (curr->per_name);
  if (curr->type == C2_ACCOUNT_POP) {
    if (curr->protocol.pop.usr_name) c2_free (curr->protocol.pop.usr_name);
    if (curr->protocol.pop.pass) c2_free (curr->protocol.pop.pass);
    if (curr->protocol.pop.host) c2_free (curr->protocol.pop.host);
  } else if (curr->type == C2_ACCOUNT_SPOOL) {
    if (curr->protocol.spool.file) c2_free (curr->protocol.spool.file);
  }
  if (curr->smtp) c2_free (curr->smtp);
  c2_free (curr);

  account_update_clist ();
  on_account_clist_select_row_or_unselect_row (account_clist);
}

#if FALSE /* TODO */
static void on_account_up_btn_clicked (void) {
  char *acc_name;
  Account *account;
  guint nth;

  g_return_if_fail (GTK_CLIST (account_clist)->selection);

  nth = (guint) GTK_CLIST (account_clist)->selection->data;
  gtk_clist_get_text (GTK_CLIST (account_clist), nth, 0, &acc_name);
  if (!acc_name) return;
  account = account_copy (search_account_acc_name (account_list, acc_name));
  if (!account) return;
  account_remove_nth (&account_list, nth);
  account_insert (&account_list, account, nth-1);
  gtk_clist_freeze (GTK_CLIST (account_clist));
  gtk_clist_row_move (GTK_CLIST (account_clist), nth, nth-1);
  gtk_clist_thaw (GTK_CLIST (account_clist));
  on_account_clist_select_row_or_unselect_row (account_clist);
}

static void on_account_down_btn_clicked (void) {
  char *acc_name;
  Account *account;
  guint nth;

  g_return_if_fail (GTK_CLIST (account_clist)->selection);

  nth = (guint) GTK_CLIST (account_clist)->selection->data;
  gtk_clist_get_text (GTK_CLIST (account_clist), nth, 0, &acc_name);
  if (!acc_name) return;
  account = account_copy (search_account_acc_name (account_list, acc_name));
  if (!account) return;
  account_remove_nth (&account_list, nth);
  account_insert (&account_list, account, nth+1);
  gtk_clist_freeze (GTK_CLIST (account_clist));
  gtk_clist_row_move (GTK_CLIST (account_clist), nth, nth+1);
  gtk_clist_thaw (GTK_CLIST (account_clist));
  on_account_clist_select_row_or_unselect_row (account_clist);
}
#endif

static void prefs_add_page_accounts (void) {
  GtkWidget *hbox;
  GtkWidget *scroll;
  GtkWidget *hsep;
  GtkWidget *vbox;
  GtkWidget *btn;
  GtkWidget *xpm;
  GtkWidget *tmp_box;
  GtkWidget *label;
 
  account_list = account_copy_linked_list (config->account_head);
  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 1);

  /************
   ** Scroll **
   ************/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /*******************
   ** Account Clist **
   *******************/
  account_clist = gtk_clist_new (4);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), account_clist);
  gtk_widget_show (account_clist);
  gtk_clist_set_column_title (GTK_CLIST (account_clist), 0, _("Account Name"));
  gtk_clist_set_column_title (GTK_CLIST (account_clist), 1, _("Type"));
  gtk_clist_set_column_title (GTK_CLIST (account_clist), 2, _("Name"));
  gtk_clist_set_column_title (GTK_CLIST (account_clist), 3, _("E-Mail"));
  gtk_clist_column_titles_show (GTK_CLIST (account_clist));
  gtk_signal_connect (GTK_OBJECT (account_clist), "select_row",
      			GTK_SIGNAL_FUNC (on_account_clist_select_row_or_unselect_row), NULL);
  gtk_signal_connect (GTK_OBJECT (account_clist), "unselect_row",
      			GTK_SIGNAL_FUNC (on_account_clist_select_row_or_unselect_row), NULL);
  account_update_clist (); 

  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
  gtk_widget_show (vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /*********
   ** New **
   *********/
  tmp_box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

  xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_NEW, 16, 16);
  label = gtk_label_new (_("New"));

  gtk_box_pack_start (GTK_BOX (tmp_box), xpm, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (tmp_box), label, FALSE, FALSE, 3);

  gtk_widget_show (xpm);
  gtk_widget_show (label);
  gtk_widget_show (tmp_box);
  
  btn = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (btn), tmp_box);
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, TRUE, 0);
  gtk_widget_show (btn);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (btn, "clicked", accel_group,
                              GDK_N, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (account_ask_account_type), NULL);

  /**********
   ** Edit **
   **********/
  tmp_box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

  xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_INDEX, 16, 16);
  label = gtk_label_new (_("Edit"));

  gtk_box_pack_start (GTK_BOX (tmp_box), xpm, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (tmp_box), label, FALSE, FALSE, 3);

  gtk_widget_show (xpm);
  gtk_widget_show (label);
  gtk_widget_show (tmp_box);
  
  account_edit_btn = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (account_edit_btn), tmp_box);
  gtk_box_pack_start (GTK_BOX (vbox), account_edit_btn, FALSE, TRUE, 0);
  gtk_widget_show (account_edit_btn);
  gtk_widget_set_sensitive (account_edit_btn, FALSE);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (account_edit_btn, "clicked", accel_group,
                              GDK_E, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (account_edit_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_account_edit), NULL);

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  /************
   ** Delete **
   ************/
  tmp_box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

  xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_TRASH, 16, 16);
  label = gtk_label_new (_("Delete"));

  gtk_box_pack_start (GTK_BOX (tmp_box), xpm, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (tmp_box), label, FALSE, FALSE, 3);

  gtk_widget_show (xpm);
  gtk_widget_show (label);
  gtk_widget_show (tmp_box);
  
  account_delete_btn = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (account_delete_btn), tmp_box);
  gtk_box_pack_start (GTK_BOX (vbox), account_delete_btn, FALSE, TRUE, 0);
  gtk_widget_show (account_delete_btn);
  gtk_widget_set_sensitive (account_delete_btn, FALSE);
  gtk_widget_set_events (btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (account_delete_btn, "clicked", accel_group,
                              GDK_D, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (account_delete_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_account_delete), NULL);
#if FALSE /* TODO */
  /**********
   ** HSep **
   **********/
  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  /********
   ** Up **
   ********/
  tmp_box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

  xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_UP, 16, 16);
  label = gtk_label_new (_("Up"));

  gtk_box_pack_start (GTK_BOX (tmp_box), xpm, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (tmp_box), label, FALSE, FALSE, 3);

  gtk_widget_show (xpm);
  gtk_widget_show (label);
  gtk_widget_show (tmp_box);
  
  account_up_btn = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (account_up_btn), tmp_box);
  gtk_box_pack_start (GTK_BOX (vbox), account_up_btn, FALSE, TRUE, 0);
  gtk_widget_show (account_up_btn);
  gtk_widget_set_sensitive (account_up_btn, FALSE);
  gtk_widget_set_events (account_up_btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (account_up_btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (account_up_btn, "clicked", accel_group,
                              GDK_KP_4, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (account_up_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_account_up_btn_clicked), NULL);

  /**********
   ** Down **
   **********/
  tmp_box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tmp_box), 2);

  xpm = gnome_stock_pixmap_widget_at_size (WMain->window, GNOME_STOCK_PIXMAP_DOWN, 16, 16);
  label = gtk_label_new (_("Down"));

  gtk_box_pack_start (GTK_BOX (tmp_box), xpm, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (tmp_box), label, FALSE, FALSE, 3);

  gtk_widget_show (xpm);
  gtk_widget_show (label);
  gtk_widget_show (tmp_box);
  
  account_down_btn = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (account_down_btn), tmp_box);
  gtk_box_pack_start (GTK_BOX (vbox), account_down_btn, FALSE, TRUE, 0);
  gtk_widget_show (account_down_btn);
  gtk_widget_set_sensitive (account_down_btn, FALSE);
  gtk_widget_set_events (account_down_btn, GDK_KEY_PRESS_MASK);
  gtk_widget_set_extension_events (account_down_btn, GDK_EXTENSION_EVENTS_ALL);
  gtk_widget_add_accelerator (account_down_btn, "clicked", accel_group,
                              GDK_KP_1, 0,
                              GTK_ACCEL_VISIBLE);
  gtk_signal_connect (GTK_OBJECT (account_down_btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_account_down_btn_clicked), NULL);
#endif
  prefs_append_page (hbox, _("Accounts"), ctree_mail);
  gtk_widget_show (hbox);
}

/*******************************************************************
 *******************************************************************
 ****			     MAILBOXES				****
 *******************************************************************
 *******************************************************************/
static char *mailbox_selected_mbox = NULL;

static void on_mailboxes_tree_select_row (GtkCTree *ctree, gint row, gint col,
    							GdkEvent *event, gpointer data) {
  GtkCTreeNode *node;
  Mailbox *mbox;

  node = gtk_ctree_node_nth(ctree, row);
  mbox = gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node);

  if (!mbox) {
    return;
  }

  mailbox_selected_mbox = mbox->name;
}

static void on_mailboxes_tree_unselect_row (void) {
  mailbox_selected_mbox = NULL;
}

static void on_mailboxes_new_btn_clicked (void) {
  Mailbox *parent=NULL;
  Mailbox *new;
  char *new_mbox_name;
  
  /* Check that the user has written a mbox name */
  new_mbox_name = gtk_entry_get_text (GTK_ENTRY (mailboxes_new_mailbox_entry));
  if (!new_mbox_name) return;
  if (!strlen (new_mbox_name)) return;

  /* Check that is different from all others mailboxes */
  if (search_mailbox_name (mailboxes_list, new_mbox_name)) {
    g_warning (_("The mailbox name %s is already taken."), new_mbox_name);
    return;
  }

  if (mailbox_selected_mbox) {
    /* Search for the selected mailbox in the linked list */
    parent = search_mailbox_name (mailboxes_list, mailbox_selected_mbox);
    if (!parent) {
      g_warning (_("Couldn't found %s in the list\n"), mailbox_selected_mbox);
      return;
    }
  }
  
  /* Create the new node */
  new			= g_new0 (Mailbox, 1);
  new->name		= g_strdup (new_mbox_name);
  new->self_id		= c2_mailbox_next_avaible_id (mailboxes_list);
  if (parent) {
    new->parent_id	= parent->self_id;
  } else {
    new->parent_id	= new->self_id;
  }
  new->higgest_mid	= 0;
  new->next		= NULL;
  new->child		= NULL;

  if (parent) {
    /* Append the new item */
    if (!parent->child) {
      parent->child = new;
    } else {
      parent = search_mailbox_last_element (parent->child);
      parent->next = new;
    }
  } else {
    parent = search_mailbox_last_element (mailboxes_list);
    parent->next = new;
  }
  
  /* Reload the tree */
  gtk_clist_freeze (GTK_CLIST (mailboxes_tree));
  gtk_clist_clear (GTK_CLIST (mailboxes_tree));
  create_mailbox_tree (window, mailboxes_tree, NULL, mailboxes_list);
  gtk_clist_thaw (GTK_CLIST (mailboxes_tree));
}

static void on_mailboxes_delete_btn_clicked (void) {
  Mailbox *search_mbox;
  Mailbox *working;
  Mailbox *parent;
  Mailbox *prev;
  Mailbox *next=NULL;
  Mailbox *curr;
  
  /* Check if there's some tree item selected */
  if (!mailbox_selected_mbox) return;

  /* Check that it's not an special mailbox */
  if (!strcmp (mailbox_selected_mbox, MAILBOX_INBOX) ||
      !strcmp (mailbox_selected_mbox, MAILBOX_DRAFTS) ||
      !strcmp (mailbox_selected_mbox, MAILBOX_OUTBOX) ||
      !strcmp (mailbox_selected_mbox, MAILBOX_QUEUE) ||
      !strcmp (mailbox_selected_mbox, MAILBOX_GARBAGE)) {
    g_warning (_("You can't delete the mailbox %s because it's a special mailbox\n"), mailbox_selected_mbox);
    return;
  }

  /* Search for the selected mailbox in the linked list */
  search_mbox = search_mailbox_name (mailboxes_list, mailbox_selected_mbox);
  if (!search_mbox) {
    g_warning (_("Couldn't found %s in the list\n"), mailbox_selected_mbox);
    return;
  }

  if (search_mbox->self_id != search_mbox->parent_id) {
    parent = search_mailbox_id (mailboxes_list, search_mbox->parent_id);
    if (!parent) return;
    working = parent->child;
  } else {
    working = mailboxes_list;
  }

  prev = working;
  if (!strcmp (prev->name, mailbox_selected_mbox)) {
    curr = prev;
    next = curr->next;
    parent->child = next;
  } else {
    for (; prev->next; prev = prev->next) {
      if (!strcmp (prev->next->name, mailbox_selected_mbox)) {
	curr = prev->next;
	next = curr->next;
	break;
      }
    }
    prev->next = next;
  }
  
  /* Reload the tree */
  gtk_clist_freeze (GTK_CLIST (mailboxes_tree));
  gtk_clist_clear (GTK_CLIST (mailboxes_tree));
  create_mailbox_tree (window, mailboxes_tree, NULL, mailboxes_list);
  gtk_clist_thaw (GTK_CLIST (mailboxes_tree));
  mailbox_selected_mbox = NULL;
}

static void prefs_add_page_mailboxes (void) {
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *scroll;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *btn;
  
  /* The work must be done in a copy of the mailboxes linked list */
  mailboxes_list = NULL;
  mailboxes_list = c2_mailbox_copy_linked_list (config->mailbox_head);
  mailbox_selected_mbox = NULL;

  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Mailboxes"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /************
   ** Scroll **
   ************/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /******************
   ** Mailbox Tree **
   ******************/
  mailboxes_tree = gtk_ctree_new (1, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), mailboxes_tree);
  gtk_widget_show (mailboxes_tree);
  gtk_signal_connect(GTK_OBJECT(mailboxes_tree), "select_row", GTK_SIGNAL_FUNC(on_mailboxes_tree_select_row), NULL);
  gtk_signal_connect(GTK_OBJECT(mailboxes_tree), "unselect_row", GTK_SIGNAL_FUNC(on_mailboxes_tree_unselect_row), NULL);
  create_mailbox_tree (window, mailboxes_tree, NULL, mailboxes_list);

  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  /***********
   ** Label **
   ***********/
  label = gtk_label_new (_("New Mailbox Name: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /***********
   ** Entry **
   ***********/
  mailboxes_new_mailbox_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), mailboxes_new_mailbox_entry, TRUE, TRUE, 0);
  gtk_widget_show (mailboxes_new_mailbox_entry);

  /****************
   ** Add Button **
   ****************/
  btn = gtk_button_new_with_label (_("Add Mailbox"));
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_mailboxes_new_btn_clicked), NULL);

  /*******************
   ** Delete Button **
   *******************/
  btn = gtk_button_new_with_label (_("Delete Mailbox"));
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, TRUE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_mailboxes_delete_btn_clicked), NULL);
 
  prefs_append_page (vbox, _("Mailboxes"), ctree_mail);
  gtk_widget_show (vbox);
}

/*******************************************************************
 *******************************************************************
 ****			     OPTIONS				****
 *******************************************************************
 *******************************************************************/
static void prefs_add_page_options (void) {
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *scale;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkAdjustment *adj;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Check for mail timeout (in minutes)"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  /************************
   ** Mail Check Timeout **
   ************************/
  options_mail_check = GTK_ADJUSTMENT (gtk_adjustment_new (config->check_timeout, 0, 100, 1, 0, 0));
  scale = gtk_hscale_new (options_mail_check);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_widget_show (scale);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Seconds before marking a mail as readed"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  /******************
   ** Mark as read **
   ******************/
  options_mark_as_read = GTK_ADJUSTMENT (gtk_adjustment_new (config->mark_as_read, 0, 10, 1, 0, 0));
  scale = gtk_hscale_new (options_mark_as_read);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_widget_show (scale);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Repling and forwarding"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);
  
  /***********
   ** Label **
   ***********/
  label = gtk_label_new (_("String to prepend to the original mail: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  /*****************************
   ** Prepend Character Entry **
   *****************************/
  options_prepend_character_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), options_prepend_character_entry, FALSE, TRUE, 0);
  gtk_widget_show (options_prepend_character_entry);
  gtk_entry_set_text (GTK_ENTRY (options_prepend_character_entry), config->prepend_char_on_re);

  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /***********
   ** label **
   ***********/
  label = gtk_label_new (_("Ask before downloading messages bigger than "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  
  /******************************************
   ** Do not download messages bigger than **
   ******************************************/
  adj = (GtkAdjustment *) gtk_adjustment_new (config->message_bigger, 0, 10000, 1, 100, 0);
  options_message_bigger = gtk_spin_button_new (adj, 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), options_message_bigger, FALSE, TRUE, 0);
  gtk_widget_show (options_message_bigger);

  /***********
   ** label **
   ***********/
  label = gtk_label_new (" KB");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label); 

  /*******************
   ** Empty Garbage **
   *******************/
  options_empty_garbage = gtk_check_button_new_with_label (_("Empty Garbage on exit"));
  gtk_box_pack_start (GTK_BOX (vbox), options_empty_garbage, FALSE, FALSE, 0);
  gtk_widget_show (options_empty_garbage);
  if (config->empty_garbage) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options_empty_garbage), TRUE);

  /*****************
   ** Keep a copy **
   *****************/
  options_keep_copy = gtk_check_button_new_with_label (_("Store a copy of sent mails in the Outbox mailbox"));
  gtk_box_pack_start (GTK_BOX (vbox), options_keep_copy, FALSE, FALSE, 0);
  gtk_widget_show (options_keep_copy);
  if (config->use_outbox) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options_keep_copy), TRUE);

  /** Check for mail at start **/
  options_check_at_start = gtk_check_button_new_with_label (_("Check for mail at start up."));
  gtk_box_pack_start (GTK_BOX (vbox), options_check_at_start, FALSE, FALSE, 0);
  gtk_widget_show (options_check_at_start);
  if (config->check_at_start) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options_check_at_start), TRUE);
  gtk_tooltips_set_tip (tooltips, options_check_at_start,
      		_("Whether to check activated accounts when Cronos II starts or not."), NULL);  
  
  prefs_append_page (vbox, _("Options"), ctree_general);
  gtk_widget_show (vbox);
}

/*******************************************************************
 *******************************************************************
 ****			     APPAREANCE				****
 *******************************************************************
 *******************************************************************/
static void on_appareance_font_ok_btn_clicked (GtkWidget* btn, gpointer data) {
  PreferencesFont *widget;
  char *font;
  GtkStyle *style, *style2;
  
  widget = (PreferencesFont *)data;
  font = gtk_font_selection_get_font_name (GTK_FONT_SELECTION (widget->font_sel));

  style = gtk_widget_get_style (widget->entry);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load (font);
  gtk_widget_set_style (widget->entry, style2);
  gtk_entry_set_text (GTK_ENTRY (widget->entry), font);
  gtk_widget_destroy (widget->window);
  c2_free (widget);
}

static void on_appareance_font_cancel_btn_clicked (GtkWidget* btn, gpointer data) {
  PreferencesFont *widget;
  
  widget = (PreferencesFont *)data;
  
  gtk_widget_destroy (widget->window);
  c2_free (widget);
}

static void on_appareance_font_btn_clicked (GtkWidget *button, gpointer data) {
  PreferencesFont *widgets;
  GtkWidget *btn;
  GtkWidget *vbox;
  GtkWidget *hsep;
  GtkWidget *hbox;

  widgets = g_new0 (PreferencesFont, 1);

  widgets->entry = (GtkWidget *) data;
  if (!GTK_IS_ENTRY (widgets->entry)) return;
  
  widgets->window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_window_set_title (GTK_WINDOW (widgets->window), _("Font Selection"));
  gtk_window_set_modal (GTK_WINDOW (widgets->window), TRUE);
  gtk_window_set_policy (GTK_WINDOW (widgets->window), FALSE, FALSE, FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (widgets->window), GTK_WINDOW (WMain->window));
  gtk_container_set_border_width (GTK_CONTAINER (widgets->window), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (widgets->window), vbox);
  gtk_widget_show (vbox);

  widgets->font_sel = gtk_font_selection_new ();
  gtk_box_pack_start (GTK_BOX (vbox), widgets->font_sel, TRUE, TRUE, 0);
  gtk_font_selection_set_font_name (GTK_FONT_SELECTION (widgets->font_sel),
		  gtk_entry_get_text (GTK_ENTRY (widgets->entry)));
  gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (widgets->font_sel), _("Peter was in the field"));
  gtk_widget_show (widgets->font_sel);

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
  gtk_box_pack_end (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_appareance_font_cancel_btn_clicked), (gpointer) widgets);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
  gtk_box_pack_end (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_appareance_font_ok_btn_clicked), (gpointer) widgets);

  gtk_widget_show (widgets->window);
}

static void prefs_add_page_interface (void) {
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *_vbox;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Customize application's title"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /**********
   ** VBox **
   **********/
  _vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (frame), _vbox);
  gtk_widget_show (_vbox);

  /*****************
   ** Title Entry **
   *****************/
  appareance_title = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (_vbox), appareance_title, FALSE, TRUE, 0);
  gtk_widget_show (appareance_title);
  gtk_entry_set_text (GTK_ENTRY (appareance_title), rc->title);

  /***********
   ** Label **
   ***********/
  label = gtk_label_new (_("Conversion chars:\n%a = App Name (Cronos II),\n%v = Version,\n%m = Messages in selected mailbox,\n%n = New messages in selected mailbox,\n%M = Selected mailbox."));
  gtk_box_pack_start (GTK_BOX (_vbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);

  /***********
   ** Frame **
   ***********/
  frame = gtk_frame_new (_("Toolbar"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /**********
   ** VBox **
   **********/
  _vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), _vbox);
  gtk_widget_show (_vbox);

  /*******************
   ** Radio Buttons **
   *******************/
  appareance_toolbar_both = gtk_radio_button_new_with_label (NULL, _("Icons and text"));
  gtk_box_pack_start (GTK_BOX (_vbox), appareance_toolbar_both, FALSE, TRUE, 0);
  gtk_widget_show (appareance_toolbar_both);
  if (rc->toolbar == GTK_TOOLBAR_BOTH)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (appareance_toolbar_both), TRUE);

  appareance_toolbar_xpm = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (appareance_toolbar_both)),
				_("Icons only"));
  gtk_box_pack_start (GTK_BOX (_vbox), appareance_toolbar_xpm, FALSE, TRUE, 0);
  gtk_widget_show (appareance_toolbar_xpm);
  if (rc->toolbar == GTK_TOOLBAR_ICONS)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (appareance_toolbar_xpm), TRUE);

  appareance_toolbar_text = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (appareance_toolbar_xpm)),
				_("Text only"));
  gtk_box_pack_start (GTK_BOX (_vbox), appareance_toolbar_text, FALSE, TRUE, 0);
  gtk_widget_show (appareance_toolbar_text);
  if (rc->toolbar == GTK_TOOLBAR_TEXT)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (appareance_toolbar_text), TRUE);

  {
    char *label[2] = { _("Interface"), NULL };
    
    ctree_interface = gtk_ctree_insert_node (GTK_CTREE (sections_ctree), NULL, NULL, label, 4,
				NULL, NULL, NULL, NULL, FALSE, TRUE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, gtk_label_new (_("Interface")));
  }
  gtk_widget_show (vbox);
}

/* Fonts */
static void prefs_add_page_fonts (void) {
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *btn;
  GtkStyle *style, *style2;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /***********
   ** Table **
   ***********/
  table = gtk_table_new (4, 3, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);

  /************
   ** Labels **
   ************/
  label = gtk_label_new (_("Message Body"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Unread Message List"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Read Message List"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (label);
  
  label = gtk_label_new (_("Printing"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (label);

  /*************
   ** Entries **
   *************/
  appareance_font_message_body = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_font_message_body, 1, 2, 0, 1,
		  	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_font_message_body);
  gtk_entry_set_editable (GTK_ENTRY (appareance_font_message_body), FALSE);
  gtk_entry_set_text (GTK_ENTRY (appareance_font_message_body), rc->font_body);
  style = gtk_widget_get_style (appareance_font_message_body);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load (rc->font_body);
  gtk_widget_set_style (appareance_font_message_body, style2);

  appareance_font_unread_message_list = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_font_unread_message_list, 1, 2, 1, 2,
		  	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_font_unread_message_list);
  gtk_entry_set_editable (GTK_ENTRY (appareance_font_unread_message_list), FALSE);
  gtk_entry_set_text (GTK_ENTRY (appareance_font_unread_message_list), rc->font_unread);
  style = gtk_widget_get_style (appareance_font_unread_message_list);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load (rc->font_unread);
  gtk_widget_set_style (appareance_font_unread_message_list, style2);

  appareance_font_read_message_list = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_font_read_message_list, 1, 2, 2, 3,
		  	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_font_read_message_list);
  gtk_entry_set_editable (GTK_ENTRY (appareance_font_read_message_list), FALSE);
  gtk_entry_set_text (GTK_ENTRY (appareance_font_read_message_list), rc->font_read);
  style = gtk_widget_get_style (appareance_font_read_message_list);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load (rc->font_read);
  gtk_widget_set_style (appareance_font_read_message_list, style2);

  appareance_font_print = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_font_print, 1, 2, 3, 4,
		  	(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_font_print);
  gtk_entry_set_editable (GTK_ENTRY (appareance_font_print), FALSE);
  gtk_entry_set_text (GTK_ENTRY (appareance_font_print), rc->font_print);
  style = gtk_widget_get_style (appareance_font_print);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load (rc->font_print);
  gtk_widget_set_style (appareance_font_print, style2);

  /*************
   ** Buttons **
   *************/
  btn = gnome_stock_button (GNOME_STOCK_BUTTON_FONT);
  gtk_table_attach (GTK_TABLE (table), btn, 2, 3, 0, 1,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		GTK_SIGNAL_FUNC (on_appareance_font_btn_clicked), (gpointer) appareance_font_message_body);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_FONT);
  gtk_table_attach (GTK_TABLE (table), btn, 2, 3, 1, 2,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
			GTK_SIGNAL_FUNC (on_appareance_font_btn_clicked),
			(gpointer) appareance_font_unread_message_list);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_FONT);
  gtk_table_attach (GTK_TABLE (table), btn, 2, 3, 2, 3,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
			GTK_SIGNAL_FUNC (on_appareance_font_btn_clicked),
			(gpointer) appareance_font_read_message_list);
  gtk_widget_show (btn);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_FONT);
  gtk_table_attach (GTK_TABLE (table), btn, 2, 3, 3, 4,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
			GTK_SIGNAL_FUNC (on_appareance_font_btn_clicked),
			(gpointer) appareance_font_print);
  gtk_widget_show (btn);

  prefs_append_page (vbox, _("Fonts"), ctree_interface);
  gtk_widget_show (vbox);
}

/* Colors */
static void prefs_add_page_colors (void) {
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *viewport;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /***********
   ** Table **
   ***********/
  table = gtk_table_new (4, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 1);
  
  /*****************************
   ** Replying and forwarding **
   *****************************/
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), viewport, 0, 2, 0, 1,
		  	(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_OUT);
  label = gtk_label_new (_("Replying and forwarding"));
  gtk_container_add (GTK_CONTAINER (viewport), label);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  gtk_widget_show (label);

  /************
   ** Labels **
   ************/
  label = gtk_label_new (_("Original message:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.06, 0.5);
  gtk_widget_show (label);

  /************
   ** Colors **
   ************/
  appareance_colors_reply_original_message = gnome_color_picker_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_colors_reply_original_message, 1, 2, 1, 2,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_colors_reply_original_message);
  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (appareance_colors_reply_original_message),
			config->color_reply_original_message.red,
			config->color_reply_original_message.green,
			config->color_reply_original_message.blue, 0);

  /*******************
   ** Miscellaneous **
   *******************/
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), viewport, 0, 2, 2, 3,
		  	(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_OUT);
  label = gtk_label_new (_("Miscellaneous"));
  gtk_container_add (GTK_CONTAINER (viewport), label);
  gtk_misc_set_alignment (GTK_MISC (label), 7.45058e-09, 0.5);
  gtk_widget_show (label);

  /************
   ** Labels **
   ************/
  label = gtk_label_new (_("Message body text color:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		  	(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.06, 0.5);
  gtk_widget_show (label);

  /************
   ** Colors **
   ************/
  appareance_colors_misc_body = gnome_color_picker_new ();
  gtk_table_attach (GTK_TABLE (table), appareance_colors_misc_body, 1, 2, 3, 4,
		  	(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 0, 0);
  gtk_widget_show (appareance_colors_misc_body);
  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (appareance_colors_misc_body),
			config->color_misc_body.red,
			config->color_misc_body.green,
			config->color_misc_body.blue, 0);

  prefs_append_page (vbox, _("Colors"), ctree_interface);
  gtk_widget_show (vbox);
}

/*******************************************************************
 *******************************************************************
 ****			     ADVANCED				****
 *******************************************************************
 *******************************************************************/
static void
on_advanced_use_persistent_smtp_connection_toggled (void) {
  if (GTK_TOGGLE_BUTTON (advanced_use_persistent_smtp_connection)->active) {
    gtk_widget_set_sensitive (advanced_persistent_smtp_address, TRUE);
    gtk_widget_set_sensitive (advanced_persistent_smtp_port, TRUE);
  } else {
    gtk_widget_set_sensitive (advanced_persistent_smtp_address, FALSE);
    gtk_widget_set_sensitive (advanced_persistent_smtp_port, FALSE);
  }
}

static void prefs_add_page_advanced (void) {
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkAdjustment *adj;
  GtkWidget *menu, *menuitem;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /***********
   ** label **
   ***********/
  label = gtk_label_new (_("Timeout for net related stuff: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  
  /***********************************
   ** Timeout for net related stuff **
   ***********************************/
  adj = (GtkAdjustment *) gtk_adjustment_new (config->timeout, 0, 150, 1, 100, 0);
  advanced_timeout = gtk_spin_button_new (adj, 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), advanced_timeout, FALSE, FALSE, 0);
  gtk_widget_show (advanced_timeout);
#if !(defined(HAVE_SETSOCKOPT) && defined(SO_RCVTIMEO))
  gtk_widget_set_sensitive (advanced_timeout, FALSE);
#endif

  /***********
   ** label **
   ***********/
  label = gtk_label_new (_(" seconds."));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  /** HBox **/
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);
  
  /** Use persistent SMTP connection **/
  advanced_use_persistent_smtp_connection = gtk_check_button_new_with_label (
      				_("Use persistent SMTP connection: "));
  gtk_box_pack_start (GTK_BOX (hbox), advanced_use_persistent_smtp_connection, FALSE, FALSE, 0);
  gtk_widget_show (advanced_use_persistent_smtp_connection);
  if (config->use_persistent_smtp_connection)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_use_persistent_smtp_connection), TRUE);
  gtk_tooltips_set_tip (tooltips, advanced_use_persistent_smtp_connection,
      			_("Keep connected to the SMTP host to use again that connection to send "
			  "outgoing messages"), NULL);

  /** Persistent SMTP address **/
  advanced_persistent_smtp_address = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), advanced_persistent_smtp_address, TRUE, TRUE, 0);
  gtk_widget_show (advanced_persistent_smtp_address);
  if (config->persistent_smtp_address) gtk_entry_set_text (GTK_ENTRY (advanced_persistent_smtp_address),
      					config->persistent_smtp_address);
  if (!config->use_persistent_smtp_connection)
    gtk_widget_set_sensitive (advanced_persistent_smtp_address, FALSE);
  gtk_tooltips_set_tip (tooltips, advanced_persistent_smtp_address,
      			_("Persistant connection to the SMTP host"), NULL);

  /** Port **/
  label = gtk_label_new (_("Port: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /** Persistent SMTP port **/
  adj = (GtkAdjustment *) gtk_adjustment_new (config->persistent_smtp_port, 1, 99999, 1, 100, 0);
  advanced_persistent_smtp_port = gtk_spin_button_new (adj, 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), advanced_persistent_smtp_port, FALSE, FALSE, 0);
  gtk_widget_show (advanced_persistent_smtp_port);
  if (!config->use_persistent_smtp_connection)
    gtk_widget_set_sensitive (advanced_persistent_smtp_port, FALSE);
  gtk_tooltips_set_tip (tooltips, advanced_persistent_smtp_port,
      			_("Persistant connection to the SMTP host"), NULL);

  gtk_signal_connect (GTK_OBJECT (advanced_use_persistent_smtp_connection), "toggled",
      				GTK_SIGNAL_FUNC (on_advanced_use_persistent_smtp_connection_toggled), NULL);

#ifdef BUILD_ADDRESS_BOOK
  /** HBox **/
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 2);
  gtk_widget_show (hbox);
  
  /** Label **/
  label = gtk_label_new (_("Initialize Address Book"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /** Initialize Address Book **/
  advanced_addrbook_init = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (hbox), advanced_addrbook_init, FALSE, FALSE, 0);
  gtk_widget_show (advanced_addrbook_init);
  if (config->use_persistent_smtp_connection)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_use_persistent_smtp_connection), TRUE);
  gtk_tooltips_set_tip (tooltips, advanced_use_persistent_smtp_connection,
      			_("Keep connected to the SMTP host to use again that connection to send "
			  "outgoing messages"), NULL);
  menu = gtk_menu_new ();
  menuitem = gtk_menu_item_new_with_label (_("at start."));
  gtk_widget_show (menuitem);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  menuitem = gtk_menu_item_new_with_label (_("when required."));
  gtk_widget_show (menuitem);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  menuitem = gtk_menu_item_new_with_label (_("when it is opened."));
  gtk_widget_show (menuitem);
  gtk_menu_append (GTK_MENU (menu), menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (advanced_addrbook_init), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (advanced_addrbook_init), config->addrbook_init);
  gtk_widget_set_usize (advanced_addrbook_init, -1, 23);
#endif

  prefs_append_page (vbox, _("Advanced"), NULL);
  gtk_widget_show (vbox);
}

/*******************************************************************
 *******************************************************************
 ****			     PLUG-INS				****
 *******************************************************************
 *******************************************************************/
#if USE_PLUGINS
static void
on_plugins_refresh (void) {
  char *title[2] = { NULL, NULL };
  const C2DynamicModule *s;

  gtk_clist_freeze (GTK_CLIST (plugins_loaded_clist));
  gtk_clist_clear (GTK_CLIST (plugins_loaded_clist));
  for (s = config->module_head; s; s = s->next) {
    title[0] = s->name;
    gtk_clist_append (GTK_CLIST (plugins_loaded_clist), title);
  }
  gtk_clist_thaw (GTK_CLIST (plugins_loaded_clist));  

  gtk_label_set_text (GTK_LABEL (plugins_label_name), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_version), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_author), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_url), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_description), "");
  gtk_widget_set_sensitive (plugins_unload_btn, FALSE);
  gtk_widget_set_sensitive (plugins_loaded_btn_configure, FALSE);
}

static void
on_plugins_loaded_clist_select_row (GtkWidget *widget, gint row, gint column,
		GdkEventButton *event, gpointer data) {
  C2DynamicModule *s;
  int i;

  for (s = config->module_head, i = 0; i < row && s; s = s->next, i++);
  if (!s) return;

  gtk_widget_set_sensitive (plugins_unload_btn, TRUE);
  if (s->configure) gtk_widget_set_sensitive (plugins_loaded_btn_configure, TRUE);
  else gtk_widget_set_sensitive (plugins_loaded_btn_configure, FALSE);
  gtk_label_set_text (GTK_LABEL (plugins_label_name), s->name);
  gtk_label_set_text (GTK_LABEL (plugins_label_version), s->version);
  gtk_label_set_text (GTK_LABEL (plugins_label_author), s->author);
  gtk_label_set_text (GTK_LABEL (plugins_label_url), s->url);
  gtk_label_set_text (GTK_LABEL (plugins_label_description), s->description);
}

static void
on_plugins_loaded_clist_unselect_row (void) {
  gtk_widget_set_sensitive (plugins_unload_btn, FALSE);
  gtk_widget_set_sensitive (plugins_loaded_btn_configure, FALSE);
  gtk_label_set_text (GTK_LABEL (plugins_label_name), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_version), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_author), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_url), "");
  gtk_label_set_text (GTK_LABEL (plugins_label_description), "");
}

static void
on_plugins_load_btn_clicked (void) {
  char *dir;
  char *buf, *buf2;
  char *file = gui_select_file_new (DATADIR "/cronosII/plugins", NULL, "*.so", NULL);

  if (!file) return;

  dir = g_strconcat (getenv ("HOME"), ROOT, "/Plugins/", NULL);
  if (!c2_file_exists (dir)) {
    if (mkdir (dir, 0700) < 0) {
      buf2 = g_strerror (errno);
      buf = g_strdup_printf (_("Couldn't create the directory %s: %s"), dir, buf2);
      c2_free (dir);
      gnome_dialog_run_and_close (GNOME_DIALOG (gnome_ok_dialog (buf)));
      return;
    }
  }
  c2_free (dir);
  dir = g_strconcat (getenv ("HOME"), ROOT, "/Plugins/", g_basename (file), NULL);
  if (c2_dynamic_module_load (file, &config->module_head)) fd_bin_cp (file, dir);
  on_plugins_refresh ();
}

static void
on_plugins_unload_btn_clicked (void) {
  char *name = NULL;

  if (!GTK_CLIST (plugins_loaded_clist)->selection) return;

  gtk_clist_get_text (GTK_CLIST (plugins_loaded_clist),
      			(int) GTK_CLIST (plugins_loaded_clist)->selection->data, 0, &name);
  if (!name) return;
 
  c2_dynamic_module_unload (name, &config->module_head);
  on_plugins_refresh ();
}

static void
on_on_plugins_configure_btn_clicked_destroy (GtkWidget *widget) {
  gtk_window_set_modal (GTK_WINDOW (widget), FALSE);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
}

static void
on_plugins_configure_btn_clicked (void) {
  C2DynamicModule *module;
  char *name = NULL;
  GtkWidget *widget;

  if (!GTK_CLIST (plugins_loaded_clist)->selection) return;

  gtk_clist_get_text (GTK_CLIST (plugins_loaded_clist),
      			(int) GTK_CLIST (plugins_loaded_clist)->selection->data, 0, &name);
  if (!name) return;
 
  module = c2_dynamic_module_find (name, config->module_head);
  if (!module) return;
  widget = module->configure (module);
  if (widget) {
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (widget));
    gtk_window_set_modal (GTK_WINDOW (window), FALSE);
    gtk_window_set_modal (GTK_WINDOW (widget), TRUE);
    gtk_signal_connect (GTK_OBJECT (widget), "destroy",
			GTK_SIGNAL_FUNC (on_on_plugins_configure_btn_clicked_destroy), NULL);
  }
}

static void prefs_add_page_plugins (void) {
  GtkWidget *vbox, *vbox2;
  GtkWidget *hbox, *hbox2;
  GtkWidget *pixmap;
  GtkWidget *btn;
  GtkWidget *viewport;
  GtkWidget *scroll;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *table;
  GtkStyle *style, *style2;
  
  /**********
   ** VBox **
   **********/
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  /**********
   ** HBox **
   **********/
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /** Frame **/
  frame = gtk_frame_new (_("Loaded Plug-Ins"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /** Hbox2 **/
  hbox2 = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox2);
  gtk_widget_show (hbox2);

  /** Scroll **/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox2), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  
  /** Loaded_clist **/
  plugins_loaded_clist = gtk_clist_new (1);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), plugins_loaded_clist);
  gtk_widget_show (plugins_loaded_clist);
  gtk_signal_connect (GTK_OBJECT (plugins_loaded_clist), "select_row",
      			GTK_SIGNAL_FUNC (on_plugins_loaded_clist_select_row), NULL);
  gtk_signal_connect (GTK_OBJECT (plugins_loaded_clist), "unselect_row",
      			GTK_SIGNAL_FUNC (on_plugins_loaded_clist_unselect_row), NULL);

  /** Vbox2 **/
  vbox2 = gtk_vbox_new (TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, TRUE, 0);
  gtk_widget_show (vbox2);

  /** plugins_load_btn **/
  btn = gtk_button_new ();
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
  pixmap = gnome_stock_pixmap_widget_at_size (window, GNOME_STOCK_PIXMAP_FORWARD, 16, 16);
  label = gtk_label_new (_("Load..."));
  gtk_box_pack_start (GTK_BOX (hbox2), pixmap, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 3);
  gtk_container_add (GTK_CONTAINER (btn), hbox2);
  gtk_box_pack_start (GTK_BOX (vbox2), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_widget_show (pixmap);
  gtk_widget_show (hbox2);
  gtk_widget_show (label);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_plugins_load_btn_clicked), NULL);

  /** plugins_unload_btn **/
  plugins_unload_btn = gtk_button_new ();
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
  pixmap = gnome_stock_pixmap_widget_at_size (window, GNOME_STOCK_PIXMAP_BACK, 16, 16);
  label = gtk_label_new (_("Unload"));
  gtk_box_pack_start (GTK_BOX (hbox2), pixmap, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 3);
  gtk_container_add (GTK_CONTAINER (plugins_unload_btn), hbox2);
  gtk_box_pack_start (GTK_BOX (vbox2), plugins_unload_btn, FALSE, FALSE, 0);
  gtk_widget_show (plugins_unload_btn);
  gtk_widget_show (pixmap);
  gtk_widget_show (hbox2);
  gtk_widget_show (label);
  gtk_widget_set_sensitive (plugins_unload_btn, FALSE);
  gtk_signal_connect (GTK_OBJECT (plugins_unload_btn), "clicked",
      			GTK_SIGNAL_FUNC (on_plugins_unload_btn_clicked), NULL);

  /** plugins_loaded_btn_configure **/
  plugins_loaded_btn_configure = gtk_button_new ();
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
  pixmap = gnome_stock_pixmap_widget_at_size (window, GNOME_STOCK_PIXMAP_INDEX, 16, 16);
  label = gtk_label_new (_("Configure"));
  gtk_box_pack_start (GTK_BOX (hbox2), pixmap, FALSE, FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 3);
  gtk_container_add (GTK_CONTAINER (plugins_loaded_btn_configure), hbox2);
  gtk_box_pack_start (GTK_BOX (vbox2), plugins_loaded_btn_configure, FALSE, FALSE, 0);
  gtk_widget_show (plugins_loaded_btn_configure);
  gtk_widget_show (pixmap);
  gtk_widget_show (hbox2);
  gtk_widget_show (label);
  gtk_widget_set_sensitive (plugins_loaded_btn_configure, FALSE);
  gtk_signal_connect (GTK_OBJECT (plugins_loaded_btn_configure), "clicked",
      			GTK_SIGNAL_FUNC (on_plugins_configure_btn_clicked), NULL);
  
  /** Frame **/
  frame = gtk_frame_new (_("Information"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);
  
  /** Scroll **/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /** Viewport **/
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), viewport);
  gtk_widget_show (viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  
  /** Table **/
  table = gtk_table_new (5, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (viewport), table);
  gtk_widget_show (table);

  /** Name **/
  label = gtk_label_new (_("Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
      			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0);
  gtk_widget_show (label);
  style = gtk_widget_get_style (label);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
  gtk_widget_set_style (label, style2);
  plugins_label_name = gtk_label_new ("");
  gtk_widget_show (plugins_label_name);
  gtk_table_attach (GTK_TABLE (table), plugins_label_name, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (plugins_label_name), TRUE);
  gtk_misc_set_alignment (GTK_MISC (plugins_label_name), 0, 0);

  /** Version **/
  label = gtk_label_new (_("Version:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
      			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0);
  gtk_widget_show (label);
  gtk_widget_set_style (label, style2);
  plugins_label_version = gtk_label_new ("");
  gtk_widget_show (plugins_label_version);
  gtk_table_attach (GTK_TABLE (table), plugins_label_version, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (plugins_label_version), TRUE);
  gtk_misc_set_alignment (GTK_MISC (plugins_label_version), 0, 0);

  /** Author **/
  label = gtk_label_new (_("Author:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
      			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0);
  gtk_widget_show (label);
  gtk_widget_set_style (label, style2);
  plugins_label_author = gtk_label_new ("");
  gtk_widget_show (plugins_label_author);
  gtk_table_attach (GTK_TABLE (table), plugins_label_author, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (plugins_label_author), TRUE);
  gtk_misc_set_alignment (GTK_MISC (plugins_label_author), 0, 0);

  /** URL **/
  label = gtk_label_new (_("URL:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
      			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0);
  gtk_widget_show (label);
  gtk_widget_set_style (label, style2);
  plugins_label_url = gtk_label_new ("");
  gtk_widget_show (plugins_label_url);
  gtk_table_attach (GTK_TABLE (table), plugins_label_url, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (plugins_label_url), TRUE);
  gtk_misc_set_alignment (GTK_MISC (plugins_label_url), 0, 0);

  /** Description **/
  label = gtk_label_new (_("Description:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
      			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0);
  gtk_widget_show (label);
  gtk_widget_set_style (label, style2);
  plugins_label_description = gtk_label_new ("");
  gtk_widget_show (plugins_label_description);
  gtk_table_attach (GTK_TABLE (table), plugins_label_description, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (plugins_label_description), TRUE);
  gtk_misc_set_alignment (GTK_MISC (plugins_label_description), 0, 0);

  on_plugins_refresh ();

  prefs_append_page (vbox, _("Plug-Ins"), NULL);
  gtk_widget_show (vbox);
}
#endif
