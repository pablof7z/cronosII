/* Cronos II
 * Copyright (C) 2000-2001 Pablo Fernández Navarro
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Address book backend functions for CronosII
 * Originally implemented by Bosko Blagojevic in 2001
 * Help on architecture from Pablo Fernández Navarro 
 */

#ifndef ADDRBOOK_H
#define ADDRBOOK_H

#ifdef __cplusplus
extern "C" {
#endif
  
#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef BUILD_ADDRESS_BOOK

typedef struct {
  enum {
    C2_VCARD_EMAIL_AOL = 1 << 1,
    C2_VCARD_EMAIL_APPLELINK = 1 << 2,
    C2_VCARD_EMAIL_ATTMAIL = 1 << 3,
    C2_VCARD_EMAIL_CIS = 1 << 4,
    C2_VCARD_EMAIL_EWORLD = 1 << 5,
    C2_VCARD_EMAIL_INTERNET = 1 << 6,
    C2_VCARD_EMAIL_IBMMail = 1 << 7,
    C2_VCARD_EMAIL_MCIMAIL = 1 << 8,
    C2_VCARD_EMAIL_POWERSHARE = 1 << 9,
    C2_VCARD_EMAIL_PRODIGY = 1 << 10,
    C2_VCARD_EMAIL_TLX = 1 << 11,
    C2_VCARD_EMAIL_X400 = 1 << 12
  } type;
  char *address;
} C2VCardEmail;

typedef struct {
  enum {
    C2_VCARD_ADDRESSES_HOME = 1 << 1,
    C2_VCARD_ADDRESSES_WORK = 1 << 2,
    C2_VCARD_ADDRESSES_POSTALBOX = 1 << 3,
    C2_VCARD_ADDRESSES_PARCEL = 1 << 4,
    C2_VCARD_ADDRESSES_DOMESTIC = 1 << 5,
    C2_VCARD_ADDRESSES_INTERNATIONAL = 1 << 6
  } type;
  char *post_office;
  char *extended;
  char *street;
  char *city;
  char *region;
  char *postal_code;
  char *country;
} C2VCardAddress;

typedef struct {
  enum {
    C2_VCARD_PHONE_PREFERRED = 1 << 1,
    C2_VCARD_PHONE_WORK = 1 << 2,
    C2_VCARD_PHONE_HOME = 1 << 3,
    C2_VCARD_PHONE_VOICE = 1 << 4,
    C2_VCARD_PHONE_FAX = 1 << 5,
    C2_VCARD_PHONE_MESSAGERECORDER = 1 << 6,
    C2_VCARD_PHONE_CELLULAR = 1 << 7,
    C2_VCARD_PHONE_PAGER = 1 << 8,
    C2_VCARD_PHONE_BULLETINBOARD = 1 << 9,
    C2_VCARD_PHONE_MODEM = 1 << 10,
    C2_VCARD_PHONE_CAR = 1 << 11,
    C2_VCARD_PHONE_ISDN = 1 << 12,
    C2_VCARD_PHONE_VIDEO = 1 << 13
  } type;
  char *number;
} C2VCardPhone;

typedef enum {
  C2_VCARD_SECURITY_PGP,
  C2_VCARD_SECURITY_X509
} C2VCardSecurityType;

typedef struct {
  char *name_show;
  char *name_salutation;
  char *name_first;
  char *name_middle;
  char *name_last;
  char *name_suffix;

  char *birthday;
  
  char *org_name;
  char *org_title;

  GList *groups;

  char *web;
  
  GList *email;
  GList *address;
  GList *phone;

  char *categories;
  char *comment;
  char *security;
  C2VCardSecurityType sectype;

  /* The @others member stores the information found in the vCard when loading it
   * that didn't match any of the supported information.
   * This member is JUST not to lose any user information and shouldn't be
   * used but when writing information to the cards file.
   * If you need any other field, like TZ, you might find it in @others but don't
   * use it, just send me a mail asking for a patch telling me the field you want
   * and some examples of how that field looks like, or if you want to get really
   * wet with the code, send me a patch yourself.
   */
  char *others;
} C2VCard;

#define c2_address_book_card_new()		(g_new0 (C2VCard, 1))
#define c2_address_book_card_email_new()	(g_new0 (C2VCardEmail, 1))
#define c2_address_book_card_address_new()	(g_new0 (C2VCardAddress, 1))
#define c2_address_book_card_phone_new()	(g_new0 (C2VCardPhone, 1))
#define c2_address_book_card_init(card)		card->name_show = NULL; \
						card->name_salutation = NULL; \
						card->name_first = NULL; \
						card->name_middle = NULL; \
						card->name_last = NULL; \
						card->name_suffix = NULL; \
						card->birthday = NULL; \
						card->org_name = NULL; \
						card->org_title = NULL; \
						card->groups = NULL; \
						card->web = NULL; \
						card->email = NULL; \
						card->address = NULL; \
						card->phone = NULL; \
						card->categories = NULL; \
						card->comment = NULL; \
						card->security = NULL; \
						card->others = NULL
#define C2VCARD(obj)				((C2VCard*)obj)
#define C2VCARDEMAIL(obj)			((C2VCardEmail*)obj)
#define C2VCARDADDRESS(obj)			((C2VCardAddress*)obj)
#define C2VCARDPHONE(obj)			((C2VCardPhone*)obj)

GList *addrbook;

void
c2_address_book_init							(void);

gint
c2_address_book_get_num_of_cards					(void);

const C2VCard *
c2_address_book_card_get						(guint offset);

int
c2_address_book_card_add						(const C2VCard *card);

void
c2_address_book_card_remove						(const C2VCard *card);

void
c2_address_book_flush							(void);

void
c2_address_book_card_free						(C2VCard *card);

int
c2_address_book_search							(GList *search_list,
					 	 	 	 	 const char *pattern,
    									 guint offset);

GList *
c2_address_book_search_all						(const char *pattern);

gboolean
c2_address_book_check_name_exists					(const gchar *name);

GList *
c2_address_book_get_available_groups					(void);

C2VCard *
c2_address_book_get_card_from_str					(const char *string);

gchar *
cut_leading_chars							(gchar *str, gint num);

gchar *
cut_leading_spaces							(gchar *str);

#endif

#ifdef __cplusplus
}
#endif

#endif
