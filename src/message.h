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

/* This functions usually give at least two flags. Functions that the first two arguments are
 * Message *message and char *msg means that the first is a CronosMessage and if the calling
 * function doesn't uses Messages you should pass the message string in the second argument
 * and set message to NULL:
 * Using Message: message_function (message, NULL, ...)
 * Not using Message: message_function (NULL, message, ...)
 */
#ifndef MESSAGES_H
#define MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#ifdef HAVE_CONFIG_H
#  include <config.h>
#  include "mailbox.h"
#else
#  include <cronosII.h>
#endif

typedef struct {
  const char *pos;
  int len;

  char *part;

  char *type, *subtype, *id;
  char *parameter;
  char *disposition;
  char *encoding;
} MimeHash;

#define MIMEHASH(obj)	((MimeHash*) (obj))

typedef struct {
  char *mbox;	/* Freeable string */
  mid_t mid;
  char *message;/* Freeable string */
  char *header; /* Freeable string */
  char *body;	/* Non-freeable string (points to the (first \n\n)+2 of message */
  GList *mime;/* Mime Parts */
} Message;

#define MESSAGE(obj)		((Message*)obj)
#define c2_message_new(message)	{ \
  				  message = g_new0 (Message, 1); \
				  message->mbox = NULL; \
				  message->mid = -1; \
				  message->message = NULL; \
				  message->header = NULL; \
				  message->body = NULL; \
				  message->mime = NULL; \
				}
				

Message *
message_get_message					(const char *mbox, mid_t mid);

Message *
message_get_message_from_file				(const char *filename);

char *
message_get_message_header				(Message *message, const char *msg);

const char *
message_get_message_body				(Message *message, const char *msg);

char *
message_get_header_field				(Message *message, const char *header,
    							 const char *field);

Message *
message_copy						(const Message *message);

void
message_free						(Message *message);

GList *
message_mime_parse					(Message *message, const char *msg);

char *
message_mime_get_parameter_value			(const char *parameter, const char *field);

void
message_mime_parse_content_type				(const char *content_type, char **type,										char **subtype, char **parameter);

MimeHash *
message_mime_get_default_part				(const GList *list);

void
message_mime_get_part					(MimeHash *mime);

char *
encode_base64						(char *data, int *length);

char *
decode_base64						(char *data, int *length);

char *
decode_quoted_printable					(char *message, int *length);

int
message_messages_in_mailbox				(const char *mbox);

#ifdef __cplusplus
}
#endif

#endif
