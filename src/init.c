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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "init.h"
#include "error.h"
#include "mailbox.h"
#include "utils.h"
#include "account.h"
#include "debug.h"
#include "rc.h"
#include "main.h"
#include "version.h"

#include "gui-logo.h"

static void file_init (Conf *config);
static void first_run (char path[]);

void cronos_init (void) {
  config = g_new0 (Conf, 1);

  /* Defaults */
  config->account_head		= NULL;
  config->mailbox_head		= NULL;
#if USE_PLUGINS
  config->module_head		= NULL;
#endif
  config->empty_garbage		= FALSE;
  config->check_timeout		= 30;
  config->check_at_start	= FALSE;
  config->use_outbox		= TRUE;
  config->prepend_char_on_re	= g_strdup ("> ");
  config->message_bigger	= 0;
  config->timeout		= 15;
  config->mark_as_read		= 1;
  config->default_mime_part	= INIT_DEFAULT_MIME_PART_PLAIN;
#ifdef BUILD_ADDRESS_BOOK
  config->addrbook_init		= INIT_ADDRESS_BOOK_INIT_START;
#endif
  config->use_persistent_smtp_connection = 0;
  config->persistent_smtp_address = NULL;
  config->persistent_smtp_port = 25;

  config->color_reply_original_message.pixel = 0;
  config->color_reply_original_message.red = 0;
  config->color_reply_original_message.green = 0;
  config->color_reply_original_message.blue = 0xffff;
  config->color_misc_body.red = 0;
  config->color_misc_body.green = 0;
  config->color_misc_body.blue = 0;
  config->color_misc_body.pixel = 0;
  
  /* File */
  file_init (config);

  rc_init ();
#if USE_PLUGINS
  c2_dynamic_module_autoload ();
#endif
}

void file_init (Conf *config) {
  char *buffer;
  char *key, *val;
  FILE *fd;
  char *old_version = NULL;
  
  buffer = CONFIG_FILE;
  
  if ((fd = fopen (buffer, "r")) == NULL) {
    first_run (buffer);

    if ((fd = fopen (buffer, "r")) == NULL) {
      exit(1);
    }
  }

  c2_free (buffer);
  
  /* Start reading the file */
  for (;;) {
    if ((key = fd_get_word (fd)) == NULL) break;
    if ((val = fd_get_word (fd)) == NULL) break;
    if (fd_move_to (fd, '\n', 1, TRUE, TRUE) == EOF) fseek (fd, -1, SEEK_CUR);

    if (streq (key, "empty_garbage")) {
      config->empty_garbage = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "account")) {
      load_account (&config->account_head, val);
    } else
    if (streq (key, "mailbox")) {
      c2_mailbox_load (&config->mailbox_head, val);
    } else
    if (streq (key, "check_timeout")) {
      config->check_timeout = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "check_at_start")) {
      config->check_at_start = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "use_outbox")) {
      config->use_outbox = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "use_persistent_smtp_connection")) {
      config->use_persistent_smtp_connection = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "persistent_smtp_address")) {
      config->persistent_smtp_address = val;
    } else
    if (streq (key, "persistent_smtp_port")) {
      config->persistent_smtp_port = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "prepend_char_on_re")) {
      c2_free (config->prepend_char_on_re);
      config->prepend_char_on_re = val;
    } else
    if (streq (key, "message_bigger")) {
      config->message_bigger = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "timeout")) {
      config->timeout = atoi (val);
      c2_free (val);
    } else
    if (streq (key, "mark_as_read")) {
      config->mark_as_read = atoi (val);
      c2_free (val);
    }
#ifdef BUILD_ADDRESS_BOOK
    else if (streq (key, "addrbook_init")) {
      config->addrbook_init = atoi (val);
      c2_free (val);
    }
#endif
    else if (streq (key, "color_reply_original_message")) {
      sscanf (val, "%dx%dx%d",
		(int *) &config->color_reply_original_message.red,
		(int *) &config->color_reply_original_message.green,
		(int *) &config->color_reply_original_message.blue);
      c2_free (val);
    } else
    if (streq (key, "color_misc_body")) {
      sscanf (val, "%dx%dx%d",
		(int *) &config->color_misc_body.red,
		(int *) &config->color_misc_body.green,
		(int *) &config->color_misc_body.blue);
      c2_free (val);
    } else
    if (streq (key, "cronosII")) {
      if (strne (val+2, VERSION)) {
	old_version = g_strdup (val+2);
      }
    }
    else {
      buffer = g_strdup_printf (_("Unknown command in config file: %s"), key);
      cronos_error (ERROR_INTERNAL, buffer, ERROR_WARNING);
      c2_free (val);
    }

    c2_free (key);
  }
 
  fclose (fd);

  if (old_version) {
    pthread_t thread;
    
    pthread_create (&thread, NULL, PTHREAD_FUNC (c2_version_difference), old_version);
  }
}

static void first_run (char path[]) {
  char *buffer;
  int calc;
  FILE *fd;

  /* If the error is that the file doesn't exists... */
  if (errno != ENOENT) {
    cronos_error (errno, _("Reading the config file"), ERROR_FATAL);
    return;
  }
  /* ...create it */
  /* First create the dir */
  calc = strlen (path)-strlen (CONFIG);
  buffer = (char *) malloc ((calc+1) * sizeof (char));
  strncpy (buffer, path, calc);
  buffer[calc] = '\0';
  
  if ((calc = mkdir (buffer, 0700)) == -1) {
    c2_free (buffer);
    cronos_error (errno, "Creating the directory", ERROR_FATAL);
  }
  
  /* Create the file */
  if ((fd = fopen (path, "w")) == NULL) {
    /* Failed creating the file! */
    cronos_error (errno, "Creating the config file", ERROR_FATAL);
  }
  fprintf (fd, "\nmailbox \"0\r%s\r0\"\n"
		 "mailbox \"2\r%s\r2\"\n"
  		 "mailbox \"1\r%s\r1\"\n"
		 "mailbox \"3\r%s\r3\"\n"
		 "mailbox \"4\r%s\r4\"\n", MAILBOX_INBOX, MAILBOX_OUTBOX,
		 			 MAILBOX_QUEUE, MAILBOX_GARBAGE, MAILBOX_DRAFTS);
  fclose (fd);
  chmod (path, S_IRUSR | S_IWUSR);

  /* Now create the mailbox's dirs */
  /* Check which is the longest name (MAILBOX_*) */
  calc = strlen (MAILBOX_INBOX);
  if (calc < strlen (MAILBOX_OUTBOX)) calc = strlen (MAILBOX_OUTBOX);
  if (calc < strlen (MAILBOX_QUEUE)) calc = strlen (MAILBOX_QUEUE);
  if (calc < strlen (MAILBOX_GARBAGE)) calc = strlen (MAILBOX_GARBAGE);
  if (calc < strlen (MAILBOX_DRAFTS)) calc = strlen (MAILBOX_DRAFTS);
  c2_free (buffer);
  calc += strlen (path)-strlen (CONFIG)+15;
  buffer = (char *) malloc (calc * sizeof (char));
  
  snprintf (buffer, calc, "%s%s/%s.mbx", getenv ("HOME"), ROOT, MAILBOX_INBOX);
  if (mkdir (buffer, 0700) == -1) {
    cronos_error (errno, "Creating the Inbox directory", ERROR_WARNING);
  } else {
    strcat (buffer, "/index");
    fd = fopen (buffer, "w");
    fclose (fd);
    chmod (buffer, S_IRUSR | S_IWUSR);
  }

  snprintf (buffer, calc, "%s%s/%s.mbx", getenv ("HOME"), ROOT, MAILBOX_OUTBOX);
  if (mkdir (buffer, 0700) == -1) {
    cronos_error (errno, "Creating the Outbox directory", ERROR_WARNING);
  } else {
    strcat (buffer, "/index");
    fd = fopen (buffer, "w");
    fclose (fd);
    chmod (buffer, S_IRUSR | S_IWUSR);
  }

  snprintf (buffer, calc, "%s%s/%s.mbx", getenv ("HOME"), ROOT, MAILBOX_QUEUE);
  if (mkdir (buffer, 0700) == -1) {
    cronos_error (errno, "Creating the Queue directory", ERROR_WARNING);
  } else {
    strcat (buffer, "/index");
    fd = fopen (buffer, "w");
    fclose (fd);
    chmod (buffer, S_IRUSR | S_IWUSR);
  }

  snprintf (buffer, calc, "%s%s/%s.mbx", getenv ("HOME"), ROOT, MAILBOX_GARBAGE);
  if (mkdir (buffer, 0700) == -1) {
    cronos_error (errno, "Creating the Garbage directory", ERROR_WARNING);
  } else {
    strcat (buffer, "/index");
    fd = fopen (buffer, "w");
    fclose (fd);
    chmod (buffer, S_IRUSR | S_IWUSR);
  }

  snprintf (buffer, calc, "%s%s/%s.mbx", getenv ("HOME"), ROOT, MAILBOX_DRAFTS);
  if (mkdir (buffer, 0700) == -1) {
    cronos_error (errno, "Creating the Drafts directory", ERROR_WARNING);
  } else {
    strcat (buffer, "/index");
    fd = fopen (buffer, "w");
    fclose (fd);
    chmod (buffer, S_IRUSR | S_IWUSR);
  }

  c2_free (buffer);

  buffer = g_strconcat (getenv ("HOME"), ROOT, "/cronos.rc", NULL);
  if ((fd = fopen (buffer, "w")) == NULL) {
    cronos_error (errno, "Creating the rc file", ERROR_FATAL);
    c2_free (buffer);
    return;
  }
  c2_free (buffer);
  fclose (fd);
}
