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
#ifndef NET_H
#define NET_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  include <cronosII.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define C2_RESOLVE_NODE_NEW	(g_new0 (C2ResolveNode, 1))

#ifndef MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN 64
#endif

struct _C2ResolveNode {
  char *hostname;
  char *ip;
  struct _C2ResolveNode *next;
};

typedef struct _C2ResolveNode C2ResolveNode;

int
sock_printf							(int sock, const char *fmt, ...);

char *
sock_read							(int sock, int *timedout);

C2ResolveNode *
c2_resolve							(const char *hostname, char **error);

#if USE_CACHE
void
c2_resolve_cache						(const C2ResolveNode *node);

C2ResolveNode *
c2_resolve_cached						(const char *hostname);
#endif

#ifdef __cplusplus
}
#endif

#endif
