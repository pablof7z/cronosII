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
#ifndef GUI_ADDRBOOK_H
#define GUI_ADDRBOOK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gnome.h>
#if HAVE_CONFIG_H
#  include <config.h>
#  include "addrbook.h"
#else
#  include <cronosII.h>
#endif

#ifdef BUILD_ADDRESS_BOOK

typedef struct {
  GtkWidget *window;

  GtkWidget *menu_file_edit;
  GtkWidget *menu_file_delete;
  GtkWidget *menu_view_normal;
  GtkWidget *menu_view_group;
  
  GtkWidget *search;
  GtkWidget *group;

  gboolean groups_are_loaded;
  
  GtkWidget *clist;
} C2AddressBook;

typedef struct {
  GtkWidget *dialog;
  GtkWidget *help;
  
  GtkWidget *personal_name_salutation;
  GtkWidget *personal_name_suffix;
  GtkWidget *personal_name_first;
  GtkWidget *personal_name_middle;
  GtkWidget *personal_name_last;
  GtkWidget *personal_name_show;
  GtkWidget *personal_birthday;
  GtkWidget *personal_organization_name;
  GtkWidget *personal_organization_title;

  gboolean i_may_edit_the_name_show_field;
  gboolean name_show_focus_in_other_widget;

  GtkWidget *group_list;
  GtkWidget *group_entry;

  gboolean groups_are_loaded;

  GtkWidget *network_url;
  GtkWidget *network_list;
  GtkWidget *network_entry;
  GtkWidget *network_type_aol;
  GtkWidget *network_type_applelink;
  GtkWidget *network_type_attmail;
  GtkWidget *network_type_cis;
  GtkWidget *network_type_eworld;
  GtkWidget *network_type_internet;
  GtkWidget *network_type_ibmmail;
  GtkWidget *network_type_mcimail;
  GtkWidget *network_type_powershare;
  GtkWidget *network_type_prodigy;
  GtkWidget *network_type_tlx;
  GtkWidget *network_type_x400;
  GtkWidget *network_add_btn;
  GtkWidget *network_modify_btn;
  GtkWidget *network_remove_btn;
  GtkWidget *network_up_btn;
  GtkWidget *network_down_btn;

  GtkWidget *address_post_office;
  GtkWidget *address_extended;
  GtkWidget *address_street;
  GtkWidget *address_city;
  GtkWidget *address_province;
  GtkWidget *address_postal_code;
  GtkWidget *address_country;
  GtkWidget *address_type_private;
  GtkWidget *address_type_work;
  GtkWidget *address_type_postal_box;
  GtkWidget *address_type_parcel;
  GtkWidget *address_type_domestic;
  GtkWidget *address_type_international;
  GtkWidget *address_list;
  GtkWidget *address_add_btn;
  GtkWidget *address_modify_btn;
  GtkWidget *address_remove_btn;
  GtkWidget *address_up_btn;
  GtkWidget *address_down_btn;
  

  GtkWidget *phone_entry;
  GtkWidget *phone_type_preferred;
  GtkWidget *phone_type_work;
  GtkWidget *phone_type_home;
  GtkWidget *phone_type_voice;
  GtkWidget *phone_type_fax;
  GtkWidget *phone_type_message_recorder;
  GtkWidget *phone_type_cellular;
  GtkWidget *phone_type_pager;
  GtkWidget *phone_type_bulletin_board;
  GtkWidget *phone_type_modem;
  GtkWidget *phone_type_car;
  GtkWidget *phone_type_isdn;
  GtkWidget *phone_type_video;
  GtkWidget *phone_list;
  GtkWidget *phone_add_btn;
  GtkWidget *phone_modify_btn;
  GtkWidget *phone_remove_btn;
  GtkWidget *phone_up_btn;
  GtkWidget *phone_down_btn;

  GtkWidget *etcetera_categories;
  GtkWidget *etcetera_comments;
  GtkWidget *etcetera_security;
  GtkWidget *etcetera_security_type_pgp;
  GtkWidget *etcetera_security_type_x509;

  C2VCard *card;
  int nth;
} C2AddressBookCardProperties;

typedef enum {
  C2_ADDRESS_BOOK_NORMAL, /* Default View, all users */
  C2_ADDRESS_BOOK_GROUPS, /* All groups */
  C2_ADDRESS_BOOK_SEARCH, /* Search */
  C2_ADDRESS_BOOK_GROUP   /* Specific group */
} C2AddressBookFillType;

C2AddressBook *gaddrbook;

C2AddressBook *
c2_address_book_new						(const char *package);

void
c2_address_book_fill						(C2AddressBook *addrbook,
								 C2AddressBookFillType type,
								 const char *flags);

void
gui_address_book_card_properties				(C2VCard *card, int nth);

#endif

#ifdef __cplusplus
}
#endif

#endif
