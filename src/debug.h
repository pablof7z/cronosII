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
#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif
  
#ifdef HAVE_CONFIG_H
#  include <config.h>
#else 
#  include <cronosII.h>
#endif

#ifdef USE_DEBUG
#  include <assert.h>
#  if USE_GNOME
#    include <gnome.h>
#  else
#    include <gtk/gtk.h>
#  endif
  
#  include "main.h"

#  define DEBUG	TRUE
#  define L printf ("%s:%d\n", __FILE__, __LINE__);
#else
#  define DEBUG FALSE
#  define L ;
#endif

#ifdef __cplusplus
}
#endif

#endif
