/* Cronos II /src/addrbook.c
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
 * Major changes by Pablo Fernández Navarro
 */

/* TODO:
 * Implement function to check if vCard is valid
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fnmatch.h>

#include "main.h"
#include "utils.h"
#include "init.h"
#include "addrbook.h"
#include "error.h"
#include "debug.h"
#include "message.h"

#include "gui-addrbook.h"

#ifdef BUILD_ADDRESS_BOOK

static C2VCard *c2_address_book_get_card_from_fd (FILE * fd);

static char *c2_address_book_get_line (FILE * fd);

static char *c2_address_book_get_line_field_n (const char *line,
					       char separator, guint n);

static char *get_key (const char *string, char separator);

static char learn_separator (const char *string, int *offset);

/**
 * c2_adress_book_get_card
 * @offset: offset of which card to get as stored in file 
 *          (i.e. offset=0 would get the first card)
 *
 * Gets a card from the list.
 *
 * Return Value:
 * A non-freeable C2VCard object containing the card or NULL
 * if there's no such a card.
 **/
const C2VCard *
c2_address_book_card_get (guint offset)
{
	GList *node;
	C2VCard *card = NULL;

	if (!addrbook)
		c2_address_book_init ();

	node = g_list_nth (addrbook, offset);
	if (node)
		card = C2VCARD (node->data);

	return card;
}

static int
sort_cards_by_name_show (const C2VCard * a, const C2VCard * b)
{
	return g_strcasecmp (a->name_show, b->name_show);
}

/**
 * c2_address_book_card_add
 * @card: A pointer to a C2VCard object.
 *
 * Adds a card into the Address Book GList.
 *
 * Return Value:
 * The position where the card has been added.
 **/
int
c2_address_book_card_add (const C2VCard * card)
{
	g_return_val_if_fail (card, -1);

	if (!addrbook)
		c2_address_book_init ();

	addrbook =
		g_list_insert_sorted (addrbook, (gpointer) card,
				      (GCompareFunc) sort_cards_by_name_show);
	return g_list_position (addrbook,
				g_list_find (addrbook, (gpointer) card));
}

/**
 * c2_address_book_card_remove
 * @card: Card to be removed.
 *
 * Removes a card from the Address Book GList.
 * Note: Most of the time, after using this function
 * you want to use c2_address_book_card_free (card).
 **/
void
c2_address_book_card_remove (const C2VCard * card)
{
	g_return_if_fail (card);

	addrbook = g_list_remove (addrbook, (gpointer) card);
}

/**
 * c2_address_book_card_free
 * @card: A pointer to a C2VCard object.
 *
 * Frees the card @card.
 **/
void
c2_address_book_card_free (C2VCard * card)
{
	GList *li;

	g_return_if_fail (card);

	c2_free (card->name_show);
	c2_free (card->name_salutation);
	c2_free (card->name_first);
	c2_free (card->name_middle);
	c2_free (card->name_last);
	c2_free (card->name_suffix);
	c2_free (card->birthday);
	c2_free (card->org_name);
	c2_free (card->org_title);
	c2_free (card->web);
	c2_free (card->categories);
	c2_free (card->comment);
	c2_free (card->security);
	c2_free (card->others);

	for (li = card->groups; li != NULL; li = li->next)
	{
		c2_free (li->data);
	}
	g_list_free (card->groups);

	for (li = card->email; li != NULL; li = li->next)
	{
		c2_free (C2VCARDEMAIL (li->data)->address);
		c2_free (li->data);
	}
	g_list_free (card->email);

	for (li = card->address; li != NULL; li = li->next)
	{
		C2VCardAddress *addr = C2VCARDADDRESS (li->data);
		c2_free (addr->post_office);
		c2_free (addr->extended);
		c2_free (addr->street);
		c2_free (addr->city);
		c2_free (addr->region);
		c2_free (addr->postal_code);
		c2_free (addr->country);
		c2_free (addr);
	}
	g_list_free (card->address);

	for (li = card->phone; li != NULL; li = li->next)
	{
		c2_free (C2VCARDPHONE (li->data)->number);
		c2_free (li->data);
	}
	g_list_free (card->phone);

	c2_free (card);
}

/**
 * c2_address_book_flush
 *
 * Flushes the Address Book GList into
 * the file ~/.gnome/GnomeCard.gcrd
 **/
void
c2_address_book_flush (void)
{
	GList *list, *l;
	FILE *fd;
	C2VCard *card;
	GString *buf;
	char *path;

	path = g_strconcat (getenv ("HOME"), "/.gnome/GnomeCard.gcrd", NULL);
	if ((fd = fopen (path, "w")) == NULL)
	{
		cronos_error (errno, _("Could not open the GnomeCard file\n"),
			      ERROR_WARNING);
		c2_free (path);
	}
	c2_free (path);

	for (list = addrbook; list != NULL; list = list->next)
	{
		card = C2VCARD (list->data);

		fprintf (fd, "BEGIN:VCARD\r\n");

		fprintf (fd, "FN:%s\r\n",
			 card->name_show ? card->name_show : "");
		fprintf (fd, "N:%s;%s;%s;%s;%s\r\n",
			 card->name_last ? card->name_last : "",
			 card->name_first ? card->name_first : "",
			 card->name_middle ? card->name_middle : "",
			 card->name_salutation ? card->name_salutation : "",
			 card->name_suffix ? card->name_suffix : "");

		/* Generate the address */
		for (l = card->address; l != NULL; l = l->next)
		{
			C2VCardAddress *addr = C2VCARDADDRESS (l->data);
			buf = g_string_new (NULL);
			if (addr->type & C2_VCARD_ADDRESSES_HOME)
			{
				g_string_append (buf, ";HOME");
			}
			if (addr->type & C2_VCARD_ADDRESSES_WORK)
			{
				g_string_append (buf, ";WORK");
			}
			if (addr->type & C2_VCARD_ADDRESSES_POSTALBOX)
			{
				g_string_append (buf, ";POSTAL");
			}
			if (addr->type & C2_VCARD_ADDRESSES_PARCEL)
			{
				g_string_append (buf, ";PARCEL");
			}
			if (addr->type & C2_VCARD_ADDRESSES_DOMESTIC)
			{
				g_string_append (buf, ";DOM");
			}
			if (addr->type & C2_VCARD_ADDRESSES_INTERNATIONAL)
			{
				g_string_append (buf, ";INTL");
			}
			g_string_append (buf, ":");
			if (addr->post_office)
				g_string_append (buf, addr->post_office);
			g_string_append (buf, ";");
			if (addr->extended)
				g_string_append (buf, addr->extended);
			g_string_append (buf, ";");
			if (addr->street)
				g_string_append (buf, addr->street);
			g_string_append (buf, ";");
			if (addr->city)
				g_string_append (buf, addr->city);
			g_string_append (buf, ";");
			if (addr->region)
				g_string_append (buf, addr->region);
			g_string_append (buf, ";");
			if (addr->postal_code)
				g_string_append (buf, addr->postal_code);
			g_string_append (buf, ";");
			if (addr->country)
				g_string_append (buf, addr->country);
			fprintf (fd, "ADR%s\r\n", buf->str);
			g_string_free (buf, TRUE);
		}

		/* Generate the telephone */
		for (l = card->phone; l != NULL; l = l->next)
		{
			C2VCardPhone *phone = C2VCARDPHONE (l->data);
			buf = g_string_new (NULL);
			if (phone->type & C2_VCARD_PHONE_PREFERRED)
			{
				g_string_append (buf, ";PREF");
			}
			if (phone->type & C2_VCARD_PHONE_WORK)
			{
				g_string_append (buf, ";WORK");
			}
			if (phone->type & C2_VCARD_PHONE_HOME)
			{
				g_string_append (buf, ";HOME");
			}
			if (phone->type & C2_VCARD_PHONE_VOICE)
			{
				g_string_append (buf, ";VOICE");
			}
			if (phone->type & C2_VCARD_PHONE_FAX)
			{
				g_string_append (buf, ";FAX");
			}
			if (phone->type & C2_VCARD_PHONE_MESSAGERECORDER)
			{
				g_string_append (buf, ";MSG");
			}
			if (phone->type & C2_VCARD_PHONE_CELLULAR)
			{
				g_string_append (buf, ";CELL");
			}
			if (phone->type & C2_VCARD_PHONE_PAGER)
			{
				g_string_append (buf, ";PAGER");
			}
			if (phone->type & C2_VCARD_PHONE_BULLETINBOARD)
			{
				g_string_append (buf, ";BBS");
			}
			if (phone->type & C2_VCARD_PHONE_MODEM)
			{
				g_string_append (buf, ";MODEM");
			}
			if (phone->type & C2_VCARD_PHONE_CAR)
			{
				g_string_append (buf, ";CAR");
			}
			if (phone->type & C2_VCARD_PHONE_ISDN)
			{
				g_string_append (buf, ";ISDN");
			}
			if (phone->type & C2_VCARD_PHONE_VIDEO)
			{
				g_string_append (buf, ";VIDEO");
			}
			g_string_append (buf, ":");
			if (phone->number)
				g_string_append (buf, phone->number);
			fprintf (fd, "TEL%s\r\n", buf->str);
			g_string_free (buf, TRUE);
		}

		/* Generate the email */
		for (l = card->email; l != NULL; l = l->next)
		{
			C2VCardEmail *email = C2VCARDEMAIL (l->data);
			buf = g_string_new (NULL);
			if (email->type & C2_VCARD_EMAIL_AOL)
			{
				g_string_append (buf, ";AOL");
			}
			if (email->type & C2_VCARD_EMAIL_APPLELINK)
			{
				g_string_append (buf, ";APPLELINK");
			}
			if (email->type & C2_VCARD_EMAIL_ATTMAIL)
			{
				g_string_append (buf, ";ATTMAIL");
			}
			if (email->type & C2_VCARD_EMAIL_CIS)
			{
				g_string_append (buf, ";CIS");
			}
			if (email->type & C2_VCARD_EMAIL_EWORLD)
			{
				g_string_append (buf, ";EWORLD");
			}
			if (email->type & C2_VCARD_EMAIL_INTERNET)
			{
				g_string_append (buf, ";INTERNET");
			}
			if (email->type & C2_VCARD_EMAIL_IBMMail)
			{
				g_string_append (buf, ";IBMMail");
			}
			if (email->type & C2_VCARD_EMAIL_MCIMAIL)
			{
				g_string_append (buf, ";MCIMAIL");
			}
			if (email->type & C2_VCARD_EMAIL_POWERSHARE)
			{
				g_string_append (buf, ";POWERSHARE");
			}
			if (email->type & C2_VCARD_EMAIL_PRODIGY)
			{
				g_string_append (buf, ";PRODIGY");
			}
			if (email->type & C2_VCARD_EMAIL_TLX)
			{
				g_string_append (buf, ";TLX");
			}
			if (email->type & C2_VCARD_EMAIL_X400)
			{
				g_string_append (buf, ";X400");
			}
			g_string_append (buf, ":");
			if (email->address)
				g_string_append (buf, email->address);
			fprintf (fd, "EMAIL%s\r\n", buf->str);
			g_string_free (buf, TRUE);
		}

		/* Generate Groups */
		if (card->groups)
		{
			fprintf (fd, "GRP:");
			for (l = card->groups; l != NULL; l = l->next)
			{
				if (l == card->groups)
					fprintf (fd, "%s", CHAR (l->data));
				else
					fprintf (fd, ";%s", CHAR (l->data));
			}
			fprintf (fd, "\r\n");
		}

		if (card->org_title)
			fprintf (fd, "TITLE:%s\r\n", card->org_title);
		if (card->org_name)
			fprintf (fd, "ORG:%s\r\n", card->org_name);
		if (card->web)
			fprintf (fd, "URL:%s\r\n", card->web);
		if (card->birthday)
			fprintf (fd, "BDAY:%s\r\n", card->birthday);
		if (card->categories)
		{
			unsigned char *ptr;

			buf = g_string_new (NULL);
			for (ptr = card->categories; *ptr != '\0'; ptr++)
			{
				if (*ptr < 128 && *ptr != '\n')
					g_string_append_c (buf, *ptr);
				else
				{
					char *lbuf =
						g_strdup_printf ("=%2X",
								 *ptr);
					g_string_append (buf, lbuf);
					c2_free (lbuf);
				}
			}
			fprintf (fd, "CATEGORIES;QUOTED-PRINTABLE:%s\r\n",
				 buf->str);
			g_string_free (buf, TRUE);
		}
		if (card->comment)
		{
			unsigned char *ptr;

			buf = g_string_new (NULL);
			for (ptr = card->comment; *ptr != '\0'; ptr++)
			{
				if (*ptr < 128 && *ptr != '\n')
					g_string_append_c (buf, *ptr);
				else
				{
					char *lbuf =
						g_strdup_printf ("=%02X",
								 *ptr);
					g_string_append (buf, lbuf);
					c2_free (lbuf);
				}
			}
			fprintf (fd, "NOTE;QUOTED-PRINTABLE:%s\r\n",
				 buf->str);
			g_string_free (buf, TRUE);
		}
		if (card->security)
		{
			if (card->sectype == C2_VCARD_SECURITY_PGP)
				fprintf (fd, "KEY;PGP:%s\r\n",
					 card->security);
			else
				fprintf (fd, "KEY;X509:%s\r\n",
					 card->security);
		}
		if (card->others)
		{
			char *line;
			char *ptr;
			size_t size;

			for (ptr = card->others; *ptr != '\0';)
			{
				if (!(line = str_get_line (ptr)))
					break;
				size = strlen (line);
				ptr += size;
				line[size - 1] = '\r';
				fprintf (fd, "%s\n", line);
				c2_free (line);
			}
		}

		fprintf (fd, "END:VCARD\r\n\r\n");
	}

	fclose (fd);
}

/**
 * c2_address_book_search
 * @list: The list of vCards that the search is going to take place.
 * @pattern: The pattern to search for.
 * @offset: The offset where to start looking.
 *
 * Return Value:
 * The card number where the pattern was found or -1 if it wasn't found.
 **/
int
c2_address_book_search (GList * search_list, const char *pattern,
			guint offset)
{
	int i;
	char *mpattern = NULL;
	GList *list, *li;
	C2VCard *card;

	g_return_val_if_fail (pattern, -1);

	if (!search_list)
		c2_address_book_init ();

	mpattern = g_new0 (char, strlen (pattern) + 3);
	sprintf (mpattern, "*%s*", pattern);
	g_strup (mpattern);

	for (list = g_list_nth (search_list, offset), i = offset;
	     list != NULL; list = list->next, i++)
	{
		card = list->data;
		if (!card)
			continue;

		/* Names */
		if ((card->name_show) &&
		    (!fnmatch (mpattern, card->name_show, 1 << 4)))
			goto end_search;
		if ((card->name_salutation) &&
		    (!fnmatch (mpattern, card->name_salutation, 1 << 4)))
			goto end_search;
		if ((card->name_first) &&
		    (!fnmatch (mpattern, card->name_first, 1 << 4)))
			goto end_search;
		if ((card->name_middle) &&
		    (!fnmatch (mpattern, card->name_middle, 1 << 4)))
			goto end_search;
		if ((card->name_last) &&
		    (!fnmatch (mpattern, card->name_last, 1 << 4)))
			goto end_search;
		if ((card->name_suffix) &&
		    (!fnmatch (mpattern, card->name_suffix, 1 << 4)))
			goto end_search;

		/* Birthday */
		if ((card->birthday) &&
		    (!fnmatch (mpattern, card->birthday, 1 << 4)))
			goto end_search;

		/* Organisation */
		if ((card->org_name) &&
		    (!fnmatch (mpattern, card->org_name, 1 << 4)))
			goto end_search;
		if ((card->org_title) &&
		    (!fnmatch (mpattern, card->org_title, 1 << 4)))
			goto end_search;

		/* Groups */
		for (li = card->groups; li != NULL; li = li->next)
			if (!fnmatch (mpattern, li->data, 1 << 4))
				goto end_search;

		/* Web */
		if ((card->web) && (!fnmatch (mpattern, card->web, 1 << 4)))
			goto end_search;

		/* Email */
		for (li = card->email; li != NULL; li = li->next)
			if ((C2VCARDEMAIL (li->data)->address) &&
			    (!fnmatch
			     (mpattern, C2VCARDEMAIL (li->data)->address,
			      1 << 4))) goto end_search;

		/* Address */
		for (li = card->address; li != NULL; li = li->next)
		{
			if ((C2VCARDADDRESS (li->data)->post_office) &&
			    (!fnmatch (mpattern,
				       C2VCARDADDRESS (li->data)->post_office,
				       1 << 4)))
				goto end_search;
			if ((C2VCARDADDRESS (li->data)->extended) &&
			    (!fnmatch
			     (mpattern, C2VCARDADDRESS (li->data)->extended,
			      1 << 4)))
				goto end_search;
			if ((C2VCARDADDRESS (li->data)->street) &&
			    (!fnmatch
			     (mpattern, C2VCARDADDRESS (li->data)->street,
			      1 << 4)))
				goto end_search;
			if ((C2VCARDADDRESS (li->data)->city) &&
			    (!fnmatch
			     (mpattern, C2VCARDADDRESS (li->data)->city,
			      1 << 4))) goto end_search;
			if ((C2VCARDADDRESS (li->data)->region)
			    &&
			    (!fnmatch
			     (mpattern, C2VCARDADDRESS (li->data)->region,
			      1 << 4)))
				goto end_search;
			if ((C2VCARDADDRESS (li->data)->postal_code) &&
			    (!fnmatch (mpattern,
				       C2VCARDADDRESS (li->data)->postal_code,
				       1 << 4)))
				goto end_search;
			if ((C2VCARDADDRESS (li->data)->country) &&
			    (!fnmatch
			     (mpattern, C2VCARDADDRESS (li->data)->country,
			      1 << 4)))
				goto end_search;
		}

		/* Phone */
		for (li = card->phone; li != NULL; li = li->next)
			if ((C2VCARDPHONE (li->data)->number) &&
			    (!fnmatch
			     (mpattern, C2VCARDPHONE (li->data)->number,
			      1 << 4))) goto end_search;

		/* Categories */
		if ((card->categories) &&
		    (!fnmatch (mpattern, card->categories, 1 << 4)))
			goto end_search;

		/* Comment */
		if ((card->comment) &&
		    (!fnmatch (mpattern, card->comment, 1 << 4)))
			goto end_search;

		/* Security */
		if ((card->security) &&
		    (!fnmatch (mpattern, card->security, 1 << 4)))
			goto end_search;

		/* Should @others be scanned? */
	}
      end_search:

	c2_free (mpattern);
	if (list)
		return i;
	return -1;
}

/**
 * c2_address_book_search_all
 * @pattern: The pattern to search for.
 *
 * c2_address_book_search searchs for the first matching
 * card, this function will return EVERY matching card.
 *
 * Return Value:
 * All card that matched in a GList.
 **/
GList *
c2_address_book_search_all (const char *pattern)
{
	GList *list, *li, *ret = NULL;
	C2VCard *card;
	char *mpattern;

	if (!addrbook)
		c2_address_book_init ();

	mpattern = g_strdup_printf ("%s*", pattern);

	for (list = addrbook; list != NULL; list = list->next)
	{
		card = list->data;
		if (!card)
			continue;

		/* Names */
		if (!fnmatch (mpattern, card->name_show, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->name_salutation, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->name_first, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->name_middle, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->name_last, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->name_suffix, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Birthday */
		if (!fnmatch (mpattern, card->birthday, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Organisation */
		if (!fnmatch (mpattern, card->org_name, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}
		if (!fnmatch (mpattern, card->org_title, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Groups */
		for (li = card->groups; li != NULL; li = li->next)
			if (!fnmatch (mpattern, li->data, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}

		/* Web */
		if (!fnmatch (mpattern, card->web, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Email */
		for (li = card->email; li != NULL; li = li->next)
			if (!fnmatch
			    (mpattern, C2VCARDEMAIL (li->data)->address, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}

		/* Address */
		for (li = card->address; li != NULL; li = li->next)
		{
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->post_office,
			     0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->extended,
			     0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->street, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->city, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->region, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->postal_code,
			     0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
			if (!fnmatch
			    (mpattern, C2VCARDADDRESS (li->data)->country, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}
		}

		/* Phone */
		for (li = card->phone; li != NULL; li = li->next)
			if (!fnmatch
			    (mpattern, C2VCARDPHONE (li->data)->number, 0))
			{
				ret = g_list_append (ret, card);
				goto move_on;
			}

		/* Categories */
		if (!fnmatch (mpattern, card->categories, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Comment */
		if (!fnmatch (mpattern, card->comment, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Security */
		if (!fnmatch (mpattern, card->security, 0))
		{
			ret = g_list_append (ret, card);
			goto move_on;
		}

		/* Should @others be scanned? */
	      move_on:
	}

	c2_free (mpattern);

	return ret;
}

/**
 * c2_address_book_get_num_of_cards
 * 
 * Return the number of loaded cards.
 * 
 * Return Value:
 * gint with number of vCards
 **/
int
c2_address_book_get_num_of_cards (void)
{
	if (!addrbook)
		c2_address_book_init ();

	return g_list_length (addrbook);
}

/**
 * c2_adress_book_check_name_exists
 * @name = name to check in fields fn
 *  
 * Checks to see if a vCard with the name name already exists.
 * 
 * Return Value:
 * TRUE if exists, FALSE otherwise
 **/
gboolean c2_address_book_check_name_exists (const char *name)
{
	GList *list;
	C2VCard *card;

	g_return_val_if_fail (name, TRUE);

	if (!addrbook)
		c2_address_book_init ();

	for (list = addrbook; list != NULL; list = list->next)
	{
		card = C2VCARD (list->data);
		if (streq (card->name_show, name))
			return TRUE;
	}

	return FALSE;
}

/**
 * c2_address_book_get_available_groups
 *
 * Reads from all the cards the available
 * groups.
 *
 * Return Value:
 * A GList containing the available groups names.
 **/
GList *
c2_address_book_get_available_groups (void)
{
	C2VCard *card;
	GList *list = NULL;
	GList *s, *li, *le;

	for (li = addrbook; li != NULL; li = li->next)
	{
		card = C2VCARD (li->data);
		for (le = card->groups; le != NULL; le = le->next)
		{
			/* Check if this groups is already written */
			for (s = list; s != NULL; s = s->next)
			{
				if (streq (CHAR (s->data), CHAR (le->data)))
					goto skip;
			}

			list = g_list_append (list, le->data);
		      skip:
		}
	}

	return list;
}

static char *
c2_address_book_get_line_field_n (const char *line, char separator, guint n)
{
	const char *start, *end;
	int i;

	g_return_val_if_fail (line, NULL);
	g_return_val_if_fail (!(separator != ';' && separator != ':'), NULL);

	/* Look for the beggining */
	for (start = line, i = 0; *start != '\0' && i < n; start++)
		if (*start == separator)
			i++;

	if (i != n)
		return NULL;

	/* Look for the end */
	for (end = start; *end != '\0'; end++)
		if (*end == separator)
			break;

	if (*end != '\0')
	{
		return g_strndup (start, end - start);
	}
	else
	{
		char *ret = g_strdup (start);
		int size = strlen (ret);

		if (ret[size - 1] == '\n')
			ret[size - 1] = '\0';
		return ret;
	}
}

/**
 * c2_address_book_init
 *
 * Initializates the Address Book (loads the address book into
 * a GList).
 * Note: This function should be called before any other of this API.
 **/
void
c2_address_book_init (void)
{
	C2VCard *card;
	char *path;
	FILE *fd;

	if (addrbook)
		return;

	path = g_strconcat (getenv ("HOME"), "/.gnome/GnomeCard.gcrd", NULL);
	if ((fd = fopen (path, "r")) == NULL)
	{
		if (errno != ENOENT)
		{
			gdk_threads_enter ();
			cronos_error (errno,
				      _("Couldn't open the GnomeCard file"),
				      ERROR_WARNING);
			gdk_threads_leave ();
		}
		c2_free (path);
		return;
	}

	/* Loop until I load all cards */
	for (;;)
	{
		/* Get the card */
		if ((card = c2_address_book_get_card_from_fd (fd)) == NULL)
			break;

		/* Append the card */
		addrbook = g_list_append (addrbook, card);
	}
}

/**
 * c2_address_book_get_card_from_str
 * @string: A pointer to a string containing a vCard.
 *
 * Will load the string @string into a vCard object.
 *
 * Return Value:
 * A C2VCard object containing @string.
 **/
C2VCard *
c2_address_book_get_card_from_str (const char *string)
{
	C2VCard *card;
	const char *ptr;
	char *line;
	char separator;
	int offset;
	GString *other = NULL;

	g_return_val_if_fail (string, NULL);

	/* TODO Check if the card is valid */

	card = c2_address_book_card_new ();
	c2_address_book_card_init (card);

	/* Find the start of the card */
	for (ptr = string;;)
	{
		if ((line = str_get_line (ptr)) == NULL)
			return NULL;
		if (strneq (line, "BEGIN:VCARD", 11))
			break;
		c2_free (line);
	}
	c2_free (line);

	/* Start getting all the fields */
	for (;;)
	{
		if ((line = str_get_line (ptr)) == NULL)
			break;
		ptr += strlen (line);

		/* Learn the separator */
		separator = learn_separator (line, &offset);

		if ((strneq (line, "FN", 2)) &&
		    (line[2] == separator == ';' ? ';' : ':'))
		{
			card->name_show =
				g_strndup (line + offset + 1,
					   strlen (line) - 4);
		}
		else if ((strneq (line, "TITLE", 5)) &&
			 (line[5] == separator == ';' ? ';' : ':'))
		{
			card->org_title =
				g_strndup (line + offset + 1,
					   strlen (line) - 7);
		}
		else if ((strneq (line, "ORG", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->org_name =
				g_strndup (line + offset + 1,
					   strlen (line) - 5);
		}
		else if ((strneq (line, "CATEGORIES", 10)) &&
			 (line[10] == separator == ';' ? ';' : ':'))
		{
			card->categories =
				g_strndup (line + offset + 1,
					   strlen (line) - 12);
		}
		else if ((strneq (line, "NOTE", 4)) &&
			 (line[4] == separator == ';' ? ';' : ':'))
		{
			card->comment =
				g_strndup (line + offset + 1,
					   strlen (line) - 6);
		}
		else if ((strneq (line, "URL", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->web =
				g_strndup (line + offset + 1,
					   strlen (line) - 5);
		}
		else if ((strneq (line, "BDAY", 4)) &&
			 (line[4] == separator == ';' ? ';' : ':'))
		{
			card->birthday =
				g_strndup (line + offset + 1,
					   strlen (line) - 6);
		}
		else if ((strneq (line, "KEY", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->security =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);
		}
		else if ((strneq (line, "N", 1)) &&
			 (line[1] == separator == ';' ? ';' : ':'))
		{
			card->name_last =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			card->name_first =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);
			card->name_middle =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  2);
			card->name_salutation =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  3);
			card->name_suffix =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  4);
		}
		else if ((strneq (line, "GRP", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			int i;
			char *buf;

			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (line +
								       offset
								       + 1,
								       separator,
								       i)) ==
				    NULL) break;

				card->groups =
					g_list_append (card->groups, buf);
			}
		}
		else if ((strneq (line, "ADR", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			C2VCardAddress *addr =
				c2_address_book_card_address_new ();
			char *types, *spec, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			spec =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "DOM"))
					addr->type ^= 1 << 5;
				else if (streq (buf, "INTL"))
					addr->type ^= 1 << 6;
				else if (streq (buf, "POSTAL"))
					addr->type ^= 1 << 3;
				else if (streq (buf, "PARCEL"))
					addr->type ^= 1 << 4;
				else if (streq (buf, "HOME"))
					addr->type ^= 1 << 1;
				else if (streq (buf, "WORK"))
					addr->type ^= 1 << 2;
				c2_free (buf);
			}
			c2_free (types);

			/* Learn the address */
			addr->post_office =
				c2_address_book_get_line_field_n (spec, mysep,
								  0);
			addr->extended =
				c2_address_book_get_line_field_n (spec, mysep,
								  1);
			addr->street =
				c2_address_book_get_line_field_n (spec, mysep,
								  2);
			addr->city =
				c2_address_book_get_line_field_n (spec, mysep,
								  3);
			addr->region =
				c2_address_book_get_line_field_n (spec, mysep,
								  4);
			addr->postal_code =
				c2_address_book_get_line_field_n (spec, mysep,
								  5);
			addr->country =
				c2_address_book_get_line_field_n (spec, mysep,
								  6);

			c2_free (spec);

			card->address = g_list_append (card->address, addr);
		}
		else if ((strneq (line, "TEL", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			C2VCardPhone *ph = c2_address_book_card_phone_new ();
			char *types, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			ph->number =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "PREF"))
					ph->type ^= 1 << 1;
				else if (streq (buf, "WORK"))
					ph->type ^= 1 << 2;
				else if (streq (buf, "HOME"))
					ph->type ^= 1 << 3;
				else if (streq (buf, "VOICE"))
					ph->type ^= 1 << 4;
				else if (streq (buf, "FAX"))
					ph->type ^= 1 << 5;
				else if (streq (buf, "MSG"))
					ph->type ^= 1 << 6;
				else if (streq (buf, "CELL"))
					ph->type ^= 1 << 7;
				else if (streq (buf, "PAGER"))
					ph->type ^= 1 << 8;
				else if (streq (buf, "BBS"))
					ph->type ^= 1 << 9;
				else if (streq (buf, "MODEM"))
					ph->type ^= 1 << 10;
				else if (streq (buf, "CAR"))
					ph->type ^= 1 << 11;
				else if (streq (buf, "ISDN"))
					ph->type ^= 1 << 12;
				else if (streq (buf, "VIDEO"))
					ph->type ^= 1 << 13;
				c2_free (buf);
			}
			c2_free (types);

			card->phone = g_list_append (card->phone, ph);
		}
		else if ((strneq (line, "EMAIL", 5)) &&
			 (line[5] == separator == ';' ? ';' : ':'))
		{
			C2VCardEmail *em = c2_address_book_card_email_new ();
			char *types, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			em->address =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "AOL"))
					em->type ^= 1 << 1;
				else if (streq (buf, "APPLELINK"))
					em->type ^= 1 << 2;
				else if (streq (buf, "ATTMAIL"))
					em->type ^= 1 << 3;
				else if (streq (buf, "CIS"))
					em->type ^= 1 << 4;
				else if (streq (buf, "EWORLD"))
					em->type ^= 1 << 5;
				else if (streq (buf, "INTERNET"))
					em->type ^= 1 << 6;
				else if (streq (buf, "IBMMail"))
					em->type ^= 1 << 7;
				else if (streq (buf, "MCIMAIL"))
					em->type ^= 1 << 8;
				else if (streq (buf, "POWERSHARE"))
					em->type ^= 1 << 9;
				else if (streq (buf, "PRODIGY"))
					em->type ^= 1 << 10;
				else if (streq (buf, "TLX"))
					em->type ^= 1 << 11;
				else if (streq (buf, "X400"))
					em->type ^= 1 << 12;
				c2_free (buf);
				break;
			}
			c2_free (types);

			card->email = g_list_append (card->email, em);
		}
		else if (strneq (line, "END:VCARD", 9))
		{
			c2_free (line);
			break;
		}
		else
		{
			if (!other)
				other = g_string_new (NULL);
			g_string_append (other, line);
		}
		c2_free (line);
	}

	if (other)
	{
		card->others = other->str;
		g_string_free (other, FALSE);
	}

	return card;
}

static C2VCard *
c2_address_book_get_card_from_fd (FILE * fd)
{
	C2VCard *card = c2_address_book_card_new ();
	char *line = NULL;
	char separator;
	int offset;
	GString *other = NULL;

	c2_address_book_card_init (card);

	/* Find the start of the card */
	for (;;)
	{
		if ((line = c2_address_book_get_line (fd)) == NULL)
			return NULL;
		if (strneq (line, "BEGIN:VCARD", 11))
			break;
		c2_free (line);
	}
	c2_free (line);


	/* Start getting all the fields */
	for (;;)
	{
		if ((line = c2_address_book_get_line (fd)) == NULL)
			break;

		/* Learn the separator */
		separator = learn_separator (line, &offset);

		if ((strneq (line, "FN", 2)) &&
		    (line[2] == separator == ';' ? ';' : ':'))
		{
			card->name_show =
				g_strndup (line + offset + 1,
					   strlen (line) - 4);
		}
		else if ((strneq (line, "TITLE", 5)) &&
			 (line[5] == separator == ';' ? ';' : ':'))
		{
			card->org_title =
				g_strndup (line + offset + 1,
					   strlen (line) - 7);
		}
		else if ((strneq (line, "ORG", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->org_name =
				g_strndup (line + offset + 1,
					   strlen (line) - 5);
		}
		else if ((strneq (line, "CATEGORIES", 10)) &&
			 (line[10] == separator == ';' ? ';' : ':'))
		{
			card->categories =
				g_strndup (line + offset + 1,
					   strlen (line) - 12);
		}
		else if ((strneq (line, "NOTE", 4)) &&
			 (line[4] == separator == ';' ? ';' : ':'))
		{
			card->comment =
				g_strndup (line + offset + 1,
					   strlen (line) - 6);
		}
		else if ((strneq (line, "URL", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->web =
				g_strndup (line + offset + 1,
					   strlen (line) - 5);
		}
		else if ((strneq (line, "BDAY", 4)) &&
			 (line[4] == separator == ';' ? ';' : ':'))
		{
			card->birthday =
				g_strndup (line + offset + 1,
					   strlen (line) - 6);
		}
		else if ((strneq (line, "KEY", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			card->security =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);
		}
		else if ((strneq (line, "N", 1)) &&
			 (line[1] == separator == ';' ? ';' : ':'))
		{
			card->name_last =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			card->name_first =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);
			card->name_middle =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  2);
			card->name_salutation =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  3);
			card->name_suffix =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  4);
		}
		else if ((strneq (line, "GRP", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			int i;
			char *buf;

			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (line +
								       offset
								       + 1,
								       separator,
								       i)) ==
				    NULL) break;

				card->groups =
					g_list_append (card->groups, buf);
			}
		}
		else if ((strneq (line, "ADR", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			C2VCardAddress *addr =
				c2_address_book_card_address_new ();
			char *types, *spec, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			spec =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "DOM"))
					addr->type ^= 1 << 5;
				else if (streq (buf, "INTL"))
					addr->type ^= 1 << 6;
				else if (streq (buf, "POSTAL"))
					addr->type ^= 1 << 3;
				else if (streq (buf, "PARCEL"))
					addr->type ^= 1 << 4;
				else if (streq (buf, "HOME"))
					addr->type ^= 1 << 1;
				else if (streq (buf, "WORK"))
					addr->type ^= 1 << 2;
				c2_free (buf);
			}
			c2_free (types);

			/* Learn the address */
			addr->post_office =
				c2_address_book_get_line_field_n (spec, mysep,
								  0);
			addr->extended =
				c2_address_book_get_line_field_n (spec, mysep,
								  1);
			addr->street =
				c2_address_book_get_line_field_n (spec, mysep,
								  2);
			addr->city =
				c2_address_book_get_line_field_n (spec, mysep,
								  3);
			addr->region =
				c2_address_book_get_line_field_n (spec, mysep,
								  4);
			addr->postal_code =
				c2_address_book_get_line_field_n (spec, mysep,
								  5);
			addr->country =
				c2_address_book_get_line_field_n (spec, mysep,
								  6);

			c2_free (spec);

			card->address = g_list_append (card->address, addr);
		}
		else if ((strneq (line, "TEL", 3)) &&
			 (line[3] == separator == ';' ? ';' : ':'))
		{
			C2VCardPhone *ph = c2_address_book_card_phone_new ();
			char *types, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			ph->number =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "PREF"))
					ph->type ^= 1 << 1;
				else if (streq (buf, "WORK"))
					ph->type ^= 1 << 2;
				else if (streq (buf, "HOME"))
					ph->type ^= 1 << 3;
				else if (streq (buf, "VOICE"))
					ph->type ^= 1 << 4;
				else if (streq (buf, "FAX"))
					ph->type ^= 1 << 5;
				else if (streq (buf, "MSG"))
					ph->type ^= 1 << 6;
				else if (streq (buf, "CELL"))
					ph->type ^= 1 << 7;
				else if (streq (buf, "PAGER"))
					ph->type ^= 1 << 8;
				else if (streq (buf, "BBS"))
					ph->type ^= 1 << 9;
				else if (streq (buf, "MODEM"))
					ph->type ^= 1 << 10;
				else if (streq (buf, "CAR"))
					ph->type ^= 1 << 11;
				else if (streq (buf, "ISDN"))
					ph->type ^= 1 << 12;
				else if (streq (buf, "VIDEO"))
					ph->type ^= 1 << 13;
				c2_free (buf);
			}
			c2_free (types);

			card->phone = g_list_append (card->phone, ph);
		}
		else if ((strneq (line, "EMAIL", 5)) &&
			 (line[5] == separator == ';' ? ';' : ':'))
		{
			C2VCardEmail *em = c2_address_book_card_email_new ();
			char *types, *buf;
			char mysep;
			int i;

			mysep = separator == ';' ? ':' : ';';

			/* Get the types and spec of address */
			types =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  0);
			em->address =
				c2_address_book_get_line_field_n (line +
								  offset + 1,
								  separator,
								  1);

			/* Learn the types to the object */
			for (i = 0;; i++)
			{
				if (
				    (buf =
				     c2_address_book_get_line_field_n (types,
								       mysep,
								       i)) ==
				    NULL) break;

				if (streq (buf, "AOL"))
					em->type ^= 1 << 1;
				else if (streq (buf, "APPLELINK"))
					em->type ^= 1 << 2;
				else if (streq (buf, "ATTMAIL"))
					em->type ^= 1 << 3;
				else if (streq (buf, "CIS"))
					em->type ^= 1 << 4;
				else if (streq (buf, "EWORLD"))
					em->type ^= 1 << 5;
				else if (streq (buf, "INTERNET"))
					em->type ^= 1 << 6;
				else if (streq (buf, "IBMMail"))
					em->type ^= 1 << 7;
				else if (streq (buf, "MCIMAIL"))
					em->type ^= 1 << 8;
				else if (streq (buf, "POWERSHARE"))
					em->type ^= 1 << 9;
				else if (streq (buf, "PRODIGY"))
					em->type ^= 1 << 10;
				else if (streq (buf, "TLX"))
					em->type ^= 1 << 11;
				else if (streq (buf, "X400"))
					em->type ^= 1 << 12;
				c2_free (buf);
				break;
			}
			c2_free (types);

			card->email = g_list_append (card->email, em);
		}
		else if (strneq (line, "END:VCARD", 9))
		{
			c2_free (line);
			break;
		}
		else
		{
			if (!other)
				other = g_string_new (NULL);
			g_string_append (other, line);
		}
		c2_free (line);
	}

	if (other)
	{
		card->others = other->str;
		g_string_free (other, FALSE);
	}

	return card;
}

/**
 * c2_address_book_get_line
 * @fd: A pointer to a FILE object.
 *
 * This function will get the next line from the Address Book file
 * dialing with decoding stuff and multilines entries.
 *
 * Return Value:
 * The new line.
 **/
static char *
c2_address_book_get_line (FILE * fd)
{
	GString *abline = g_string_new (NULL);
	char *line = NULL, *ptr;
	char separator;
	int offset = -1, size;
	char *key;
	char *buf;
	int i;

	if ((line = fd_get_line (fd)) == NULL)
		return NULL;
	if (!strlen (line))
		return NULL;

	/* Strip the \r and add the \n */
	size = strlen (line);
	if (line[size - 1] == '\r')
		line[size - 1] = '\n';
	else
	{
		buf = g_strdup_printf ("%s\n", line);
		c2_free (line);
		line = buf;
	}

	/* Learn which is the separator */
	separator = learn_separator (line, &offset);

	buf = g_strndup (line, offset + 1);
	g_string_append (abline, buf);
	c2_free (buf);

	ptr = line + offset + 1;

	for (i = 0;;)
	{
		if (i)
			g_string_append_c (abline, separator);
		key = get_key (ptr, separator);
		ptr += strlen (key);
		if (*ptr == separator)
			ptr++;

		if (streq (key, "QUOTED-PRINTABLE"))
		{
			GString *qpbuf = g_string_new (NULL);
			char *decode;

			/* This is a hard one, load everything until the \n but if the
			 * character before the \n is = the next line should be loaded too.
			 */
			c2_free (key);
			key = g_strdup (ptr);

			while (key[strlen (key) - 2] == '=')
			{
				g_string_append (qpbuf, key);
				i++;
				c2_free (key);
				key = fd_get_line (fd);
				size = strlen (key);
				if (key[size - 1] == '\r')
					key[size - 1] = '\n';
				else
				{
					buf = g_strdup_printf ("%s\n", key);
					c2_free (key);
					key = buf;
				}
			}
			g_string_append (qpbuf, key);
			c2_free (key);

			decode =
				decode_quoted_printable (qpbuf->str,
							 &qpbuf->len);
			g_string_free (qpbuf, TRUE);
			g_string_append (abline, decode);
			c2_free (decode);
			break;
		}
		else
		{
			g_string_append (abline, key);
			i++;
		}
		if (!strlen (ptr))
			break;
	}

	line = abline->str;
	g_string_free (abline, FALSE);
	return line;
}

static char *
get_key (const char *string, char separator)
{
	const char *ptr;

#if DEBUG
//  printf ("%s:%d:Start: @string: %s\n", __PRETTY_FUNCTION__, __LINE__, string);
#endif
	g_return_val_if_fail (string, NULL);

	for (ptr = string; *ptr != '\0'; ptr++)
	{
		if (*ptr == separator)
			break;
	}

	if (ptr)
	{
		char *ret;
		ret = g_new0 (char, (ptr - string) + 1);
		strncpy (ret, string, ptr - string);
		ret[ptr - string] = 0;
		return ret;
	}
	else
		return g_strdup (string);
}

static char
learn_separator (const char *string, int *offset)
{
	const char *ptr;

	g_return_val_if_fail (string, 0);

	for (ptr = string; *ptr != '\0'; ptr++)
		if (*ptr == ';')
		{
			*offset = ptr - string;
			return ':';
		}
		else if (*ptr == ':')
		{
			*offset = ptr - string;
			return ';';
		}

	return 0;
}

/**
 * 
 * cut_leading_chars(gchar *str, gint *num)
 * @str = a freeable string to cut chars from
 * @num = number of characters to cut off
 * 
 * This function basically creates a new pointer
 * and fills it with what was in the old one
 * but cutting off num characters from the begining 
 * of the old one. Then the function frees the old
 * one. Sample use: str = cut_leading_chars(str, 6);
 * 
 * Returns:
 * The new string. This new string should be freed
 * when no longer needed.
 **/
gchar *
cut_leading_chars (gchar * str, gint num)
{
	gint i;
	gchar *new_str;

	/* safety check */
	if (num >= strlen (str))
		return str;
	if (num < 1)
		return str;

	new_str = g_new0 (gchar, (strlen (str) - num + 1));

	for (i = 0; i < (strlen (str) - num); i++)
		new_str[i] = str[i + num];

	g_free (str);

	return new_str;
}

/**
 * cut_leading_space
 * @str = freeable string to cut leading spaces from
 * 
 * Cuts leading spaces from str, if and only if str
 * is not composed of all spaces
 * 
 * Returns: the new str
 */
gchar *
cut_leading_spaces (gchar * str)
{
	gint counter;

	for (counter = 0; counter < strlen (str); counter++)
	{
		if (str[counter] != ' ')
			break;
	}

	if (counter < strlen (str))
		str = cut_leading_chars (str, counter);

	return str;
}

#endif
