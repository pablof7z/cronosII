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
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <config.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "main.h"
#include "gui-composer.h"
#include "gui-utils.h"
#include "gui-decision_dialog.h"
#include "gui-select_file.h"
#include "gui-window_main.h"

#if USE_PLUGINS
#  include "plugin.h"
#endif
#include "message.h"
#include "rc.h"
#include "account.h"
#include "init.h"
#include "mailbox.h"
#include "utils.h"
#include "error.h"
#include "search.h"
#include "index.h"
#include "smtp.h"
#include "addrbook.h"
#ifdef BUILD_ADDRESS_BOOK
#include "autocompletition.h"
#endif

#include "xpm/sendqueue.xpm"

typedef enum {
  REQUIRE_NOTHING	= 0,
  REQUIRE_DRAFTS	= 1 << 1,
  REQUIRE_QUEUE		= 1 << 2
} Requires;

static void
make_menubar						(WidgetsComposer *widget);

static void
make_toolbar						(WidgetsComposer *widget);

static Message *
make_message						(WidgetsComposer *widget, Requires require);

static void
on_icon_list_drag_data_received				(GtkWidget *widget, GdkDragContext *context, int x,
    							 gint y, GtkSelectionData *data, guint info,
							 guint time);
static void
on_send_clicked						(GtkWidget *w, WidgetsComposer *widget);

static void
on_sendqueue_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_sendqueue_clicked_thread 				(WidgetsComposer *widget);

static void
on_save_clicked						(GtkWidget *w, WidgetsComposer *widget);

static void
on_save_as_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_attachs_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_icon_list_select_icon				(GtkWidget *w, int num, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_icon_list_unselect_icon				(GtkWidget *w, int num, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_attachs_add_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_attachs_remove_clicked				(GtkWidget *w, WidgetsComposer *widget);

static void
on_focus_in_event					(void);

static void
on_close_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_delete_event						(GtkWidget *w, GdkEvent *event,
    							 WidgetsComposer *widget);

static void
on_insert_file_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_select_all_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_clear_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_body_changed						(GtkWidget *w, WidgetsComposer *widget);

static void
on_contact_btn_clicked					(GtkWidget *w, GtkWidget *entry);

static void
on_view_account_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_to_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_cc_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_bcc_activate					(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_subject_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_view_priority_activate				(GtkWidget *w, WidgetsComposer *widget);

static void
on_insert_file_clicked					(GtkWidget *w, WidgetsComposer *widget);

static void
on_insert_signature_clicked				(GtkWidget *w, WidgetsComposer *widget);

static GtkTargetEntry target_table_text_plain[] = {
         { "text/plain", 0, 0 }
};

void
c2_composer_new (Message *message, C2ComposerType type) {
  WidgetsComposer *widget;
  MimeHash *mime;
  GtkWidget *btn;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *tmp_toolbar_icon;
  GtkWidget *toolbar;
  GtkWidget *viewport;
  GtkStyle *style, *style2;
  char *buf, *buf2;
  GList *list = NULL;
  Account *account;
  int i;
  GtkWidget *scroll;
#ifdef BUILD_ADDRESS_BOOK
  extern GCompletion *EMailCompletion;
#endif
  GtkTooltips *tooltips = gtk_tooltips_new ();

  /* Parse the message */
  if (message) {
    message_mime_parse (message, NULL);
    if (message->mime) {
      mime = message_mime_get_default_part (message->mime);
      message_mime_get_part (mime);
    } else mime = NULL;
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_OPEN, message, "composer", NULL, NULL, NULL);
#endif
  }
  
  widget = C2_WIDGETS_COMPOSER_NEW;
  widget->message = message;
  widget->sending = FALSE;
  widget->type = type;
  if (message && type == C2_COMPOSER_DRAFTS)
    widget->drafts_mid = message->mid;
  else
    widget->drafts_mid = -1;

  widget->window = gnome_app_new ("Message", _("Composer"));
  gtk_widget_realize (widget->window);
  gtk_widget_set_usize (widget->window, rc->main_window_width, rc->main_window_height);
  gtk_signal_connect (GTK_OBJECT (widget->window), "delete_event",
      			GTK_SIGNAL_FUNC (on_delete_event), widget);
  gtk_signal_connect (GTK_OBJECT (widget->window), "focus_in_event",
      			GTK_SIGNAL_FUNC (on_focus_in_event), NULL);
  gtk_window_set_policy (GTK_WINDOW (widget->window), TRUE, TRUE, FALSE);

  make_menubar (widget);
  make_toolbar (widget);

  /** vbox **/
  vbox = gtk_vbox_new (FALSE, 2);
  gnome_app_set_contents (GNOME_APP (widget->window), vbox);
  gtk_widget_show (vbox);

  /** header_notebook */
  widget->header_notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox), widget->header_notebook, FALSE, TRUE, 0);
  gtk_widget_show (widget->header_notebook);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget->header_notebook), FALSE);

  /** header_titles */
  table = gtk_table_new (4, 4, FALSE);
  gtk_notebook_append_page (GTK_NOTEBOOK (widget->header_notebook), table, NULL);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);
  widget->header_table = table;

  /** labels **/
  label = gtk_label_new (_("Account:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4) gtk_widget_show (label);
  widget->header_titles[HEADER_TITLES_ACCOUNT][0] = label;
  
  label = gtk_label_new (_("To:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0) gtk_widget_show (label);
  widget->header_titles[HEADER_TITLES_TO][0] = label;

  label = gtk_label_new (_("CC:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5) gtk_widget_show (label);
  widget->header_titles[HEADER_TITLES_CC][0] = label;

  label = gtk_label_new (_("BCC:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6) gtk_widget_show (label);
  widget->header_titles[HEADER_TITLES_BCC][0] = label;

  label = gtk_label_new (_("Subject:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3) gtk_widget_show (label);
  widget->header_titles[HEADER_TITLES_SUBJECT][0] = label;

  widget->header_titles[HEADER_TITLES_ACCOUNT][1] = gtk_combo_new ();
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_ACCOUNT][1], 1, 3, 0, 1,
      				GTK_EXPAND | GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
    gtk_widget_show (widget->header_titles[HEADER_TITLES_ACCOUNT][1]);
  gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry),
      				FALSE);
  for (account = config->account_head, i = 1; account; account = account->next, i++) {
    buf = g_strdup_printf (_("%d. %s <%s>"), i, account->per_name, account->mail_addr);
    list = g_list_append (list, buf);
  }
  gtk_combo_set_popdown_strings (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1]), list);
  if (message) {
    buf = message_get_header_field (message, NULL, "X-CronosII-Account:");
    if (buf) {
      for (account = config->account_head, i = 1; account; account = account->next, i++)
	if (streq (account->acc_name, buf)) break;
      if (account) {
	c2_free (buf);
	buf = g_strdup_printf (_("%d. %s <%s>"), i, account->per_name, account->mail_addr);
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry),
	    			buf);
      }
    }
  }
  widget->header_titles[HEADER_TITLES_TO][1] = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_TO][1], 1, 2, 1, 2,
      				GTK_EXPAND | GTK_FILL, 0, 0, 0);

  /* To::Contact button */
  widget->header_titles[HEADER_TITLES_TO][2] = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (widget->header_titles[HEADER_TITLES_TO][2]),
      			gnome_stock_pixmap_widget_at_size (table, GNOME_STOCK_PIXMAP_BOOK_GREEN, 14, 14));
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_TO][2], 2, 3, 1, 2, 0, 0, 0, 0);
  gtk_button_set_relief (GTK_BUTTON (widget->header_titles[HEADER_TITLES_TO][2]), GTK_RELIEF_NONE);
  gtk_tooltips_set_tip (tooltips, widget->header_titles[HEADER_TITLES_TO][2],
      					_("Use Address Book to add contact"), NULL);
  gtk_signal_connect (GTK_OBJECT (widget->header_titles[HEADER_TITLES_TO][2]), "clicked",
      				GTK_SIGNAL_FUNC (on_contact_btn_clicked),
				widget->header_titles[HEADER_TITLES_TO][1]);
  
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_TO][1]);
    gtk_widget_show_all (widget->header_titles[HEADER_TITLES_TO][2]);
  }
  
  if (message) {
    if (type == C2_COMPOSER_REPLY || type == C2_COMPOSER_REPLY_ALL) {
      buf = message_get_header_field (message, NULL, "\nReply-To:");
      
      /** Mailing-list stuff. **/
      /** Patch by Daniel Fairhead **/
      if (!buf) buf = message_get_header_field (message, NULL, "\nMail-Followup-To:");
      if (!buf) buf = message_get_header_field (message, NULL, "\nX-Loop:"); 
      if (!buf) buf = message_get_header_field (message, NULL, "\nList-Post:"); 

      if (!buf) buf = message_get_header_field (message, NULL, "\nFrom:");
      if (buf) {
	buf = str_replace(buf,"<mailto:","<");
	gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_TO][1]), buf);
      }
    }
    else if (type == C2_COMPOSER_DRAFTS) {
      buf = message_get_header_field (message, NULL, "\nTo:");
      if (buf) {
	gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_TO][1]), buf);
      }
    }
  } else /** Patch by Jeremy Witt **/
    gtk_widget_grab_focus(widget->header_titles[HEADER_TITLES_TO][1]);

#ifdef BUILD_ADDRESS_BOOK
  /** This is for the autocompletition stuff in the to field **/
  gtk_signal_connect_after (GTK_OBJECT (widget->header_titles[HEADER_TITLES_TO][1]),
      				"key_press_event",
				GTK_SIGNAL_FUNC (autocompletition_key_pressed), widget);
#endif

  widget->header_titles[HEADER_TITLES_CC][1] = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_CC][1], 1, 2, 2, 3,
      				GTK_EXPAND | GTK_FILL, 0, 0, 0);
  /* CC::Contact button */
  widget->header_titles[HEADER_TITLES_CC][2] = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (widget->header_titles[HEADER_TITLES_CC][2]),
      			gnome_stock_pixmap_widget_at_size (table, GNOME_STOCK_PIXMAP_BOOK_RED, 14, 14));
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_CC][2], 2, 3, 2, 3, 0, 0, 0, 0);
  gtk_button_set_relief (GTK_BUTTON (widget->header_titles[HEADER_TITLES_CC][2]), GTK_RELIEF_NONE);
  gtk_tooltips_set_tip (tooltips, widget->header_titles[HEADER_TITLES_CC][2],
      					_("Use Address Book to add contact"), NULL);
  gtk_signal_connect (GTK_OBJECT (widget->header_titles[HEADER_TITLES_CC][2]), "clicked",
      				GTK_SIGNAL_FUNC (on_contact_btn_clicked),
				widget->header_titles[HEADER_TITLES_CC][1]);
  
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_CC][1]);
    gtk_widget_show_all (widget->header_titles[HEADER_TITLES_CC][2]);
  }
  if (message) {
    if (type == C2_COMPOSER_REPLY_ALL) {
      buf = message_get_header_field (message, NULL, "\nCC:");
      if (buf) {
	gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_CC][1]), buf);
      }
    }
    else if (type == C2_COMPOSER_DRAFTS) {
      buf = message_get_header_field (message, NULL, "\nCC:");
      if (buf) {
	gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_CC][1]), buf);
      }
    }
  }
#ifdef BUILD_ADDRESS_BOOK
  /** This is for the autocompletition stuff in the cc field **/
  gtk_signal_connect_after (GTK_OBJECT (widget->header_titles[HEADER_TITLES_CC][1]),
      				"key_press_event",
				GTK_SIGNAL_FUNC (autocompletition_key_pressed), widget);
#endif

  widget->header_titles[HEADER_TITLES_BCC][1] = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_BCC][1], 1, 2, 3, 4,
      				GTK_EXPAND | GTK_FILL, 0, 0, 0); 
  /* BCC::Contact button */
  widget->header_titles[HEADER_TITLES_BCC][2] = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (widget->header_titles[HEADER_TITLES_BCC][2]),
			gnome_stock_pixmap_widget_at_size (table, GNOME_STOCK_PIXMAP_BOOK_YELLOW, 14, 14));
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_BCC][2], 2, 3, 3, 4, 0, 0, 0, 0);
  gtk_button_set_relief (GTK_BUTTON (widget->header_titles[HEADER_TITLES_BCC][2]), GTK_RELIEF_NONE);
  gtk_tooltips_set_tip (tooltips, widget->header_titles[HEADER_TITLES_BCC][2],
      					_("Use Address Book to add contact"), NULL);
  gtk_signal_connect (GTK_OBJECT (widget->header_titles[HEADER_TITLES_BCC][2]), "clicked",
      				GTK_SIGNAL_FUNC (on_contact_btn_clicked),
				widget->header_titles[HEADER_TITLES_BCC][1]);

  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_BCC][1]);
    gtk_widget_show_all (widget->header_titles[HEADER_TITLES_BCC][2]);
  }
  
  if (message) {
    if (type == C2_COMPOSER_DRAFTS) {
      buf = message_get_header_field (message, NULL, "\nBCC:");
      if (buf) {
	gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_BCC][1]), buf);
      }
    }
  }
#ifdef BUILD_ADDRESS_BOOK
  /** This is for the autocompletition stuff in the bcc field **/
  gtk_signal_connect_after (GTK_OBJECT (widget->header_titles[HEADER_TITLES_BCC][1]),
      				"key_press_event",
				GTK_SIGNAL_FUNC (autocompletition_key_pressed), widget);
#endif

  widget->header_titles[HEADER_TITLES_SUBJECT][1] = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), widget->header_titles[HEADER_TITLES_SUBJECT][1], 1, 3, 4, 5,
      				GTK_EXPAND | GTK_FILL, 0, 0, 0);
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3)
    	gtk_widget_show (widget->header_titles[HEADER_TITLES_SUBJECT][1]);
  if (message) {
    buf = message_get_header_field (message, NULL, "\nSubject:");
    if (type == C2_COMPOSER_REPLY || type == C2_COMPOSER_REPLY_ALL) {
      buf2 = g_strdup_printf (_("Re: %s"), buf); /* Reply: %s */
      c2_free (buf);
      buf = buf2;
    }
    else if (type == C2_COMPOSER_FORWARD) {
      buf2 = g_strdup_printf (_("Fw: %s"), buf); /* Forward: %s */
      c2_free (buf);
      buf = buf2;
    }
    else if (type == C2_COMPOSER_DRAFTS) {
      buf = message_get_header_field (message, NULL, "\nSubject:");
    }
    if (buf) {
      gtk_entry_set_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_SUBJECT][1]), buf);
    }
  }

  /** attachs **/
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_notebook_append_page (GTK_NOTEBOOK (widget->header_notebook), hbox, NULL);
  gtk_widget_show (hbox);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  
  /** toolbar **/
  toolbar = gtk_toolbar_new (GTK_ORIENTATION_VERTICAL, GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (toolbar), TRUE);
  gtk_widget_show (toolbar);
  gtk_box_pack_start (GTK_BOX (hbox), toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_space_style (GTK_TOOLBAR (widget->toolbar), GTK_TOOLBAR_SPACE_LINE);
  gtk_toolbar_set_button_relief (GTK_TOOLBAR (widget->toolbar), GTK_RELIEF_NONE);
  gtk_toolbar_set_space_size (GTK_TOOLBAR (widget->toolbar), 2);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_ADD);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, NULL, N_("Add an attachment"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_attachs_add_clicked), (gpointer) widget);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_REMOVE);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, NULL, N_("Remove selected attachment"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_attachs_remove_clicked), (gpointer) widget);

  /** viewport **/
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), viewport, TRUE, TRUE, 0);
  gtk_widget_show (viewport);
  
  /** scroll **/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (viewport), scroll);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /** icon_list **/
  widget->icon_list = gnome_icon_list_new (70, NULL, 0);
  gtk_container_add (GTK_CONTAINER (scroll), widget->icon_list);
  gtk_widget_show (widget->icon_list);
  widget->selected_icon = -1;
  gtk_signal_connect (GTK_OBJECT (widget->icon_list), "select_icon",
      			GTK_SIGNAL_FUNC (on_icon_list_select_icon), widget);
  gtk_signal_connect (GTK_OBJECT (widget->icon_list), "unselect_icon",
      			GTK_SIGNAL_FUNC (on_icon_list_unselect_icon), widget);
  gtk_drag_dest_set (widget->icon_list,
                    GTK_DEST_DEFAULT_ALL,
                    target_table_text_plain, 1,
                    GDK_ACTION_COPY);
  gtk_signal_connect (GTK_OBJECT (widget->icon_list), "drag_data_received",
                     GTK_SIGNAL_FUNC (on_icon_list_drag_data_received),
                     NULL);
  
  /** scroll **/
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  /** body **/
  widget->body = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), widget->body);
  gtk_widget_show (widget->body);
  style = gtk_widget_get_style (widget->body);
  style2 = gtk_style_copy (style);
  style2->font = font_body;
  gtk_widget_set_style (widget->body, style2);

  if (type != C2_COMPOSER_DRAFTS && type != C2_COMPOSER_NEW) {
    gtk_text_freeze (GTK_TEXT (widget->body));
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, "\n\n", -1);
    gtk_text_thaw (GTK_TEXT (widget->body));
  }
  gdk_threads_leave ();
  gtk_text_set_editable (GTK_TEXT (widget->body), TRUE);
  gtk_text_set_word_wrap (GTK_TEXT (widget->body), 2);
  widget->body_changed = FALSE;
  widget->changed_signal = gtk_signal_connect (GTK_OBJECT (widget->body), "changed",
      					GTK_SIGNAL_FUNC (on_body_changed), widget);

  if (message && type != C2_COMPOSER_DRAFTS) {
    int len = 0;
    char *date, *from;
    
    gdk_threads_leave ();
    date = message_get_header_field (message, NULL, "\nDate:");
    from = message_get_header_field (message, NULL, "From:");
    if (date && from && strlen (date) && strlen (from)) 
      buf = g_strdup_printf (_("On %s, %s wrote:\n"), date, from);
    else if (!date && strlen (from))
      buf = g_strdup_printf (_("%s wrote:\n"), from);
    else
      buf = g_strdup_printf (_("-- Original Message --\n"));
    c2_free (date);
    c2_free (from);
    message_get_message_body (message, NULL);
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (widget->body));
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_reply_original_message, NULL, buf, -1);
    gdk_threads_leave ();
    for (buf = (mime) ? mime->part : message->body;;) {
      if ((buf2 = str_get_line (buf+len)) == NULL) break;
      len+=strlen (buf2);
      buf2 = g_strchomp (buf2);
      gdk_threads_enter ();
      gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_reply_original_message, NULL, config->prepend_char_on_re, -1);
      gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_reply_original_message, NULL, buf2, -1);
      gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, "\n", -1);
      gtk_editable_set_position (GTK_EDITABLE (widget->body), 0);
      gdk_threads_leave ();
    }
    gdk_threads_enter ();
    gtk_text_thaw (GTK_TEXT (widget->body));
  }
  else if (type == C2_COMPOSER_DRAFTS) {
    message_get_message_body (message, NULL);
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, message->body, -1);
  }

  widget->appbar = gnome_appbar_new (TRUE, TRUE, GNOME_PREFERENCES_NEVER);
  gtk_box_pack_start (GTK_BOX (vbox), widget->appbar, FALSE, TRUE, 0);
  gtk_widget_show (widget->appbar);
  gtk_widget_show (widget->window);

  if (type == C2_COMPOSER_DRAFTS || type == C2_COMPOSER_REPLY || type == C2_COMPOSER_REPLY_ALL)
    gtk_widget_grab_focus (widget->body); /** Patch by Jeremy Witt **/

#ifdef BUILD_ADDRESS_BOOK
  /* Make competitions list */
  if (!EMailCompletion)
    autocompletition_generate_emailcompletion ();
#endif
}

/************************
 ****** VIEW MENU *******
 ************************/
static GnomeUIInfo viewmenu[] = {
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Account"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("To"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("CC"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("BCC"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Subject"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Priority"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

static GnomeUIInfo filemenu[] = {
  {
    GNOME_APP_UI_ITEM, N_("_Send"),
    N_("Send this message"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_MAIL_SND,
    GDK_Return, GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Send _Later"),
    N_("Send this message later"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_MAIL_SND,
    GDK_Return, GDK_SHIFT_MASK, NULL,
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_SAVE_ITEM (NULL, NULL),
  {
    GNOME_APP_UI_ITEM, N_("Save to file"),
    N_("Save to file"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS,
    GDK_S, GDK_MOD1_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CLOSE_ITEM (NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

static GnomeUIInfo insertmenu[] = {
  {
    GNOME_APP_UI_ITEM, N_("_File"),
    N_("Insert a file"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    GDK_O, GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Signature"),
    N_("Insert signature"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
    GDK_S, GDK_MOD1_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Attachment"),
    N_("Insert an attachment"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ATTACH,
    GDK_A, GDK_MOD1_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

static GnomeUIInfo editmenu[] = {
  GNOMEUIINFO_MENU_SELECT_ALL_ITEM (NULL, NULL),
  GNOMEUIINFO_MENU_CLEAR_ITEM (NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_SUBTREE, N_("_Insert"),
    NULL, insertmenu, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, N_("_Insert"),
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar[] = {
  GNOMEUIINFO_MENU_FILE_TREE (filemenu),
  GNOMEUIINFO_MENU_EDIT_TREE (editmenu),
  GNOMEUIINFO_MENU_VIEW_TREE (viewmenu),
  GNOMEUIINFO_END
};

static void
make_menubar (WidgetsComposer *widget) {
  GtkWidget *menu;

  g_return_if_fail (widget);

  gnome_app_create_menus (GNOME_APP (widget->window), menubar);
  widget->menu_send = filemenu[0].widget;
  gtk_signal_connect (GTK_OBJECT (widget->menu_send), "activate",
      			GTK_SIGNAL_FUNC (on_send_clicked), widget);
  widget->menu_sendqueue = filemenu[1].widget;
  menu = filemenu[1].widget;
  gtk_signal_connect (GTK_OBJECT (widget->menu_sendqueue), "activate",
      				GTK_SIGNAL_FUNC (on_sendqueue_clicked), widget);
  menu = filemenu[3].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_save_clicked), widget);
  menu = filemenu[4].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_save_as_clicked), widget);
  menu = filemenu[6].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_close_clicked), widget);
  menu = insertmenu[0].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_insert_file_clicked), widget);
  menu = insertmenu[1].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_insert_signature_clicked), widget);
  menu = insertmenu[2].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_attachs_add_clicked), widget);
  menu = editmenu[0].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_select_all_clicked), widget);
  menu = editmenu[1].widget;
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_clear_clicked), widget);
  menu = viewmenu[0].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_account_activate), widget);
  menu = viewmenu[1].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_to_activate), widget);
  menu = viewmenu[2].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_cc_activate), widget);
  menu = viewmenu[3].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_bcc_activate), widget);
  menu = viewmenu[4].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_subject_activate), widget);
  menu = viewmenu[5].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 7)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu), TRUE);
  gtk_signal_connect (GTK_OBJECT (menu), "activate",
      			GTK_SIGNAL_FUNC (on_view_priority_activate), widget);
  gtk_widget_set_sensitive (menu, FALSE);
}

static void
make_toolbar (WidgetsComposer *widget) {
  GtkWidget *tmp_toolbar_icon;
  GtkWidget *btn;
  GdkPixmap *icon;
  GdkBitmap *mask;
  
  g_return_if_fail (widget);

  widget->toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, rc->toolbar);
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (widget->toolbar), TRUE);
  gtk_widget_show (widget->toolbar);
  gnome_app_add_toolbar (GNOME_APP (widget->window), GTK_TOOLBAR (widget->toolbar), "toolbar",
      				GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
                                GNOME_DOCK_TOP, 1, 0, 94);
  gtk_toolbar_set_space_style (GTK_TOOLBAR (widget->toolbar), GTK_TOOLBAR_SPACE_LINE);
  gtk_toolbar_set_button_relief (GTK_TOOLBAR (widget->toolbar), GTK_RELIEF_NONE);
  gtk_toolbar_set_space_size (GTK_TOOLBAR (widget->toolbar), 3);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_MAIL_SND);
  widget->tool_send = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("Send"), _("Send this message"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (widget->tool_send), "clicked",
		  	GTK_SIGNAL_FUNC (on_send_clicked), (gpointer) widget);

  icon = gdk_pixmap_create_from_xpm_d(WMain->window->window,
      					&mask, &WMain->window->style->white, sendqueue_xpm);
  tmp_toolbar_icon = gtk_pixmap_new(icon, mask);
  widget->tool_sendqueue = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                 GTK_TOOLBAR_CHILD_BUTTON,
                                 NULL, _("Send to queue"), _("Send this message to the queue folder"), NULL,
                                 tmp_toolbar_icon, NULL, NULL);
   gtk_signal_connect (GTK_OBJECT (widget->tool_sendqueue), "clicked",
	 		  	GTK_SIGNAL_FUNC (on_sendqueue_clicked), (gpointer) widget);
  
  gtk_toolbar_append_space (GTK_TOOLBAR (widget->toolbar));
  
  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_SAVE);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("Save"), _("Save this message to the Drafts mailbox"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_save_clicked), (gpointer) widget);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_SAVE_AS);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("To File"), _("Save this message to a file"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_save_as_clicked), (gpointer) widget);

  gtk_toolbar_append_space (GTK_TOOLBAR (widget->toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_OPEN);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("Insert"), _("Insert a file"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_insert_file_clicked), (gpointer) widget);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_PROPERTIES);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("Sign"), _("Sign this message"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_insert_signature_clicked), (gpointer) widget);

  gtk_toolbar_append_space (GTK_TOOLBAR (widget->toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_ATTACH);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
                                NULL, _("Attachs"), _("Show attachments list"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_attachs_clicked), (gpointer) widget);
  
  gtk_toolbar_append_space (GTK_TOOLBAR (widget->toolbar));
  
  tmp_toolbar_icon = gnome_stock_pixmap_widget (widget->window, GNOME_STOCK_PIXMAP_CLOSE);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (widget->toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL, _("Close"), _("Close this message"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
		  	GTK_SIGNAL_FUNC (on_close_clicked), (gpointer) widget);
}

static char *
make_message_boundary (void) {
  char *boundary = NULL;
  char *ptr;
  int i;

  srand (time (NULL));
  boundary = g_new0 (char, 50);
  sprintf (boundary, "Cronos-II=");
  ptr = boundary+10;
  for (i = 0; i < 39; i++) {
    *(ptr+i) = (rand () % 26)+97; /* From a to z */
  }
  if (*(ptr+i-1) == '-') *(ptr+i-1) = '.';
  *(ptr+i) = '\0';
  
  return boundary;
}

static char *
make_message_part (const char *filename, const char *body, const char *boundary) {
  char *part = NULL;

  g_return_val_if_fail (boundary, NULL);
  g_return_val_if_fail (!(!filename && !body), NULL);

  if (!filename) {
    part = g_strdup_printf ("--%s\n"
			    "Content-Type: text/plain\n" /* TODO Charset */
			    "Content-Transfer-Encoding: 8bit\n"
			    "Content-Disposition: inline\n"
			    "\n"
			    "%s\n",
			    boundary, body);
  } else {
    const char *content_type;
    char *content_disposition;
    char *buffer;
    char *enc;
    struct stat st;
    guint size;
    FILE *fd;

    /* File size */
    if (stat (filename, &st) < 0) return NULL;
    size = st.st_size;
    if (!size) return NULL;
    
    /* Content-Type */
    content_type = gnome_mime_type_or_default (filename, "application/octet-stream");
    if (streq (content_type, "application/octet-stream")) {
      content_type = g_strdup_printf ("application/octet-stream; name=\"%s\"", g_basename (filename));
    }

    /* Disposition */
    content_disposition = g_strdup_printf ("attachment; filename=\"%s\"", g_basename (filename));
    
    if ((fd = fopen (filename, "rt")) == NULL) return NULL;
    
    buffer = g_malloc0 (sizeof (char) * (size+1));
    fread (buffer, sizeof (char), size, fd);
    fclose (fd);
    chdir (getenv ("HOME"));
    buffer[size] = '\0';

    /* Encode */
    enc = encode_base64 (buffer, &size);
    c2_free (buffer);

    /* Part */
    part = g_strdup_printf ("--%s\n"
			    "Content-Type: %s\n"
			    "Content-Transfer-Encoding: base64\n"
			    "Content-Disposition: %s\n"
			    "\n"
			    "%s",
			    boundary, content_type, content_disposition, enc);
    c2_free (content_disposition);
    c2_free (enc);

  }

  return part;
}

static Message *
make_message (WidgetsComposer *widget, Requires require) {
  Message *message;
  char *buf;
  Account *account;
  char	*From = NULL,
  	*To = NULL,
  	*CC = NULL,
	*BCC = NULL,
	*In_Reply_To = NULL,
	*References = NULL,
	*Subject = NULL;
  char *boundary = NULL;
  char *body;
  char *filename;
  int i, account_n;
  int attachments = 0;
  GSList *parts = NULL;
  GSList *s;
  char *part;
  char *tmpfile;
  FILE *fd;
 
  /* Open the tmp file */
  tmpfile = cronos_tmpfile ();
  if ((fd = fopen (tmpfile, "w")) == NULL) {
    return NULL;
  }
  
  /* From */
  buf = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry));
  account_n = atoi (buf);
  if (!account_n) account_n = 1;
  
  for (i = 1, account = config->account_head; i < account_n && account; i++, account = account->next);
  if (!account) return NULL;
  From = g_strdup_printf ("%s <%s>", account->per_name, account->mail_addr);

  /* To */
  To = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_TO][1]));

  /* CC */
  CC = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_CC][1]));

  /* BCC */
  BCC = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_BCC][1]));

  /* Subject */
  Subject = gtk_entry_get_text (GTK_ENTRY (widget->header_titles[HEADER_TITLES_SUBJECT][1]));

  if (widget->type == C2_COMPOSER_REPLY || widget->type == C2_COMPOSER_REPLY_ALL) {
    /* In-Reply-To */
    In_Reply_To = message_get_header_field (widget->message, NULL, "\nMessage-ID:");
    
    /* References */
    buf = message_get_header_field (widget->message, NULL, "\nReferences:");
    if (In_Reply_To && buf)
      References = g_strdup_printf ("%s %s", In_Reply_To, buf);
  else if (In_Reply_To)
    References = g_strdup_printf ("%s", In_Reply_To);
    c2_free (buf);
  }

  /* Attachments */
  attachments = GNOME_ICON_LIST (widget->icon_list)->icons;
  
  /* Body */
  body = gtk_editable_get_chars (GTK_EDITABLE (widget->body), 0, -1);

  if (attachments) {
    boundary = make_message_boundary ();
   
    for (i = 0; i <= attachments; i++) {
      /* Get the filename */
      if (i) filename = gnome_icon_list_get_icon_data (GNOME_ICON_LIST (widget->icon_list), i-1);
      else filename = NULL;

      part = make_message_part (filename, body, boundary);
      if (part) parts = g_slist_append (parts, part);
      if (body) c2_free (body);
    }
  }
  
  fprintf (fd, "From: %s\n"
      	       "To: %s\n"
		"Subject: %s\n"
		"X-Mailer: " XMAILER "\n"
		"X-CronosII-Account: %s\n"
		"MIME-Version: 1.0\n"
		"Content-Type: %s%s%s"
		"%s",
		From, To, Subject, account->acc_name,
		attachments ? "multipart/mixed; boundary=\"" : "text/plain",
		attachments ? boundary : "",
		attachments ? "\"\n": "\n",
		attachments ? "" : "Content-Transfer-Encoding: 8bit\n");
  
  if (strlen (CC)) {
    fprintf (fd, "CC: %s\n", CC);
  }

  if (strlen (BCC)) {
    fprintf (fd, "BCC: %s\n", BCC);
  }

  if (In_Reply_To) {
    fprintf (fd, "In-Reply-To: %s\n", In_Reply_To);
    c2_free (In_Reply_To);
  }

  if (References) {
    fprintf (fd, "References: %s\n", References);
    c2_free (References);
  }

  /* Fields required by Drafts */
  if (require & REQUIRE_DRAFTS || require & REQUIRE_QUEUE) {
    if (widget->message) {
      fprintf (fd, "X-CronosII-Original-Mailbox: %s\n", widget->message->mbox);
      fprintf (fd, "X-CronosII-Original-MID: %d\n", widget->message->mid);
    }
    
    fprintf (fd, "X-CronosII-Message-Action: %d\n", widget->type);
  }

  fprintf (fd, "\n");

  if (attachments) {
    fprintf (fd, _("This is a multipart message in MIME format.\n"
	  	   "The fact that you reached this text means that your\n"
		   "mail client does not understand MIME messages and you will\n"
		   "not be able to understand the attachments. You should consider moving\n"
		   "to another mail client or to a higher version.\n"
		   "For further information, connect to http://cronosII.sourceforge.net\n"
		   "or contact cronosII@users.sourceforge.net\n"));
    for (s = parts; s; s = s->next) {
      fprintf (fd, CHAR (s->data));
      c2_free (s->data);
    }
    fprintf (fd, "--%s--\n", boundary);
    g_slist_free (parts);
  } else fprintf (fd, "%s", body);
  c2_free (From);
  c2_free (In_Reply_To);
  c2_free (References);
  c2_free (boundary);

  fclose (fd);
  
  message = message_get_message_from_file (tmpfile);
  if (widget->message) {
    message->mbox = widget->message->mbox;
    message->mid = widget->message->mid;
  }
  else {
    message->mbox = NULL;
    message->mid = 0;
  }

  return message;
}

static void
on_contact_btn_clicked (GtkWidget *w, GtkWidget *entry) {
  g_return_if_fail (GTK_IS_ENTRY (entry));

  autocompletation_display_contacts (entry);
}

static void
on_view_account_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 4;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_ACCOUNT][1]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_ACCOUNT][1]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void
on_view_to_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 0;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_TO][1]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_TO][2]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_TO][1]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_TO][2]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void
on_view_cc_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 5;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_CC][1]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_CC][2]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_CC][1]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_CC][2]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void
on_view_bcc_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 6;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_BCC][1]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_BCC][2]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_BCC][1]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_BCC][2]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void
on_view_subject_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 3;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_SUBJECT][1]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_SUBJECT][1]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void
on_view_priority_activate (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (w));
  g_return_if_fail (widget);
  rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] ^= 1 << 7;
  if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 7) {
    gtk_widget_show (widget->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_show (widget->header_titles[HEADER_TITLES_PRIORITY][1]);
  } else {
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_hide (widget->header_titles[HEADER_TITLES_PRIORITY][1]);
    gtk_widget_set_usize (widget->header_table, -1, -1);
  }
}

static void on_body_changed (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (widget);
  widget->body_changed = TRUE;
  gtk_signal_disconnect (GTK_OBJECT (widget->body), widget->changed_signal);
}

static void on_close_clicked (GtkWidget *w, WidgetsComposer *widget) {
  if (widget->sending) {
    gtk_widget_hide (widget->window);
    return;
  }
  if (widget->body_changed) {
    switch (gui_decision_dialog (_("Confirmation"),
		_("This message was modified.\nAre you sure you want to close it?"),
		3, _("Yes"), _("No"), _("Save"))) {
      case 0:
	break;
      case 1:
	return;
      case 2:
	on_save_clicked (w, widget);
	break;
    }
  }
  
  gtk_widget_destroy (widget->window);
  if (widget->message) {
    c2_free (widget->message->message);
    c2_free (widget->message->header);
    c2_free (widget->message->mbox);
    c2_free (widget->message);
  }
}

#if USE_PLUGINS
static void
on_focus_in_event_thread (void) {
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_WINDOW_FOCUS, "composer", NULL, NULL, NULL, NULL);
}

static void
on_focus_in_event (void) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_focus_in_event_thread), NULL);
}
#endif

static void
on_delete_event (GtkWidget *w, GdkEvent *event, WidgetsComposer *widget) {
  on_close_clicked (w, widget);
}

static void
add_dir (GtkWidget *widget, const char *dir_path) {
  DIR *directory;
  struct dirent *dir;
  char *path;
  const char *icon;
  GdkImlibImage *img;
  
  g_return_if_fail (widget);
  g_return_if_fail (dir_path);

  if ((directory = opendir (dir_path)) == NULL) {
    cronos_error (errno, _("Couldn't open the directory for adding the attachments"), ERROR_WARNING);
    return;
  }

  for (; (dir = readdir (directory)) != NULL;) {
    if (strneq (dir->d_name, ".", 1)) continue; /* Don't attach hidden files */
#ifndef G_DIR_SEPARATOR
#define G_DIR_SEPARATOR '/'
#endif
    if (*(dir_path+strlen (dir_path)-1) == G_DIR_SEPARATOR) {
      path = g_new0 (char, strlen (dir_path)+strlen (dir->d_name)+1);
    } else {
      path = g_new0 (char, strlen (dir_path)+strlen (dir->d_name)+2);
    }

    strcpy (path, dir_path);
    if (*(dir_path+strlen (dir_path)-1) != G_DIR_SEPARATOR) {
      path[strlen (dir_path)] = G_DIR_SEPARATOR;
      path[strlen (dir_path)+1] = 0;
    }
    strcat (path, dir->d_name);

    if (fd_is_dir (path)) {
      add_dir (widget, path);
      continue;
    }

    icon = pixmap_get_icon_by_mime_type (gnome_mime_type_or_default (path, "default"));
    img = gdk_imlib_load_image (CHAR (icon));
    
    gnome_icon_list_append_imlib (GNOME_ICON_LIST (widget), img, g_basename (path));

    gnome_icon_list_set_icon_data (GNOME_ICON_LIST (widget),
      					GNOME_ICON_LIST (widget)->icons-1,
					path);
  }

  closedir (directory);
}

static void
on_icon_list_drag_data_received (GtkWidget *widget, GdkDragContext *context, int x,
    				 gint y, GtkSelectionData *data, guint info,
				 guint time) {
  char *d;
  char *file, *ptr;
  const char *icon;
  GdkImlibImage *img;

  for (d = data->data;;) {
    if ((file = str_get_line (d)) == NULL) break;
    d += strlen (file);
    
    if (strnne (file, "file:", 5)) continue;
    if (*(file+strlen (file)-1) == '\n') g_strchomp (file);
    ptr = file+5;

    if (!c2_file_exists (ptr)) {
      c2_free (file);
      continue;
    }

    if (fd_is_dir (ptr)) {
      add_dir (widget, ptr);
    }

    icon = pixmap_get_icon_by_mime_type (gnome_mime_type_or_default (ptr, "default"));
    img = gdk_imlib_load_image (CHAR (icon));
    
    gnome_icon_list_append_imlib (GNOME_ICON_LIST (widget), img, g_basename (ptr));

    gnome_icon_list_set_icon_data (GNOME_ICON_LIST (widget),
      					GNOME_ICON_LIST (widget)->icons-1,
					ptr);
  }  
}

static void
on_send_clicked_thread (WidgetsComposer *widget) {
  Account *account;
  char *buf;
  char *signature;
  int account_n, i;
  Message *message;
  Pthread2 *helper;

  /* Append the signature */
  buf = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry));
  account_n = atoi (buf);
  if (!account_n) account_n = 1;
  
  for (i = 1, account = config->account_head; i < account_n && account; i++, account = account->next);
  if (account) {
    signature = g_strdup (account->signature);
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE,
					&signature, NULL, NULL, NULL, NULL);
#endif
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (widget->body));
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, signature, -1);
    gtk_text_thaw (GTK_TEXT (widget->body));
    gdk_threads_leave ();
  }

  gdk_threads_enter ();
  if (widget->type == C2_COMPOSER_DRAFTS)
    message = make_message (widget, REQUIRE_DRAFTS);
  else
    message = make_message (widget, REQUIRE_NOTHING);
  gdk_threads_leave ();

#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_COMPOSER_SEND, message, NULL, NULL, NULL, NULL);
#endif

  widget->sending = TRUE;
  gdk_threads_enter ();
  gtk_widget_set_sensitive (widget->tool_send, FALSE);
  gtk_widget_set_sensitive (widget->menu_send, FALSE);
  gdk_threads_leave ();
  
  helper = g_new0 (Pthread2, 1);
  helper->v1 = (gpointer) message;
  helper->v2 = (gpointer) widget;
  
  smtp_main (helper);
}

static int
on_send_clicked_timeout (WidgetsComposer *widget) {
  pthread_t thread;
  
  if (config->use_persistent_smtp_connection && smtp_is_busy ()) return TRUE;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_send_clicked_thread), widget);
  return FALSE;
}

static void
on_send_clicked (GtkWidget *w, WidgetsComposer *widget) {
  pthread_t thread;
  
  if (config->use_persistent_smtp_connection && smtp_is_busy ()) {
    gtk_widget_set_sensitive (widget->tool_send, FALSE);
    gtk_widget_set_sensitive (widget->menu_send, FALSE);
    gtk_timeout_add (5000, (GtkFunction) on_send_clicked_timeout, widget);
    return;
  }
  pthread_create (&thread, NULL, PTHREAD_FUNC (on_send_clicked_thread), widget);
}

static void
on_sendqueue_clicked_thread (WidgetsComposer *widget) {
  Account *account; /* Signature */
  char *buf;
  char *signature;
  int account_n, i;
  Message *message; /* Storage */
  Mailbox *mbox;
  FILE *fd;
  char *Subject, *To, Date[40], *c_account;
  time_t now;
  struct tm *ptr;
  
  /* Disable widgets */
  gdk_threads_enter ();
  gtk_widget_set_sensitive (widget->tool_send, FALSE);
  gtk_widget_set_sensitive (widget->menu_send, FALSE);
  gtk_widget_set_sensitive (widget->tool_sendqueue, FALSE);
  gtk_widget_set_sensitive (widget->menu_sendqueue, FALSE);
  gdk_threads_leave ();

  /* Append the signature */
  buf = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry));
  account_n = atoi (buf);
  if (!account_n) account_n = 1;
  
  for (i = 1, account = config->account_head; i < account_n && account; i++, account = account->next);
  if (account) {
    signature = g_strdup (account->signature);
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE,
					&signature, NULL, NULL, NULL, NULL);
#endif
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (widget->body));
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, signature, -1);
    gtk_text_thaw (GTK_TEXT (widget->body));
    gdk_threads_leave ();
  }

  g_return_if_fail (widget);
  message = make_message (widget, REQUIRE_QUEUE);

  mbox = search_mailbox_name (config->mailbox_head, MAILBOX_QUEUE);
  if (!mbox) return;
  buf = c2_mailbox_index_path (MAILBOX_QUEUE);

  /* get the next mid to store mail */
  if (widget->queue_mid < 0) widget->queue_mid = c2_mailbox_get_next_mid (mbox);
  else widget->queue_mid = c2_mailbox_get_next_mid (mbox);
  
  if ((fd = fopen (buf, "a")) == NULL) {
    cronos_error (errno, _("Opening mailbox's index file"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Failed to append the mail in the index file."));
    c2_free (buf);
    return;
  }
  c2_free (buf);

  Subject = message_get_header_field (message, NULL, "\nSubject:");
  To = message_get_header_field (message, NULL, "\nTo:");
  c_account = message_get_header_field (message, NULL, "\nX-CronosII-Account:");
  time (&now);
  ptr = localtime (&now);
  strftime (Date, sizeof (Date), "%a, %d %b %Y %H:%M:%S %z", ptr);
  if (!Subject) Subject = '\0';

  fprintf (fd, "N\r\r\r%s\r%s\r%s\r%s\r%d\n",
		  Subject, To, Date, c_account, widget->queue_mid);

  fclose (fd);

  buf = c2_mailbox_mail_path (MAILBOX_QUEUE, widget->queue_mid);
  if ((fd = fopen (buf, "w")) == NULL) {
    cronos_error (errno, _("Opening the mail file"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Failed to write the mail in its file."));
    c2_free (buf);
    return;
  }
  c2_free (buf);

  fprintf (fd, "%s", message->message);
  fclose (fd);
  
  c2_free (Subject);
  c2_free (To);
  c2_free (c_account);
  c2_free (message->message);
  c2_free (message->header);
  c2_free (message->mbox);
  c2_free (message);
  
  widget->body_changed = FALSE;
  widget->changed_signal = gtk_signal_connect (GTK_OBJECT (widget->body), "changed",
      				GTK_SIGNAL_FUNC (on_body_changed), widget);
  gdk_threads_enter ();
  gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Message saved successfully."));

  if (streq (selected_mbox, MAILBOX_QUEUE)) update_clist (MAILBOX_QUEUE, TRUE, TRUE);
  gdk_threads_leave ();
  
  /* -------- close the window ---------- */
  gdk_threads_enter ();
  gtk_widget_destroy (widget->window);
  if (widget->message) {
    c2_free (widget->message->message);
    c2_free (widget->message->header);
    c2_free (widget->message->mbox);
    c2_free (widget->message);
  }
  gdk_threads_leave ();  

  /* Enable widgets */
  gdk_threads_enter ();
  gtk_widget_set_sensitive (WMain->tb_w.sendqueue, TRUE);
  gtk_widget_set_sensitive (WMain->mb_w.menu_sendqueue, TRUE);
  gdk_threads_leave ();
}

static void
on_sendqueue_clicked (GtkWidget *w, WidgetsComposer *widget) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_sendqueue_clicked_thread), widget);
}

static void on_save_clicked (GtkWidget *w, WidgetsComposer *widget) {
  Message *message;
  Mailbox *mbox;
  char *buf;
  FILE *fd;
  char *Subject, *To, Date[40], *account;
  time_t now;
  struct tm *ptr;

  g_return_if_fail (widget);
  message = make_message (widget, REQUIRE_DRAFTS);

  mbox = search_mailbox_name (config->mailbox_head, MAILBOX_DRAFTS);
  if (!mbox) return;
  buf = c2_mailbox_index_path (MAILBOX_DRAFTS);

  if (widget->drafts_mid < 0) widget->drafts_mid = c2_mailbox_get_next_mid (mbox);
  else expunge_mail (MAILBOX_DRAFTS, widget->drafts_mid);

  if ((fd = fopen (buf, "a")) == NULL) {
    cronos_error (errno, _("Opening mailbox's index file"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Failed to append the mail in the index file."));
    c2_free (buf);
    return;
  }
  c2_free (buf);

  Subject = message_get_header_field (message, NULL, "\nSubject:");
  To = message_get_header_field (message, NULL, "\nTo:");
  account = message_get_header_field (message, NULL, "\nX-CronosII-Account:");
  time (&now);
  ptr = localtime (&now);
  strftime (Date, sizeof (Date), "%a, %d %b %Y %H:%M:%S %z", ptr);
  if (!Subject) Subject = '\0';

  fprintf (fd, "N\r\r\r%s\r%s\r%s\r%s\r%d\n",
		  Subject, To, Date, account, widget->drafts_mid);

  fclose (fd);

  buf = c2_mailbox_mail_path (MAILBOX_DRAFTS, widget->drafts_mid);
  if ((fd = fopen (buf, "w")) == NULL) {
    cronos_error (errno, _("Opening the mail file"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Failed to write the mail in its file."));
    c2_free (buf);
    return;
  }
  c2_free (buf);

  fprintf (fd, "%s", message->message);
  fclose (fd);
  
  c2_free (Subject);
  c2_free (To);
  c2_free (account);
  c2_free (message->message);
  c2_free (message->header);
  c2_free (message->mbox);
  c2_free (message);
  
  widget->body_changed = FALSE;
  widget->changed_signal = gtk_signal_connect (GTK_OBJECT (widget->body), "changed",
      				GTK_SIGNAL_FUNC (on_body_changed), widget);
  gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Message saved successfully."));

  if (streq (selected_mbox, MAILBOX_DRAFTS)) update_clist (MAILBOX_DRAFTS, TRUE, TRUE);
}

static void on_save_as_clicked (GtkWidget *w, WidgetsComposer *widget) {
  Message *message;
  FILE *fd;
  char *file;
  char *buf;
  char *Subject, *To, Date[40], *account;
  time_t now;
  struct tm *ptr;

  g_return_if_fail (widget);
  
  file = gui_select_file_new (NULL, NULL, NULL, NULL);
  if (!file) return;

  message = make_message (widget, REQUIRE_NOTHING);
  if (!message) return;

  Subject = message_get_header_field (message, NULL, "\nSubject:");
  To = message_get_header_field (message, NULL, "\nTo:");
  account = message_get_header_field (message, NULL, "\nX-CronosII-Account:");
  time (&now);
  ptr = localtime (&now);
  strftime (Date, sizeof (Date), "%a, %d %b %Y %H:%M:%S %z", ptr);
  if (!Subject) Subject = '\0';

  if ((fd = fopen (file, "w")) == NULL) {
    cronos_error (errno, _("Opening the selected file"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar),
			_("Failed to write the mail in the selected file."));
    c2_free (buf);
    return;
  }

  message_get_message_body (message, NULL);
  
  fprintf (fd, _("To: %s\nSubject: %s\nDate: %s\nAccount: %s\n\n%s\n"),
      		To, Subject, Date, account, message->body);
  fclose (fd);
  c2_free (To);
  c2_free (Subject);
  c2_free (account);
  c2_free (message->header);
  c2_free (message->message);
  c2_free (message);

  gnome_appbar_set_status (GNOME_APPBAR (widget->appbar),
			_("Message saved successfully."));
}

static void
on_attachs_clicked (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (widget);

  if (GTK_TOGGLE_BUTTON (w)->active) gtk_notebook_set_page (GTK_NOTEBOOK (widget->header_notebook), 1);
  else gtk_notebook_set_page (GTK_NOTEBOOK (widget->header_notebook), 0);
}

static void
on_icon_list_select_icon (GtkWidget *w, int num, GdkEvent *event, WidgetsComposer *widget) {
  g_return_if_fail (widget);

  widget->selected_icon = num;
}

static void
on_icon_list_unselect_icon (GtkWidget *w, int num, GdkEvent *event, WidgetsComposer *widget) {
  g_return_if_fail (widget);

  widget->selected_icon = -1;
}

static void
on_attachs_add_clicked (GtkWidget *w, WidgetsComposer *widget) {
  char *file;
  const char *icon;
  GdkImlibImage *img;
  
  g_return_if_fail (widget);
  
  file = gui_select_file_new (NULL, NULL, NULL, NULL);
  if (!file) return;

  printf ("adding: %s\n", file);
  icon = pixmap_get_icon_by_mime_type (gnome_mime_type_or_default (file, "default"));
  img = gdk_imlib_load_image (CHAR (icon));

  gnome_icon_list_append_imlib (GNOME_ICON_LIST (widget->icon_list), img, g_basename (file));

  gnome_icon_list_set_icon_data (GNOME_ICON_LIST (widget->icon_list),
      					GNOME_ICON_LIST (widget->icon_list)->icons-1,
					file);
}

static void
on_attachs_remove_clicked (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (widget);
  if (widget->selected_icon < 0) return;
  
  gnome_icon_list_remove (GNOME_ICON_LIST (widget->icon_list), widget->selected_icon);
}

static void
on_insert_file_clicked (GtkWidget *w, WidgetsComposer *widget) {
  char *file;
  char *line;
  FILE *fd;
  
  file = gui_select_file_new (NULL, NULL, NULL, NULL);
  if (!file) return;

  if ((fd = fopen (file, "r")) == NULL) {
    cronos_error (errno, _("Opening the selected file for reading"), ERROR_WARNING);
    gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("Failed to insert the file."));
    c2_free (file);
    return;
  }

  gtk_text_freeze (GTK_TEXT (widget->body));
  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, line, -1);
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, "\n", -1);
  }
  gtk_text_thaw (GTK_TEXT (widget->body));
  fclose (fd);

  gnome_appbar_set_status (GNOME_APPBAR (widget->appbar), _("File inserted successfully."));
}

static void
on_insert_signature_clicked_thread (WidgetsComposer *widget) {
  Account *account;
  char *buf;
  char *signature;
  int account_n, i;

  /* Append the signature */
  buf = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (widget->header_titles[HEADER_TITLES_ACCOUNT][1])->entry));
  account_n = atoi (buf);
  if (!account_n) account_n = 1;
  
  for (i = 1, account = config->account_head; i < account_n && account; i++, account = account->next);
  if (account) {
    signature = g_strdup (account->signature);
#if USE_PLUGINS
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE,
					&signature, NULL, NULL, NULL, NULL);
#endif
    gdk_threads_enter ();
    gtk_text_freeze (GTK_TEXT (widget->body));
    gtk_text_insert (GTK_TEXT (widget->body), NULL, &config->color_misc_body, NULL, signature, -1);
    gtk_text_thaw (GTK_TEXT (widget->body));
    gdk_threads_leave ();
  }
}

static void
on_insert_signature_clicked (GtkWidget *w, WidgetsComposer *widget) {
  pthread_t thread;

  pthread_create (&thread, NULL, PTHREAD_FUNC (on_insert_signature_clicked_thread), widget);
}

static void on_select_all_clicked (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (widget);
  gtk_text_freeze (GTK_TEXT (widget->body));
  gtk_editable_select_region (GTK_EDITABLE (widget->body), 0, -1);
  gtk_text_thaw (GTK_TEXT (widget->body));
}

static void on_clear_clicked (GtkWidget *w, WidgetsComposer *widget) {
  g_return_if_fail (widget);
  gtk_text_freeze (GTK_TEXT (widget->body));
  gtk_editable_delete_text (GTK_EDITABLE (widget->body), 0, -1);
  gtk_text_thaw (GTK_TEXT (widget->body));
}
