#include <gnome.h>
#include <config.h>

#ifdef BUILD_ADDRESS_BOOK
#include "addrbook.h"
#include "autocompletition.h"
#include "debug.h"
#include "rc.h"
#include "utils.h"

GCompletion *EMailCompletion = NULL;

GtkWidget *alternatives_window = NULL;
GtkWidget *contacts_window = NULL;

void
autocompletatition_alternatives_destroy					(void);

void
autocompletatition_alternatives_display					(GtkWidget *entry,
    									 WidgetsComposer *composer,
									 GList *list);

static gboolean
on_alternatives_window_button_press_event				(GtkWidget *widget,
				    				         GdkEventButton *event,
									 GtkWidget *alternatives_window);

static gboolean
on_alternatives_window_key_press_event					(GtkWidget *widget,
				    				 	 GdkEventKey *event,
									 GtkWidget *location_entry);

static void 
on_alternatives_clist_select_row_cb					(GtkCList *clist,
    									 gint row, gint column,
									 GdkEventButton *event,
									 GtkWidget *entry);

void
autocompletatition_contacts_destroy					(void);

static void 
on_contacts_clist_select_row_cb						(GtkCList *clist,
    									 gint row, gint column,
									 GdkEventButton *event,
									 GtkWidget *entry);

static gboolean
on_contacts_window_button_press_event					(GtkWidget *widget,
    									 GdkEventButton *event,
									 GtkWidget *contacts_window);

static gboolean
on_contacts_window_key_press_event					(GtkWidget *widget,
    									 GdkEventKey *event,
									 GtkWidget *location_entry);

gboolean
autocompletition_key_pressed (GtkWidget *entry, GdkEventKey *key, WidgetsComposer *composer) {
  char *string;
  GList *matches;

  g_return_val_if_fail (key, TRUE);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), TRUE);
  g_return_val_if_fail (composer, TRUE);

  /* Get the text of this entry */
  string = gtk_entry_get_text (GTK_ENTRY (entry)); 
  
  /* If the user presses Enter */
  if (key->keyval == GDK_Return) {
    autocompletatition_alternatives_destroy ();
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, 0);
  }

  /* If the user presses Escape */
  if (key->keyval == GDK_Escape) {
    autocompletatition_alternatives_destroy ();
  }

  /* If the user presses a letter, a number or other symbol... */
  if (key->keyval >= 32 && key->keyval < 126) {
    char *start, *end, *pattern, *ptr;
    int i, curpos = gtk_editable_get_position (GTK_EDITABLE (entry));
    
    autocompletatition_alternatives_destroy ();
    
    /* Get the pattern we are going to work into */
    for (i = curpos-1, start = string+curpos-1; i > 0; i--, start--) {
      if (*start == ';' || *start == ',' || *start == '\0') break;
    }

    if (*start == ',' || *start == ';') {
      start++;
      while (*start == ' ') start++;
    }

    if (!start || !strlen (start)) return TRUE;

    /* Now, get from start to the next ',' or ';' or NULL */
    for (end = start; *end != '\0'; end++) if (*end == ';' || *end == ',') break;

    if (end) {
      pattern = g_new0 (char, end-start);
      strncpy (pattern, start, end-start);
    } else pattern = g_strdup (start);
    if (end) end = g_strdup (end);

    ptr = g_new0 (char, start-string);
    strncpy (ptr, string, start-string);
    start = ptr;
   
    matches = g_completion_complete (EMailCompletion, pattern, NULL);
    if (g_list_length (matches) == 1) {
      char *sel;

      sel = g_strdup_printf ("%s%s%s", start ? start : "", CHAR (matches->data), end ? end : "");
      gtk_entry_set_text (GTK_ENTRY (entry), sel);
      gtk_editable_select_region (GTK_EDITABLE (entry), curpos, -1);
      gtk_editable_set_position (GTK_EDITABLE (entry), curpos);
    }
    else if (g_list_length (matches))
      autocompletatition_alternatives_display (entry, composer, matches);
  }

  /* If the user presses Shift, Ctrl, Alt or End */
  if ((!((key->state & GDK_SHIFT_MASK) ||
	(key->state & GDK_CONTROL_MASK) ||
	(key->state & GDK_MOD1_MASK)) &&
        (key->keyval == GDK_End))) {
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, 0); 
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    return TRUE;
  }

  /* If the user presses Tab */
  if (key->keyval == GDK_Tab) {
    gboolean forward;
    if (key->state & GDK_Shift_L || key->state & GDK_Shift_R) forward = FALSE;
    else forward = TRUE;
    
    if (entry == composer->header_titles[HEADER_TITLES_TO][1]) {
      if (forward) {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_CC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_BCC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_SUBJECT][1]);
	else
	  gtk_widget_grab_focus (composer->body);
      } else {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
	  gtk_widget_grab_focus (GTK_COMBO (composer->header_titles[HEADER_TITLES_ACCOUNT][1])->entry);
	else
	  gtk_widget_grab_focus (composer->body);
      }
    }
    else if (entry == composer->header_titles[HEADER_TITLES_CC][1]) {
      if (forward) {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_BCC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_SUBJECT][1]);
	else
	  gtk_widget_grab_focus (composer->body);
      } else {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_TO][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
	  gtk_widget_grab_focus (GTK_COMBO (composer->header_titles[HEADER_TITLES_ACCOUNT][1])->entry);
	else
	  gtk_widget_grab_focus (composer->body);
      }
    }
    else if (entry == composer->header_titles[HEADER_TITLES_BCC][1]) {
      if (forward) {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 3)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_SUBJECT][1]);
	else
	  gtk_widget_grab_focus (composer->body);
      } else {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_CC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_TO][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
	  gtk_widget_grab_focus (GTK_COMBO (composer->header_titles[HEADER_TITLES_ACCOUNT][1])->entry);
	else
	  gtk_widget_grab_focus (composer->body);
      }
    }
    else if (entry == composer->header_titles[HEADER_TITLES_SUBJECT][1]) {
      if (forward) {
	gtk_widget_grab_focus (composer->body);
      } else {
	if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 6)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_BCC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 5)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_CC][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 0)
	  gtk_widget_grab_focus (composer->header_titles[HEADER_TITLES_TO][1]);
	else if (rc->showable_headers[SHOWABLE_HEADERS_COMPOSE] & 1 << 4)
	  gtk_widget_grab_focus (GTK_COMBO (composer->header_titles[HEADER_TITLES_ACCOUNT][1])->entry);
	else
	  gtk_widget_grab_focus (composer->body);
      }
    }
  }
  
  if (!strlen (string))
    autocompletatition_alternatives_destroy ();
  return TRUE;
}

void
autocompletition_generate_emailcompletion (void) {
  GList *list = NULL;
  const C2VCard *card;
  char *str;
  int i;
  
  if (EMailCompletion)
    g_completion_clear_items (EMailCompletion);
  else
    EMailCompletion = g_completion_new (NULL);

  for (i = 0;;i++) {
    if ((card = c2_address_book_card_get (i)) == NULL) break;
    if (!card->email) continue;

    str = g_strdup_printf ("%s <%s>", card->name_show, C2VCARDEMAIL (card->email->data)->address);

    list = g_list_append (list, str);
  }

  if (list)
    g_completion_add_items (EMailCompletion, list);
}

void
autocompletatition_alternatives_display (GtkWidget *entry, WidgetsComposer *composer, GList *list) {
  GtkWidget *scroll;
  GtkWidget *clist;
  GtkWidget *frame;
  GList *l;
  gint x, y, height, width, depth;
  GtkRequisition r;
	
  g_return_if_fail (entry);
  g_return_if_fail (composer);
  g_return_if_fail (list);
  
  if (alternatives_window)
    gtk_widget_destroy (alternatives_window);
  
  alternatives_window = gtk_window_new (GTK_WINDOW_POPUP);
  
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (alternatives_window), frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_widget_show (frame);
  
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  
  clist = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (scroll), clist);
  gtk_widget_show (clist);
  gtk_signal_connect (GTK_OBJECT(clist), "select_row", 
      			GTK_SIGNAL_FUNC(on_alternatives_clist_select_row_cb), 
			entry);
  
 
  for (l = list; l; l = l->next) {
    char *text[] = { l->data, NULL };
    gtk_clist_append (GTK_CLIST (clist), text);
  }
  
  gdk_window_get_geometry (entry->window, &x, &y, &width, &height, &depth);
  gdk_window_get_deskrelative_origin (entry->window, &x, &y);
  y += height;
  gtk_widget_set_uposition (alternatives_window, x, y);
  gtk_widget_size_request (clist, &r);
  gtk_widget_set_usize (alternatives_window, width, r.height+4);
  gtk_widget_show (alternatives_window);       
  gtk_widget_size_request (clist, &r);
  
  if ((y + r.height) > gdk_screen_height ()) {
    gtk_window_set_policy
      (GTK_WINDOW (alternatives_window), TRUE, FALSE, FALSE);
    gtk_widget_set_usize 
      (alternatives_window, width, gdk_screen_height () - y);
    
  }
 
  gtk_signal_connect (GTK_OBJECT(alternatives_window),
			    "button-press-event",
			    GTK_SIGNAL_FUNC(on_alternatives_window_button_press_event),
			    alternatives_window);
  gtk_signal_connect (GTK_OBJECT(alternatives_window),
			    "key-press-event",
			    GTK_SIGNAL_FUNC(on_alternatives_window_key_press_event),
			    entry);

  gdk_pointer_grab (alternatives_window->window, TRUE,
      GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
      GDK_BUTTON_RELEASE_MASK,
      NULL, NULL, GDK_CURRENT_TIME);
  gtk_grab_add (alternatives_window);
}

void
autocompletatition_alternatives_destroy (void) {
  if (!alternatives_window) return;

  gtk_widget_destroy (alternatives_window);
  alternatives_window = NULL;
}

static void 
on_alternatives_clist_select_row_cb (GtkCList *clist,
    				     gint row, gint column,
				     GdkEventButton *event,
				     GtkWidget *entry) {
  GdkEventKey tmp_event;
  
  if (GTK_IS_ENTRY (entry)) {
    char *string;
    char *str;
    char *text;
    char *start, *end, *ptr;
    int i, curpos = gtk_editable_get_position (GTK_EDITABLE (entry));

    string = gtk_entry_get_text (GTK_ENTRY (entry));
    
    /* Get the pattern we are going to work into */
    for (i = curpos-1, start = string+curpos-1; i > 0; i--, start--) {
      if (*start == ';' || *start == ',' || *start == '\0') break;
    }

    if (*start == ',' || *start == ';') {
      start++;
      while (*start == ' ') start++;
    }

    if (!start || !strlen (start)) return;

    /* Now, get from start to the next ',' or ';' or NULL */
    for (end = start; *end != '\0'; end++) if (*end == ';' || *end == ',') break;

    if (end) end = g_strdup (end);

    ptr = g_new0 (char, start-string);
    strncpy (ptr, string, start-string);
    start = ptr;
    
    gtk_clist_get_text (clist, row, column, &text);

    str = g_strdup_printf ("%s%s%s", start ? start : "", text, end ? end : "");
    gtk_entry_set_text (GTK_ENTRY (entry), str);
    gtk_editable_set_position (GTK_EDITABLE (entry), curpos+strlen (text));
    autocompletatition_alternatives_destroy ();
    
    /* send a synthetic return keypress to the entry */
    tmp_event.type = GDK_KEY_PRESS;
    tmp_event.window = entry->window;
    tmp_event.send_event = TRUE;
    tmp_event.time = GDK_CURRENT_TIME;
    tmp_event.state = 0;
    tmp_event.keyval = GDK_Return;
    gtk_widget_event(entry, (GdkEvent *)&tmp_event);
  }
}

static gboolean
on_alternatives_window_button_press_event (GtkWidget *widget,
    					   GdkEventButton *event,
					   GtkWidget *alternatives_window) {
  GtkWidget *event_widget;
  
  event_widget = gtk_get_event_widget((GdkEvent *)event);
  
  /* Check to see if button press happened inside the alternatives
   * window.  If not, destroy the window. */
  if (event_widget != widget) {
    while (event_widget) {
      if (event_widget == widget)
	return FALSE;
      event_widget = event_widget->parent;
    }
  }
  autocompletatition_alternatives_destroy ();
  
  return TRUE;
}

static gboolean
on_alternatives_window_key_press_event (GtkWidget *widget,
    					GdkEventKey *event,
					GtkWidget *location_entry) {
  GdkEventKey tmp_event;
  GtkCList *clist = GTK_CLIST (GTK_BIN (GTK_BIN (GTK_BIN (widget)->child)->child)->child);
  
  /* allow keyboard navigation in the alternatives clist */
  if (event->keyval == GDK_Up || event->keyval == GDK_Down ||
      event->keyval == GDK_Page_Up ||  event->keyval == GDK_Page_Down ||
      event->keyval == GDK_space)
    return FALSE;
  
  if (event->keyval == GDK_Return) {
    if (!GTK_CLIST_CHILD_HAS_FOCUS (clist)) {
      event->keyval = GDK_space;
      return FALSE;
    }
    autocompletatition_alternatives_destroy ();
  }
  
  /* else send the key event to the location entry */
  tmp_event.type = event->type;
  tmp_event.window = location_entry->window;
  tmp_event.send_event = TRUE;
  tmp_event.time = event->time;
  tmp_event.state = event->state;
  tmp_event.keyval = event->keyval;
  tmp_event.length = event->length;
  tmp_event.string = event->string;
  gtk_widget_event(location_entry, (GdkEvent *)&tmp_event);
  
  return TRUE;
}

/**
 * autocompletation_display_contacts
 * @entry: A pointer to the entry which will get the
 *         return.
 *
 * This will show the list of available contacts and groups.
 **/
void
autocompletation_display_contacts (GtkWidget *entry) {
  GtkWidget *scroll;
  GtkWidget *clist;
  GtkWidget *frame;
  GList *l, *grp, *l2, *l3;
  int i, groups;
  gint x, y, height, width, depth;
  GtkRequisition r;
  GdkColor grey = { 0, 0xdcdc, 0xdcdc, 0xdcdc };
	
  g_return_if_fail (entry);
  
  if (contacts_window)
    gtk_widget_destroy (contacts_window);
  
  contacts_window = gtk_window_new (GTK_WINDOW_POPUP);
  
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (contacts_window), frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_widget_show (frame);
  
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_widget_show (scroll);
  
  clist = gtk_clist_new (1);
  gtk_container_add (GTK_CONTAINER (scroll), clist);
  gtk_widget_show (clist);
  gtk_signal_connect (GTK_OBJECT(clist), "select_row", 
      			GTK_SIGNAL_FUNC(on_contacts_clist_select_row_cb), 
			entry);
  
 
  /* Add contacts */
  {
    char *text[] = { _("Contacts"), NULL };
    gtk_clist_append (GTK_CLIST (clist), text);
  }
  gtk_clist_set_selectable (GTK_CLIST (clist), 0, FALSE);
  gdk_color_alloc (gdk_colormap_get_system (), &grey);
  gtk_clist_set_background (GTK_CLIST (clist), 0, &grey);
  for (l = addrbook; l; l = l->next) {
    C2VCard *card = C2VCARD (l->data);
    char *text[] = { card->name_show, NULL };
    gtk_clist_append (GTK_CLIST (clist), text);
    gtk_clist_set_shift (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1, 0, 0, 3);
    if (card->email && C2VCARDEMAIL (card->email->data))
      	gtk_clist_set_row_data (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1,
	    	g_strdup_printf ("%s <%s>", card->name_show, C2VCARDEMAIL (card->email->data)->address));
  }
  
  /* Add groups */
  {
    char *text[] = { _("Groups"), NULL };
    gtk_clist_append (GTK_CLIST (clist), text);
  }
  gtk_clist_set_selectable (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1, FALSE);
  gtk_clist_set_background (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1, &grey);
  
  /* Go through the groups */
  grp = c2_address_book_get_available_groups ();
  groups = g_list_length (grp);
  for (l = grp; l != NULL; l = l->next) {
    /* Add the group */
    char *text[] = { CHAR (l->data), NULL };
    gtk_clist_append (GTK_CLIST (clist), text);
    gtk_clist_set_shift (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1, 0, 0, 3);
    gtk_clist_set_row_data (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1, "GROUP");
  }
  
#if FALSE
  /* Go through the addrbook */
  for (l = addrbook; l != NULL; l = l->next) {
    C2VCard *card = l->data;
    char *text, *tmp;

    /* Go through the groups of this card */
    for (l2 = card->groups; l2 != NULL; l2 = l2->next) {
      /* Learn which is the position of this group in the grp list */
      for (l3 = grp, i = 0; l3 != NULL; l3 = l3->next) {
	if (streq (l3->data, l2->data)) break;
      }

      /* Get the data of the row */
      tmp = CHAR (gtk_clist_get_row_data (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1-i));
      if (card->email) {
	if (tmp && strlen (tmp))
	  text = g_strdup_printf ("%s, %s <%s>", tmp, card->name_show, C2VCARDEMAIL (card->email)->address);
	else text = g_strdup_printf ("%s <%s>", card->name_show, C2VCARDEMAIL (card->email)->address);
      }
      
      printf ("%s\n", text);
      gtk_clist_set_row_data (GTK_CLIST (clist), GTK_CLIST (clist)->rows-1-i,
				  text);
      c2_free (tmp);
    }
  }
#endif
  
  gdk_window_get_geometry (entry->window, &x, &y, &width, &height, &depth);
  gdk_window_get_deskrelative_origin (entry->window, &x, &y);
  y += height;
  gtk_widget_set_uposition (contacts_window, x, y);
  gtk_widget_size_request (clist, &r);
  gtk_widget_set_usize (contacts_window, width, r.height+4);
  gtk_widget_show (contacts_window);       
  gtk_widget_size_request (clist, &r);
  
  if ((y + r.height) > gdk_screen_height ()) {
    gtk_window_set_policy
      (GTK_WINDOW (contacts_window), TRUE, FALSE, FALSE);
    gtk_widget_set_usize 
      (contacts_window, width, gdk_screen_height () - y);
    
  }
 
  gtk_signal_connect (GTK_OBJECT(contacts_window),
			    "button-press-event",
			    GTK_SIGNAL_FUNC(on_contacts_window_button_press_event),
			    contacts_window);
  gtk_signal_connect (GTK_OBJECT(contacts_window),
			    "key-press-event",
			    GTK_SIGNAL_FUNC(on_contacts_window_key_press_event),
			    entry);

  gdk_pointer_grab (contacts_window->window, TRUE,
      GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
      GDK_BUTTON_RELEASE_MASK,
      NULL, NULL, GDK_CURRENT_TIME);
  gtk_grab_add (contacts_window);
}

void
autocompletition_contacts_destroy (void) {
  if (!contacts_window) return;

  gtk_widget_destroy (contacts_window);
  contacts_window = NULL;
}

static void 
on_contacts_clist_select_row_cb (GtkCList *clist,
    				 gint row, gint column,
				 GdkEventButton *event,
				 GtkWidget *entry) {
  GdkEventKey tmp_event;
  
  if (GTK_IS_ENTRY (entry)) {
    char *string;
    char *str;
    char *text;

    string = gtk_entry_get_text (GTK_ENTRY (entry));
    text = CHAR (gtk_clist_get_row_data (clist, row));
    if (!text) return;

    if (strne (text, "GROUP")) {
      if (string && strlen (string)) str = g_strdup_printf ("%s, %s", string, text);
      else str = g_strdup (text);
    } else {
      /* OK, this is a group, we should go through the addrbook list
       * looking for cards that are in this group. */
      GList *l, *li;
      char *group_name, *tmp;
      C2VCard *card;
      GString *gstring = NULL;

      gtk_clist_get_text (GTK_CLIST (clist), row, 0, &group_name);

      /* Go through the addrbook list */
      for (l = addrbook; l != NULL; l = l->next) {
	card = l->data;

	/* Go through the groups */
	if (card->email) {
	  for (li = card->groups; li != NULL; li = li->next) {
	    if (streq (CHAR (li->data), group_name)) {
	      if (!gstring) {
		if (string && strlen (string)) gstring = g_string_new (", ");
		else gstring = g_string_new (NULL);
	      }
	      else g_string_append (gstring, ", ");
	      tmp = g_strdup_printf ("%s <%s>", card->name_show, C2VCARDEMAIL (card->email->data)->address);
	      g_string_append (gstring, tmp);
	      c2_free (tmp);
	    }
	  }
	}
      }

      if (!gstring) str = NULL;
      else {
	str = gstring->str;
	g_string_free (gstring, FALSE);
      }
    }
    
    if (str)
      gtk_entry_set_text (GTK_ENTRY (entry), str);
    autocompletition_contacts_destroy ();
    
    /* send a synthetic return keypress to the entry */
    tmp_event.type = GDK_KEY_PRESS;
    tmp_event.window = entry->window;
    tmp_event.send_event = TRUE;
    tmp_event.time = GDK_CURRENT_TIME;
    tmp_event.state = 0;
    tmp_event.keyval = GDK_Return;
    gtk_widget_event(entry, (GdkEvent *)&tmp_event);
  }
}

static gboolean
on_contacts_window_button_press_event (GtkWidget *widget,
    				       GdkEventButton *event,
				       GtkWidget *contacts_window) {
  GtkWidget *event_widget;
  
  event_widget = gtk_get_event_widget((GdkEvent *)event);
  
  /* Check to see if button press happened inside the alternatives
   * window.  If not, destroy the window. */
  if (event_widget != widget) {
    while (event_widget) {
      if (event_widget == widget)
	return FALSE;
      event_widget = event_widget->parent;
    }
  }
  autocompletition_contacts_destroy ();
  
  return TRUE;
}

static gboolean
on_contacts_window_key_press_event (GtkWidget *widget,
    				    GdkEventKey *event,
				    GtkWidget *location_entry) {
  GdkEventKey tmp_event;
  GtkCList *clist = GTK_CLIST (GTK_BIN (GTK_BIN (GTK_BIN (widget)->child)->child)->child);
  
  /* allow keyboard navigation in the alternatives clist */
  if (event->keyval == GDK_Up || event->keyval == GDK_Down ||
      event->keyval == GDK_Page_Up ||  event->keyval == GDK_Page_Down ||
      event->keyval == GDK_space)
    return FALSE;
  
  if (event->keyval == GDK_Return) {
    if (!GTK_CLIST_CHILD_HAS_FOCUS (clist)) {
      event->keyval = GDK_space;
      return FALSE;
    }
    autocompletition_contacts_destroy ();
  }
  
  /* else send the key event to the location entry */
  tmp_event.type = event->type;
  tmp_event.window = location_entry->window;
  tmp_event.send_event = TRUE;
  tmp_event.time = event->time;
  tmp_event.state = event->state;
  tmp_event.keyval = event->keyval;
  tmp_event.length = event->length;
  tmp_event.string = event->string;
  gtk_widget_event(location_entry, (GdkEvent *)&tmp_event);
  
  return TRUE;
}
#endif
