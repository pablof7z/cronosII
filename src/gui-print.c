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

#include <errno.h>
#include "main.h"
#include "gui-print.h"
#include "gui-window_main.h"
#include "utils.h"
#include "debug.h"
#include "error.h"
#include "message.h"

#ifndef HAVE_GNOME_PRINT
static void cronos_print_ok_btn_clicked (GtkWidget *w, gpointer data) {
  CronosPrint *cp;

  cp = (CronosPrint *) data;
  cp->ok = TRUE;

  gtk_main_quit ();
}

static void cronos_print_cancel_btn_clicked (GtkWidget *w, gpointer data) {
  CronosPrint *cp;

  cp = (CronosPrint *) data;
  cp->ok = FALSE;
  gtk_widget_destroy (cp->window);

  gtk_main_quit ();
}

static void ask_print (CronosPrint *cp) {
  GtkWidget *vbox;
  GtkWidget *btn;
  GtkWidget *hsep;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *table;
  GtkAdjustment *adj;
  
  cp->window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_widget_set_usize (cp->window, 300, 90);
  gtk_window_set_title (GTK_WINDOW (cp->window), _("Print"));
  gtk_window_set_policy (GTK_WINDOW (cp->window), FALSE, FALSE, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (cp->window), 5);
  gtk_window_set_modal (GTK_WINDOW (cp->window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (cp->window), GTK_WINDOW (WMain->window));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (cp->window), vbox);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 1);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  
  label = gtk_label_new (_("Printing command: "));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
  gtk_widget_show (label);

  label = gtk_label_new (_("Number of copies: "));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
  gtk_widget_show (label);

  cp->cmnd = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table), cp->cmnd, 1, 2, 0, 1);
  gtk_widget_show (cp->cmnd);
  gtk_entry_set_text (GTK_ENTRY (cp->cmnd), "lpr %f");

  adj = (GtkAdjustment *) gtk_adjustment_new (1, 1, 100, 1, 100, 0);
  cp->copies = gtk_spin_button_new (adj, 1, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), cp->copies, 1, 2, 1, 2);
  gtk_widget_show (cp->copies);

  hsep = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, TRUE, 3);
  gtk_widget_show (hsep);

  hbox = gtk_hbox_new (TRUE, 0),
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      		GTK_SIGNAL_FUNC (cronos_print_ok_btn_clicked), (gpointer) cp);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
  gtk_box_pack_start (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      		GTK_SIGNAL_FUNC (cronos_print_cancel_btn_clicked), (gpointer) cp);

  gtk_widget_show (cp->window);

  gtk_main ();
}

void
cronos_print (const Message *message, const char *msg) {
  CronosPrint *cp;
  char *tmp;
  char *from, *account, *date, *subject;
  char *cmnd;
  char *buf;
  int copies;
  int i;
  FILE *fd;

  g_return_if_fail (message);
  
  cp = g_new0 (CronosPrint, 1);
  cp->ok = FALSE;
  ask_print (cp);

  if (!cp->ok) {
    c2_free (cp);
    return;
  }

#if USE_PLUGINS
  if (message)
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_OPEN, message, "print", NULL, NULL, NULL);
#endif

  tmp = cronos_tmpfile ();
  cmnd = g_strdup (gtk_entry_get_text (GTK_ENTRY (cp->cmnd)));
  copies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (cp->copies));

  gtk_widget_destroy (cp->window);
  c2_free (cp);

  tmp_files = g_slist_append (tmp_files, tmp);
  if ((fd = fopen (tmp, "w")) == NULL) {
    cronos_error (errno, _("Opening the temporary file"), ERROR_WARNING);
    c2_free (cmnd);
    return;
  }
 
  from = message_get_header_field (MESSAGE (message), NULL, "From:");
  account = message_get_header_field (MESSAGE (message), NULL, "\nX-CronosII-Account:");
  date = message_get_header_field (MESSAGE (message), NULL, "\nDate:");
  subject = message_get_header_field (MESSAGE (message), NULL, "\nSubject:");
  fprintf (fd, _("From:	   %s\n"
		 "Account: %s\n"
		 "Date:    %s\n"
		 "Subject: %s\n\n%s"),
      		from, account, date, subject, msg);

  c2_free (from);
  c2_free (account);
  c2_free (date);
  c2_free (subject);
  
  fclose (fd);

  buf = str_replace (cmnd, "%f", tmp);
  c2_free (cmnd);
  cmnd = buf;
  for (i = 1; i <= copies; i++) {
    buf = cronos_system (cmnd);
    if (buf) {
      gnome_appbar_set_status (GNOME_APPBAR (WMain->appbar), buf);
      break;
    }
  }
}
#else

/* This set of functions are based on Balsa 1.0.0 */
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

typedef struct {
  const GnomePaper *paper;
  GnomePrintMaster *master;
  GnomePrintContext *pc;

   gchar *font_name;
   gint font_size;
   float font_char_width;
   float font_char_height;

   gint pages;
   float page_width, page_height;
   float margin_top, margin_bottom, margin_left, margin_right;
   float printable_width, printable_height;
   float header_height, header_label_width;
   gint total_lines;
   gint lines_per_page, chars_per_line;
   
   /* wrapping */
   gint tab_width;
   
   const Message *message;
   const char *msg;
   char *ptr;
} C2PrintInfo;

static guint linecount(const gchar * str)
{
    gint cnt = 1;
    if (!str)
	return 0;
    while (*str) {
	if (*str == '\n' && str[1])
	    cnt++;
	str++;
    }
    return cnt;
}

static C2PrintInfo *c2_print_info_new (const char *paper, const Message *message,
    					const char *msg, GnomePrintDialog *dlg) {
  C2PrintInfo *info = g_new0 (C2PrintInfo, 1);

  info->paper = gnome_paper_with_name(paper);
  info->master = gnome_print_master_new_from_dialog(dlg);
  info->pc = gnome_print_master_get_context(info->master);

  info->font_name = g_strdup ("Courier");
  info->font_size = 10;
  info->font_char_width = 0.0808 * 72;
  info->font_char_height = 0.1400 * 72;

  info->page_width = gnome_paper_pswidth(info->paper);
  info->page_height = gnome_paper_psheight(info->paper);

  info->margin_top = 0.75 * 72;	/* printer's margins, not page margin */
  info->margin_bottom = 0.75 * 72;	/* get it from gnome-print            */
  info->margin_left = 0.75 * 72;
  info->margin_right = 0.75 * 72;
  info->printable_width = info->page_width - info->margin_left - info->margin_right;
  info->printable_height = info->page_height - info->margin_top - info->margin_bottom;

  info->chars_per_line = (gint) (info->printable_width / info->font_char_width);
  info->tab_width = 8;

  info->message = message;
  info->msg = msg;

  info->total_lines = linecount(msg);
  info->lines_per_page = (gint) ((info->printable_height - info->header_height) / info->font_char_height);
  info->pages = ((info->total_lines - 1) / info->lines_per_page) + 1;
  
  return info;
}

static int c2_print_header (C2PrintInfo *info) {
  char *from, *account, *to = NULL, *date, *cc, *subject;
  GnomeFont *font;
  int width;
  int line = 0;
  char *strfrom		= _("From:"), /* For i18n */
       *straccount	= _("Account:"),
       *strto		= _("To:"),
       *strdate		= _("Date:"),
       *strcc		= _("CC:"),
       *strsubject	= _("Subject:");
  int lenfrom		= strlen (strfrom),
      lenaccount	= strlen (straccount),
      lento		= strlen (strto),
      lendate		= strlen (strdate),
      lencc		= strlen (strcc),
      lensubject	= strlen (strsubject);
  int largest = lenfrom;
  char *largestptr = strfrom;

  if (largest < lenaccount) { largest = lenaccount; largestptr = straccount; }
  if (largest < lento) { largest = lento; largestptr = strto; }
  if (largest < lendate) { largest = lendate; largestptr = strdate; }
  if (largest < lencc) { largest = lencc; largestptr = strcc; }
  if (largest < lensubject) { largest = lensubject; largestptr = strsubject; }

  from = message_get_header_field ((Message*)info->message, NULL, "From:");
  account = message_get_header_field ((Message*)info->message, NULL, "X-CronosII-Account:");
  cc = message_get_header_field ((Message*)info->message, NULL, "CC:");
  date = message_get_header_field ((Message*)info->message, NULL, "Date:");
  subject = message_get_header_field ((Message*)info->message, NULL, "Subject:");
  if (!account) to = message_get_header_field ((Message*)info->message, NULL, "To:");
  
  if (from && !str_utf8_validate (from, -1, NULL)) str_strip_non_utf8 (from);
  if (cc && !str_utf8_validate (cc, -1, NULL)) str_strip_non_utf8 (cc);
  if (date && !str_utf8_validate (date, -1, NULL)) str_strip_non_utf8 (date);
  if (subject && !str_utf8_validate (subject, -1, NULL)) str_strip_non_utf8 (subject);
  if (account) {
    if (!str_utf8_validate (account, -1, NULL)) str_strip_non_utf8 (account);
  } else {
    if (to) {
      if (!str_utf8_validate (to, -1, NULL)) str_strip_non_utf8 (to);
    }
  }

  font = gnome_font_new ("Helvetica", 11);
  gnome_print_setfont(info->pc, font);
  width = gnome_font_get_width_string(font, largestptr);
  width+= gnome_font_get_width_string(font, "        ");
  
  if (from) {
    gnome_print_moveto(info->pc, info->margin_left,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line));
    gnome_print_show (info->pc, strfrom);
    gnome_print_moveto(info->pc, info->margin_left+width,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line++));
    gnome_print_show (info->pc, from);
  }
  
  if (account) {
    gnome_print_moveto(info->pc, info->margin_left,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line));
    gnome_print_show (info->pc, straccount);
    gnome_print_moveto(info->pc, info->margin_left+width,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line++));
    gnome_print_show (info->pc, account);
  } else {
    if (to) {
      gnome_print_moveto(info->pc, info->margin_left,
	  info->page_height - info->margin_top - info->header_height
	  - (info->font_char_height * line));
      gnome_print_show (info->pc, strto);
      gnome_print_moveto(info->pc, info->margin_left+width,
	  info->page_height - info->margin_top - info->header_height
	  - (info->font_char_height * line++));
      gnome_print_show (info->pc, to);
    }
  }
  
  if (cc) {
    gnome_print_moveto(info->pc, info->margin_left,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line));
    gnome_print_show (info->pc, strcc);
    gnome_print_moveto(info->pc, info->margin_left+width,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line++));
    gnome_print_show (info->pc, cc);
  }

  if (date) {
    gnome_print_moveto(info->pc, info->margin_left,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line));
    gnome_print_show (info->pc, strdate);
    gnome_print_moveto(info->pc, info->margin_left+width,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line++));
    gnome_print_show (info->pc, date);
  }

  if (subject) {
    gnome_print_moveto(info->pc, info->margin_left,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line));
    gnome_print_show (info->pc, strsubject);
    gnome_print_moveto(info->pc, info->margin_left+width,
	info->page_height - info->margin_top - info->header_height
	- (info->font_char_height * line++));
    gnome_print_show (info->pc, subject);
  }

  line+=2;
  return line;
}

static void c2_print_message (C2PrintInfo *info) {
  guint current_page, current_line;
  GnomeFont *font;
  char *buf;
  char *buf2;
  const char *ptr;
  int width;

  g_return_if_fail (info);
  font = gnome_font_new (info->font_name, info->font_size);

  ptr = info->msg;
  
  for (current_page = 1; current_page <= info->pages; current_page++) {
    buf = g_strdup_printf (_("Page %d of %d"), current_page, info->pages);
    gnome_print_beginpage (info->pc, buf);
    c2_free (buf);
    if (current_page == 1) current_line = c2_print_header (info);
    gnome_print_setfont (info->pc, font);

    for (; current_line < info->lines_per_page; current_line++) {
      if ((buf = str_get_line (ptr)) == NULL) break;
      if (strlen (buf) > info->chars_per_line) {
	buf2 = g_new (char, info->chars_per_line+1);
	strncpy (buf2, buf, info->chars_per_line);
	buf2[info->chars_per_line] = '\0';
	c2_free (buf);
	ptr += strlen (buf2);
      } else {
	buf2 = buf;
	ptr += strlen (buf2);
	buf2 = g_strchomp (buf2);
      }
      if (!str_utf8_validate (buf2, -1, NULL)) str_strip_non_utf8 (buf2);
      gnome_print_moveto(info->pc, info->margin_left,
		       info->page_height - info->margin_top - info->header_height
		       - (info->font_char_height * current_line));
      gnome_print_show(info->pc, buf2);
      c2_free (buf2);
    }
    buf = g_strdup_printf (_("Page %d of %d"), current_page, info->pages);
    width = gnome_font_get_width_string (font, buf);
    gnome_print_moveto(info->pc, (info->printable_width/2)-(width/2),
		       info->page_height - info->margin_top -
		       (info->lines_per_page+1) * info->font_char_height);
    gnome_print_show(info->pc, buf);
    c2_free (buf);
    current_line = 0;

    gnome_print_showpage (info->pc);
  }

  gnome_print_context_close (info->pc);
  info->pc = NULL;
  gnome_print_master_close (info->master);
  gtk_object_unref (GTK_OBJECT (font));
}

void cronos_print (const Message *message, const char *msg) {
  GtkWidget *dialog;
  gboolean preview = FALSE;
  C2PrintInfo *info;

  g_return_if_fail (message);
  g_return_if_fail (msg);

  dialog = gnome_print_dialog_new (_("Print"), GNOME_PRINT_DIALOG_COPIES);
  
  switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
    case GNOME_PRINT_PRINT:
      break;
    case GNOME_PRINT_PREVIEW:
      preview = TRUE;
      break;
    case GNOME_PRINT_CANCEL:
      gnome_dialog_close(GNOME_DIALOG(dialog));
    default:
      return;
  }

#if USE_PLUGINS
  if (message)
    c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_OPEN, (Message*)message, "print", NULL, NULL, NULL);
#endif
  info = c2_print_info_new ("A4", message, msg, GNOME_PRINT_DIALOG (dialog));
  gnome_dialog_close(GNOME_DIALOG(dialog));

  c2_print_message (info);
  
  if (preview) {
    GnomePrintMasterPreview *preview_widget;
    preview_widget = gnome_print_master_preview_new(info->master, _("Print Preview"));
    gtk_widget_show(GTK_WIDGET(preview_widget));
  } else {
    gnome_print_master_print(info->master);
  }
}
#endif
