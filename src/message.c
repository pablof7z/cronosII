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
/* This is an API provided by Cronos II for dealing better with messages and mailboxes */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#if USE_GNOME
#  include <gnome.h>
#else
#  include <gtk/gtk.h>
#endif

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "main.h"
#include "message.h"
#include "error.h"
#include "mailbox.h"
#include "init.h"
#include "utils.h"
#include "debug.h"

static GList *
mime_message_make_hash (const char *body, const char *boundary, const char *parent_boundary);

/**
 * message_get_message
 * @mbox: A pointer to the name of the mailbox where the decired mail is stored.
 * @mid: The Message ID of the decired mail.
 *
 * Loads a message into memory in a Message structure and initializates other values.
 *
 * Return Value:
 * A Message structure with the loaded message or NULL in case of error.
 **/
Message *
message_get_message (const char *mbox, mid_t mid) {
  char *path;
  Message *message;
  
  g_return_val_if_fail (mbox, NULL);
  path = c2_mailbox_mail_path (mbox, mid);
  message = message_get_message_from_file (path);
  if (!message) return NULL;
  message->mbox = g_strdup (mbox);
  message->mid = mid;
  c2_free (path);
  return message;
}

/**
 * message_get_message_from_file
 * @filename: A pointer to a char object which contains the path to a file.
 *
 * Loads the file @filename into a Message object.
 *
 * Return Value:
 * The Message object with the message or NULL in case of error.
 **/
Message *
message_get_message_from_file (const char *filename) {
  const char *path;
  Message *message;
  struct stat *stat_buf;
  int len;
  FILE *fd;

  g_return_val_if_fail (filename, NULL);
  
  path = filename; 
  stat_buf = g_new0 (struct stat, 1);
  
  if (stat (path, stat_buf) < 0 || !stat_buf) {
    cronos_error (errno, _("Stating the file"), ERROR_WARNING);
#if DEBUG
    printf ("Stating the file failed: %s\n", path);
#endif
    return NULL;
  }

  len = ((int) stat_buf->st_size * sizeof (char));
  c2_free (stat_buf);
  
  message = g_new0 (Message, 1);
  message->mbox = NULL;
  message->mid = 0;
  message->message = g_new0 (char, len+1);
  message->header = NULL;
  message->body = NULL;
  message->mime = NULL;

  if ((fd = fopen (path, "r")) == NULL) {
    cronos_error (errno, _("Opening the mail file for retrieving it"), ERROR_WARNING);
    return NULL;
  }

  fread (message->message, sizeof (char), len, fd);
  fclose (fd);

  return message;
}

/**
 * message_get_message_header
 * @message: A pointer to a Message object.
 * @msg: A pointer to a message in case @message isn't specified.
 *
 * Finds the header of a message.
 *
 * Return Value:
 * A freeable pointer to a copy of the header of the message.
 * Note that if @message is specified the return value will
 * also be set in @message->header
 **/
char *
message_get_message_header (Message *message, const char *msg) {
  char *_msg;
  char *buf;
  char *header;
  int len;

  if (!message && !msg) return NULL;
  if (!message)
    _msg = (char *) msg;
  else
    _msg = message->message;

  if (message && message->header) return message->header;

  buf = strstr (_msg, "\n\n");
  if (!buf) return NULL;
  
  len = buf - _msg+1;
  header = g_new0 (char, len+1);
  strncpy (header, _msg, len);

  if (message) message->header = header;

  return header;
}

/**
 * message_get_message_body
 * @message: A pointer to a Message object.
 * @msg: A pointer to a message in case @message isn't specified.
 *
 * Finds the start of the body of a message or of a part of a message.
 *
 * Return Value:
 * A non-freeable pointer to the start of the body of a message or
 * of a part of a message. Note that if @message is specified
 * the value will be also be set in @message->body
 **/
const char *
message_get_message_body (Message *message, const char *msg) {
  char *_msg;
  char *body = NULL;

  if (!message && !msg) return NULL;
  if (!message)
    _msg = (char *) msg;
  else
    _msg = message->message;
 
  if (message && message->body) return message->body;

  body = strstr (_msg, "\n\n");
  if (!body) return NULL;
  body += 2;
  if (message) message->body = body;

  return body;
}

/**
 * message_get_header_field
 * @message: A pointer to a Message object.
 * @header: A pointer to the header of a mail in case message isn't specified.
 * @field: The desired field of the header.
 *
 * Searchs for a header field in a message or in a header.
 * 
 * Return Value: A freeable pointer to the content of the header field or NULL
 * in case it wasn't found.
 **/
char *
message_get_header_field (Message *message, const char *header, const char *field) {
  char *tmp_found, *frmptr;
  char *work_header;
  char *rfld, *toptr;
  gboolean done = FALSE;
  int i, len, pos;
  const char *ptr_body;

  g_return_val_if_fail (!(!message && !header), NULL);
  g_return_val_if_fail (field, NULL);
  
  /* Get the header */
  if (message) {
    if (!message->header) message_get_message_header (message, NULL);
    work_header = g_strdup (message->header);
  } else work_header = g_strdup (header);
  if (!work_header) return NULL;

  /* Find the start of the body */
  ptr_body = message_get_message_body (NULL, work_header);

  /* Find the field in the header */
  tmp_found = CHAR (strcasestr (work_header, field));
  if (!tmp_found) return NULL;
  if (ptr_body && tmp_found > ptr_body) return NULL;
  tmp_found += strlen (field);

  pos = strlen (work_header) - strlen (tmp_found);
  tmp_found = CHAR ((message ? message->header : header)+pos);

  /* Go through the spaces */
  while (*tmp_found == ' ') tmp_found++;

  for (len = 0, frmptr = tmp_found; !done;) {
    /* Get the line */
    for (; frmptr; frmptr++) {
      if (*frmptr == '\0') break;
      if (*frmptr == '\n') break;
      len++;
    }

    /* Got a \n */
    frmptr++;
    if ((*frmptr == '\t') || (*frmptr == ' ')) {
      len++;
      frmptr++;
      /* Go through the spaces */
      for (; *frmptr == ' '; frmptr++);
      for (; frmptr; frmptr++, len++) {
	if (*frmptr == '\0') break;
	if (*frmptr == '\n') break;
      }
    } else done = TRUE;
  }

  rfld = g_new0 (char, len+1);

  for (i = 0, toptr = rfld, frmptr = tmp_found; i <= len;) {
    for (; frmptr && i <= len; frmptr++, i++) {
      if (*frmptr == '\0') goto out;
      if (*frmptr == '\n') break;
      *(toptr++) = *frmptr;
    }

    frmptr++;
    if ((*frmptr == '\t') || (*frmptr == ' ')) {
      i++;
      frmptr++;
      *(toptr++) = ' ';
      for (; *frmptr == ' '; frmptr++);
      for (; frmptr; frmptr++, i++) {
	if (*frmptr == '\0') goto out;
	if (*frmptr == '\n') break;
	*(toptr++) = *frmptr;
      }
    } else goto out;
  }
out:
  *(toptr++) = '\0';
  c2_free (work_header);

  return rfld;
}

/**
 * message_copy
 * @message: A pointer to a Message object.
 *
 * Copies a message.
 *
 * Return Value:
 * A newly allocated Message object with a copy of @message
 **/
Message *
message_copy (const Message *message) {
  Message *new;
  MimeHash *mime;
  GList *list;

  g_return_val_if_fail (message, NULL);
  
  new = g_new0 (Message, 1);
  new->mbox = g_strdup (message->mbox);
  new->mid = message->mid;
  new->message = g_strdup (message->message);
  new->header  = g_strdup (message->header);
  if (message->body) new->body = new->message + (message->body - message->message);
  else new->body = NULL;
  new->mime = NULL;

  /* Don't use g_list_copy: it doesn't copy ->data */
  for (list = message->mime; list != NULL; list = list->next) {
    mime = g_new0 (MimeHash, 1);
    mime->pos = new->message + (message->message - MIMEHASH (list->data)->pos);
    mime->len = MIMEHASH (list->data)->len;
    mime->part = g_strdup (MIMEHASH (list->data)->part);
    mime->type = g_strdup (MIMEHASH (list->data)->type);
    mime->subtype = g_strdup (MIMEHASH (list->data)->subtype);
    mime->id = g_strdup (MIMEHASH (list->data)->id);
    mime->parameter = g_strdup (MIMEHASH (list->data)->parameter);
    mime->disposition = g_strdup (MIMEHASH (list->data)->disposition);
    mime->encoding = g_strdup (MIMEHASH (list->data)->encoding);
    new->mime = g_list_append (new->mime, mime);
  }

  return new;
}

/**
 * message_free
 * @message: A pointer to a Message object.
 *
 * Free @message.
 **/
void
message_free (Message *message) {
  GList *s;
  MimeHash *mime;

  g_return_if_fail (message);

  c2_free (message->mbox);
  c2_free (message->message);
  message->body = NULL;
  c2_free (message->header);
  for (s = message->mime; s != NULL; s = s->next) {
    mime = MIMEHASH (s->data);
    if (!mime) continue;
    mime->pos = NULL;
    c2_free (mime->part);
    c2_free (mime->type);
    c2_free (mime->subtype);
    c2_free (mime->id);
    c2_free (mime->parameter);
    c2_free (mime->disposition);
    c2_free (mime->encoding);
    c2_free (mime);
    s->data = NULL;
  }
  g_list_free (message->mime);
  c2_free (message);
}

/**
 * message_mime_parse
 * @message: Pointer to a Message object.
 * @msg: Pointer to a message string if Message isn't specified.
 *
 * Description:
 * Parse a message to recognize the different MIME part.
 *
 * Return Value:
 * A list of mime parts. Note that if message is specified
 * the return value will be set in message->mime too.
 **/
GList *
message_mime_parse (Message *message, const char *msg) {
  char *mime_version;
  char *content_type, *type, *subtype, *parameter;
  const char *boundary;
  char *buf;
  GList *head = NULL;
  MimeHash *mime = NULL;
  
  g_return_val_if_fail (!(!message && !msg), NULL);

  /* Detect MIME-Version */
  mime_version = message_get_header_field (message, msg, "\nMIME-Version:");
  if (!mime_version) return NULL;

  if (mime_version && strnne (mime_version, "1.0", 3)) {
    g_warning (_("This message uses a MIME-Version unsupported by Cronos II: %s.\n"
	         "This might affect the interpretation of it."), mime_version);
  } else if (!mime_version) {
not_mime:
    mime = g_new0 (MimeHash, 1);
    mime->pos = message_get_message_body (message, msg);
    mime->len = strlen (mime->pos);

    mime->part = NULL;

    mime->type = g_strdup ("text");
    mime->subtype = g_strdup ("plain");
    mime->id = NULL;
    mime->parameter = NULL;
    mime->disposition = NULL;
    mime->encoding = g_strdup ("7bit");
    
    c2_free (mime_version);
    head = g_list_append (head, mime);
    return head;
  }
  c2_free (mime_version);

  /* Now that we know that this is a MIME message lets learn the Content-Type */
  content_type = message_get_header_field (message, msg, "\nContent-Type:");

  if (!content_type) {
    type = g_strdup ("text");
    subtype = g_strdup ("plain");
    parameter = NULL;
  } else {
    message_mime_parse_content_type (content_type, &type, &subtype, &parameter);
  }
  c2_free (content_type);

  if (streq (type, "multipart")) {
    if (!parameter) {
      g_warning (_("This message claims to be multipart but it seems that it's broken.\n"));
      goto not_mime;
    }
    boundary = message_mime_get_parameter_value (parameter, "boundary");
    if (!boundary) {
      g_warning (_("This message claims to be multipart but it seems that it's broken.\n"));
      goto not_mime;
    }
    head = mime_message_make_hash (message_get_message_body (message, msg), boundary, NULL);
    if (!head) goto not_mime;
  } else {
    mime = g_new0 (MimeHash, 1);
    mime->pos = message_get_message_body (message, msg);
    mime->len = strlen (mime->pos);
    mime->part = NULL;
    mime->type = type;
    mime->subtype = subtype;
    buf = message_get_header_field (message, msg, "\nContent-ID:");
    if (buf) mime->id = str_strip_enclosed (buf, '<', '>');
    else mime->id = NULL;
    c2_free (buf);
    mime->parameter = parameter;
    mime->disposition = message_get_header_field (message, msg, "\nContent-Disposition:");
    mime->encoding = message_get_header_field (message, msg, "\nContent-Transfer-Encoding:");
    head = g_list_append (head, mime);
  }
 
  if (message) message->mime = head;
  return head;
}

/**
 * mime_message_make_hash
 * @body: A pointer to the body of the next part.
 * @boundary: Boundary being used.
 * @parent_boundary: Boundary of the parent message.
 *
 * Recursively parse multipart messages. Searching for @boundary and adding each
 * part to a list to be returned. If @parent_boundary is found immediately return.
 *
 * Return Value:
 * A GList linked list of the different parts of the message.
 **/
static GList *
mime_message_make_hash (const char *body, const char *boundary, const char *parent_boundary) {
  GList *head = NULL;
  GList *child;
  MimeHash *mime = NULL;
  char *ptr = NULL, *end = NULL;
  char *content_type = NULL, *type = NULL, *subtype = NULL,
       *parameter = NULL;
  const char *myboundary = NULL;
  char *buf;
  gboolean end_reached = FALSE;

  g_return_val_if_fail (body, NULL);
  g_return_val_if_fail (boundary, NULL);

  for (ptr = CHAR (body); ptr != NULL && (ptr = strstr (ptr, boundary)) != NULL;) {
    if (end_reached) break;
    /* Check if is the ending boundary */
    end = strstr ((ptr+strlen (boundary)), boundary);
    if (end && strneq ((end+(strlen (boundary))), "--", 2)) {
      end_reached = TRUE;
    }
    if (!end && parent_boundary) {
      /* Check if I found the parent boundry*/
      end = strstr ((ptr+strlen (boundary)), parent_boundary);
      if (end) end_reached = TRUE;
    }
      
    content_type = message_get_header_field (NULL, ptr, "\nContent-Type:");
    
    if (content_type) {
      message_mime_parse_content_type (content_type, &type, &subtype, &parameter);
    }
    c2_free (content_type);

    if (streq (type, "multipart")) {
      char *pos;
      if (!parameter) {
	g_warning (_("This message claims to be multipart but it seems that it's broken.\n"));
	goto out;
      }
      
      myboundary = message_mime_get_parameter_value (parameter, "boundary");
      if (!myboundary) {
	g_warning (_("This message claims to be multipart but it seems that it's broken.\n"));
	goto out;
      }

      pos = strstr (ptr, "\n\n") + 2;
      child = mime_message_make_hash (pos, myboundary, boundary);
      head = g_list_concat (head, child);
      c2_free (parameter);
      c2_free (type);
      c2_free (subtype);
    } else {
      mime = g_new0 (MimeHash, 1);
      mime->pos = strstr (ptr, "\n\n") + 2;
      if (end) mime->len = (int)end - (int)mime->pos - 3; /* 3: '\n--' */
      else mime->len = strlen (mime->pos);
      mime->part = NULL;
      mime->type = type;
      mime->subtype = subtype;
      buf = message_get_header_field (NULL, ptr, "\nContent-ID:");
      if (buf) mime->id = str_strip_enclosed (buf, '<', '>');
      else mime->id = NULL;
      c2_free (buf);
      mime->parameter = parameter;
      mime->disposition = message_get_header_field (NULL, ptr, "\nContent-Disposition:");
      mime->encoding = message_get_header_field (NULL, ptr, "\nContent-Transfer-Encoding:");
      head = g_list_append (head, mime);
    } 

out:
    if (end)
      ptr = end-1;
    else
      ptr += strlen (boundary);
  }

  return head;
}

/**
 * message_mime_get_parameter_value
 * @parameter: A pointer to the parameter of a message part.
 * @field: The desired field.
 *
 * Looks in @parameter for the field @field.
 *
 * Return Value:
 * A freeable string with the content of the field or NULL
 * if it couldn't be found.
 **/
char *
message_mime_get_parameter_value (const char *parameter, const char *field) {
  const char *ptr;
  char *ptr2;
  
  if (!parameter) return NULL; /* This isn't a mistake */
  g_return_val_if_fail (field, NULL);

  ptr = strcasestr (parameter, field);
  if (!ptr) return NULL;
  ptr += strlen (field);
  while (*ptr != '=' && *ptr != '\0') ptr++;
  if (!ptr) return NULL;
  ptr++;
  ptr2 = g_strdup (ptr);
  str_unquote (&ptr2);
  if (*ptr2 == '"') ptr2++;
  return ptr2;
}

/**
 * message_mime_parse_content_type
 * @content_type: A pointer to a string in Content-Type format.
 * @type: This variable is used to return the type of MIME part in a freeable string.
 * @subtype: This variable is used to return the subtype of MIME part in a freeable string.
 * @parameter: This variable is used to return the parameter of MIME part in a freeable string.
 *
 * Investigate the @content_type variable and returns the type, subtype and
 * parameter of the message part.
 **/
void
message_mime_parse_content_type (const char *content_type, char **type, char **subtype, char **parameter) {
  const char *ptr_start, *ptr_end;
  
  *type = NULL;
  *subtype = NULL;
  *parameter = NULL;

  g_return_if_fail (content_type);

  /* Move ptr_start */
  ptr_start = content_type;

  /* Move ptr_end */
  for (ptr_end = ptr_start; *ptr_end != '/' && *ptr_end != '\0'; ptr_end++);
  if (!ptr_end) return;
  
  *type = g_new0 (char, (ptr_end - ptr_start)+1);
  strncpy (*type, ptr_start, (ptr_end - ptr_start));
  
  /* Move ptr_start */
  ptr_start = ptr_end+1;

  /* Move ptr_end */
  for (ptr_end = ptr_start; *ptr_end != ';' && *ptr_end != '\0'; ptr_end++);

  *subtype = g_new0 (char, (ptr_end - ptr_start)+1);
  strncpy (*subtype, ptr_start, (ptr_end - ptr_start));
  if (!ptr_end) return;

  /* Move ptr_start */
  for (ptr_start = ptr_end+1; *ptr_start == ' '; ptr_start++);

  /* Move ptr_end */
  for (ptr_end = ptr_start; *ptr_end != '\0'; ptr_end++);

  *parameter = g_new0 (char, (ptr_end - ptr_start)+1);
  strncpy (*parameter, ptr_start, (ptr_end - ptr_start));
}

/**
 * message_mime_get_default_part
 * @list = A pointer to a GList linked list of MimeHash objects.
 *
 * Checks which should be the default part to be displayed
 * according to config->default_mime_part.
 *
 * Return Value:
 * The mime part that should be displayed by default.
 **/
MimeHash *
message_mime_get_default_part (const GList *list) {
  const GList *s;
  MimeHash *mime;

  g_return_val_if_fail (list, NULL);
  
  for (s = list; s != NULL; s = s->next) {
    mime = MIMEHASH (s->data);
    if (streq (mime->type, "text")) {
      if (config->default_mime_part == INIT_DEFAULT_MIME_PART_PLAIN) {
	if (streq (mime->subtype, "plain")) return mime;
      } else if (config->default_mime_part == INIT_DEFAULT_MIME_PART_HTML) {
	if (streq (mime->subtype, "html")) return mime;
      }
    }
  }

  return MIMEHASH (list->data);
}

/**
 * message_mime_get_part
 * @mime: A pointer to a MimeHash object.
 *
 * Assign @mime->part to the decoded content
 * of the part.
 **/
void
message_mime_get_part (MimeHash *mime) {
  char *ptr;
  
  g_return_if_fail (mime);
  if (mime->part) return;
  
  ptr = g_new0 (char, mime->len+1);
  strncpy (ptr, mime->pos, mime->len);

  if (streq (mime->encoding, "base64")) {
    mime->part = decode_base64 (ptr, &mime->len);
  }
  else if (streq (mime->encoding, "quoted-printable")) {
    mime->part = decode_quoted_printable (ptr, &mime->len);
  } else
    mime->part = ptr;
}

static gchar base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/**
 * encode_base64
 * @data: A pointer to the part.
 * @length: A pointer to the length of the part.
 *
 * Encodes a part in base64.
 *
 * Return Value:
 * The encoded part.
 **/
char *
encode_base64(char *data, int *length) {
  char *encoded, *index, buffer[3];
  int pos, len;
  
  /* Invalid inputs will cause this function to crash.  Return an error. */
  if (!length || !data)
    return 0;
  
  encoded = g_malloc0(sizeof(char)*(gint)(*length * 1.40)); /* it gets 33% larger */
  pos = 0;
  len = 0;
  index = data;
  while (index-data < *length) {
    /* There was a buffer overflow with the memcpy.  If length % 3 were not 0, it would read into someone
     * else's memory, or possibly just garbage.  It can mess up the last few bits of the base64 conversion. */
    if ( index-data+3 <= *length ) {
      memcpy(buffer, index, 3);
      *(encoded+pos)   = base64_chars[(buffer[0] >> 2) & 0x3f];
      *(encoded+pos+1) = base64_chars[((buffer[0] << 4) & 0x30) | ((buffer[1] >> 4) & 0xf)];
      *(encoded+pos+2) = base64_chars[((buffer[1] << 2) & 0x3c) | ((buffer[2] >> 6) & 0x3)];
      *(encoded+pos+3) = base64_chars[buffer[2] & 0x3f];
    } else if (index-data+2 == *length) {
      memcpy(buffer, index, 2);
      *(encoded+pos)   = base64_chars[(buffer[0] >> 2) & 0x3f];
      *(encoded+pos+1) = base64_chars[((buffer[0] << 4) & 0x30) | ((buffer[1] >> 4) & 0xf)];
      *(encoded+pos+2) = base64_chars[(buffer[1] << 2) & 0x3c];
      *(encoded+pos+3) = '=';
    } else if (index-data+1 == *length) {
      memcpy(buffer, index, 1);
      *(encoded+pos)   = base64_chars[(buffer[0] >> 2) & 0x3f];
      *(encoded+pos+1) = base64_chars[(buffer[0] << 4) & 0x30];
      *(encoded+pos+2) = '=';
      *(encoded+pos+3) = '=';
    } else {
      g_error("encode_base64(): corrupt data");
      return NULL;
    }
    
    len += 4;
    
    /* base64 can only have 76 chars per line */
    if (len >= 76) {
      *(encoded + pos + 4) = '\n';
      pos++;
      len = 0;
    }
    
    pos += 4;
    index += 3;
  }
  
  /* if there were less then a full triplet left, we pad the remaining
   * encoded bytes with = */
  /*
   * if (*length % 3 == 1) {
   * *(encoded+pos-1) = '=';
   * *(encoded+pos-2) = '=';
   * }
   * if (*length % 3 == 2) {
   * *(encoded+pos-1) = '=';
   * }*/
  
  *(encoded+pos) = '\n';
  *(encoded+pos+1) = '\0';
  
  *length = strlen(encoded);
  
  return encoded;
}

/**
 * decode_base64
 * @data: A pointer to the encoded part.
 * @length: A pointer to the length of the encoded part.
 *
 * Decodes a part in base64.
 *
 * Return Value:
 * The decoded part.
 **/
char *
decode_base64 (char *data, int *length) {
  /* This function is based in decode_base64 by Jeffrey Stedfast */
  char *output, *workspace, *p;
  gulong pos = 0;
  gint i, a[4], len = 0;
  
  g_return_val_if_fail (data, NULL);
  
  workspace = CHAR (g_malloc0(sizeof(gchar) * ((gint)(strlen(data) / 1.33) + 2)));
  
  while (*data && len < *length) {
    for (i = 0; i < 4; i++, data++, len++) {
      if ((p = strchr (base64_chars, *data)))
	a[i] = (gint)(p - base64_chars);
      else
	i--;
    }
    
    workspace[pos]     = (((a[0] << 2) & 0xfc) | ((a[1] >> 4) & 0x03));
    workspace[pos + 1] = (((a[1] << 4) & 0xf0) | ((a[2] >> 2) & 0x0f));
    workspace[pos + 2] = (((a[2] << 6) & 0xc0) | (a[3] & 0x3f));
    
    if (a[2] == 64 && a[3] == 64) {
      workspace[pos + 1] = 0;
      pos -= 2;
    } else {
      if (a[3] == 64) {
	workspace[pos + 2] = 0;
	pos--;
      }
    }
    pos += 3;
  }
  
  output = g_malloc0(pos + 1);
  memcpy(output, workspace, pos);
  
  *length = pos;
  
  c2_free (workspace);
  
  return output;
}

/**
 * decode_quoted_printable
 * @data: A pointer to the encoded part.
 * @length: A pointer to the length of the encoded part.
 *
 * Decodes a part in quoted_printable.
 *
 * Return Value:
 * The decoded part.
 **/
char *
decode_quoted_printable (char *message, int *length) {
  /* This function is based in decode_quoted_printable by Jeffrey Stedfast */
  char *buffer, *index, ch[2];
  gint i = 0, temp;
  
  buffer = g_malloc0(*length + 1);
  index = message;
  
  while (index - message < *length) {
    if (*index == '=') {
      index++;
      if (*index != '\n') {
	sscanf(index, "%2x", &temp);
	sprintf(ch, "%c", temp);
	buffer[i] = ch[0];
      } else
	buffer[i] = index[1];
      i++;
      index += 2;
    } else {
      buffer[i] = *index;
      i++;
      index++;
    }
  }
  buffer[i] = '\0';
  
  *length = strlen(buffer);
  return buffer;
}

/**
 * message_messages_in_mailbox
 * @mbox: A pointer to the name of the target mailbox.
 *
 * Investigate the number of mails that there're in @mbox.
 *
 * Return Value:
 * The number of mails in @mbox.
 **/
int
message_messages_in_mailbox (const char *mbox) {
  char *path = c2_mailbox_index_path (mbox);
  char *line;
  FILE *fd;
  int messages = 0;

  g_return_val_if_fail (mbox, -1);
  g_return_val_if_fail (path, -1);

  if ((fd = fopen (path, "r")) == NULL) {
    cronos_error (errno, _("Opening the main DB file for reading the number of messages in it"),
    				ERROR_WARNING);
    c2_free (path);
    return -1;
  }
  c2_free (path);

  for (;;) {
    if ((line = fd_get_line (fd)) == NULL) break;

    if (line) messages++;
    c2_free (line);
  }

  fclose (fd);

  return messages;
}
