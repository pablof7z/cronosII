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
#ifndef ERROR_H
#define ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif
  
#define ERROR_INTERNAL		0
#define ERROR_FATAL		1
#define ERROR_WARNING		2
#define ERROR_MESSAGE		3
#define ERROR_FATAL2		4

void
cronos_error							(int _errno, const char *description,
    								 unsigned short errtype);

void
cronos_status_error						(const gchar *message);

void
cronos_gui_warning						(const char *message);

void
cronos_gui_message						(const gchar *message);

#ifdef __cplusplus
}
#endif

#endif
