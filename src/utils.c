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

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "main.h"
#include "utils.h"
#include "debug.h"
#include "error.h"
#include "net.h"

/* UTF code copied from GnomePrint 0.24 */
#define UTF8_COMPUTE(Char, Mask, Len)					      \
  if (Char < 128)							      \
    {									      \
      Len = 1;								      \
      Mask = 0x7f;							      \
    }									      \
  else if ((Char & 0xe0) == 0xc0)					      \
    {									      \
      Len = 2;								      \
      Mask = 0x1f;							      \
    }									      \
  else if ((Char & 0xf0) == 0xe0)					      \
    {									      \
      Len = 3;								      \
      Mask = 0x0f;							      \
    }									      \
  else if ((Char & 0xf8) == 0xf0)					      \
    {									      \
      Len = 4;								      \
      Mask = 0x07;							      \
    }									      \
  else if ((Char & 0xfc) == 0xf8)					      \
    {									      \
      Len = 5;								      \
      Mask = 0x03;							      \
    }									      \
  else if ((Char & 0xfe) == 0xfc)					      \
    {									      \
      Len = 6;								      \
      Mask = 0x01;							      \
    }									      \
  else									      \
    Len = -1;

#define UTF8_GET(Result, Chars, Count, Mask, Len)			      \
  (Result) = (Chars)[0] & (Mask);					      \
  for ((Count) = 1; (Count) < (Len); ++(Count))				      \
    {									      \
      if (((Chars)[(Count)] & 0xc0) != 0x80)				      \
	{								      \
	  (Result) = -1;						      \
	  break;							      \
	}								      \
      (Result) <<= 6;							      \
      (Result) |= ((Chars)[(Count)] & 0x3f);				      \
    }

/**
 * str_get_word
 * @word_n: Position of the desired word.
 * @str: A pointer to a string containing the target.
 * @ch: The character to use as a separator.
 *
 * Finds the word in position @word_n in @str.
 *
 * Return Value:
 * A freeable string containing the desired word or
 * NULL in case it doesn't exists.
 **/
char *
str_get_word (guint8 word_n, const char *str, char ch) {
  guint8 Ai=0;
  char *c = NULL;
  gboolean record = FALSE;
  gboolean quotes = FALSE;
  GString *rtn;
 
  if (str == NULL) return NULL;
  if (word_n == 0) record = TRUE;
  rtn = g_string_new (NULL);
  
  for (c = CHAR (str); *c != '\0'; c++) {
    if (*c == '"') {
      if (quotes) quotes = FALSE;
      else quotes = TRUE;
      continue;
    }
    
    if (record && *c == '"') break;
    if (record && *c == ch && !quotes) break; /* If the word ends, break */
    if (record) g_string_append_c (rtn, *c); /* Record this character */
    if (*c == ch && !quotes) Ai++;
    if (*c == ch && Ai == word_n && !quotes) record = TRUE;
  }
  
  c = rtn->str;
  g_string_free (rtn, FALSE);
  
  return c;
}

/**
 * str_caps_lock_on
 * @str: A pointer to a string containing the string to be converted.
 *
 * Converts a copy of @str to uppercase.
 *
 * Return Value:
 * A freeable string.
 **/
char *
str_caps_lock_on (const char *str) {
  char *rl;

  rl = g_strdup (str);
  g_strup (rl);
  return rl;
}

/**
 * str_strip
 * @str: A pointer to the target string.
 * @ch: The unwanted character.
 *
 * Removes all occurrances of @ch in @str.
 *
 * Return Value:
 * @str.
 **/
char *
str_strip (char *str, char ch) {
  char *frm;
  char *dst;

  for (frm = dst = str; *frm; frm++) {
	if (*frm != ch) *dst++ = *frm;
  }

  *dst = '\0';

  return str;
}

/**
 * str_strip_enclosed
 * @str: A pointer to the target string.
 * @open: A character to be used as the opening sign.
 * @close: A character to be used as the closing sign.
 *
 * If the string @str starts with @open and finish with @close
 * this function will remove the first and last character in
 * a copy of @str.
 *
 * Return Value:
 * A copy of @str which will not start with @open and finish
 * with @close.
 **/
char *
str_strip_enclosed (const char *str, char open, char close) {
  char *ptr;
  
  g_return_val_if_fail (str, NULL);
  
  if (*str == open && *(str+strlen (str)-1) == close) {
    ptr = g_new0 (char, strlen (ptr)-1);
    strncpy (ptr, str+1, strlen (str)-2);
  } else ptr = g_strdup (ptr);

  return ptr;
}

/**
 * g_utf8_validate:
 * @str: a pointer to character data
 * @max_len: max bytes to validate, or -1 to go until nul
 * @end: return location for end of valid data
 * 
 * Validates UTF-8 encoded text. @str is the text to validate;
 * if @str is nul-terminated, then @max_len can be -1, otherwise
 * @max_len should be the number of bytes to validate.
 * If @end is non-NULL, then the end of the valid range
 * will be stored there (i.e. the address of the first invalid byte
 * if some bytes were invalid, or the end of the text being validated
 * otherwise).
 *
 * Returns TRUE if all of @str was valid. Many GLib and GTK+
 * routines <emphasis>require</emphasis> valid UTF8 as input;
 * so data read from a file or the network should be checked
 * with g_utf8_validate() before doing anything else with it.
 * 
 * Return value:
 * A boolean indicating wheter @str is a valid UTF-8.
 *
 * Note:
 * This function is part of GNOME-Print 0.24 and should be patched
 * when GNOME-Print does.
 * We don't use the one with GNOME-Print because it causes problems
 * if the user doesn't have the correct GNOME-Print version.
 *
 * Copyright: 1999 Tom Tromey - 2000 Red Hat, Inc.
 **/
gboolean
str_utf8_validate (const gchar *str, gint max_len, const gchar **end) {

  const gchar *p;
  gboolean retval = TRUE;
  
  if (end)
    *end = str;
  
  p = str;
  
  while ((max_len < 0 || (p - str) < max_len) && *p)
    {
      int i, mask = 0, len;
      gunichar result;
      unsigned char c = (unsigned char) *p;
      
      UTF8_COMPUTE (c, mask, len);

      if (len == -1)
        {
          retval = FALSE;
          break;
        }

      /* check that the expected number of bytes exists in str */
      if (max_len >= 0 &&
          ((max_len - (p - str)) < len))
        {
          retval = FALSE;
          break;
        }
        
      UTF8_GET (result, p, i, mask, len);

      if (result == (gunichar)-1)
        {
          retval = FALSE;
          break;
        }
      
      p += len;
    }

  if (end)
    *end = p;
  
  return retval;
}

/**
 * str_strip_non_utf8
 * @str: A pointer to the target string.
 *
 * Searchs for non utf8 characters and tries to convert them
 * to valid utf8 characters or it deletes them.
 * Note that this limitation is due to the unsupport of
 * GNOME-Print to non utf8 characters.
 * 
 * If there're characters that this function doesn't support
 * of your language you should update the character table below
 * and send it to cronosII@users.sourceforge.net
 *
 * Return Value:
 * @str.
 **/
char *
str_strip_non_utf8 (char *str) {
  char *frm, *dst;

  g_return_val_if_fail (str, NULL);
  
  for (frm = dst = str; *frm; frm++) {
    int mask = 0, len;
    unsigned char c = (unsigned char) *frm;
    UTF8_COMPUTE (c, mask, len);
    if (len == 1) *dst++ = *frm;
    else {
      /* This char isn't utf8, try to replace */
      switch (*frm) {
	/************************************************/
	/********* This is the characters table *********/
	/* Please, send patches for your language chars */
	/************************************************/
	case 'á': *dst++ = 'a'; break;
	case 'é': *dst++ = 'e'; break;
	case 'í': *dst++ = 'i'; break;
	case 'ó': *dst++ = 'o'; break;
	case 'ú': *dst++ = 'u'; break;
	case 'ñ': *dst++ = 'n'; break;
	case 'Á': *dst++ = 'A'; break;
	case 'É': *dst++ = 'E'; break;
	case 'Í': *dst++ = 'I'; break;
	case 'Ó': *dst++ = 'O'; break;
	case 'Ú': *dst++ = 'U'; break;
	case 'Ñ': *dst++ = 'N'; break;
      }
    }
  }

  *dst = 0;

  return str;
}

/**
 * strcaseeq
 * @fst: A pointer to a string.
 * @snd: A pointer to a string.
 *
 * Comparates both string case-sensitive.
 * Use this function family instead of strcmp
 * when posible since this function will terminate
 * immeditely when it founds something different,
 * thus why is faster.
 *
 * Return Value:
 * A boolean indicating wheter the string where
 * equal.
 **/
gboolean
strcaseeq (const char *fst, const char *snd) {
  const char *ptr[2];

  for (ptr[0] = fst, ptr[1] = snd; ptr[0] || ptr[1]; ptr[0]++, ptr[1]++) {
    if (*ptr[0] == '\0') {
      if (*ptr[1] == '\0') return TRUE;
      else return FALSE;
    }
    if (*ptr[1] == '\0') return FALSE;
    if (*ptr[0] != *ptr[1]) return FALSE;
  }
  return TRUE;
}

/**
 * strncaseeq
 * @fst: A pointer to a string.
 * @snd: A pointer to a string.
 * @length: A number indicating how many of the first characters
 * 	    to comparate.
 *
 * Comparates both string case-sensitive in the first
 * @length characters.
 * Use this function family instead of strcmp
 * when posible since this function will terminate
 * immeditely when it founds something different,
 * thus why is faster.
 *
 * Return Value:
 * A boolean indicating wheter the string where
 * equal.
 **/
gboolean
strncaseeq (const char *fst, const char *snd, int length) {
  const char *ptr[2];
  int i;

  for (ptr[0] = fst, ptr[1] = snd, i = 0; i < length; ptr[0]++, ptr[1]++, i++) {
    if (*ptr[0] == '\0') {
      if (*ptr[1] == '\0') return TRUE;
      else return FALSE;
    }
    if (*ptr[1] == '\0') return FALSE;
    if (*ptr[0] != *ptr[1]) return FALSE;
  }
  return TRUE;
}

/**
 * streq
 * @fst: A pointer to a string.
 * @snd: A pointer to a string.
 *
 * Comparates both string case-insensitive.
 * Use this function family instead of strcmp
 * when posible since this function will terminate
 * immeditely when it founds something different,
 * thus why is faster.
 *
 * Return Value:
 * A boolean indicating wheter the string where
 * equal.
 **/
gboolean
streq (const char *fst, const char *snd) {
  const char *ptr[2];
  char *_fst;
  char *_snd;

  if (!fst && snd) return FALSE;
  if (fst && !snd) return FALSE;
  if (!fst && !snd) return TRUE;
  _fst = g_strdup (fst);
  g_strup (_fst);
  _snd = g_strdup (snd);
  g_strup (_snd);

  for (ptr[0] = _fst, ptr[1] = _snd; ptr[0] || ptr[1]; ptr[0]++, ptr[1]++) {
    if (*ptr[0] == '\0') {
      if (*ptr[1] == '\0') return TRUE;
      else return FALSE;
    }
    if (*ptr[1] == '\0') return FALSE;
    if (*ptr[0] != *ptr[1]) return FALSE;
  }
  c2_free (_fst);
  c2_free (_snd);
  return TRUE;
}

/**
 * strneq
 * @fst: A pointer to a string.
 * @snd: A pointer to a string.
 * @length: A number indicating how many of the first characters
 * 	    to comparate.
 *
 * Comparates both string case-insensitive in the first
 * @length characters.
 * Use this function family instead of strcmp
 * when posible since this function will terminate
 * immeditely when it founds something different,
 * thus why is faster.
 *
 * Return Value:
 * A boolean indicating wheter the string where
 * equal.
 **/
gboolean
strneq (const char *fst, const char *snd, int length) {
  const char *ptr[2];
  char *_fst;
  char *_snd;
  int i;

  if (!fst && snd) return FALSE;
  if (fst && !snd) return FALSE;
  if (!fst && !snd) return TRUE;
  if (length < 1) return TRUE;
  
  _fst = g_strdup (fst);
  g_strup (_fst);
  _snd = g_strdup (snd);
  g_strup (_snd);
  
  for (ptr[0] = _fst, ptr[1] = _snd, i = 0; i < length; ptr[0]++, ptr[1]++, i++) {
    if (*ptr[0] == '\0') {
      if (*ptr[1] == '\0') return TRUE;
      else return FALSE;
    }
    if (*ptr[1] == '\0') return FALSE;
    if (*ptr[0] != *ptr[1]) return FALSE;
  }
  c2_free (_fst);
  c2_free (_snd);
  return TRUE;
}

/**
 * str_unquote
 * @str: A double pointer to a string.
 *
 * Unquote the string if its
 * quoted.
 **/
void
str_unquote (char **str) {
  g_return_if_fail (*str);

  if (**str == '"' && *((*str) + strlen (*str)-1) == '"') {
    *((*str) + strlen (*str)-1) = '\0';
    (*str)++;
  }
}

#if TRUE
/**
 * str_get_line
 * @str: A pointer to an string object where the next line should be searched.
 *
 * Searchs for the next line.
 * Note that this function should be used with a pointer
 * to the original string in a loop by moving the pointer
 * ptr += strlen (return value).
 *
 * Return Value:
 * A freeable string containing the next line of the string @str.
 **/
char *
str_get_line (const char *str) {
  int len=0;
  int i;
  char *string;
  char *strptr, *ptr;
  
  g_return_val_if_fail (str, NULL);
 
  for (strptr = CHAR (str);; strptr++) {
    if (*strptr == '\0') {
      if (len == 0) return NULL;
      break;
    }
    len++;
    if (*strptr == '\n') break;
  }

  string = CHARALLOC (len);

  for (i=0, ptr = string, strptr = CHAR (str); i <= len; i++, strptr++) {
    if (*strptr == '\0') {
      break;
    }
    *(ptr++) = *strptr;
    if (*strptr == '\n') break;
  }
  *ptr = '\0';

  return string;
}
#else
char *str_get_line (const char *str) {
  char *c, *ptr;
  char *retval;
  int len = 0;
  gboolean reached_null = FALSE;
L
L  g_return_val_if_fail (str, NULL);
L
L  for (c = (char *) str;; c++, len++) {
L    if (*c == '\0') {
L      reached_null = TRUE;
L      break;
L    }
L    if (*c == '\n') break;
L  }
L
L  if (!len && reached_null) return NULL;
printf ("%d\n", len);
L
L  retval = CHARALLOC (len);
L  retval[len] = '\0';
L
L  for (c = (char *) str, ptr = retval; ; c++, ptr++) {
L    if (*c == '\0') break;
L    *ptr = *c;
L    if (*c == '\n') break;
L  }
L  *(++ptr) = '\0';
L
L  return retval;
}
#endif

char *
str_replace (const char *or_string, const char *se_string, const char *re_string) {
  GString *str=g_string_new (NULL);
  int pos=1;
  char *c;
  int Ia;
  
  pos = find_string (or_string, se_string);
  
  for (Ia=0, c = CHAR (or_string); *c != '\0'; Ia++, c++) {
    if (Ia == pos) {
      c += strlen (se_string)-1;
      Ia += strlen (se_string);
      pos = find_string (or_string+Ia, se_string);
      Ia--;
      g_string_append (str, re_string);
    } else {
      g_string_append_c (str, *c);
    }
  }
  
  c = str->str;
  g_string_free (str, FALSE);
  return c;
}

/**
 * strcasestr
 * @_string: A pointer to a string containing the target.
 * @_find: A pointer to a string containing what should be searched.
 *
 * Search @_find in @_string case-insensitive.
 * Is equal to strstr but case-insensitive.
 *
 * Return Value:
 * A pointer to the start of the first occurrance of @_find
 * within @_string or NULL in case it couldn't be found.
 **/
const char *
strcasestr (const char *_string, const char *_find) {
  char *string, *find;
  char *ptr;
  const char *ptr2;

  g_return_val_if_fail (_string, NULL);
  g_return_val_if_fail (_find, NULL);
  
  string = g_strdup (_string);
  find = g_strdup (_find);
  g_strup (string);
  g_strup (find);

  ptr = strstr (string, find);
  if (!ptr) return NULL;

  ptr2 = _string + (ptr - string);

  c2_free (string);
  c2_free (find);

  return ptr2;
}

/**
 * str_replace_all
 * @or_string: A pointer to a string to have the replacements made on it
 * @se_string: The string to replace
 * @re_string: The string to replace @se_string with
 *
 * Search @or_string and finds all instances of se_string, and replaces
 * them with re_string in a new freeble string that is returned.
 *
 * Return Value:
 * A new string with the replacements that should be freed when no 
 * longer needed.
 **/
char *str_replace_all (const char *or_string, const char *se_string, const char *re_string) {
  char *ptr;
  char *str, *strptr;
  int length = 0;
  
  g_return_val_if_fail (or_string, NULL);
  g_return_val_if_fail (se_string, NULL);
  g_return_val_if_fail (re_string, NULL);

  for (ptr = CHAR (or_string);;) {
    if (*ptr == '\0') break;
    if (strneq (ptr, se_string, strlen (se_string))) {
      length += strlen (re_string);
      ptr += strlen (se_string);
    } else {
      length++;
      ptr++;
    }
  }

  str = CHARALLOC (length);
  str[length] = '\0';

  for (ptr = CHAR (or_string), strptr = str;;) {
    if (*ptr == '\0') break;
    if (strneq (ptr, se_string, strlen (se_string))) {
      strcpy (strptr, re_string);
      strptr += strlen (re_string);
      ptr += strlen (se_string);
    } else {
      *strptr = *ptr;
      strptr++;
      ptr++;
    }
  }

  return str;
}

int find_string (const char *haystack, const char *needle) {
  gchar *retval;

  if (!haystack || !needle || !*haystack || !*needle) return -1;

  if ((retval = strstr (haystack, needle)) == (gchar *) NULL) return -1;

  return (retval - haystack);
}

/* TODO Recode this suckful function */
char *str_get_mail_address (const char *to) {
  char *cto, *ptr;
  char *beg=NULL;
  GString *mail;
  gboolean found = FALSE;

  g_return_val_if_fail (to, NULL);

  mail = g_string_new (NULL);

  cto = g_strdup (to);
  
  /* Look for the "<mail>" format */
  for (ptr = cto; ptr && *ptr != '\0'; ptr++) {
    if (!beg) {
      /* Still looking for the '<' */
      if (*ptr == '<') beg = ptr;
    } else {
      /* Already found '<', looking for '>' */
      if (*ptr == '>') {
	/* Found '>' */
	if (mail->len) {
	  found = TRUE;
	  break;
	}
	g_string_assign (mail, "");
      } else {
	/* Append this character */
	g_string_append_c (mail, *ptr);
      }
    }
  }
 
  if (!found) {
    /* Just in case... */
    g_string_assign (mail, "");
    
    /* Look for the "mail@host" format */
    for (ptr = cto; ptr && *ptr != '\0'; ptr++) {
      /* Look for an '@' */
      if (*ptr != ' ') {
	if (!beg) beg = ptr;
	/* Append this character */
	g_string_append_c (mail, *ptr);
	if (*ptr == '@') found = TRUE;
      } else {
	/* If there's already a word with '@' we pressume it's a mail_addr */
	if (found) break;
	else g_string_assign (mail, "");
      }
    }
  }

  if (!found) {
    /* Just in case... */
    g_string_assign (mail, "");

    /* Look for the "mail" format */
    for (ptr = cto; ptr && *ptr != '\0'; ptr++) {
      if (*ptr != ' ') {
	g_string_append_c (mail, *ptr);
      } else {
	g_string_assign (mail, "");
      }
    }
  }
  
  found = FALSE;
  /* Check if the mail has an '@' */
  for (ptr = cto; ptr && *ptr != '\0'; ptr++) {
    if (*ptr == '@') {
      found = TRUE;
      break;
    }
  }

  if (!found) {
    /* This address doesn't have an '@', we should append an @ and the name of the local machine */
    c2_free (beg);
    
    beg = (char *) g_malloc (MAXHOSTNAMELEN);
    if (!gethostname (beg, MAXHOSTNAMELEN)) {
      g_string_append_c (mail, '@');
      g_string_append (mail, beg);
    } 
  }

  beg = mail->str;
  g_string_free (mail, FALSE);
  return beg;
}

char *strstrcase (char *haystack, char *needle) {
  /* Function by Jeffrey Stedfast */
  /* find the needle in the haystack neglecting case */
  gchar *ptr;
  guint len;
  
  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);
  
  if ((len = strlen (needle)) == 0)
    return haystack;
  if (len > strlen (haystack))
    return NULL;
  
  for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
    if (!g_strncasecmp (ptr, needle, len))
      return ptr;
  
  return NULL;
}

char *strip_common_subject_prefixes (char *string) {
  gchar *pos;
  gint i;
  
  static gchar *common_prefixes[] = { " ", "\t", "Re:", "Fwd:", "Fw:", '\0' };
  
  i = 0;
  pos = string;
  while (*pos && common_prefixes[i]) {
    if ((gchar *)strstrcase (pos, common_prefixes[i]) == pos) {
      pos += strlen (common_prefixes[i]);
      i = 0;
    } else {
      i++;
    }
  }
  return g_strdup (pos);
}

/**
 * c2_file_exists
 * @file: A pointer to a character object containing the file path.
 *
 * Checks if the file exists.
 *
 * Return Value:
 * Return TRUE if the files exists or FALSE if it doesn't.
 **/
gboolean
c2_file_exists (const char *file) {
  struct stat st;

  g_return_val_if_fail (file, FALSE);
  
  if (stat (file, &st) < 0) return FALSE;
  return TRUE;
}

char *fd_get_word (FILE *fd) {
  GString *str;
  gboolean inside_quotes = FALSE;
  int buf;
  char *rtn;

  /* Move to a valid char */
  for (;;) {
    if ((buf = fgetc (fd)) == EOF) return NULL;
    if (buf >= 48 && buf <= 57) break;
    if (buf >= 65 && buf <= 90) break;
    if (buf >= 97 && buf <= 122) break;
    if (buf == 34) break;
    if (buf == '*') {
      if (fd_move_to (fd, '\n', 1, TRUE, TRUE) == EOF) fseek (fd, -1, SEEK_CUR);
      return fd_get_word (fd);
    }
  }

  fseek (fd, -1, SEEK_CUR);
  str = g_string_new (NULL);
	
  for (;;) {
    if ((buf = fgetc (fd)) == EOF) {
      if (inside_quotes)
        g_warning ("Bad syntaxis, quotes aren't closed.\n");
	break;
    }
    
    if (buf == 34) {
      if (inside_quotes) inside_quotes = FALSE;
      else inside_quotes = TRUE;
      continue;
    }
		
    if (!inside_quotes && buf == ' ') break;
    if (!inside_quotes && buf == '\n') break;
    
    g_string_append_c (str, buf);
  }
  
  fseek (fd, -1, SEEK_CUR);
  rtn = str->str;
  g_string_free (str, FALSE);
  
  return rtn;
}

#if TRUE
char *fd_get_line (FILE *fd) {
  long pos=ftell (fd);
  int len=1;
  int i;
  int buf;
  char *str;
  char *ptr;
  
  if (pos < 0) {
    cronos_error (errno, "Reading the position of the pointer of the file", ERROR_WARNING);
    return NULL;
  }

  for (;;) {
    if ((buf = fgetc (fd)) == EOF) {
      if (len == 1) return NULL;
      break;
    }
    if (buf == '\n') break;
    len++;
  }

  str = g_malloc0 (len * sizeof (char));
  fseek (fd, pos, SEEK_SET);

  for (i=0, ptr = str; i < len; i++) {
    if ((buf = fgetc (fd)) == EOF) {
      fseek (fd, 0, SEEK_END);
      break;
    }
    if (buf == '\n') break;
    *(ptr++) = buf;
  }
  *ptr = '\0';

  return str;
}
#else
char *fd_get_line (FILE *fd) {
  char *rtn;
  char buf;
  GString *str;
  
  str = g_string_new (NULL);
  
  for (;;) {
	if ((buf = fgetc (fd)) == EOF) {
	  if (!strcmp (str->str, "")) {
		g_string_free (str, TRUE);
		return NULL;
	  }
	  rtn = str->str;
	  g_string_free (str, FALSE);
	  fseek (fd, 0, SEEK_END);
	  return rtn;
	}
	if (buf == '\n') break;
	g_string_append_c (str, buf);
  }
  
  rtn = str->str;
  g_string_free (str, FALSE);
  
  return rtn;
}
#endif

gboolean fd_is_dir (char *path) {
  struct stat buf;

  if (stat (path, &buf) < 0) {
    return FALSE;
  }

  return (S_ISDIR (buf.st_mode));
}

gboolean
fd_move_to (FILE *fp, int c, guint8 cant, gboolean forward, gboolean next) {
  int s=' ';
  int ia;
  guint8 go;

  if (c == EOF) {
    for (;;) if ((s = getc (fp)) == EOF) break;
    if (!next) if (fseek (fp, -1, SEEK_CUR) < 0) return FALSE;
    return TRUE;
  }
  
  for (ia = 0, go = 0; s != c; ia++) {
    if ((s = getc (fp)) == EOF) break;
    if (s == c) {
      if (++go == cant) break;
    }
    
    if (!forward) if (fseek (fp, -2, SEEK_CUR) < 0) return FALSE;
  }
	
  if (s != c) return FALSE;
  if (!next) {
    if (fseek (fp, -1, SEEK_CUR) < 0) return FALSE;
    ia -= 1;
  }
 
  return TRUE;
}

const char *
pixmap_get_icon_by_mime_type (const char *mime_type) {
  const char *path;
  
  g_return_val_if_fail (mime_type, NULL);

  path = gnome_mime_get_value (mime_type, "icon-filename");

  if (!path || !g_file_exists (path)) {
    path = g_strdup (DATADIR "/cronosII/pixmap/unknown-file.png");
  }

  return path;
}

/**
 * cronos_tmpfile
 *
 * Generates a temporary file.
 *
 * Return Value:
 * A freeable pointer to a temporary file path.
 **/
char *
cronos_tmpfile (void) {
  char *temp;
  int fd;

  temp = g_strdup ("/tmp/c2-tmp.XXXXXXX");
  fd = mkstemp (temp);
  close (fd);
  return temp;
}

/**
 * fd_bin_cp
 * @strfrm: A pointer to a string containing the path to the source file.
 * @strdst: A pointer to a string containing the path to the target file.
 *
 * Copies a file from @strfrm to @strdst using binary compatible functions.
 *
 * Return Value:
 * TRUE in case of success, FALSE if there's an error.
 **/
int
fd_bin_cp (const char *strfrm, const char *strdst) {
  char buf[100];
  FILE *frm;
  FILE *dst;
  
  g_return_val_if_fail (strfrm, FALSE);
  g_return_val_if_fail (strdst, FALSE);

  if ((frm = fopen (strfrm, "rt")) == NULL) {
    cronos_error (errno, ("Couldn't open the source file to do a binary copy"), ERROR_WARNING);
    return FALSE;
  }

  if ((dst = fopen (strdst, "wt")) == NULL) {
    cronos_error (errno, ("Couldn't open the target file to do a binary copy"), ERROR_WARNING);
    fclose (frm);
    return FALSE;
  }

  for (;;) {
    if (fread (buf, 1, sizeof (buf), frm) < sizeof (buf)-1) break;
    fwrite (buf, 1, sizeof (buf), dst);
  }

  fclose (frm);
  fclose (dst);
  return TRUE;
}

gboolean fd_mv (char *str_frm, char *str_dst) {
  if (!fd_cp (str_frm, str_dst)) {
    if (unlink (str_frm) < 0)
#if DEBUG
      perror ("fd_mv::unlink(str_frm)");
#else
    ;
#endif
    return 0;
  } else {
    return -1;
  }
}

gboolean fd_cp (char *str_frm, char *str_dst) {
  char *buf;
  FILE *frm;
  FILE *dst;

  if ((frm = fopen (str_frm, "r")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, ("Opening a file for moving it into other (from)"), ERROR_WARNING);
    gdk_threads_leave ();
    return -1;
  }
  if ((dst = fopen (str_dst, "w")) == NULL) {
    gdk_threads_enter ();
    cronos_error (errno, ("Opening a file for moving it into other (dest)"), ERROR_WARNING);
    gdk_threads_leave ();
    fclose (frm);
    return -1;
  }

  for (;;) {
    if ((buf = fd_get_line (frm)) == NULL) break;

    fprintf (dst, "%s\n", buf);
    c2_free (buf);
  }

  fclose (frm);
  fclose (dst);

  return 0;
}

static char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", 
			  "Aug", "Sep", "Oct", "Nov", "Dec" };

date_t parse_date (char *datestr) {
  /* Function by Jeffrey Stedfast */
  GList *tokens;
  date_t date;
  
  /* Split the date string into tokens, removing quotes and parenthesis */
  tokens = g_list_strtok (datestr, " \"()");
  
  /* Per RFC1123, it is our duty to properly handle all RFC822+1123
   * dates; if this isn't an RFC-compliant date, make our best guess */
  date = parse_rfc822_date (tokens);
  if (date.bestguess != 1) {
    g_list_free (tokens);
    return date;
  } else {
    /* This is only hit if the date is not RFC822 compliant */
    date = parse_bestguess_date (tokens);
    g_list_free (tokens);
    return date;
  }
  
  return date;
}

date_t adjust_date_t_for_timezone (date_t date) {
  /* Function by Jeffrey Stedfast */
  /* Based on date_t structure, adjust time units based on
   * date.original_tz_offset */
  
  gint tz_offset_minutes, tz_offset_hours;
  
  tz_offset_minutes = (gint)(date.original_tz_offset * 100) % 100;
  tz_offset_hours = (gint)(date.original_tz_offset / 1);
  
  if (date.original_tz_offset != 0) {
    /* Adjust hours and minutes to UTC */
    date.hour -= tz_offset_hours;
    date.minute -= tz_offset_minutes;
    
    /* adjust seconds to proper range */
    if (date.second > 59) {
      date.minute += (date.second / 60);
      date.second = (date.second % 60);
    }
    
    /* adjust minutes to proper range */
    if (date.minute > 59) {
      date.hour += (date.minute / 60);
      date.minute = (date.minute % 60);
    } else if (date.minute < 0) {
      date.minute = -date.minute;
      date.hour -= (date.minute / 60) - 1;
      date.minute = 60 - (date.minute % 60);
    }
    
    /* adjust hours to the proper range */
    if (date.hour > 23) {
      date.day += (date.hour / 24);
      date.hour = (date.hour % 24);
    } else {
      if (date.hour < 0) {
	date.hour = -date.hour;
	date.day -= (date.hour / 24) - 1;
	date.hour = 24 - date.hour;
      }
    }
    
    /* adjust days to the proper range */
    while (date.day > get_days_in_month (date.month, date.year)) {
      date.day -= get_days_in_month (date.month, date.year);
      date.month++;
      if (date.month > 12) {
	date.year += (date.month / 12);
	date.month = (date.month % 12);
	if (date.month == 0) {
	  /* month sanity check */
	  date.month = 12;
	  date.year -= 1;
	}
      }
    }
    
    while (date.day < 1) {
      date.day += get_days_in_month (date.month, date.year);
      date.month--;
      if (date.month < 1) {
	date.month = -date.month;
	date.year -= (date.month / 12) - 1;
	date.month = 12 - (date.month % 12);
      }
    }
    
    /* adjust months to the proper range */
    if (date.month > 12) {
      date.year += (date.month / 12);
      date.month = (date.month % 12);
      if (date.month == 0) {
	/* month sanity check */
	date.month = 12;
	date.year -= 1;
      }
    } else {
      if (date.month < 1) {
	date.month = -date.month;
	date.year -= (date.month / 12) - 1;
	date.month = 12 - (date.month % 12);
      }
    }
  }
  
  return date;
}

date_t parse_rfc822_date (GList *tokens) {
  /* Function by Jeffrey Stedfast */
  /* Parse RFC822-compliant dates into a date_t structure; flag
   * date.bestguess if this is NOT an RFC822-compliant date */
  guint i = 0, retval = 0;
  gchar *token;
  date_t date;
  
  memset ((void*)&date, 0, sizeof (date_t));
  g_return_val_if_fail (tokens != NULL, date);
  
  /* Check to see if our first parameter is a day of week (optional) */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if ((retval = weekday_toi (token))) {
    date.day_of_week = retval;
    i++;
  }
  
  /* Next parameter should be day */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if (isdigit (*token)) {
    retval = atoi (token);
    date.day = retval;
    i++;
  } else {
    date.bestguess = 1;
    return date;
  }
  
  /* Next parameter should be month */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if ((retval = month_toi (token))) {
    date.month = retval;
    i++;
  } else {
    date.bestguess = 1;
    return date;
  }
  
  /* Next parameter should be year */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if (isdigit (*token)) {
    retval = atoi (token);
    if (retval < 100) {
      date.year = retval < 69 ? 2000 + atoi (token) 
	: 1900 + atoi (token);
    } else
      date.year = retval;
  } else {
    date.bestguess = 1;
    return date;
  }
  
  /* Verify that our day is valid for month/year */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if ((date.day > 0) && (date.day <= get_days_in_month (date.month, 
	  date.year))) {
    i++;
  } else {
    date.bestguess = 1;
    return date;
  }
  
  /* Next field should be time */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if (strchr (token, ':') && isdigit (*token)) {
    if (strlen (token) == 8) {
      sscanf (token, "%d:%d:%d", &date.hour, &date.minute, 
	  &date.second);
    } else if (strlen (token) == 5) {
      sscanf (token, "%d:%d", &date.hour, &date.minute);
      date.second = 0;
    } else {
      date.bestguess = 1;
      return date;
    }
    
    if ((date.hour < 24) && (date.minute < 60) 
	&& (date.second < 60)) {
      i++;
    } else {
      date.bestguess = 1;
      return date;
    }
  } else {
    date.bestguess = 1;
    return date;
  }
  
  /* Next field should be a time zone indicator (only bother with
   * time zone abbreviations if a numeric is not present) */
  if ((token = g_list_nth_data (tokens, i)) == NULL) {
    date.bestguess = 1;
    return date;
  }
  if ((*token == '-') || (*token == '+'))
    date.original_tz_offset = atoi (token) / 100.0;
  else
    if ((date.original_tz_offset = rfc1123_timezone_tod (token)) 
	== -100.0) {
      date.bestguess = 1;
      return date;
    }
  
  /* Adjust date for timezone */
  date = adjust_date_t_for_timezone (date);
  
  /* Return final conclusion */
  return date;
}

date_t parse_bestguess_date (GList *tokens) {
  /* Function by Jeffrey Stedfast */
  guint token_count = 0, i, retval = 0;
  date_t date;
  gchar *token, *ptr;
  
  memset ((void*)&date, 0, sizeof (date_t));
  g_return_val_if_fail (tokens != NULL, date);
  date.bestguess = 1;
  
  token_count = g_list_length (tokens);
  
  for (i = 0; i < token_count; i++) {
    token = g_list_nth_data (tokens, i);
    
    if ((retval = weekday_toi (token))) {
      date.day_of_week = retval;
    } else if ((retval = month_toi (token))) {
      date.month = retval;
    } else if (strlen (token) <= 2) {
      /* this could be a 1 or 2 digit day of the month */
      for (retval = 1, ptr = token; *ptr; ptr++)
	if (*ptr < '0' || *ptr > '9')
	  retval = 0;
      
      if (retval && atoi (token) <= 31 && !date.day)
	date.day = atoi (token);
      else
	date.year = atoi (token) < 69 ? 2000 + atoi (token) : 1900 + atoi (token);
    } else if (strlen(token) == 4) {
      /* this could be the year... */
      for (retval = 1, ptr = token; *ptr; ptr++)
	if (*ptr < '0' || *ptr > '9')
	  retval = 0;
      
      if (retval)
	date.year = atoi (token);
    } else if (strchr (token, ':')) {
      /* this must be the time: hh:mm:ss */
      sscanf (token, "%d:%d:%d", &date.hour, &date.minute, &date.second);
    } else if (*token == '-' || *token == '+') {
      date.original_tz_offset = atoi (token) / 100.0;
    } else if (strcasecmp (token, "PM") == 0) {
      if (date.hour < 12)
	date.hour += 12;
    }
  }
  
  /* Adjust date for timezone */
  date = adjust_date_t_for_timezone (date);
  
  return date;
}

gint get_days_in_month (gint month, gint year) {
  /* Function by Jeffrey Stedfast */
  /* Return the number of days in a month for a particular year;
   * return 0 on error (unknown month) */
  
  switch (month) {
    case 1: case 3: case 5: case 7: case 8: case 10: case 12:
	    return 31;
    case 4: case 6: case 9: case 11:
	    return 30;
    case 2:
	    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
	      return 29;
	    else
	      return 28;
    default:
	    return 0;
  }
}

gint weekday_toi (gchar *string) {
  /* Function by Jeffrey Stedfast */
  /* Return a numeric equivalent for the weekday passed to the function;
   * return 0 on error (unknown weekday) */
  
  int i;
  
  g_return_val_if_fail ((string != NULL), 0);
  
  for (i = 0; i < 7; i++)
    if (g_strncasecmp (string, days[i], 2) == 0)
      return i + 1;
  
  return 0;  /* unknown week day */
}

gint month_toi (gchar *string) {
  /* Function by Jeffrey Stedfast */
  /* Return a numeric equivalent for the month passed to the function;
   * return 0 on error (unknown month) */
  
  int i;
  
  g_return_val_if_fail (string != NULL, 0);
  
  for (i = 0; i < 12; i++)
    if (g_strncasecmp (string, months[i], 3) == 0)
      return i + 1; 
  
  return 0;  /* unknown month */
}

gdouble rfc1123_timezone_tod (gchar *timezonestring) {
  /* Function by Jeffrey Stedfast */
  /* Return a double representing the timezone offset from UTC (/100.0)
   * using RFC822 timezone information with modifications listed in
   * RFC1123; return -100.0 on error (invalid/unknown timezone) */
  
  /* Military time zones are specified incorrectly in RFC822 and are
   * ignored, per RFC1123 */
  
  /* RFC822-specified time zones */
  if (g_strcasecmp ("UT", timezonestring) == 0)
    return 0.0;
  else if (g_strcasecmp ("GMT", timezonestring) == 0)
    return 0.0;
  else if (g_strcasecmp ("Z", timezonestring) == 0)
    return 0.0;
  else if (g_strcasecmp ("EST", timezonestring) == 0)
    return -5.0;
  else if (g_strcasecmp ("EDT", timezonestring) == 0)
    return -4.0;
  else if (g_strcasecmp ("CST", timezonestring) == 0)
    return -6.0;
  else if (g_strcasecmp ("CDT", timezonestring) == 0)
    return -5.0;
  else if (g_strcasecmp ("MST", timezonestring) == 0)
    return -7.0;
  else if (g_strcasecmp ("MDT", timezonestring) == 0)
    return -6.0;
  else if (g_strcasecmp ("PST", timezonestring) == 0)
    return -8.0;
  else if (g_strcasecmp ("PDT", timezonestring) == 0)
    return -7.0;
  
  return -100.0;
}

GList *g_list_strtok (char *string, char *delimeters) {
  /* Function by Jeffrey Stedfast */
  gchar *token_start, *token_end;
  gchar *delimeter, *token;
  GList *result = NULL;
  
  /* Return NULL if we have no string or no delimeters */
  if ((*string == '\0') || (*delimeters == '\0'))
    return NULL;
  
  token_start = string;
  
  while (*token_start) {
    /* Start after any delimeters */
    delimeter = delimeters;
    while (*delimeter && *token_start) {
      if (*delimeter == *token_start) {
	delimeter = delimeters;
	token_start++;
      } else {
	delimeter++;
      }
    }
    
    /* Find the end of the token */
    delimeter = delimeters;
    token_end = token_start;
    while (*token_end) {
      while (*delimeter && (*delimeter != *token_end))
	delimeter++;
      
      if (*delimeter == *token_end)
	break;
      
      token_end++;
      delimeter = delimeters;
    }
    
    /* Do not produce any NULL tokens */
    if (token_end != token_start) {
      token = g_strndup (token_start, (token_end - 
	    token_start));
      result = g_list_append (result, token);
    }
    
    token_start = token_end;
  }
  
  return result;
}

char *cronos_system (const char *string) {
  FILE *pipe;
  char buf[80];
  char *s = NULL;
  GString *str;
  
  g_return_val_if_fail (string, ("No command has been passed."));
  memset (buf, 0, sizeof (buf));

  if ((pipe = popen (string, "r")) == NULL) {
    return g_strerror (errno);
  }

  str = g_string_new (NULL);
  while (fgets (buf, sizeof (buf), pipe)) {
    g_string_append (str, buf);
    memset (buf, 0, sizeof (buf));
  }

  s = str->str;
  g_string_free (str, FALSE);
  pclose (pipe);

  return s;
}
