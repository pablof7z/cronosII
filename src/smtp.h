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
#ifndef SMTP_H
#define SMTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gnome.h>
#ifdef HAVE_CONFIG_H
#  include <config.h>
#  include "message.h"
#  include "utils.h"
#  include "gui-composer.h"
#else
#  include <cronosII.h>
#endif

int persistent_sock;

void * 
smtp_main							(void *helper);

void
smtp_persistent_connect						(Pthread3 *helper);

int
smtp_do_connect							(const char *addr, int port,
    								 GtkWidget *appbar);

void
smtp_do_persistent_disconnect					(gboolean in_gui_thread);

gboolean
smtp_persistent_sock_is_connected				(void);

gboolean
smtp_is_busy							(void);

#ifdef __cplusplus
}
#endif

#endif
