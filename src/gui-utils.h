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
#ifndef GUI__UTILS_H
#define GUI__UTILS_H

#ifdef __cplusplus
extern "C" {
#endif
  
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <gdk/gdk.h>
#if HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif

#define CURSOR_BUSY	GDK_WATCH
#define CURSOR_NORMAL	GDK_LEFT_PTR

GdkCursor *cursor_busy;
GdkCursor *cursor_normal;
GdkFont *font_body;
GdkFont *font_read;
GdkFont *font_unread;
GdkBitmap *mask_unread, *mask_mark, *mask_read, *mask_reply, *mask_forward, *mask_mark, *mask_attach;
GdkPixmap *pixmap_unread, *pixmap_mark, *pixmap_read, *pixmap_reply, *pixmap_forward, *pixmap_mark, *pixmap_attach;

void
cronos_gui_init								(void);

gint
c2_progress_set_active							(GtkWidget *progress);

void
c2_progress_set_active_remove						(gint id);

#if FALSE
void
cronos_gui_update_clist_titles						(void);
#endif

void
cronos_gui_set_sensitive						(void);

GtkWidget *
c2_image_new								(const char *file);

#ifdef __cplusplus
}
#endif

#endif
