#include <config.h>
#include <gnome.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "gui-window_message.h"
#include "gui-select_file.h"
#include "gui-utils.h"

#include "message.h"
#include "init.h"
#include "rc.h"
#include "utils.h"

static void
on_mime_open_activate						(GtkWidget *widget, WindowMessage *wm);

static void
on_mime_view_activate						(GtkWidget *widget, WindowMessage *wm);

static void
on_mime_save_activate						(GtkWidget *widget, WindowMessage *wm);

static void
on_properties_activate						(GtkWidget *widget, WindowMessage *wm);

static void
on_close_activate						(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_from_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_account_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_to_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_cc_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_bcc_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_date_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_subject_activate					(GtkWidget *widget, WindowMessage *wm);

static void
on_view_menu_priority_activate					(GtkWidget *widget, WindowMessage *wm);

/********************************************
 *********** ATTACH MENU ********************
 ********************************************/
static GnomeUIInfo attach_menu[] = {
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Open"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_View"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Save"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

/************************
 ****** FILE MENU *******
 ************************/
static GnomeUIInfo file_menu[] = {
  GNOMEUIINFO_MENU_PROPERTIES_ITEM (NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CLOSE_ITEM (NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

/************************
 ****** VIEW MENU *******
 ************************/
static GnomeUIInfo view_menu[] = {
  {
    GNOME_APP_UI_TOGGLEITEM, N_("From"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Account"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("To"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("CC"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("BCC"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Date"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Subject"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Priority"),
    NULL, NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_END
};

/************************
 ****** MAIN MENU *******
 ************************/
static GnomeUIInfo main_menu[] = {
  GNOMEUIINFO_MENU_FILE_TREE (file_menu),
  GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
  GNOMEUIINFO_END
};

/**
 * gui_window_message_new
 *
 * Creates a Window Message.
 *
 * Return Value:
 * An initializated WindowMessage object.
 **/
WindowMessage *
gui_window_message_new (void) {
  WindowMessage *wm;
  GtkWidget *helper;
  GtkWidget *table;
  GtkStyle *style, *style2;
  GtkWidget *viewport;
  GtkWidget *vbox;

  wm = g_new0 (WindowMessage, 1);
  wm->message = NULL;

  wm->window = gnome_app_new (PACKAGE, _("Window Message Reader"));
  gtk_widget_set_usize (GTK_WIDGET (wm->window), rc->main_window_width, rc->main_window_height);
  gtk_window_set_policy (GTK_WINDOW (wm->window), TRUE, TRUE, FALSE);
  wm->destroy_signal = gtk_signal_connect (GTK_OBJECT (wm->window), "destroy",
      			GTK_SIGNAL_FUNC (on_close_activate), wm);
  gnome_app_create_menus (GNOME_APP (wm->window), main_menu);
  helper = file_menu[0].widget;
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_properties_activate), wm);
  helper = file_menu[2].widget;
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_close_activate), wm);
  helper = view_menu[0].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 2)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_from_activate), wm);
  helper = view_menu[1].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 4)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_account_activate), wm);
  helper = view_menu[2].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 0)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_to_activate), wm);
  helper = view_menu[3].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 5)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_cc_activate), wm);
  helper = view_menu[4].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 6)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_bcc_activate), wm);
  helper = view_menu[5].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 1)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_date_activate), wm);
  helper = view_menu[6].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 3)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_subject_activate), wm);
  helper = view_menu[7].widget;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 7)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (helper), TRUE);
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
      			GTK_SIGNAL_FUNC (on_view_menu_priority_activate), wm);

  /** Vbox **/
  vbox = gtk_vbox_new (FALSE, 3);
  gnome_app_set_contents (GNOME_APP (wm->window), vbox);
  gtk_widget_show (vbox);
  
  /** Table **/
  table = gtk_table_new (6, 4, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 2);
  gtk_widget_show (table);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  wm->header_table = table;

  /** From **/
  wm->header_titles[HEADER_TITLES_FROM][0] = gtk_label_new (_("From:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_FROM][0], 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_FROM][0]), 1, 0.5);
  style = gtk_widget_get_style (wm->header_titles[HEADER_TITLES_FROM][0]);
  style2 = gtk_style_copy (style);
  style2->font = gdk_font_load ("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-1");
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_FROM][0], style2);
  
  wm->header_titles[HEADER_TITLES_FROM][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_FROM][1], 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND) | (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_FROM][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 2) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_FROM][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_FROM][1]);
  }

  /** To **/
  wm->header_titles[HEADER_TITLES_TO][0] = gtk_label_new (_("To:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_TO][0], 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_TO][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_TO][0], style2);

  wm->header_titles[HEADER_TITLES_TO][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_TO][1], 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND) | (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_TO][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 0) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_TO][1]);
  }

  /** CC **/
  wm->header_titles[HEADER_TITLES_CC][0] = gtk_label_new (_("CC:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_CC][0], 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_CC][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_CC][0], style2);

  wm->header_titles[HEADER_TITLES_CC][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_CC][1], 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND) | (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_CC][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 5) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_CC][1]);
  }

  /** BCC **/
  wm->header_titles[HEADER_TITLES_BCC][0] = gtk_label_new (_("BCC:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_BCC][0], 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_BCC][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_BCC][0], style2);

  wm->header_titles[HEADER_TITLES_BCC][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_BCC][1], 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND) | (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_BCC][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 6) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_BCC][1]);
  }

  /** SUBJECT **/
  wm->header_titles[HEADER_TITLES_SUBJECT][0] = gtk_label_new (_("Subject:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_SUBJECT][0], 0, 1, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_SUBJECT][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_SUBJECT][0], style2);

  wm->header_titles[HEADER_TITLES_SUBJECT][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_SUBJECT][1], 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND) | (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_SUBJECT][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 3) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_SUBJECT][1]);
  }

  /** ACCOUNT **/
  wm->header_titles[HEADER_TITLES_ACCOUNT][0] = gtk_label_new (_("Account:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_ACCOUNT][0], 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_ACCOUNT][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_ACCOUNT][0], style2);

  wm->header_titles[HEADER_TITLES_ACCOUNT][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_ACCOUNT][1], 3, 4, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_ACCOUNT][1]), 7.45058e-09, 0.5);
  gtk_widget_set_usize (wm->header_titles[HEADER_TITLES_ACCOUNT][1], 100, -1);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 4) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_ACCOUNT][1]);
  }

  /** DATE **/
  wm->header_titles[HEADER_TITLES_DATE][0] = gtk_label_new (_("Date:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_DATE][0], 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_DATE][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_DATE][0], style2);

  wm->header_titles[HEADER_TITLES_DATE][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_DATE][1], 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_DATE][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 1) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_DATE][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_DATE][1]);
  }

  /** PRIORITY **/
  wm->header_titles[HEADER_TITLES_PRIORITY][0] = gtk_label_new (_("Priority:"));
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_PRIORITY][0], 2, 3, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_PRIORITY][0]), 1, 0.5);
  gtk_widget_set_style (wm->header_titles[HEADER_TITLES_PRIORITY][0], style2);

  wm->header_titles[HEADER_TITLES_PRIORITY][1] = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), wm->header_titles[HEADER_TITLES_PRIORITY][1], 3, 4, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (wm->header_titles[HEADER_TITLES_PRIORITY][1]), 7.45058e-09, 0.5);
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 7) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_PRIORITY][1]);
  }

  /** scrolled window **/
  helper = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), helper, TRUE, TRUE, 0);
  gtk_widget_show (helper);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (helper), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /** text **/
  wm->text = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (helper), wm->text);
  gtk_widget_show (wm->text);

  /** viewport **/
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), viewport, FALSE, TRUE, 0);
  gtk_widget_show (viewport);
  
  /** scrolled window **/
  helper = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (viewport), helper);
  gtk_widget_show (helper);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (helper), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /** icon_list **/
  wm->icon_list = gnome_icon_list_new (70, NULL, 0);
  gtk_container_add (GTK_CONTAINER (helper), wm->icon_list);
  gtk_widget_show (wm->icon_list);

  helper = gnome_popup_menu_new (attach_menu);
  gnome_popup_menu_attach (helper, wm->icon_list, NULL);
  helper = attach_menu[1].widget;
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
	GTK_SIGNAL_FUNC (on_mime_open_activate), wm);
  helper = attach_menu[3].widget;
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
	GTK_SIGNAL_FUNC (on_mime_view_activate), wm);
  helper = attach_menu[5].widget;
  gtk_signal_connect (GTK_OBJECT (helper), "activate",
	GTK_SIGNAL_FUNC (on_mime_save_activate), wm);

  return wm;
}

/**
 * gui_window_message_new_with_message
 * @message: A pointer to a Message object.
 *
 * Creates a new WindowMessage object,
 * sets the message for it and displays the window.
 **/
WindowMessage *
gui_window_message_new_with_message (Message *message) {
  WindowMessage *wm;
  
  g_return_val_if_fail (message, NULL);
  
  wm = gui_window_message_new ();
  gui_window_message_set_message (wm, message);
  gtk_widget_show (wm->window);
  return wm;
}

/**
 * gui_window_message_set_message
 * @wm: A pointer to a WindowMessage object.
 * @message: A pointer to a Message object.
 *
 * Sets a message in the Window Message.
 **/
void
gui_window_message_set_message (WindowMessage *wm, Message *message) {
  GList *s;
  guint32 len;
  MimeHash *mime, *tmp;
  char *buf, *buf2, *path;
  char *content_type, *parameter;
  char *from, *to, *cc, *bcc, *subject, *account, *date, *priority;
  GString *body=g_string_new(NULL);
  GdkImlibImage *img;
  
  g_return_if_fail (wm);
  g_return_if_fail (message);

#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_OPEN, message, "message", NULL, NULL, NULL);
#endif
  if (wm->message) message_free (wm->message);
  wm->message = message;

  message_mime_parse (message, NULL);

  if (message->mime) {
    mime = message_mime_get_default_part (message->mime);
    message_mime_get_part (mime);
  } else mime = NULL;
    
  gnome_icon_list_freeze (GNOME_ICON_LIST (wm->icon_list));
  gnome_icon_list_clear (GNOME_ICON_LIST (wm->icon_list));

  for (s = message->mime; s != NULL; s = s->next) {
    buf = buf2 = NULL;
    tmp = MIMEHASH (s->data);
    if (!tmp) continue;
    content_type = g_strdup_printf ("%s/%s", tmp->type, tmp->subtype);
    if (tmp->parameter) {
      parameter = message_mime_get_parameter_value (tmp->parameter, "name");
      if (!parameter) parameter = message_mime_get_parameter_value (tmp->parameter, "filename");
      if (!parameter) parameter = message_mime_get_parameter_value (tmp->disposition, "filename");
    }
    path = CHAR (pixmap_get_icon_by_mime_type (content_type));
    img = gdk_imlib_load_image (path);
    if (parameter && strlen (parameter)) {
      gnome_icon_list_append_imlib (GNOME_ICON_LIST (wm->icon_list), img, parameter);
    } else {
      gnome_icon_list_append_imlib (GNOME_ICON_LIST (wm->icon_list), img, content_type);
    }
    c2_free (content_type);
  }
  gnome_icon_list_thaw (GNOME_ICON_LIST (wm->icon_list));

  gtk_text_freeze (GTK_TEXT (wm->text));
  len = gtk_text_get_length (GTK_TEXT (wm->text));
  gtk_text_set_point (GTK_TEXT (wm->text), 0);
  gtk_text_forward_delete (GTK_TEXT (wm->text), len);
  
  if (mime)
    g_string_assign(body,mime->part);
  else {
    message_get_message_body (message, NULL);
    g_string_assign(body,message->body);
  }
#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_DISPLAY, body->str, "message", NULL, NULL, NULL);
#endif  
  gtk_text_insert (GTK_TEXT (wm->text), font_body, &config->color_misc_body, NULL, body->str, -1);
  
  g_string_free(body,TRUE);
		  
  /*if (mime)
    gtk_text_insert (GTK_TEXT (wm->text), font_body, &config->color_misc_body, NULL, mime->part, -1);
  else {
    message_get_message_body (message, NULL);
    gtk_text_insert (GTK_TEXT (wm->text), font_body, &config->color_misc_body, NULL, message->body, -1);
  }*/

  
  
  gtk_text_thaw (GTK_TEXT (wm->text));

  /* Get the header fields */
  from		= message_get_header_field (message, NULL, "From:");
  to		= message_get_header_field (message, NULL, "To:");
  cc		= message_get_header_field (message, NULL, "CC:");
  bcc		= message_get_header_field (message, NULL, "BCC:");
  subject	= message_get_header_field (message, NULL, "Subject:");
  account	= message_get_header_field (message, NULL, "X-CronosII-Account:");
  date		= message_get_header_field (message, NULL, "Date:");
  priority	= message_get_header_field (message, NULL, "Priority:");
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_FROM][1]),	from);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_TO][1]),	to);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_CC][1]),	cc);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_BCC][1]),	bcc);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_SUBJECT][1]),	subject);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_ACCOUNT][1]),	account);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_DATE][1]),	date);
  gtk_label_set_text (GTK_LABEL (wm->header_titles[HEADER_TITLES_PRIORITY][1]), priority);
  gtk_window_set_title (GTK_WINDOW (wm->window), subject);
  c2_free (from);
  c2_free (to);
  c2_free (cc);
  c2_free (bcc);
  c2_free (subject);
  c2_free (account);
  c2_free (date);
  c2_free (priority);
}

static void
on_window_message_properties_close_clicked (GtkWidget *widget, WindowMessageProperties *wmp) {
  int len;
  char *tmp;
  
  g_return_if_fail (wmp);
  
  gtk_widget_hide (wmp->window);
  tmp = gtk_editable_get_chars (GTK_EDITABLE (wmp->text), 0, -1);
  gtk_text_freeze (GTK_TEXT (wmp->text));
  len = gtk_text_get_length (GTK_TEXT (wmp->text));
  gtk_text_set_point (GTK_TEXT (wmp->text), 0);
  gtk_text_forward_delete (GTK_TEXT (wmp->text), len);
  gtk_text_thaw (GTK_TEXT (wmp->text));
  gtk_signal_disconnect (GTK_OBJECT (wmp->window), wmp->destroy_signal);
  gtk_widget_destroy (wmp->window);
  message_free (wmp->message);
  c2_free (wmp);
  c2_free (tmp);
}

WindowMessageProperties *
gui_window_message_properties_new (void) {
  WindowMessageProperties *wmp = g_new0 (WindowMessageProperties, 1);
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *scroll;
  GtkWidget *btn;

  wmp->window = gnome_app_new (PACKAGE, _("Message Properties"));
  gtk_widget_set_usize (wmp->window, rc->main_window_width, rc->main_window_height);
  wmp->destroy_signal = gtk_signal_connect (GTK_OBJECT (wmp->window), "destroy",
      			GTK_SIGNAL_FUNC (on_window_message_properties_close_clicked), wmp);
  
  vbox = gtk_vbox_new (FALSE, 2);
  gnome_app_set_contents (GNOME_APP (wmp->window), vbox);
  gtk_widget_show (vbox);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  gtk_widget_show (scroll);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  wmp->text = gtk_text_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), wmp->text);
  gtk_widget_show (wmp->text);

  btn = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, FALSE, 3);
  gtk_widget_show (btn);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  btn = gnome_stock_button (GNOME_STOCK_BUTTON_CLOSE);
  gtk_box_pack_end (GTK_BOX (hbox), btn, FALSE, FALSE, 0);
  gtk_widget_show (btn);
  gtk_signal_connect (GTK_OBJECT (btn), "clicked",
      			GTK_SIGNAL_FUNC (on_window_message_properties_close_clicked), wmp);

  return wmp;
}

WindowMessageProperties *
gui_window_message_properties_new_with_message (Message *message) {
  WindowMessageProperties *wmp;
  g_return_val_if_fail (message, NULL);
  wmp = gui_window_message_properties_new ();
  gui_window_message_properties_set_message (wmp, message);
  gtk_widget_show (wmp->window);
  return wmp;
}

void
gui_window_message_properties_set_message (WindowMessageProperties *wmp, Message *message) {
  char *tmp;
  int len;
  g_return_if_fail (wmp);
  g_return_if_fail (message);

  if (wmp->message) message_free (wmp->message);
  wmp->message = message;

  tmp = gtk_editable_get_chars (GTK_EDITABLE (wmp->text), 0, -1);
  gtk_text_freeze (GTK_TEXT (wmp->text));
  len = gtk_text_get_length (GTK_TEXT (wmp->text));
  gtk_text_set_point (GTK_TEXT (wmp->text), 0);
  gtk_text_forward_delete (GTK_TEXT (wmp->text), len);
  gtk_text_insert (GTK_TEXT (wmp->text),
      		gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1"),
		&config->color_misc_body, NULL, message->message, -1);
  gtk_text_thaw (GTK_TEXT (wmp->text));
  c2_free (tmp);
}

static void
on_mime_open_clicked_thread (char *cmnd) {
  g_return_if_fail (cmnd);
  
  cronos_system (cmnd);
}

static void
on_mime_open_activate (GtkWidget *widget, WindowMessage *wm) {
  MimeHash *mime;
  GList *list, *s;
  Message *message;
  const char *file_name;
  char *path;
  const char *program;
  char *content_type;
  char *command;
  FILE *fd;
  gboolean main_is_inline = FALSE;
  pthread_t thread;
 
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (wm);
  
  if (!GNOME_ICON_LIST (wm->icon_list)->selection) return;
  message = wm->message;
  if (!message) return;
  
  list = g_list_nth (message->mime, (int) GNOME_ICON_LIST (wm->icon_list)->selection->data);
  if (!list) return;
  
  mime = MIMEHASH (list->data);
  if (!mime) return;
  
  content_type = g_strdup_printf ("%s/%s", mime->type, mime->subtype);
  program = gnome_mime_program (content_type);
  if (!program && streq (content_type, "application/octet-stream")) program = "";
  if (!program) {
    return;
  }
  c2_free (content_type);
  
  /* Check if this part is inline */
  if (strnne (mime->disposition, "attachment", 10)) main_is_inline = TRUE;
  else main_is_inline = FALSE;

  if (main_is_inline) {
    /* Go through the rest of the other parts saving all inline parts that have a name */
    s = g_list_nth (message->mime, (int) GNOME_ICON_LIST (wm->icon_list)->selection->data);
    for (s = s->next; s; s = s->next) {
      if (!MIMEHASH (s->data)->id) continue;

      path = g_strdup_printf ("/tmp/cid:%s", MIMEHASH (s->data)->id);
      message_mime_get_part (MIMEHASH (s->data));
      
      tmp_files = g_slist_append (tmp_files, path);
      if ((fd = fopen (path, "wb")) == NULL) {
	continue;
      }
      fwrite (MIMEHASH (s->data)->part, sizeof (char), MIMEHASH (s->data)->len, fd);
      fclose (fd);
    }
  }
  
  file_name = message_mime_get_parameter_value (mime->parameter, "name");
  if (!file_name) file_name = message_mime_get_parameter_value (mime->parameter, "filename");
  if (!file_name) file_name = message_mime_get_parameter_value (mime->disposition, "filename");
  
  if (file_name) {
    path = g_strdup_printf ("/tmp/%s", file_name);
  } else {
tmpnam:
    path = cronos_tmpfile ();
  }
  
  message_mime_get_part (mime);

  if ((fd = fopen (path, "wb")) == NULL) {
    switch (errno) {
      case EEXIST:
      case EISDIR:
      case ETXTBSY:
	goto tmpnam;
    }
    return;
  }
  fwrite (mime->part, sizeof (char), mime->len, fd);
  fclose (fd);
  
  if (!strlen (program)) {
    command = path;
    chmod (path, 0775);
  } else command = str_replace (program, "%f", path);
  
  if ((command[0] == '"' )||(command[0] == '/')) return;
  printf ("%s\n",command);
  
  pthread_create (&thread, NULL, PTHREAD_FUNC (on_mime_open_clicked_thread), command);
  pthread_detach (thread);
}

static void
on_mime_view_activate (GtkWidget *widget, WindowMessage *wm) {
  Message *message;
  MimeHash *mime;
  GList *list;
  int len;
  GString *body=g_string_new(NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (wm);

  if (!GNOME_ICON_LIST (wm->icon_list)->selection) return;
  message = wm->message;
  if (!message) return;
  
  list = g_list_nth (message->mime, (int) GNOME_ICON_LIST (wm->icon_list)->selection->data);
  if (!list) return;

  mime = MIMEHASH (list->data);
  if (!mime) return;

  message_mime_get_part (mime);
  gtk_text_freeze (GTK_TEXT (wm->text));
  len = gtk_text_get_length (GTK_TEXT (wm->text));
  gtk_text_set_point (GTK_TEXT (wm->text), 0);
  gtk_text_forward_delete (GTK_TEXT (wm->text), len);
  
  g_string_assign(body,mime->part);
#if USE_PLUGINS
  c2_dynamic_module_signal_emit (C2_DYNAMIC_MODULE_MESSAGE_DISPLAY, body->str, "message", NULL, NULL, NULL);
#endif  
  gtk_text_insert (GTK_TEXT (wm->text), font_body, &config->color_misc_body, NULL, body->str, -1);
  
  g_string_free(body,TRUE);
  
  //gtk_text_insert (GTK_TEXT (wm->text), font_body, &config->color_misc_body, NULL, mime->part, -1);
  gtk_text_thaw (GTK_TEXT (wm->text));
}

static void
on_mime_save_activate (GtkWidget *widget, WindowMessage *wm) { 
  Message *message;
  MimeHash *mime;
  GList *list;
  GList *s;
  const char *file_name;
  char *path;
  FILE *fd;
 
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (wm);

  if (!GNOME_ICON_LIST (wm->icon_list)->selection) return;

  message = wm->message;
  if (!message) return;
  
  for (s = GNOME_ICON_LIST (wm->icon_list)->selection; s != NULL; s = s->next) {
    list = g_list_nth (message->mime, (int) s->data);
    if (!list) return;
    
    mime = MIMEHASH (list->data);
    if (!mime) return;
    
    file_name = message_mime_get_parameter_value (mime->parameter, "name");
    if (!file_name) file_name = message_mime_get_parameter_value (mime->parameter, "filename");
    if (!file_name) file_name = message_mime_get_parameter_value (mime->disposition, "filename");
    
    path = gui_select_file_new (NULL, NULL, NULL, file_name);
    if (!path) continue;
    message_mime_get_part (mime);
    
    if ((fd = fopen (path, "wb")) == NULL) {
      return;
    }
    fwrite (mime->part, sizeof (char), mime->len, fd);
    fclose (fd);
  }
}

static void
on_properties_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (wm);

  if (!wm->message) return;
  gui_window_message_properties_new_with_message (message_copy (wm->message));
}
  
static void
on_close_activate (GtkWidget *widget, WindowMessage *wm) {
  int len;
  char *tmp;
  g_return_if_fail (wm);

  gtk_widget_hide (wm->window);
  tmp = gtk_editable_get_chars (GTK_EDITABLE (wm->text), 0, -1);
  gtk_text_freeze (GTK_TEXT (wm->text));
  len = gtk_text_get_length (GTK_TEXT (wm->text));
  gtk_text_set_point (GTK_TEXT (wm->text), 0);
  gtk_text_forward_delete (GTK_TEXT (wm->text), len);
  gtk_text_thaw (GTK_TEXT (wm->text));
  gtk_signal_disconnect (GTK_OBJECT (wm->window), wm->destroy_signal);
  gtk_widget_destroy (wm->window);
  message_free (wm->message);
  c2_free (wm);
  c2_free (tmp);
}

static void
on_view_menu_from_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 2;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 2) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_FROM][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_FROM][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_FROM][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_FROM][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_account_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 4;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 4) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_ACCOUNT][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_ACCOUNT][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_ACCOUNT][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_to_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 0;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 0) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_TO][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_TO][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_TO][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_cc_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 5;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 5) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_CC][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_CC][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_CC][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_bcc_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 6;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 6) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_BCC][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_BCC][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_BCC][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_date_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 1;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 1) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_DATE][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_DATE][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_DATE][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_DATE][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_subject_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 3;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 3) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_SUBJECT][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_SUBJECT][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_SUBJECT][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}

static void
on_view_menu_priority_activate (GtkWidget *widget, WindowMessage *wm) {
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (widget));
  g_return_if_fail (wm);
  rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] ^= 1 << 7;
  if (rc->showable_headers[SHOWABLE_HEADERS_MESSAGE] & 1 << 7) {
    gtk_widget_show (wm->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_show (wm->header_titles[HEADER_TITLES_PRIORITY][1]);
  } else {
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_PRIORITY][0]);
    gtk_widget_hide (wm->header_titles[HEADER_TITLES_PRIORITY][1]);
    gtk_widget_set_usize (wm->header_table, -1, -1);
  }
}
