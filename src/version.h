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

#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif

typedef struct {
  int major;
  int minor;
  int micro;
  char *patch;
} C2Version;

void
c2_version_difference				(const char *version);

int
c2_version_is_minor_or_equal_to_version 	(C2Version *old_version, const char *str_ask_version);

C2Version *
c2_version_get_version_from_string 		(const char *string);

#ifdef __cplusplus
}
#endif

#endif
