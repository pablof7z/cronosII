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
#ifndef PREFERENCES_H
#define PREFERENCES_H

#ifdef __cplusplus
extern "C" {
#endif
  
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif

#if HAVE_CONFIG_H
#  include <config.h>
#  include "account.h"
#else
#  include <cronosII.h>
#endif

void
gui_preferences_new							(void);

typedef struct {
  GtkWidget *window;
  GtkWidget *acc_name;
  GtkWidget *per_name;
  GtkWidget *mail_addr;
  C2AccountType type;
  union {
    struct {
      GtkWidget *usr_name;
      GtkWidget *pass;
      GtkWidget *host;
      GtkWidget *host_port;
    } pop;
    struct {
      GtkWidget *file;
    } spool;
  } protocol;
  GtkWidget *smtp;
  GtkWidget *smtp_port;
  GtkWidget *mailbox;
  GtkWidget *keep_copy;
  GtkWidget *use_it;
  GtkWidget *always_append_signature;
  GtkWidget *signature;
} PreferencesAccount;

typedef struct {
  GtkWidget *window;
  GtkWidget *pop_btn;
  GtkWidget *spool_btn;
  GtkWidget *text;
} PreferencesAccountType;

typedef struct {
  GtkWidget *window;
  GtkWidget *font_sel;
  GtkWidget *entry;
} PreferencesFont;

#ifdef __cplusplus
}
#endif

#endif
