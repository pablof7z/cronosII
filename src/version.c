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
#include <gnome.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "version.h"
#include "debug.h"
#include "error.h"
#include "utils.h"
#include "mailbox.h"
#include "init.h"
#include "main.h"

#include "gui-window_main.h"

static void
on_restart_ok_btn_clicked					(GtkWidget *obj, GtkWidget *window);

static void
on_restart_close_btn_clicked					(void);

/**
 * c2_version_difference
 * @version: A pointer to a character object containing the version of the configuration.
 *
 * Handles the differences between the version that saved the configuration and
 * the running version.
 **/
void
c2_version_difference (const char *version) {
  C2Version *old_version;
  char *buf;
  char *path, *strtmp;
  FILE *fd, *tmp;
  
  g_return_if_fail (version);
  
  old_version = c2_version_get_version_from_string (version);
 
  if (c2_version_is_minor_or_equal_to_version (old_version, "0.2.2")) {
    gboolean prompt_to_restart = FALSE;
    
    path = g_strdup_printf ("%s" ROOT "/%s.mbx", getenv ("HOME"), MAILBOX_QUEUE);
    if (!c2_file_exists (path)) {
      if (mkdir (path, 0700) == -1) {
	cronos_error (errno, "Creating the Queue directory", ERROR_WARNING);
      } else {
	c2_free (path);
	path = c2_mailbox_index_path (MAILBOX_QUEUE);
	fd = fopen (path, "w");
	fclose (fd);
	chmod (path, S_IRUSR | S_IWUSR);
	c2_free (path);
	
	/* Update the cronos.conf file */
	path = CONFIG_FILE;
	if ((fd = fopen (path, "r")) != NULL) {
	  strtmp = cronos_tmpfile ();
	  tmp_files = g_slist_append (tmp_files, strtmp);
	  if ((tmp = fopen (strtmp, "w")) != NULL) {
	    for (;;) {
	      if ((buf = fd_get_line (fd)) == NULL) break;
	      if (strneq (buf, "mailbox ", 8)) {
		char *word, *str;
		
		str = str_get_word (1, buf, ' ');
		word = str_get_word (1, str, '\r');
		if (streq (word, MAILBOX_OUTBOX)) {
		  int id = c2_mailbox_next_avaible_id (config->mailbox_head);
		  fprintf (tmp, "%s\n", buf);
		  fprintf (tmp, "mailbox \"%d\r%s\r%d\"\n", id, MAILBOX_QUEUE, id); 
		  prompt_to_restart = TRUE;
		} else fprintf (tmp, "%s\n", buf);
		c2_free (str);
		c2_free (word);
	      }
	      else if (strneq (buf, "cronosII v.", 11)) {
		fprintf (tmp, "cronosII v.%s\n", VERSION);
	      }
	      else fprintf (tmp, "%s\n", buf);
	      c2_free (buf);
	    }
	    fclose (tmp);
	  }
	  fclose (fd);
	  fd_mv (strtmp, path);
	}
	c2_free (path);
      }
    }

    if (prompt_to_restart) {
      GtkWidget *window;
      GtkWidget *label;
      gdk_threads_enter ();
      window = gnome_dialog_new (_("Information"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CLOSE, NULL);
      label = gtk_label_new (_(
		"You have installed a new version of Cronos II.\n"
		"Since this is the first time you run this version\n"
		"the configuration was upgraded.\n"
		"To be able to use the new configuration and the\n"
		"new features in this version you need to restart\n"
		"this software.\n"
		"To do so, click the Close button and restart the application."
				));
      gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), label, TRUE, TRUE, GNOME_PAD_SMALL);
      gtk_widget_show (label);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gnome_dialog_button_connect (GNOME_DIALOG (window), 0, on_restart_ok_btn_clicked, window);
      gnome_dialog_button_connect (GNOME_DIALOG (window), 1, on_restart_close_btn_clicked, NULL);
      gtk_window_set_modal (GTK_WINDOW (window), TRUE);
      gtk_widget_show (window);
      gdk_threads_leave ();
    }
  }
}

static void
on_restart_ok_btn_clicked (GtkWidget *obj, GtkWidget *window) {
  gtk_window_set_modal (GTK_WINDOW (window), FALSE);
  gnome_dialog_close (GNOME_DIALOG (window));
}

static void
on_restart_close_btn_clicked (void) {
  gtk_main_quit ();
}

/**
 * c2_version_is_minor_or_equal_to_version
 * @old_version: The version that is saved in the configuration in an int array.
 * @str_ask_version: The version that the function needs to compare.
 *
 * This function will check if @str_old_version is a minor or equal version
 * than @str_ask_version.
 *
 * Return Value:
 * TRUE if @str_old_version is minor or equal to @str_ask_version else FALSE.
 **/
int
c2_version_is_minor_or_equal_to_version (C2Version *old_version, const char *str_ask_version) {
  C2Version *ask_version;
  gboolean ret = FALSE;
  
  g_return_val_if_fail (old_version, FALSE);
  g_return_val_if_fail (str_ask_version, FALSE);

  ask_version = c2_version_get_version_from_string (str_ask_version);

  if ((old_version->major <= ask_version->major) &&
      (old_version->minor <= ask_version->minor) &&
      (old_version->micro <= ask_version->micro) ||
      (strne (old_version->patch, ask_version->patch))) ret = TRUE;

  c2_free (ask_version->patch);
  c2_free (ask_version);
  return ret;
}

/**
 * c2_version_get_version_from_string
 * @string: A pointer to a character object describing the version number.
 *
 * Learns the version number.
 *
 * Return Value:
 * A C2Version object containing the version information.
 **/
C2Version *
c2_version_get_version_from_string (const char *string) {
  C2Version *version;
  char *buf;
  const char *ptr, *ptr2, *ptr3, *ptr4;
  int found_dots = 0;
  
  g_return_val_if_fail (string, NULL);

  version = g_new0 (C2Version, 1);
  version->patch = NULL;
  
  for (ptr = ptr2 = string;; ptr2++) {
    if (*ptr2 == '.' || *ptr2 == '\0') {
      found_dots++;
      if (*ptr2 != '\0') buf = g_strndup (ptr, ptr2-ptr);
      else buf = g_strdup (ptr);
      ptr = ptr2+1;
      switch (found_dots) {
	case 1:
	  version->major = atoi (buf);
	  break;
	case 2:
	  version->minor = atoi (buf);
	  break;
	case 3:
	  version->micro = atoi (buf);
	  /* Go through the numbers */
	  for (ptr3 = buf; *ptr3 != '\0'; ptr3++) if (!isdigit (*ptr3)) break;
	  if (!ptr3) break;
	  ptr4 = g_strdup (ptr3);
	  c2_free (buf);
	  buf = ptr4;
	default:
	  if (!version->patch)
	    version->patch = g_strdup (buf);
	  else {
	    char *buf2;
	    
	    buf2 = version->patch;
	    version->patch = g_strconcat (buf2, buf, NULL);
	    c2_free (buf2);
	  }
      }
      c2_free (buf);
      if (*ptr2 == '\0') break;
    }
  }
  
  return version;
}
