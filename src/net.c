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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <errno.h>

#include "main.h"
#include "net.h"
#include "utils.h"
#include "debug.h"
#include "init.h"

static C2ResolveNode *cache = NULL;

int
sock_printf (int sock, const char *fmt, ...) {
  va_list args;
  char *str;
  int rt;
  
  va_start (args, fmt);
  str = g_strdup_vprintf (fmt, args);
  va_end (args);

  printf ("C: %s\n", str);
  rt = send (sock, str, strlen (str), 0);
  c2_free (str);
  return rt;
}

char *
sock_read (int sock, int *timedout) {
  char str[1024];
  char *ptr;
  char c[1];
#if defined(HAVE_SETSOCKOPT) && defined(SO_RCVTIMEO)
  if (config->timeout) {
    struct timeval tv;
    
    tv.tv_sec = config->timeout;
    tv.tv_usec = 0;
    
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
  }
#endif

  str[1023] = '\0';
  *timedout = FALSE;
  
  for (ptr = str; ptr; ptr++) {
    if (read (sock, c, 1) < 0) {
      if (errno == EINTR) *timedout = TRUE;
      return NULL;
    }
    *ptr = *c;
    if (*c == '\n') break;
  }
  *(++ptr) = '\0';

  printf ("S: %s\n", str);
  return g_strdup (str);
}

C2ResolveNode *
c2_resolve (const char *hostname, char **error) {
  C2ResolveNode *node;
  struct hostent *host;
  struct sockaddr_in sin;
  *error = NULL;

  g_return_val_if_fail (hostname, NULL);

#if USE_CACHE
  if ((node = c2_resolve_cached (hostname)) != NULL) return node;
#endif
  
  node = C2_RESOLVE_NODE_NEW;

  if ((host = gethostbyname (hostname)) == NULL) {
    *error = g_strdup (_("Couldn't resolve the server"));
    return NULL;
  }

  memcpy (&sin.sin_addr, host->h_addr, host->h_length);
  node->ip = g_strdup (inet_ntoa (sin.sin_addr));
  node->hostname = CHAR (hostname);

#if USE_CACHE
  c2_resolve_cache ((const C2ResolveNode *) node);
#endif

  return node;
}

#if USE_CACHE
void
c2_resolve_cache (const C2ResolveNode *node) {
  C2ResolveNode *s = NULL;

  g_return_if_fail (node);

  /* Check if its already cached */
  for (s = cache; s; s = s->next) {
    if (streq (s->hostname, node->hostname)) {
      c2_free (s->ip);
      s->ip = g_strdup (node->ip);
      return;
    }
  }

  if (cache)
    for (s = cache; s->next; s = s->next);

  if (s)
    s->next = (C2ResolveNode *) node;
  else
    cache = (C2ResolveNode *) node;
}

C2ResolveNode *
c2_resolve_cached (const char *hostname) {
  C2ResolveNode *s;

  g_return_val_if_fail (hostname, NULL);
  
  for (s = cache; s; s = s->next)
    if (streq (s->hostname, hostname)) return s;

  return NULL;
}
#endif
