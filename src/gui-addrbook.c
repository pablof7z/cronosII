#include <gnome.h>
#include <config.h>
#include <time.h>

#include "main.h"

#include "gui-addrbook.h"

#include "addrbook.h"
#include "utils.h"
#include "rc.h"
#include "init.h"

#ifdef BUILD_ADDRESS_BOOK

static void
on_gui_address_book_new_btn_clicked				(void);

static void
on_gui_address_book_edit_btn_clicked				(void);

static void
on_gui_address_book_delete_btn_clicked				(void);

static void
on_gui_address_book_save_btn_clicked				(void);

static void
on_address_book_search_activate					(GtkWidget *object);

static void
on_address_book_group_changed					(void);

static void
on_address_book_delete_event					(GtkWidget *object, GdkEvent *event);

static void
on_address_book_close_clicked					(GtkWidget *object);

static void
c2_address_book_create_menubar					(void);

static void
c2_address_book_create_toolbar					(void);

C2AddressBookFillType current_fill_type = 0;
const char *current_flags = NULL;

C2AddressBook *
c2_address_book_new (const char *package) {
  char *titles[] = {
    N_("Name"),
    N_("E-Mail Address"),
    N_("Comments"),
    NULL
  };

  if (gaddrbook) return gaddrbook;
  gaddrbook = g_new0 (C2AddressBook, 1);

  /* Initialize if required */
  if (config->addrbook_init == INIT_ADDRESS_BOOK_INIT_OPEN) c2_address_book_init ();

  gaddrbook->window = gnome_app_new (package, _("Cronos II Address Book"));
  gtk_widget_set_usize (gaddrbook->window, rc->main_window_width, rc->main_window_height);
  gtk_signal_connect (GTK_OBJECT (gaddrbook->window), "delete_event",
      				GTK_SIGNAL_FUNC (on_address_book_delete_event), NULL);
  c2_address_book_create_menubar ();
  c2_address_book_create_toolbar ();
  
  gaddrbook->clist = gtk_clist_new_with_titles (3, titles);
  gnome_app_set_contents (GNOME_APP (gaddrbook->window), gaddrbook->clist);
  gtk_widget_show (gaddrbook->clist);
  gtk_clist_set_column_width (GTK_CLIST (gaddrbook->clist), 0, 220);
  gtk_clist_set_column_width (GTK_CLIST (gaddrbook->clist), 1, 280);

  gtk_widget_show (gaddrbook->window);
  c2_address_book_fill (gaddrbook, 0, NULL);

  return gaddrbook;
}

void
c2_address_book_fill (C2AddressBook *addrbook, C2AddressBookFillType type, const char *flags) {
  int i;
  const C2VCard *card;
  gchar *fields[3];
  static C2AddressBook *my_addrbook = NULL;
  
  g_return_if_fail (!(!addrbook && !my_addrbook));
  g_return_if_fail (!(type == C2_ADDRESS_BOOK_SEARCH && !flags));
  g_return_if_fail (!(type == C2_ADDRESS_BOOK_GROUP && !flags));

  if (addrbook)
    my_addrbook = addrbook;

  current_fill_type = type;
  current_flags = flags;

  gtk_clist_freeze (GTK_CLIST (my_addrbook->clist));
  gtk_clist_clear (GTK_CLIST (my_addrbook->clist));
  for (i = 0;; i++) {
    if ((card = c2_address_book_card_get (i)) == NULL) break;
    
    fields[0] = card->name_show;
    fields[1] = card->email ? C2VCARDEMAIL (card->email->data)->address : "";
    fields[2] = card->comment;

    gtk_clist_append (GTK_CLIST (my_addrbook->clist), fields);
    gtk_clist_set_row_data (GTK_CLIST (gaddrbook->clist), GTK_CLIST (gaddrbook->clist)->rows-1, card);
  }
  gtk_clist_thaw (GTK_CLIST (my_addrbook->clist));
}

static void
on_address_book_search_activate (GtkWidget *object) {
  const char *pattern;
  int offset = -1;
  int select_ = 0;
  GList *list = NULL;
  int i;
  C2VCard *card;
  
  g_return_if_fail (gaddrbook);
  
  pattern = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gaddrbook->search))));

  /* Get the list that is being shown */
  for (i = 0; i < GTK_CLIST (gaddrbook->clist)->rows; i++) {
    card = C2VCARD (gtk_clist_get_row_data (GTK_CLIST (gaddrbook->clist), i));
    list = g_list_append (list, card);
  }

  if (GTK_CLIST (gaddrbook->clist)->selection)
    offset = (int) GTK_CLIST (gaddrbook->clist)->selection->data;
search_again:
  select_ = c2_address_book_search (list, pattern, offset+1);
  if (select_ >= 0) {
    gtk_clist_select_row (GTK_CLIST (gaddrbook->clist), select_, 0);
    gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gaddrbook->search))), "");
  }
  else if (offset >= 0) {
    /* No record found, wanna check again? */
    GtkWidget *dialog;

    dialog = gnome_question_dialog (_("No entry match your search.\n"
	  			      "Start from the beggining?"), NULL, NULL);
    gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
    if (!gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
      offset = -1;
      goto search_again;
    }
  }
  else {
    /* No record found! */
    GtkWidget *dialog;

    dialog = gnome_warning_dialog (_("No entry match your search.\n"));
    gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
  }
  g_list_free (list);
}

static void
on_address_book_group_changed (void) {
  char *pattern;
  C2VCard *card;
  GList *list, *l;
  char *fields[3];
  
  g_return_if_fail (gaddrbook);
  pattern = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (gaddrbook->group)->entry));
  gtk_clist_freeze (GTK_CLIST (gaddrbook->clist));
  gtk_clist_clear (GTK_CLIST (gaddrbook->clist));
  if (!strlen (pattern)) {
    gtk_clist_thaw (GTK_CLIST (gaddrbook->clist));
    c2_address_book_fill (gaddrbook, current_fill_type, current_flags);
    return;
  }
  for (list = addrbook; list != NULL; list = list->next) {
    card = C2VCARD (list->data);
    if (!card) continue;
    for (l = card->groups; l != NULL; l = l->next) {
      if (!l || !l->data) continue;
      if (streq (CHAR (l->data), pattern)) {
	fields[0] = card->name_show;
	fields[1] = card->email ? C2VCARDEMAIL (card->email->data)->address : "";
	fields[2] = card->comment;
	gtk_clist_append (GTK_CLIST (gaddrbook->clist), fields);
	gtk_clist_set_row_data (GTK_CLIST (gaddrbook->clist), GTK_CLIST (gaddrbook->clist)->rows-1,
	    				card);
      }
    }
  }
  gtk_clist_thaw (GTK_CLIST (gaddrbook->clist));
}

static void
on_address_book_delete_event (GtkWidget *object, GdkEvent *event) {
  g_return_if_fail (gaddrbook);

  gtk_widget_hide (gaddrbook->window);
}

static void
on_address_book_close_clicked (GtkWidget *object) {
  g_return_if_fail (gaddrbook);

  gtk_widget_hide (gaddrbook->window);
}

static GnomeUIInfo file_menu[] = {
  GNOMEUIINFO_MENU_SAVE_ITEM (NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_New card"),
    N_("Create a new card"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, DATADIR "/cronosII/pixmap/mcard_menu.png",
    'N', GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Edit card"),
    N_("Edit the selected card"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, DATADIR "/cronosII/pixmap/mcard-edit_menu.png",
    'E', GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Delete card"),
    N_("Delete the selected card"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE,
    'D', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Close"),
    N_("Close the Address Book"),
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_QUIT,
    'W', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

static GnomeUIInfo menu_bar[] = {
  GNOMEUIINFO_MENU_FILE_TREE (file_menu),
  GNOMEUIINFO_END
};

static void
c2_address_book_create_menubar (void) {
  g_return_if_fail (gaddrbook);
  
  gnome_app_create_menus (GNOME_APP (gaddrbook->window), menu_bar);

  gtk_signal_connect (GTK_OBJECT (file_menu[0].widget), "activate",
      			GTK_SIGNAL_FUNC (on_gui_address_book_save_btn_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (file_menu[2].widget), "activate",
      			GTK_SIGNAL_FUNC (on_gui_address_book_new_btn_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (file_menu[3].widget), "activate",
      			GTK_SIGNAL_FUNC (on_gui_address_book_edit_btn_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (file_menu[4].widget), "activate",
      			GTK_SIGNAL_FUNC (on_gui_address_book_delete_btn_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (file_menu[6].widget), "activate",
      			GTK_SIGNAL_FUNC (on_address_book_close_clicked), NULL);
}

static void
c2_address_book_create_toolbar (void) {
  GtkWidget *btn;
  GtkWidget *tmp_toolbar_icon;
  GtkWidget *toolbar;
  GtkWidget *label;
  GList *list;
  
  g_return_if_fail (gaddrbook);

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, rc->toolbar);
  gnome_app_add_toolbar (GNOME_APP (gaddrbook->window), GTK_TOOLBAR (toolbar), "toolbar",
      			GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
			GNOME_DOCK_TOP, 1, 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (toolbar), 1);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (gaddrbook->window, GNOME_STOCK_PIXMAP_SAVE);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Save"),
                                _("Save changes"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_gui_address_book_save_btn_clicked), NULL);
  gtk_widget_show (btn);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (gaddrbook->window, DATADIR "/cronosII/pixmap/mcard.png");
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("New"),
                                _("New Card"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_gui_address_book_new_btn_clicked), NULL);
  gtk_widget_show (btn);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (gaddrbook->window, DATADIR "/cronosII/pixmap/mcard-edit.png");
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Edit"),
                                _("Edit the selected card"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_gui_address_book_edit_btn_clicked), NULL);
  gtk_widget_show (btn);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (gaddrbook->window, GNOME_STOCK_PIXMAP_CLOSE);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Delete"),
                                _("Delete the selected card"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_gui_address_book_delete_btn_clicked), NULL);
  gtk_widget_show (btn);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (gaddrbook->window, GNOME_STOCK_PIXMAP_QUIT);
  btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Close"),
                                _("Close this window"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      				GTK_SIGNAL_FUNC (on_address_book_close_clicked), addrbook);
  gtk_widget_show (btn);

  gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 5);
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (toolbar), TRUE);
  gtk_widget_show (toolbar);





  
  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, rc->toolbar);
  gnome_app_add_toolbar (GNOME_APP (gaddrbook->window), GTK_TOOLBAR (toolbar), "search_toolbar",
      			GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
			GNOME_DOCK_TOP, 2, 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (toolbar), 2);
  
  label = gtk_label_new (_("Search: "));
  gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), label, NULL, NULL);
  gtk_widget_show (label);

  gaddrbook->search = gnome_entry_new ("c2_address_book_search");
  gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), gaddrbook->search, NULL, NULL);
  gtk_widget_show (gaddrbook->search);
  gtk_signal_connect (GTK_OBJECT (gnome_entry_gtk_entry (GNOME_ENTRY (gaddrbook->search))), "activate",
      			GTK_SIGNAL_FUNC (on_address_book_search_activate), NULL);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  label = gtk_label_new (_("Group View: "));
  gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), label, NULL, NULL);
  gtk_widget_show (label);

  gaddrbook->group = gtk_combo_new ();
  gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), gaddrbook->group, NULL, NULL);
  gtk_widget_show (gaddrbook->group);
  if ((list = c2_address_book_get_available_groups ()) != NULL) {
    list = g_list_prepend (list, "");
    gtk_combo_set_popdown_strings (GTK_COMBO (gaddrbook->group), list);
    g_list_free (list);
  }
  gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (gaddrbook->group)->entry), FALSE);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (gaddrbook->group)->entry), "changed",
      			GTK_SIGNAL_FUNC (on_address_book_group_changed), NULL);

  gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 15);
  gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_EMPTY);
  gtk_widget_show (toolbar);
}

static void
on_gui_address_book_new_btn_clicked (void) {
  gui_address_book_card_properties (NULL, -1);
}

static void
on_gui_address_book_edit_btn_clicked (void) {
  C2VCard *card;
  
  if (!GTK_CLIST (gaddrbook->clist)->selection) return;

  card = C2VCARD (gtk_clist_get_row_data (GTK_CLIST (gaddrbook->clist),
				GPOINTER_TO_INT (GTK_CLIST (gaddrbook->clist)->selection->data)));
  if (!card) return;
  gui_address_book_card_properties (card,
  				GPOINTER_TO_INT (GTK_CLIST (gaddrbook->clist)->selection->data));
}

static void
on_gui_address_book_delete_btn_clicked (void) {
  C2VCard *card;
  
  if (!GTK_CLIST (gaddrbook->clist)->selection) return;

  card = C2VCARD (gtk_clist_get_row_data (GTK_CLIST (gaddrbook->clist),
				GPOINTER_TO_INT (GTK_CLIST (gaddrbook->clist)->selection->data)));
  if (!card) return;
  addrbook = g_list_remove (addrbook, card);
  gtk_clist_remove (GTK_CLIST (gaddrbook->clist),
      				 GPOINTER_TO_INT (GTK_CLIST (gaddrbook->clist)->selection->data));
  c2_address_book_card_free (card);
}

static void
on_gui_address_book_save_btn_clicked (void) {
  c2_address_book_flush ();
}

static void
on_gui_address_book_card_properties_ok_or_apply_clicked		(GtkWidget *widget, int page,
    							 	 C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_personal_information_page	(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_group_information_page	(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_network_page		(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_address_page		(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_phone_page			(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_etcetera_page		(C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_set_active			(GtkWidget *object,
    								 C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_notebook_switch_page		(GtkWidget *object, GtkNotebookPage *page,
    								 guint n, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_set_help			(GtkWidget *object, GdkEventFocus *focus,
    								 C2AddressBookCardProperties *w);

void
gui_address_book_card_properties (C2VCard *card, int nth) {
  C2AddressBookCardProperties *w = g_new0 (C2AddressBookCardProperties, 1);
  GtkWidget *scroll, *viewport;

  if (!card) w->i_may_edit_the_name_show_field = TRUE;
  w->name_show_focus_in_other_widget = FALSE;
  w->groups_are_loaded = FALSE;
  w->card = card;
  w->nth = nth;

  /* window */
  w->dialog = gnome_property_box_new ();
  gtk_window_set_title (GTK_WINDOW (w->dialog), !card ? _("New Contact") : _("Edit Contact"));
  gtk_signal_connect (GTK_OBJECT (GNOME_PROPERTY_BOX (w->dialog)->notebook), "switch_page",
      				GTK_SIGNAL_FUNC (gui_address_book_card_properties_notebook_switch_page), w);
  gtk_signal_connect (GTK_OBJECT (w->dialog), "apply",
      				GTK_SIGNAL_FUNC (on_gui_address_book_card_properties_ok_or_apply_clicked), w);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (w->dialog)->vbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  /* viewport */
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_ETCHED_OUT);
  gtk_widget_show (viewport);

  /* help */
  w->help = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (viewport), w->help);
  gtk_widget_show (w->help);
  gtk_label_set_justify (GTK_LABEL (w->help), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (w->help), 0.01, 0.5);

  gui_address_book_card_properties_add_personal_information_page (w); 
  gui_address_book_card_properties_add_group_information_page (w);
  gui_address_book_card_properties_add_network_page (w);
  gui_address_book_card_properties_add_address_page (w);
  gui_address_book_card_properties_add_phone_page (w);
  gui_address_book_card_properties_add_etcetera_page (w);

  gtk_widget_show (w->dialog);
}

enum {
  NOTEBOOK_PAGE_PERSONAL_INFORMATION,
  NOTEBOOK_PAGE_GROUPS,
  NOTEBOOK_PAGE_NETWORK,
  NOTEBOOK_PAGE_ADDRESSES,
  NOTEBOOK_PAGE_PHONE,
  NOTEBOOK_PAGE_ETCETERA,
  NOTEBOOK_PAGE_LAST
};

static void
on_gui_address_book_card_properties_ok_or_apply_clicked (GtkWidget *widget, int page,
    							 C2AddressBookCardProperties *w) {
  char *buf;
  int i;
  GList *list;

  g_return_if_fail (w);
  if (page < 0) return;
  
  if (!w->card) {
    w->card = c2_address_book_card_new ();
    c2_address_book_card_init (w->card);
  }

  switch (page) {
    case NOTEBOOK_PAGE_PERSONAL_INFORMATION:
      if (w->card->name_show) c2_free (w->card->name_show);
      if (w->card->name_salutation) c2_free (w->card->name_salutation);
      if (w->card->name_first) c2_free (w->card->name_first);
      if (w->card->name_middle) c2_free (w->card->name_middle);
      if (w->card->name_last) c2_free (w->card->name_last);
      if (w->card->name_suffix) c2_free (w->card->name_suffix);
      if (w->card->birthday) c2_free (w->card->birthday);
      if (w->card->org_name) c2_free (w->card->org_name);
      if (w->card->org_title) c2_free (w->card->org_title);

      w->card->name_show = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_name_show)));
      w->card->name_salutation = g_strdup (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (w->personal_name_salutation)->entry)));
      w->card->name_first = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_name_first)));
      w->card->name_middle = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_name_middle)));
      w->card->name_last = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_name_last)));
      w->card->name_suffix = g_strdup (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (w->personal_name_suffix)->entry)));
      {
	time_t time;
	struct tm tm;
	
	time = gnome_date_edit_get_date (GNOME_DATE_EDIT (w->personal_birthday));
	tm = *localtime (&time);
	w->card->birthday = g_new0 (char, 11);
      }
      
      w->card->org_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_organization_name)));
      w->card->org_title = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->personal_organization_title)));
      break;
    case NOTEBOOK_PAGE_GROUPS:
      if (w->card->groups) {
	g_list_free (w->card->groups);
	w->card->groups = NULL;
      }
      for (i = 0; i < GTK_CLIST (w->group_list)->rows; i ++) {
	gtk_clist_get_text (GTK_CLIST (w->group_list), i, 0, &buf);
	w->card->groups = g_list_append (w->card->groups, g_strdup (buf));
      }
      break;
    case NOTEBOOK_PAGE_NETWORK:
      if (w->card->web) c2_free (w->card->web);
      if (w->card->email) {
	g_list_free (w->card->email);
	w->card->email = NULL;
      }
      
      w->card->web = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->network_url)));
      for (i = 0; i < GTK_CLIST (w->network_list)->rows; i++) {
	C2VCardEmail *email;

	email = C2VCARDEMAIL (gtk_clist_get_row_data (GTK_CLIST (w->network_list), i));
	w->card->email = g_list_append (w->card->email, email);
      }
      break;
    case NOTEBOOK_PAGE_ADDRESSES:
      if (w->card->address) {
	g_list_free (w->card->address);
	w->card->address = NULL;
      }
      
      for (i = 0; i < GTK_CLIST (w->address_list)->rows; i++) {
	C2VCardAddress *addr;

	addr = C2VCARDADDRESS (gtk_clist_get_row_data (GTK_CLIST (w->address_list), i));
	w->card->address = g_list_append (w->card->address, addr);
      }
      break;
    case NOTEBOOK_PAGE_PHONE:
      if (w->card->phone) {
	g_list_free (w->card->phone);
	w->card->phone = NULL;
      }
      
      for (i = 0; i < GTK_CLIST (w->phone_list)->rows; i++) {
	C2VCardPhone *phone;

	phone = C2VCARDPHONE (gtk_clist_get_row_data (GTK_CLIST (w->phone_list), i));
	w->card->phone = g_list_append (w->card->phone, phone);
      }
      break;
    case NOTEBOOK_PAGE_ETCETERA:
      if (w->card->categories) c2_free (w->card->categories);
      if (w->card->comment) c2_free (w->card->comment);
      if (w->card->security) c2_free (w->card->security);
      
      w->card->categories = gtk_editable_get_chars (GTK_EDITABLE (w->etcetera_categories), 0, -1);
      w->card->comment = gtk_editable_get_chars (GTK_EDITABLE (w->etcetera_comments), 0, -1);
      w->card->security = gtk_editable_get_chars (GTK_EDITABLE (w->etcetera_security), 0, -1);
      if (GTK_TOGGLE_BUTTON (w->etcetera_security_type_pgp)->active)
	w->card->sectype = C2_VCARD_SECURITY_PGP;
      else
	w->card->sectype = C2_VCARD_SECURITY_X509;
      break;
  }

  if (w->nth >= 0) addrbook = g_list_remove_link (addrbook, g_list_nth (addrbook, w->nth));
  w->nth = c2_address_book_card_add (w->card);
  if (!strlen (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (gaddrbook->group)->entry))))
    c2_address_book_fill (NULL, current_fill_type, current_flags);
  else
    on_address_book_group_changed ();
  if ((list = c2_address_book_get_available_groups ()) == NULL) return;
  list = g_list_prepend (list, "");
  gtk_combo_set_popdown_strings (GTK_COMBO (gaddrbook->group), list);
  g_list_free (list);
}

/* Personal Information */
static void
on_address_book_card_properties_add_personal_information_page_name_person_changed (GtkWidget *widget,
    									C2AddressBookCardProperties *w);

static void
on_address_book_card_properties_add_personal_information_page_name_show_changed (GtkWidget *widget,
    									C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_personal_information_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *hbox, *table;
  GtkWidget *frame, *label, *hsep;
  GList *list = NULL;
  
  g_return_if_fail (w);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

  frame = gtk_frame_new (_("Name"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  /* table */
  table = gtk_table_new (5, 4, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);

  /* labels */
  label = gtk_label_new (_("Salutation:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label); 

  label = gtk_label_new (_("First Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  label = gtk_label_new (_("Middle Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  label = gtk_label_new (_("Last Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  label = gtk_label_new (_("Suffix:"));
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 2, 3,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  hsep = gtk_hseparator_new ();
  gtk_table_attach (GTK_TABLE (table), hsep, 0, 4, 3, 4,
      		    GTK_FILL, 
		    0, 0, 0);
  gtk_widget_show (hsep);
  
  label = gtk_label_new (_("Show Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  /* entries */
  w->personal_name_salutation = gtk_combo_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_salutation, 1, 2, 0, 1,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_salutation);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_salutation)->entry), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_salutation)->entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  list = g_list_append (list, "");
  list = g_list_append (list, _("Mr.")); /* Mr. Mister */
  list = g_list_append (list, _("Mrs.")); /* Mrs. Mistress */
  list = g_list_append (list, _("Dr.")); /* Dr. Doctor */
  gtk_combo_set_popdown_strings (GTK_COMBO (w->personal_name_salutation), list);
  g_list_free (list);
  list = NULL; 
  if (w->card) gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (w->personal_name_salutation)->entry),
 					w->card->name_salutation);

  w->personal_name_first = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_first, 1, 2, 1, 2,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_first);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->personal_name_first),
 					w->card->name_first);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_first), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_first), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w); 

  w->personal_name_middle = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_middle, 3, 4, 1, 2,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_middle);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->personal_name_middle),
 					w->card->name_middle);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_middle), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_middle), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  w->personal_name_last = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_last, 1, 2, 2, 3,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_last);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->personal_name_last),
 					w->card->name_last);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_last), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_last), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  w->personal_name_suffix = gtk_combo_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_suffix, 3, 4, 2, 3,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_suffix);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_suffix)->entry), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_suffix)->entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  list = g_list_append (list, "");
  list = g_list_append (list, _("Sr.")); /* Sr. ???? */
  list = g_list_append (list, _("Jr.")); /* Jr. Junior */
  list = g_list_append (list, "I");
  list = g_list_append (list, "II");
  list = g_list_append (list, "III");
  list = g_list_append (list, "Esq"); /* Esq. ???? */
  gtk_combo_set_popdown_strings (GTK_COMBO (w->personal_name_suffix), list);
  g_list_free (list);
  list = NULL;
  if (w->card) gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (w->personal_name_suffix)->entry),
 					w->card->name_suffix);

  w->personal_name_show = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_name_show, 1, 4, 4, 5,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_name_show);
  if (w->card) {
    gtk_entry_set_text (GTK_ENTRY (w->personal_name_show), w->card->name_show);
    w->i_may_edit_the_name_show_field = FALSE;
  }
  
  gtk_signal_connect (GTK_OBJECT (w->personal_name_show), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_show), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_show_changed), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_show), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  /* hbox */
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* birthday label */
  label = gtk_label_new (_("Birthday:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* birthday */
  w->personal_birthday = gnome_date_edit_new (0, FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), w->personal_birthday, FALSE, FALSE, 0);
  gtk_widget_show (w->personal_birthday);
  gnome_date_edit_set_flags (GNOME_DATE_EDIT (w->personal_birthday),
                             GNOME_DATE_EDIT_24_HR | GNOME_DATE_EDIT_WEEK_STARTS_ON_MONDAY);
  if (w->card && w->card->birthday) {
    struct tm tm;
    time_t time;
    
    time = gnome_date_edit_get_date (GNOME_DATE_EDIT (w->personal_birthday));
    tm = *localtime (&time);
    sscanf (w->card->birthday, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon--;
    gnome_date_edit_set_time (GNOME_DATE_EDIT (w->personal_birthday), mktime (&tm));
  }
  gtk_signal_connect (GTK_OBJECT (w->personal_birthday), "date_changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (w->personal_birthday)->date_entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  gtk_signal_connect (GTK_OBJECT (GNOME_DATE_EDIT (w->personal_birthday)->date_button), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  /* frame */
  frame = gtk_frame_new (_("Organization"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* table */
  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);

  /* labels */
  label = gtk_label_new (_("Name:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label); 

  label = gtk_label_new (_("Title:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL,
                    0, 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_widget_show (label);

  /* entries */
  w->personal_organization_name = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_organization_name, 1, 2, 0, 1,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_organization_name);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->personal_organization_name),
 					w->card->org_name);
  gtk_signal_connect (GTK_OBJECT (w->personal_organization_name), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_organization_name), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  w->personal_organization_title = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), w->personal_organization_title, 1, 2, 1, 2,
                    GTK_FILL | GTK_EXPAND,
                    0, 0, 0);
  gtk_widget_show (w->personal_organization_title);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->personal_organization_title),
 					w->card->org_title);
  gtk_signal_connect (GTK_OBJECT (w->personal_organization_title), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_organization_title), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_salutation)->entry), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_person_changed), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_first), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_person_changed), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_middle), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_person_changed), w);
  gtk_signal_connect (GTK_OBJECT (w->personal_name_last), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_person_changed), w);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->personal_name_suffix)->entry), "changed",
      GTK_SIGNAL_FUNC (on_address_book_card_properties_add_personal_information_page_name_person_changed), w);

  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), vbox,
      					gtk_label_new (_("Personal Information")),
					NOTEBOOK_PAGE_PERSONAL_INFORMATION);
  gtk_widget_show (vbox);
}

static void
on_address_book_card_properties_add_personal_information_page_name_person_changed (GtkWidget *widget,
    									C2AddressBookCardProperties *w) {
  char *salutation, *first, *middle, *last, *suffix;
  char *str;
  int len = 0;
  
  g_return_if_fail (w);
  if (!w->i_may_edit_the_name_show_field) return;

  salutation = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (w->personal_name_salutation)->entry));
  first = gtk_entry_get_text (GTK_ENTRY (w->personal_name_first));
  middle = gtk_entry_get_text (GTK_ENTRY (w->personal_name_middle));
  last = gtk_entry_get_text (GTK_ENTRY (w->personal_name_last));
  suffix = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (w->personal_name_suffix)->entry));

  if (salutation && strlen (salutation)) {
    len += strlen (salutation);
  }
  if (first && strlen (first)) {
    if (len) len++;
    len += strlen (first);
  }
  if (middle && strlen (middle)) {
    if (len) len++;
    len += strlen (middle);
  }
  if (last && strlen (last)) {
    if (len) len++;
    len += strlen (last);
  }
  if (suffix && strlen (suffix)) {
    if (len) len++;
    len += strlen (suffix);
  }

  str = g_new0 (char, len+1);
  if (salutation && strlen (salutation)) {
    strcpy (str, salutation);
  }
  if (first && strlen (first)) {
    if (strlen (str)) strcat (str, " ");
    strcat (str, first);
  }
  if (middle && strlen (middle)) {
    if (strlen (str)) strcat (str, " ");
    strcat (str, middle);
  }
  if (last && strlen (last)) {
    if (strlen (str)) strcat (str, " ");
    strcat (str, last); 
  }
  if (suffix && strlen (suffix)) {
    if (strlen (str)) strcat (str, " ");
    strcat (str, suffix);
  }

  w->name_show_focus_in_other_widget = TRUE;
  gtk_entry_set_text (GTK_ENTRY (w->personal_name_show), str);
}

static void
on_address_book_card_properties_add_personal_information_page_name_show_changed (GtkWidget *widget,
    									C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  if (!w->name_show_focus_in_other_widget)
    w->i_may_edit_the_name_show_field = FALSE;

  w->name_show_focus_in_other_widget = FALSE;
}

/** Groups **/
static void
gui_address_book_card_properties_group_page_entry_focus_in_event (GtkWidget *widget, GdkEventFocus *focus,
    								  C2AddressBookCardProperties *w);

static void
on_address_book_card_properties_group_page_down_btn_clicked	(GtkWidget *widget,
    							     	 C2AddressBookCardProperties *w);

static void
on_address_book_card_properties_group_page_up_btn_clicked	(GtkWidget *widget,
    							   	 C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_group_information_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *hbox, *hbox1, *btn;
  GtkWidget *scroll, *viewport, *label, *arrow;
  GList *list;
  
  g_return_if_fail (w);

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 7);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* vbox */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
  gtk_widget_show (vbox);

  /* group_entry */
  w->group_entry = gtk_combo_new ();
  gtk_box_pack_start (GTK_BOX (vbox), w->group_entry, FALSE, TRUE, 0);
  gtk_widget_show (w->group_entry);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->group_entry)->entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_group_page_entry_focus_in_event), w);
  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (w->group_entry)->entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* btn */
  btn = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_address_book_card_properties_group_page_down_btn_clicked), w);

  /* arrow */
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (btn), arrow);
  gtk_widget_show (arrow);

  /* btn */
  btn = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_address_book_card_properties_group_page_up_btn_clicked), w);

  /* arrow */
  arrow = gtk_arrow_new (GTK_ARROW_UP, GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (btn), arrow);
  gtk_widget_show (arrow);
  
  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /* viewport */
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_ETCHED_IN);
  gtk_widget_show (viewport);

  /* group_list */
  w->group_list = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (viewport), w->group_list);
  gtk_widget_show (w->group_list);
  if (w->card) {
    for (list = w->card->groups; list != NULL; list = list->next) {
      gtk_clist_append (GTK_CLIST (w->group_list), (char **) &list->data);
    }
  }

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), hbox,
      					gtk_label_new (_("Groups")),
					NOTEBOOK_PAGE_GROUPS);
  gtk_widget_show (hbox);
}

static void
gui_address_book_card_properties_group_page_entry_focus_in_event (GtkWidget *widget, GdkEventFocus *focus,
    								  C2AddressBookCardProperties *w) {
  GList *list = NULL;

  g_return_if_fail (w);

  if (w->groups_are_loaded) return;

  if ((list = c2_address_book_get_available_groups ()) == NULL) return;

  list = g_list_prepend (list, "");
  gtk_combo_set_popdown_strings (GTK_COMBO (w->group_entry), list);
  g_list_free (list);

  w->groups_are_loaded = TRUE;
}

static void
on_address_book_card_properties_group_page_down_btn_clicked (GtkWidget *widget,
    							     C2AddressBookCardProperties *w) {
  int i;
  char *text;
  char *str;
  
  g_return_if_fail (w);

  text = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (w->group_entry)->entry));
  if (!text || !strlen (text)) return;
  
  /* Check if this group is not set already */
  for (i = 0; i < GTK_CLIST (w->group_list)->rows; i++) {
    gtk_clist_get_text (GTK_CLIST (w->group_list), i, 0, &str);

    if (streq (text, str)) return;
  }

  gtk_clist_append (GTK_CLIST (w->group_list), &text);
}

static void
on_address_book_card_properties_group_page_up_btn_clicked (GtkWidget *widget,
    							   C2AddressBookCardProperties *w) {
  g_return_if_fail (w);

  if (!GTK_CLIST (w->group_list)->selection) return;
  
  gtk_clist_remove (GTK_CLIST (w->group_list), GPOINTER_TO_INT (GTK_CLIST (w->group_list)->selection->data));
}

/* Network */
static void
gui_address_book_card_properties_network_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_network_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_network_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *vbox1, *hbox, *hbox1, *table;
  GtkWidget *frame, *scroll, *label;
  GList *list;

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

  /* hbox */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* label */
  label = gtk_label_new (_("Web Site:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* network_url */
  w->network_url = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), w->network_url, TRUE, TRUE, 0);
  gtk_widget_show (w->network_url);
  gtk_signal_connect (GTK_OBJECT (w->network_url), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->network_url), w->card->web);

  /* hbox */
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* frame */
  frame = gtk_frame_new (_("E-mail data"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox1);
  gtk_widget_show (vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new (_("Address:"));
  gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* network_entry */
  w->network_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), w->network_entry, TRUE, TRUE, 0);
  gtk_widget_show (w->network_entry);
  gtk_signal_connect (GTK_OBJECT (w->network_entry), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_set_sensitive), w);
  gtk_signal_connect (GTK_OBJECT (w->network_entry), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);

  /* frame */
  frame = gtk_frame_new (_("Type"));
  gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* table */
  table = gtk_table_new (7, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);
  
  /* btns */
  w->network_type_aol = gtk_radio_button_new_with_label (NULL, _("America On-Line"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_aol, 0, 1, 0, 1);
  gtk_widget_show (w->network_type_aol);

  w->network_type_applelink = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("Apple Link"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_applelink, 0, 1, 1, 2);
  gtk_widget_show (w->network_type_applelink);

  w->network_type_attmail = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("AT&T"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_attmail, 0, 1, 2, 3);
  gtk_widget_show (w->network_type_attmail);

  w->network_type_cis = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("CIS"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_cis, 0, 1, 3, 4);
  gtk_widget_show (w->network_type_cis);

  w->network_type_eworld = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("e-World"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_eworld, 0, 1, 4, 5);
  gtk_widget_show (w->network_type_eworld);

  w->network_type_internet = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("Internet"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_internet, 0, 1, 5, 6);
  gtk_widget_show (w->network_type_internet);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_internet), TRUE);

  w->network_type_ibmmail = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("IBM"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_ibmmail, 0, 1, 6, 7);
  gtk_widget_show (w->network_type_ibmmail);

  w->network_type_mcimail = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("MCI"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_mcimail, 1, 2, 0, 1);
  gtk_widget_show (w->network_type_mcimail);

  w->network_type_powershare = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("Power Share"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_powershare, 1, 2, 1, 2);
  gtk_widget_show (w->network_type_powershare);

  w->network_type_prodigy = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("Prodigy"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_prodigy, 1, 2, 2, 3);
  gtk_widget_show (w->network_type_prodigy);

  w->network_type_tlx = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("TLX"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_tlx, 1, 2, 3, 4);
  gtk_widget_show (w->network_type_tlx);

  w->network_type_x400 = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->network_type_aol)),
				_("X400"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->network_type_x400, 1, 2, 4, 5);
  gtk_widget_show (w->network_type_x400);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox1, FALSE, FALSE, 0);
  gtk_widget_show (vbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* network_add_btn */
  w->network_add_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_ADD), _("Add"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->network_add_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->network_add_btn);
  gtk_signal_connect (GTK_OBJECT (w->network_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_add_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->network_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* network_modify_btn */
  w->network_modify_btn = gnome_pixmap_button (
    	gnome_stock_pixmap_widget (w->dialog, GNOME_STOCK_PIXMAP_PREFERENCES), _("Modify"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->network_modify_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->network_modify_btn);
  gtk_signal_connect (GTK_OBJECT (w->network_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_modify_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->network_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* network_remove_btn */
  w->network_remove_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_REMOVE), _("Remove"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->network_remove_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->network_remove_btn);
  gtk_signal_connect (GTK_OBJECT (w->network_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_remove_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->network_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
 
  /* frame */
  frame = gtk_frame_new (_("E-mail list"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox1);
  gtk_widget_show (vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox1), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /* list */
  w->network_list = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (scroll), w->network_list);
  gtk_widget_show (w->network_list);
  gtk_clist_set_selection_mode (GTK_CLIST (w->network_list), GTK_SELECTION_SINGLE);
  gtk_signal_connect (GTK_OBJECT (w->network_list), "select_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_clist_select_set_sensitive), w);
  gtk_signal_connect (GTK_OBJECT (w->network_list), "unselect_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_clist_unselect_set_sensitive), w);
  if (w->card) {
    for (list = w->card->email; list != NULL; list = list->next) {
      C2VCardEmail *email = C2VCARDEMAIL (list->data);
      gtk_clist_append (GTK_CLIST (w->network_list), (char **) &email->address);
      gtk_clist_set_row_data (GTK_CLIST (w->network_list), GTK_CLIST (w->network_list)->rows-1, email);
    }
  }

  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  /* up_button */
  w->network_up_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->network_up_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_UP));
  gtk_box_pack_start (GTK_BOX (hbox1), w->network_up_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->network_up_btn);
  gtk_signal_connect (GTK_OBJECT (w->network_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_up_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->network_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* down_button */
  w->network_down_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->network_down_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_DOWN));
  gtk_box_pack_start (GTK_BOX (hbox1), w->network_down_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->network_down_btn);
  gtk_signal_connect (GTK_OBJECT (w->network_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_network_page_down_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->network_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  gui_address_book_card_properties_network_page_set_sensitive (w->network_entry, w);
  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), vbox,
      					gtk_label_new (_("Network")),
					NOTEBOOK_PAGE_NETWORK);

  gtk_widget_show (vbox);
}

static void
gui_address_book_card_properties_network_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardEmail *email;

  g_return_if_fail (w);

  email = c2_address_book_card_email_new ();
  if (GTK_TOGGLE_BUTTON (w->network_type_aol)->active)
    email->type = C2_VCARD_EMAIL_AOL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_applelink)->active)
    email->type = C2_VCARD_EMAIL_APPLELINK;
  else if (GTK_TOGGLE_BUTTON (w->network_type_attmail)->active)
    email->type = C2_VCARD_EMAIL_ATTMAIL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_cis)->active)
    email->type = C2_VCARD_EMAIL_CIS;
  else if (GTK_TOGGLE_BUTTON (w->network_type_eworld)->active)
    email->type = C2_VCARD_EMAIL_EWORLD;
  else if (GTK_TOGGLE_BUTTON (w->network_type_internet)->active)
    email->type = C2_VCARD_EMAIL_INTERNET;
  else if (GTK_TOGGLE_BUTTON (w->network_type_ibmmail)->active)
    email->type = C2_VCARD_EMAIL_IBMMail;
  else if (GTK_TOGGLE_BUTTON (w->network_type_mcimail)->active)
    email->type = C2_VCARD_EMAIL_MCIMAIL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_powershare)->active)
    email->type = C2_VCARD_EMAIL_POWERSHARE;
  else if (GTK_TOGGLE_BUTTON (w->network_type_prodigy)->active)
    email->type = C2_VCARD_EMAIL_PRODIGY;
  else if (GTK_TOGGLE_BUTTON (w->network_type_tlx)->active)
    email->type = C2_VCARD_EMAIL_TLX;
  else if (GTK_TOGGLE_BUTTON (w->network_type_x400)->active)
    email->type = C2_VCARD_EMAIL_X400;

  email->address = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->network_entry)));

  gtk_clist_append (GTK_CLIST (w->network_list), &email->address);
  gtk_clist_set_row_data (GTK_CLIST (w->network_list), GTK_CLIST (w->network_list)->rows-1, email);

  gtk_entry_set_text (GTK_ENTRY (w->network_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_internet), TRUE);
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_network_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardEmail *email;
  int row;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->network_list)->selection) return;
  row = GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data);

  email = C2VCARDEMAIL (gtk_clist_get_row_data (GTK_CLIST (w->network_list),
					GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data)));
  if (email) {
    c2_free (email->address);
    c2_free (email);
  }

  email = c2_address_book_card_email_new ();
  if (GTK_TOGGLE_BUTTON (w->network_type_aol)->active)
    email->type = C2_VCARD_EMAIL_AOL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_applelink)->active)
    email->type = C2_VCARD_EMAIL_APPLELINK;
  else if (GTK_TOGGLE_BUTTON (w->network_type_attmail)->active)
    email->type = C2_VCARD_EMAIL_ATTMAIL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_cis)->active)
    email->type = C2_VCARD_EMAIL_CIS;
  else if (GTK_TOGGLE_BUTTON (w->network_type_eworld)->active)
    email->type = C2_VCARD_EMAIL_EWORLD;
  else if (GTK_TOGGLE_BUTTON (w->network_type_internet)->active)
    email->type = C2_VCARD_EMAIL_INTERNET;
  else if (GTK_TOGGLE_BUTTON (w->network_type_ibmmail)->active)
    email->type = C2_VCARD_EMAIL_IBMMail;
  else if (GTK_TOGGLE_BUTTON (w->network_type_mcimail)->active)
    email->type = C2_VCARD_EMAIL_MCIMAIL;
  else if (GTK_TOGGLE_BUTTON (w->network_type_powershare)->active)
    email->type = C2_VCARD_EMAIL_POWERSHARE;
  else if (GTK_TOGGLE_BUTTON (w->network_type_prodigy)->active)
    email->type = C2_VCARD_EMAIL_PRODIGY;
  else if (GTK_TOGGLE_BUTTON (w->network_type_tlx)->active)
    email->type = C2_VCARD_EMAIL_TLX;
  else if (GTK_TOGGLE_BUTTON (w->network_type_x400)->active)
    email->type = C2_VCARD_EMAIL_X400;

  email->address = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->network_entry)));

  gtk_clist_freeze (GTK_CLIST (w->network_list));
  gtk_clist_remove (GTK_CLIST (w->network_list), row);
  gtk_clist_insert (GTK_CLIST (w->network_list), row, &email->address);
  gtk_clist_thaw (GTK_CLIST (w->network_list)); 
  gtk_clist_set_row_data (GTK_CLIST (w->network_list), row, email);

  gtk_entry_set_text (GTK_ENTRY (w->network_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_internet), TRUE);
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_network_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardEmail *email;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->network_list)->selection) return;

  email = C2VCARDEMAIL (gtk_clist_get_row_data (GTK_CLIST (w->network_list),
					GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data)));
  if (email) {
    c2_free (email->address);
    c2_free (email);
  }
  
  gtk_clist_remove (GTK_CLIST (w->network_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data));
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_network_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->network_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data)-1);
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_network_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->network_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data)+1);
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_network_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w) {
  char *buf;
  
  g_return_if_fail (w);

  buf = gtk_entry_get_text (GTK_ENTRY (w->network_entry));
  if (!buf || !strlen (buf)) {
    gtk_widget_set_sensitive (w->network_type_aol, FALSE);
    gtk_widget_set_sensitive (w->network_type_applelink, FALSE);
    gtk_widget_set_sensitive (w->network_type_attmail, FALSE);
    gtk_widget_set_sensitive (w->network_type_cis, FALSE);
    gtk_widget_set_sensitive (w->network_type_eworld, FALSE);
    gtk_widget_set_sensitive (w->network_type_internet, FALSE);
    gtk_widget_set_sensitive (w->network_type_ibmmail, FALSE);
    gtk_widget_set_sensitive (w->network_type_mcimail, FALSE);
    gtk_widget_set_sensitive (w->network_type_powershare, FALSE);
    gtk_widget_set_sensitive (w->network_type_prodigy, FALSE);
    gtk_widget_set_sensitive (w->network_type_tlx, FALSE);
    gtk_widget_set_sensitive (w->network_type_x400, FALSE);
    gtk_widget_set_sensitive (w->network_add_btn, FALSE); 
  } else {
    gtk_widget_set_sensitive (w->network_type_aol, TRUE);
    gtk_widget_set_sensitive (w->network_type_applelink, TRUE);
    gtk_widget_set_sensitive (w->network_type_attmail, TRUE);
    gtk_widget_set_sensitive (w->network_type_cis, TRUE);
    gtk_widget_set_sensitive (w->network_type_eworld, TRUE);
    gtk_widget_set_sensitive (w->network_type_internet, TRUE);
    gtk_widget_set_sensitive (w->network_type_ibmmail, TRUE);
    gtk_widget_set_sensitive (w->network_type_mcimail, TRUE);
    gtk_widget_set_sensitive (w->network_type_powershare, TRUE);
    gtk_widget_set_sensitive (w->network_type_prodigy, TRUE);
    gtk_widget_set_sensitive (w->network_type_tlx, TRUE);
    gtk_widget_set_sensitive (w->network_type_x400, TRUE);
    gtk_widget_set_sensitive (w->network_add_btn, TRUE); 
  }

  if (GTK_CLIST (w->network_list)->selection) {
    gtk_widget_set_sensitive (w->network_modify_btn, TRUE);
    gtk_widget_set_sensitive (w->network_remove_btn, TRUE);
    gtk_widget_set_sensitive (w->network_up_btn, TRUE);
    gtk_widget_set_sensitive (w->network_down_btn, TRUE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data) ==
	GTK_CLIST (w->network_list)->rows-1)
      gtk_widget_set_sensitive (w->network_down_btn, FALSE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->network_list)->selection->data) == 0)
      gtk_widget_set_sensitive (w->network_up_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (w->network_modify_btn, FALSE);
    gtk_widget_set_sensitive (w->network_remove_btn, FALSE);
    gtk_widget_set_sensitive (w->network_up_btn, FALSE);
    gtk_widget_set_sensitive (w->network_down_btn, FALSE);
  }
}

static void
gui_address_book_card_properties_network_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  C2VCardEmail *email;
  
  g_return_if_fail (w);
  
  email = C2VCARDEMAIL (gtk_clist_get_row_data (GTK_CLIST (w->network_list), row));
  if (!email) {
    gtk_clist_remove (GTK_CLIST (w->network_list), row);
    return;
  }

  gtk_entry_set_text (GTK_ENTRY (w->network_entry), email->address);
  if (email->type == C2_VCARD_EMAIL_AOL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_aol), TRUE);
  else if (email->type == C2_VCARD_EMAIL_APPLELINK)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_applelink), TRUE);
  else if (email->type == C2_VCARD_EMAIL_ATTMAIL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_attmail), TRUE);
  else if (email->type == C2_VCARD_EMAIL_CIS)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_cis), TRUE);
  else if (email->type == C2_VCARD_EMAIL_EWORLD)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_eworld), TRUE);
  else if (email->type == C2_VCARD_EMAIL_INTERNET)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_internet), TRUE);
  else if (email->type == C2_VCARD_EMAIL_IBMMail)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_ibmmail), TRUE);
  else if (email->type == C2_VCARD_EMAIL_MCIMAIL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_mcimail), TRUE);
  else if (email->type == C2_VCARD_EMAIL_POWERSHARE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_powershare), TRUE);
  else if (email->type == C2_VCARD_EMAIL_PRODIGY)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_prodigy), TRUE);
  else if (email->type == C2_VCARD_EMAIL_TLX)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_tlx), TRUE);
  else if (email->type == C2_VCARD_EMAIL_X400)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_x400), TRUE);
  gui_address_book_card_properties_network_page_set_sensitive (ob, w);
}

static void
gui_address_book_card_properties_network_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_entry_set_text (GTK_ENTRY (w->network_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->network_type_internet), TRUE);
  gui_address_book_card_properties_network_page_set_sensitive (NULL, w);
}

/* Address */
static void
gui_address_book_card_properties_address_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_address_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_address_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *vbox1, *hbox, *hbox1, *table;
  GtkWidget *frame, *scroll, *label;
  GList *list;

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);

  /* frame */
  frame = gtk_frame_new (_("Address data"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* vbox */
  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /* table */
  table = gtk_table_new (7, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
  gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);

  /* labels */
  label = gtk_label_new (_("Post Office:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("Extended:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("Street:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("City:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("Region:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("Postal Code:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 5, 6, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  label = gtk_label_new (_("Country:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 6, 7, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

  /* entries */
  w->address_post_office = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_post_office, 1, 2, 0, 1);
  gtk_widget_show (w->address_post_office);
  gtk_signal_connect (GTK_OBJECT (w->address_post_office), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_extended = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_extended, 1, 2, 1, 2);
  gtk_widget_show (w->address_extended);
  gtk_signal_connect (GTK_OBJECT (w->address_extended), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_street = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_street, 1, 2, 2, 3);
  gtk_widget_show (w->address_street);
  gtk_signal_connect (GTK_OBJECT (w->address_street), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_city = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_city, 1, 2, 3, 4);
  gtk_widget_show (w->address_city);
  gtk_signal_connect (GTK_OBJECT (w->address_city), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_province = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_province, 1, 2, 4, 5);
  gtk_widget_show (w->address_province);
  gtk_signal_connect (GTK_OBJECT (w->address_province), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_postal_code = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_postal_code, 1, 2, 5, 6);
  gtk_widget_show (w->address_postal_code);
  gtk_signal_connect (GTK_OBJECT (w->address_postal_code), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  w->address_country = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_country, 1, 2, 6, 7);
  gtk_widget_show (w->address_country);
  gtk_signal_connect (GTK_OBJECT (w->address_country), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_set_sensitive), w);

  /* frame */
  frame = gtk_frame_new (_("Type"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* table */
  table = gtk_table_new (7, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);
  
  /* btns */
  w->address_type_private = gtk_check_button_new_with_label (_("Home"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_private, 0, 1, 0, 1);
  gtk_widget_show (w->address_type_private);

  w->address_type_work = gtk_check_button_new_with_label (_("Work"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_work, 0, 1, 1, 2);
  gtk_widget_show (w->address_type_work);

  w->address_type_postal_box = gtk_check_button_new_with_label (_("Postal Box"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_postal_box, 0, 1, 2, 3);
  gtk_widget_show (w->address_type_postal_box);

  w->address_type_parcel = gtk_check_button_new_with_label (_("Parcel"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_parcel, 1, 2, 0, 1);
  gtk_widget_show (w->address_type_parcel);

  w->address_type_domestic = gtk_check_button_new_with_label (_("Domestic"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_domestic, 1, 2, 1, 2);
  gtk_widget_show (w->address_type_domestic);

  w->address_type_international = gtk_check_button_new_with_label (_("International"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->address_type_international, 1, 2, 2, 3);
  gtk_widget_show (w->address_type_international);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox1, FALSE, FALSE, 0);
  gtk_widget_show (vbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* address_add_btn */
  w->address_add_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_ADD), _("Add"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->address_add_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->address_add_btn);
  gtk_signal_connect (GTK_OBJECT (w->address_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_add_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->address_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* address_modify_btn */
  w->address_modify_btn = gnome_pixmap_button (
    	gnome_stock_pixmap_widget (w->dialog, GNOME_STOCK_PIXMAP_PREFERENCES), _("Modify"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->address_modify_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->address_modify_btn);
  gtk_signal_connect (GTK_OBJECT (w->address_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_modify_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->address_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* address_remove_btn */
  w->address_remove_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_REMOVE), _("Remove"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->address_remove_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->address_remove_btn);
  gtk_signal_connect (GTK_OBJECT (w->address_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_remove_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->address_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
 
  /* frame */
  frame = gtk_frame_new (_("Address list"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox1);
  gtk_widget_show (vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox1), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /* list */
  w->address_list = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (scroll), w->address_list);
  gtk_widget_show (w->address_list);
  gtk_clist_set_selection_mode (GTK_CLIST (w->address_list), GTK_SELECTION_SINGLE);
  gtk_signal_connect (GTK_OBJECT (w->address_list), "select_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_clist_select_set_sensitive), w);
  gtk_signal_connect (GTK_OBJECT (w->address_list), "unselect_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_clist_unselect_set_sensitive), w);
  if (w->card) {
    for (list = w->card->address; list != NULL; list = list->next) {
      C2VCardAddress *addr = C2VCARDADDRESS (list->data);
      gtk_clist_append (GTK_CLIST (w->address_list), (char **) &addr->post_office);
      gtk_clist_set_row_data (GTK_CLIST (w->address_list), GTK_CLIST (w->address_list)->rows-1, addr);
    }
  }

  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  /* up_button */
  w->address_up_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->address_up_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_UP));
  gtk_box_pack_start (GTK_BOX (hbox1), w->address_up_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->address_up_btn);
  gtk_signal_connect (GTK_OBJECT (w->address_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_up_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->address_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* down_button */
  w->address_down_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->address_down_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_DOWN));
  gtk_box_pack_start (GTK_BOX (hbox1), w->address_down_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->address_down_btn);
  gtk_signal_connect (GTK_OBJECT (w->address_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_address_page_down_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->address_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), hbox,
      					gtk_label_new (_("Addresses")),
					NOTEBOOK_PAGE_ADDRESSES);

  gtk_widget_show (hbox);
}

static void
gui_address_book_card_properties_address_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardAddress *addr;

  g_return_if_fail (w);

  addr = c2_address_book_card_address_new ();
  if (GTK_TOGGLE_BUTTON (w->address_type_private)->active)
    addr->type ^= C2_VCARD_ADDRESSES_HOME;
  if (GTK_TOGGLE_BUTTON (w->address_type_work)->active)
    addr->type ^= C2_VCARD_ADDRESSES_WORK;
  if (GTK_TOGGLE_BUTTON (w->address_type_postal_box)->active)
    addr->type ^= C2_VCARD_ADDRESSES_POSTALBOX;
  if (GTK_TOGGLE_BUTTON (w->address_type_parcel)->active)
    addr->type ^= C2_VCARD_ADDRESSES_PARCEL;
  if (GTK_TOGGLE_BUTTON (w->address_type_domestic)->active)
    addr->type ^= C2_VCARD_ADDRESSES_DOMESTIC;
  if (GTK_TOGGLE_BUTTON (w->address_type_international)->active)
    addr->type ^= C2_VCARD_ADDRESSES_INTERNATIONAL;

  addr->post_office = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_post_office)));
  addr->extended = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_extended)));
  addr->street = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_street)));
  addr->city = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_city)));
  addr->region = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_province)));
  addr->postal_code = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_postal_code)));
  addr->country = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_country)));

  gtk_clist_append (GTK_CLIST (w->address_list), &addr->post_office);
  gtk_clist_set_row_data (GTK_CLIST (w->address_list), GTK_CLIST (w->address_list)->rows-1, addr);

  gtk_entry_set_text (GTK_ENTRY (w->address_post_office), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_extended), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_street), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_city), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_province), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_postal_code), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_country), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_private), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_postal_box), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_parcel), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_domestic), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_international), FALSE);
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_address_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardAddress *addr;
  int row;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->address_list)->selection) return;
  row = GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data);

  addr = C2VCARDADDRESS (gtk_clist_get_row_data (GTK_CLIST (w->address_list),
					GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data)));
  if (addr) {
    c2_free (addr->post_office);
    c2_free (addr->extended);
    c2_free (addr->street);
    c2_free (addr->city);
    c2_free (addr->region);
    c2_free (addr->postal_code);
    c2_free (addr->country);
    c2_free (addr);
  }

  addr = c2_address_book_card_address_new ();
  if (GTK_TOGGLE_BUTTON (w->address_type_private)->active)
    addr->type ^= C2_VCARD_ADDRESSES_HOME;
  if (GTK_TOGGLE_BUTTON (w->address_type_work)->active)
    addr->type ^= C2_VCARD_ADDRESSES_WORK;
  if (GTK_TOGGLE_BUTTON (w->address_type_postal_box)->active)
    addr->type ^= C2_VCARD_ADDRESSES_POSTALBOX;
  if (GTK_TOGGLE_BUTTON (w->address_type_parcel)->active)
    addr->type ^= C2_VCARD_ADDRESSES_PARCEL;
  if (GTK_TOGGLE_BUTTON (w->address_type_domestic)->active)
    addr->type ^= C2_VCARD_ADDRESSES_DOMESTIC;
  if (GTK_TOGGLE_BUTTON (w->address_type_international)->active)
    addr->type ^= C2_VCARD_ADDRESSES_INTERNATIONAL;

  addr->post_office = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_post_office)));
  addr->extended = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_extended)));
  addr->street = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_street)));
  addr->city = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_city)));
  addr->region = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_province)));
  addr->postal_code = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_postal_code)));
  addr->country = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->address_country)));

  gtk_clist_freeze (GTK_CLIST (w->address_list));
  gtk_clist_remove (GTK_CLIST (w->address_list), row);
  gtk_clist_insert (GTK_CLIST (w->address_list), row, &addr->post_office);
  gtk_clist_thaw (GTK_CLIST (w->address_list)); 
  gtk_clist_set_row_data (GTK_CLIST (w->address_list), row, addr);

  gtk_entry_set_text (GTK_ENTRY (w->address_post_office), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_extended), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_street), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_city), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_province), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_postal_code), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_country), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_private), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_postal_box), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_parcel), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_domestic), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_international), FALSE);
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_address_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardAddress *addr;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->address_list)->selection) return;

  addr = C2VCARDADDRESS (gtk_clist_get_row_data (GTK_CLIST (w->address_list),
					GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data)));
  if (addr) {
    c2_free (addr->post_office);
    c2_free (addr->extended);
    c2_free (addr->street);
    c2_free (addr->city);
    c2_free (addr->region);
    c2_free (addr->postal_code);
    c2_free (addr->country);
    c2_free (addr);
  }
  
  gtk_clist_remove (GTK_CLIST (w->address_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data));
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_address_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->address_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data)-1);
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_address_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->address_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data)+1);
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_address_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w) {
  char *buf;
  
  g_return_if_fail (w);

  buf = gtk_entry_get_text (GTK_ENTRY (w->address_post_office));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_extended));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_street));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_city));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_province));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_postal_code));
  if (!buf || !strlen (buf))
    buf = gtk_entry_get_text (GTK_ENTRY (w->address_country));
  if (!buf || !strlen (buf)) {
    gtk_widget_set_sensitive (w->address_type_private, FALSE);
    gtk_widget_set_sensitive (w->address_type_work, FALSE);
    gtk_widget_set_sensitive (w->address_type_postal_box, FALSE);
    gtk_widget_set_sensitive (w->address_type_parcel, FALSE);
    gtk_widget_set_sensitive (w->address_type_domestic, FALSE);
    gtk_widget_set_sensitive (w->address_type_international, FALSE);
    gtk_widget_set_sensitive (w->address_add_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (w->address_type_private, TRUE);
    gtk_widget_set_sensitive (w->address_type_work, TRUE);
    gtk_widget_set_sensitive (w->address_type_postal_box, TRUE);
    gtk_widget_set_sensitive (w->address_type_parcel, TRUE);
    gtk_widget_set_sensitive (w->address_type_domestic, TRUE);
    gtk_widget_set_sensitive (w->address_type_international, TRUE);
    gtk_widget_set_sensitive (w->address_add_btn, TRUE);
  }

  if (GTK_CLIST (w->address_list)->selection) {
    gtk_widget_set_sensitive (w->address_modify_btn, TRUE);
    gtk_widget_set_sensitive (w->address_remove_btn, TRUE);
    gtk_widget_set_sensitive (w->address_up_btn, TRUE);
    gtk_widget_set_sensitive (w->address_down_btn, TRUE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data) ==
	GTK_CLIST (w->address_list)->rows-1)
      gtk_widget_set_sensitive (w->address_down_btn, FALSE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->address_list)->selection->data) == 0)
      gtk_widget_set_sensitive (w->address_up_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (w->address_modify_btn, FALSE);
    gtk_widget_set_sensitive (w->address_remove_btn, FALSE);
    gtk_widget_set_sensitive (w->address_up_btn, FALSE);
    gtk_widget_set_sensitive (w->address_down_btn, FALSE);
  }
}

static void
gui_address_book_card_properties_address_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  C2VCardAddress *addr;
  
  g_return_if_fail (w);
  
  addr = C2VCARDADDRESS (gtk_clist_get_row_data (GTK_CLIST (w->address_list), row));
  if (!addr) {
    gtk_clist_remove (GTK_CLIST (w->address_list), row);
    return;
  }

  gtk_entry_set_text (GTK_ENTRY (w->address_post_office), addr->post_office);
  gtk_entry_set_text (GTK_ENTRY (w->address_extended), addr->extended);
  gtk_entry_set_text (GTK_ENTRY (w->address_street), addr->street);
  gtk_entry_set_text (GTK_ENTRY (w->address_city), addr->city);
  gtk_entry_set_text (GTK_ENTRY (w->address_province), addr->region);
  gtk_entry_set_text (GTK_ENTRY (w->address_postal_code), addr->postal_code);
  gtk_entry_set_text (GTK_ENTRY (w->address_country), addr->country);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_private), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_postal_box), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_parcel), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_domestic), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_international), FALSE);
  if (addr->type & C2_VCARD_ADDRESSES_HOME)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_private), TRUE);
  if (addr->type & C2_VCARD_ADDRESSES_WORK)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_work), TRUE);
  if (addr->type & C2_VCARD_ADDRESSES_POSTALBOX)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_postal_box), TRUE);
  if (addr->type & C2_VCARD_ADDRESSES_PARCEL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_parcel), TRUE);
  if (addr->type & C2_VCARD_ADDRESSES_DOMESTIC)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_domestic), TRUE);
  if (addr->type & C2_VCARD_ADDRESSES_INTERNATIONAL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_international), TRUE);
  gui_address_book_card_properties_address_page_set_sensitive (ob, w);
}

static void
gui_address_book_card_properties_address_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_entry_set_text (GTK_ENTRY (w->address_post_office), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_extended), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_street), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_city), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_province), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_postal_code), "");
  gtk_entry_set_text (GTK_ENTRY (w->address_country), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_private), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_postal_box), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_parcel), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_domestic), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->address_type_international), FALSE);
  gui_address_book_card_properties_address_page_set_sensitive (NULL, w);
}

/* Phone */
static void
gui_address_book_card_properties_phone_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    								          GdkEvent *event,
								          C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_phone_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w);

static void
gui_address_book_card_properties_add_phone_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *vbox1, *hbox, *hbox1, *table;
  GtkWidget *frame, *scroll, *label;
  GList *list;

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);

  /* frame */
  frame = gtk_frame_new (_("Telephone data"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* vbox */
  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new (_("Number:"));
  gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* phone_entry */
  w->phone_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), w->phone_entry, TRUE, TRUE, 0);
  gtk_widget_show (w->phone_entry);
  gtk_signal_connect (GTK_OBJECT (w->phone_entry), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_set_sensitive), w);

  /* frame */
  frame = gtk_frame_new (_("Type"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* table */
  table = gtk_table_new (7, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);
  
  /* btns */
  w->phone_type_preferred = gtk_check_button_new_with_label (_("Preferred"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_preferred, 0, 1, 0, 1);
  gtk_widget_show (w->phone_type_preferred);

  w->phone_type_work = gtk_check_button_new_with_label (_("Work"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_work, 0, 1, 1, 2);
  gtk_widget_show (w->phone_type_work);

  w->phone_type_home = gtk_check_button_new_with_label (_("Home"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_home, 0, 1, 2, 3);
  gtk_widget_show (w->phone_type_home);

  w->phone_type_voice = gtk_check_button_new_with_label (_("Voice"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_voice, 0, 1, 3, 4);
  gtk_widget_show (w->phone_type_voice);

  w->phone_type_fax = gtk_check_button_new_with_label (_("Fax"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_fax, 0, 1, 4, 5);
  gtk_widget_show (w->phone_type_fax);

  w->phone_type_message_recorder = gtk_check_button_new_with_label (_("Message Recorder"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_message_recorder, 0, 1, 5, 6);
  gtk_widget_show (w->phone_type_message_recorder);

  w->phone_type_cellular = gtk_check_button_new_with_label (_("Cellular"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_cellular, 0, 1, 6, 7);
  gtk_widget_show (w->phone_type_cellular);

  w->phone_type_pager = gtk_check_button_new_with_label (_("Pager"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_pager, 1, 2, 0, 1);
  gtk_widget_show (w->phone_type_pager);

  w->phone_type_bulletin_board = gtk_check_button_new_with_label (_("Bulletin Board"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_bulletin_board, 1, 2, 1, 2);
  gtk_widget_show (w->phone_type_bulletin_board);

  w->phone_type_modem = gtk_check_button_new_with_label (_("Modem"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_modem, 1, 2, 2, 3);
  gtk_widget_show (w->phone_type_modem);

  w->phone_type_car = gtk_check_button_new_with_label (_("Car"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_car, 1, 2, 3, 4);
  gtk_widget_show (w->phone_type_car);

  w->phone_type_isdn = gtk_check_button_new_with_label (_("ISDN"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_isdn, 1, 2, 4, 5);
  gtk_widget_show (w->phone_type_isdn);

  w->phone_type_video = gtk_check_button_new_with_label (_("Video"));
  gtk_table_attach_defaults (GTK_TABLE (table), w->phone_type_video, 1, 2, 5, 6);
  gtk_widget_show (w->phone_type_video);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox1, FALSE, FALSE, 0);
  gtk_widget_show (vbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* phone_add_btn */
  w->phone_add_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_ADD), _("Add"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->phone_add_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->phone_add_btn);
  gtk_signal_connect (GTK_OBJECT (w->phone_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_add_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_add_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* phone_modify_btn */
  w->phone_modify_btn = gnome_pixmap_button (
    	gnome_stock_pixmap_widget (w->dialog, GNOME_STOCK_PIXMAP_PREFERENCES), _("Modify"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->phone_modify_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->phone_modify_btn);
  gtk_signal_connect (GTK_OBJECT (w->phone_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_modify_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_modify_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* phone_remove_btn */
  w->phone_remove_btn = gnome_pixmap_button (
      gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_REMOVE), _("Remove"));
  gtk_box_pack_start (GTK_BOX (vbox1), w->phone_remove_btn, FALSE, FALSE, 0);
  gtk_widget_show (w->phone_remove_btn);
  gtk_signal_connect (GTK_OBJECT (w->phone_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_remove_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_remove_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  
  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
 
  /* frame */
  frame = gtk_frame_new (_("Phone list"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox1);
  gtk_widget_show (vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox1), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  /* list */
  w->phone_list = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (scroll), w->phone_list);
  gtk_widget_show (w->phone_list);
  gtk_clist_set_selection_mode (GTK_CLIST (w->phone_list), GTK_SELECTION_SINGLE);
  gtk_signal_connect (GTK_OBJECT (w->phone_list), "select_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_clist_select_set_sensitive), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_list), "unselect_row",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_clist_unselect_set_sensitive), w);
  if (w->card) {
    for (list = w->card->phone; list != NULL; list = list->next) {
      C2VCardPhone *phone = C2VCARDPHONE (list->data);
      gtk_clist_append (GTK_CLIST (w->phone_list), (char **) &phone->number);
      gtk_clist_set_row_data (GTK_CLIST (w->phone_list), GTK_CLIST (w->phone_list)->rows-1, phone);
    }
  }


  /* hbox1 */
  hbox1 = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  /* up_button */
  w->phone_up_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->phone_up_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_UP));
  gtk_box_pack_start (GTK_BOX (hbox1), w->phone_up_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->phone_up_btn);
  gtk_signal_connect (GTK_OBJECT (w->phone_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_up_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_up_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* down_button */
  w->phone_down_btn = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (w->phone_down_btn),
      	gnome_stock_pixmap_widget (hbox1, GNOME_STOCK_PIXMAP_DOWN));
  gtk_box_pack_start (GTK_BOX (hbox1), w->phone_down_btn, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all (w->phone_down_btn);
  gtk_signal_connect (GTK_OBJECT (w->phone_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_phone_page_down_btn_clicked), w);
  gtk_signal_connect (GTK_OBJECT (w->phone_down_btn), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);

  /* label */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), hbox,
      					gtk_label_new (_("Phone")),
					NOTEBOOK_PAGE_PHONE);

  gtk_widget_show (hbox);
}

static void
gui_address_book_card_properties_phone_page_add_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardPhone *phone;

  g_return_if_fail (w);

  phone = c2_address_book_card_phone_new ();
  if (GTK_TOGGLE_BUTTON (w->phone_type_preferred)->active)
    phone->type ^= C2_VCARD_PHONE_PREFERRED;
  if (GTK_TOGGLE_BUTTON (w->phone_type_work)->active)
    phone->type ^= C2_VCARD_PHONE_WORK;
  if (GTK_TOGGLE_BUTTON (w->phone_type_home)->active)
    phone->type ^= C2_VCARD_PHONE_HOME;
  if (GTK_TOGGLE_BUTTON (w->phone_type_voice)->active)
    phone->type ^= C2_VCARD_PHONE_VOICE;
  if (GTK_TOGGLE_BUTTON (w->phone_type_fax)->active)
    phone->type ^= C2_VCARD_PHONE_FAX;
  if (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder)->active)
    phone->type ^= C2_VCARD_PHONE_MESSAGERECORDER;
  if (GTK_TOGGLE_BUTTON (w->phone_type_cellular)->active)
    phone->type ^= C2_VCARD_PHONE_CELLULAR;
  if (GTK_TOGGLE_BUTTON (w->phone_type_pager)->active)
    phone->type ^= C2_VCARD_PHONE_PAGER;
  if (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board)->active)
    phone->type ^= C2_VCARD_PHONE_BULLETINBOARD;
  if (GTK_TOGGLE_BUTTON (w->phone_type_modem)->active)
    phone->type ^= C2_VCARD_PHONE_MODEM;
  if (GTK_TOGGLE_BUTTON (w->phone_type_car)->active)
    phone->type ^= C2_VCARD_PHONE_CAR;
  if (GTK_TOGGLE_BUTTON (w->phone_type_isdn)->active)
    phone->type ^= C2_VCARD_PHONE_ISDN;
  if (GTK_TOGGLE_BUTTON (w->phone_type_video)->active)
    phone->type ^= C2_VCARD_PHONE_VIDEO;

  phone->number = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->phone_entry)));

  gtk_clist_append (GTK_CLIST (w->phone_list), &phone->number);
  gtk_clist_set_row_data (GTK_CLIST (w->phone_list), GTK_CLIST (w->phone_list)->rows-1, phone);

  gtk_entry_set_text (GTK_ENTRY (w->phone_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_preferred), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_home), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_voice), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_fax), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_cellular), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_pager), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_modem), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_car), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_isdn), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_video), FALSE);
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_phone_page_modify_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardPhone *phone;
  int row;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->phone_list)->selection) return;
  row = GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data);

  phone = C2VCARDPHONE (gtk_clist_get_row_data (GTK_CLIST (w->phone_list),
					GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data)));
  if (phone) {
    c2_free (phone->number);
    c2_free (phone);
  }

  phone = c2_address_book_card_phone_new ();
  if (GTK_TOGGLE_BUTTON (w->phone_type_preferred)->active)
    phone->type ^= C2_VCARD_PHONE_PREFERRED;
  if (GTK_TOGGLE_BUTTON (w->phone_type_work)->active)
    phone->type ^= C2_VCARD_PHONE_WORK;
  if (GTK_TOGGLE_BUTTON (w->phone_type_home)->active)
    phone->type ^= C2_VCARD_PHONE_HOME;
  if (GTK_TOGGLE_BUTTON (w->phone_type_voice)->active)
    phone->type ^= C2_VCARD_PHONE_VOICE;
  if (GTK_TOGGLE_BUTTON (w->phone_type_fax)->active)
    phone->type ^= C2_VCARD_PHONE_FAX;
  if (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder)->active)
    phone->type ^= C2_VCARD_PHONE_MESSAGERECORDER;
  if (GTK_TOGGLE_BUTTON (w->phone_type_cellular)->active)
    phone->type ^= C2_VCARD_PHONE_CELLULAR;
  if (GTK_TOGGLE_BUTTON (w->phone_type_pager)->active)
    phone->type ^= C2_VCARD_PHONE_PAGER;
  if (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board)->active)
    phone->type ^= C2_VCARD_PHONE_BULLETINBOARD;
  if (GTK_TOGGLE_BUTTON (w->phone_type_modem)->active)
    phone->type ^= C2_VCARD_PHONE_MODEM;
  if (GTK_TOGGLE_BUTTON (w->phone_type_car)->active)
    phone->type ^= C2_VCARD_PHONE_CAR;
  if (GTK_TOGGLE_BUTTON (w->phone_type_isdn)->active)
    phone->type ^= C2_VCARD_PHONE_ISDN;
  if (GTK_TOGGLE_BUTTON (w->phone_type_video)->active)
    phone->type ^= C2_VCARD_PHONE_VIDEO;

  phone->number = g_strdup (gtk_entry_get_text (GTK_ENTRY (w->phone_entry)));

  gtk_clist_freeze (GTK_CLIST (w->phone_list));
  gtk_clist_remove (GTK_CLIST (w->phone_list), row);
  gtk_clist_insert (GTK_CLIST (w->phone_list), row, &phone->number);
  gtk_clist_thaw (GTK_CLIST (w->phone_list)); 
  gtk_clist_set_row_data (GTK_CLIST (w->phone_list), row, phone);

  gtk_entry_set_text (GTK_ENTRY (w->phone_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_preferred), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_home), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_voice), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_fax), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_cellular), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_pager), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_modem), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_car), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_isdn), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_video), FALSE);
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_phone_page_remove_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  C2VCardPhone *phone;

  g_return_if_fail (w);

  if (!GTK_CLIST (w->phone_list)->selection) return;

  phone = C2VCARDPHONE (gtk_clist_get_row_data (GTK_CLIST (w->phone_list),
					GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data)));
  if (phone) {
    c2_free (phone->number);
    c2_free (phone);
  }
  
  gtk_clist_remove (GTK_CLIST (w->phone_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data));
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_phone_page_up_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->phone_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data)-1);
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_phone_page_down_btn_clicked (GtkWidget *ob, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_clist_swap_rows (GTK_CLIST (w->phone_list),
      			GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data),
			GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data)+1);
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

static void
gui_address_book_card_properties_phone_page_set_sensitive (GtkWidget *ob, C2AddressBookCardProperties *w) {
  char *buf;
  
  g_return_if_fail (w);

  buf = gtk_entry_get_text (GTK_ENTRY (w->phone_entry));
  if (!buf || !strlen (buf)) {
    gtk_widget_set_sensitive ((w->phone_type_preferred), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_work), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_home), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_voice), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_fax), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_message_recorder), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_cellular), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_pager), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_bulletin_board), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_modem), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_car), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_isdn), FALSE);
    gtk_widget_set_sensitive ((w->phone_type_video), FALSE);
    gtk_widget_set_sensitive (w->phone_add_btn, FALSE);
  } else {
    gtk_widget_set_sensitive ((w->phone_type_preferred), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_work), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_home), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_voice), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_fax), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_message_recorder), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_cellular), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_pager), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_bulletin_board), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_modem), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_car), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_isdn), TRUE);
    gtk_widget_set_sensitive ((w->phone_type_video), TRUE);
    gtk_widget_set_sensitive (w->phone_add_btn, TRUE);
  }

  if (GTK_CLIST (w->phone_list)->selection) {
    gtk_widget_set_sensitive (w->phone_modify_btn, TRUE);
    gtk_widget_set_sensitive (w->phone_remove_btn, TRUE);
    gtk_widget_set_sensitive (w->phone_up_btn, TRUE);
    gtk_widget_set_sensitive (w->phone_down_btn, TRUE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data) ==
	GTK_CLIST (w->phone_list)->rows-1)
      gtk_widget_set_sensitive (w->phone_down_btn, FALSE);
    if (GPOINTER_TO_INT (GTK_CLIST (w->phone_list)->selection->data) == 0)
      gtk_widget_set_sensitive (w->phone_up_btn, FALSE);
  } else {
    gtk_widget_set_sensitive (w->phone_modify_btn, FALSE);
    gtk_widget_set_sensitive (w->phone_remove_btn, FALSE);
    gtk_widget_set_sensitive (w->phone_up_btn, FALSE);
    gtk_widget_set_sensitive (w->phone_down_btn, FALSE);
  }
}

static void
gui_address_book_card_properties_phone_page_clist_select_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  C2VCardPhone *phone;
  
  g_return_if_fail (w);
  
  phone = C2VCARDPHONE (gtk_clist_get_row_data (GTK_CLIST (w->phone_list), row));
  if (!phone) {
    gtk_clist_remove (GTK_CLIST (w->phone_list), row);
    return;
  }

  gtk_entry_set_text (GTK_ENTRY (w->phone_entry), phone->number);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_preferred), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_home), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_voice), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_fax), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_cellular), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_pager), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_modem), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_car), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_isdn), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_video), FALSE);
  if (phone->type & C2_VCARD_PHONE_PREFERRED)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_preferred), TRUE);
  if (phone->type & C2_VCARD_PHONE_WORK)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_work), TRUE);
  if (phone->type & C2_VCARD_PHONE_HOME)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_home), TRUE);
  if (phone->type & C2_VCARD_PHONE_VOICE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_voice), TRUE);
  if (phone->type & C2_VCARD_PHONE_FAX)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_fax), TRUE);
  if (phone->type & C2_VCARD_PHONE_MESSAGERECORDER)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder), TRUE);
  if (phone->type & C2_VCARD_PHONE_CELLULAR)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_cellular), TRUE);
  if (phone->type & C2_VCARD_PHONE_PAGER)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_pager), TRUE);
  if (phone->type & C2_VCARD_PHONE_BULLETINBOARD)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board), TRUE);
  if (phone->type & C2_VCARD_PHONE_MODEM)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_modem), TRUE);
  if (phone->type & C2_VCARD_PHONE_CAR)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_car), TRUE);
  if (phone->type & C2_VCARD_PHONE_ISDN)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_isdn), TRUE);
  if (phone->type & C2_VCARD_PHONE_VIDEO)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_video), TRUE);
  gui_address_book_card_properties_phone_page_set_sensitive (ob, w);
}

static void
gui_address_book_card_properties_phone_page_clist_unselect_set_sensitive (GtkWidget *ob, int row, int col,
    									  GdkEvent *event,
								   	  C2AddressBookCardProperties *w) {
  g_return_if_fail (w);
  gtk_entry_set_text (GTK_ENTRY (w->phone_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_preferred), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_work), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_home), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_voice), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_fax), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_message_recorder), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_cellular), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_pager), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_bulletin_board), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_modem), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_car), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_isdn), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->phone_type_video), FALSE);
  gui_address_book_card_properties_phone_page_set_sensitive (NULL, w);
}

/** Etcetera **/
static void
gui_address_book_card_properties_add_etcetera_page (C2AddressBookCardProperties *w) {
  GtkWidget *vbox, *hbox, *vbox1;
  GtkWidget *label, *scroll, *frame;
  
  g_return_if_fail (w);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

  /* hbox */
  hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);
  
  /* label */
  label = gtk_label_new (_("Categories:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label); 

  /* categories */
  w->etcetera_categories = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), w->etcetera_categories, TRUE, TRUE, 0);
  gtk_widget_show (w->etcetera_categories);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_categories), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_categories), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  if (w->card) gtk_entry_set_text (GTK_ENTRY (w->etcetera_categories), w->card->categories);

  /* label */
  label = gtk_label_new (_("Comment:"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, FALSE, FALSE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  
  /* comment */
  w->etcetera_comments = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), w->etcetera_comments);
  gtk_widget_show (w->etcetera_comments);
  gtk_text_set_editable (GTK_TEXT (w->etcetera_comments), TRUE);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_comments), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_comments), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  if (w->card) gtk_text_insert (GTK_TEXT (w->etcetera_comments), NULL, NULL, NULL, w->card->comment, -1);

  /* frame */
  frame = gtk_frame_new (_("Security"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* vbox1 */
  vbox1 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_add (GTK_CONTAINER (frame), vbox1);
  gtk_widget_show (vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 4);

  /* label */
  label = gtk_label_new (_("Public Key:"));
  gtk_box_pack_start (GTK_BOX (vbox1), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* scroll */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox1), scroll, FALSE, FALSE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  
  /* security */
  w->etcetera_security = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), w->etcetera_security);
  gtk_widget_show (w->etcetera_security);
  gtk_text_set_editable (GTK_TEXT (w->etcetera_security), TRUE);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_security), "changed",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_security), "focus_in_event",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_help), w);
  if (w->card) gtk_text_insert (GTK_TEXT (w->etcetera_security), NULL, NULL, NULL, w->card->security, -1);

  /* hbox */
  hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* btns */
  w->etcetera_security_type_pgp = gtk_radio_button_new_with_label (NULL, _("PGP"));
  gtk_box_pack_start (GTK_BOX (hbox), w->etcetera_security_type_pgp, FALSE, FALSE, 0);
  gtk_widget_show (w->etcetera_security_type_pgp);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_security_type_pgp), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  if (w->card && w->card->sectype == C2_VCARD_SECURITY_PGP)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->etcetera_security_type_pgp), TRUE);

  w->etcetera_security_type_x509 = gtk_radio_button_new_with_label (
		  		gtk_radio_button_group (GTK_RADIO_BUTTON (w->etcetera_security_type_pgp)),
				_("X509"));
  gtk_box_pack_start (GTK_BOX (hbox), w->etcetera_security_type_x509, FALSE, FALSE, 0);
  gtk_widget_show (w->etcetera_security_type_x509);
  gtk_signal_connect (GTK_OBJECT (w->etcetera_security_type_x509), "clicked",
      			GTK_SIGNAL_FUNC (gui_address_book_card_properties_set_active), w);
  if (w->card && w->card->sectype == C2_VCARD_SECURITY_X509)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w->etcetera_security_type_x509), TRUE);
  
  gtk_notebook_insert_page (GTK_NOTEBOOK (GNOME_PROPERTY_BOX (w->dialog)->notebook), vbox,
      					gtk_label_new (_("Etcetera")),
					NOTEBOOK_PAGE_ETCETERA);
  gtk_widget_show (vbox);
}

static void
gui_address_book_card_properties_set_active (GtkWidget *object, C2AddressBookCardProperties *w) {
  g_return_if_fail (w);

  gnome_property_box_set_state (GNOME_PROPERTY_BOX (w->dialog), TRUE);
}

static void
gui_address_book_card_properties_notebook_switch_page (GtkWidget *object, GtkNotebookPage *page,
    							guint n, C2AddressBookCardProperties *w) {
  GtkLabel *label = GTK_LABEL (w->help);
  
  g_return_if_fail (w);
  
  if (n == NOTEBOOK_PAGE_PERSONAL_INFORMATION)
    gtk_label_set_text (label, _("Provide the personal information of this contact in this page."));
  else if (n == NOTEBOOK_PAGE_GROUPS)
    gtk_label_set_text (label, _("Contacts can be organizated by groups for a better Address Book\n"
	  			 "management. Select here the groups where you want this contact to\n"
				 "be found."));
}

static void
gui_address_book_card_properties_set_help (GtkWidget *object, GdkEventFocus *focus,
    						C2AddressBookCardProperties *w) {
  GtkLabel *label = GTK_LABEL (w->help);

  if (object == GTK_COMBO (w->personal_name_salutation)->entry)
    gtk_label_set_text (label, _(""));
  else if (object == GTK_COMBO (w->personal_name_suffix)->entry)
    gtk_label_set_text (label, _(""));
  else if (object == w->personal_name_first)
    gtk_label_set_text (label, _("First name of contact"));
  else if (object == w->personal_name_middle)
    gtk_label_set_text (label, _("Middle name of contact"));
  else if (object == w->personal_name_last)
    gtk_label_set_text (label, _("Last name of contact"));
  else if (object == w->personal_name_show)
    gtk_label_set_text (label, _("Type here the name you want to represent this contact in the list"));
  else if (object == GNOME_DATE_EDIT (w->personal_birthday)->date_entry)
    gtk_label_set_text (label, _("Birthday of the contact"));
  else if (object == w->personal_organization_name)
    gtk_label_set_text (label, _("Organization where this person works."));
  else if (object == w->personal_organization_title)
    gtk_label_set_text (label, _("Role of the person in the organization."));
  else if (object == w->group_list)
    gtk_label_set_text (label, _(""));
  else if (object == GTK_COMBO (w->group_entry)->entry)
    gtk_label_set_text (label, _("Type here the name of a new group or select an existent one from the pull down list."));
  else if (object == w->network_url)
    gtk_label_set_text (label, _("Homepage of this contact's web site"));
  else if (object == w->network_list)
    gtk_label_set_text (label, _(""));
  else if (object == w->network_entry)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_post_office)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_extended)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_street)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_city)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_province)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_country)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_private)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_work)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_postal_box)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_parcel)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_domestic)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_type_international)
    gtk_label_set_text (label, _(""));
  else if (object == w->address_list)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_entry)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_preferred)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_work)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_home)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_voice)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_fax)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_message_recorder)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_cellular)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_pager)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_bulletin_board)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_modem)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_car)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_isdn)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_type_video)
    gtk_label_set_text (label, _(""));
  else if (object == w->phone_list)
    gtk_label_set_text (label, _(""));
  else if (object == w->etcetera_categories)
    gtk_label_set_text (label, _(""));
  else if (object == w->etcetera_comments)
    gtk_label_set_text (label, _("Any other thing?"));
  else if (object == w->etcetera_security)
    gtk_label_set_text (label, _("Public PGP or GnuPG key."));
  else
    gtk_label_set_text (label, _(""));
}
#endif
