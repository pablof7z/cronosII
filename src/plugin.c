#include <gnome.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <config.h>

#include "main.h"
#include "plugin.h"
#include "utils.h"
#include "init.h"
#include "debug.h"
#include "error.h"

#if USE_PLUGINS
/**
 * c2_dynamic_module_load
 * @filename: A pointer to the name of the file to be loaded.
 * @head: A double-pointer to the first element of the linked list where to load this module.
 *
 * Loads a module into memory.
 *
 * Return Value:
 * TRUE in case of success, FALSE if there's an error.
 **/
int
c2_dynamic_module_load (const char *filename, C2DynamicModule **head) {
  void *fd;
  C2DynamicModule *module;
  C2DynamicModule *s;
  char *(*module_init) (int major_version, int minor_version, int micro_version, C2DynamicModule *module);
  char *error = NULL;
  
  g_return_val_if_fail (filename, FALSE);

  if ((fd = dlopen (filename, RTLD_NOW)) == NULL) {
#define HAVE_DLERROR 1
#ifdef HAVE_DLERROR
    g_warning ("%s\n", CHAR (dlerror ()));
#endif
    return FALSE;
  }

  if ((module_init = dlsym (fd, "module_init")) == NULL) {
    g_warning (_("The %s is not a valid module (failed call to module_init)\n"), filename);
    return FALSE;
  }
  module = g_new0 (C2DynamicModule, 1);
  if ((error = module_init (C2_PLUGIN_MAJOR_VERSION, C2_PLUGIN_MINOR_VERSION, C2_PLUGIN_MICRO_VERSION,module))) {
    GtkWidget *window = gnome_ok_dialog (error);

    gnome_dialog_run_and_close (GNOME_DIALOG (window));
    dlclose (fd);
    c2_free (module);
    return FALSE;
  }
  module->fd = fd;
  module->filename = g_strdup (g_basename (filename));
  module->next = NULL;

  /* Append to the linked list */
  for (s = *head; s != NULL && s->next != NULL; s = s->next);
  if (s) s->next = module;
  else *head = module;

  return TRUE;
}

/**
 * c2_dynamic_module_unload
 * @name: Name of module.
 * @head: First element of linked list where to search for this module.
 *
 * Unloads a module.
 *
 * Return Value:
 * TRUE in case of success, FALSE if there's an error.
 **/
int
c2_dynamic_module_unload (const char *name, C2DynamicModule **head) {
  C2DynamicModule *module = NULL, *prev = NULL, *next = NULL;
  char *path;
  void (*module_cleanup) (C2DynamicModule *module);
  
  g_return_val_if_fail (name, FALSE);
  g_return_val_if_fail (head, FALSE);

  for (module = *head; module; module = module->next) {
    if (streq (module->name, name)) break;
    if (!prev) prev = *head;
    else prev = prev->next;
  }
  
  if (!module) {
    g_warning (_("Unable to find Plug-In with name %s\n"), name);
    return FALSE;
  }
  
  next = module->next;

  module_cleanup = dlsym (module->fd, "module_cleanup");
  if (module_cleanup) {
    module_cleanup (module);
  }

  path = g_strconcat (getenv ("HOME"), ROOT, "/Plugins/", module->filename, NULL);
  unlink (path);
  c2_free (path);
  if (prev) prev->next = next; 
  else *head = next;
  c2_dynamic_module_free (&module);

  return TRUE;
}

/**
 * c2_dynamic_module_find
 * @name: Name of desired module.
 * @head: First element of linked list where to search for the module.
 *
 * Searchs for a module in a linked list.
 *
 * Return Value:
 * A pointer to the desired module or NULL if it wasn't found.
 **/
C2DynamicModule *
c2_dynamic_module_find (const char *name, const C2DynamicModule *head) {
  const C2DynamicModule *s;

  g_return_val_if_fail (name, NULL);
  if (!head) return NULL;

  for (s = head; s; s = s->next) {
    if (streq (s->name, name)) return (C2DynamicModule*)s;
  }

  return NULL;
}

/**
 * c2_dynamic_module_get_config_file
 * @name: Name of module.
 *
 * Creates the configuration directory if it doesn't exists
 * and creates a char object containing the
 * path to the config file of the module.
 *
 * Return Value:
 * A pointer to a char object or NULL in case
 * of error.
 **/
char *
c2_dynamic_module_get_config_file (const char *name) {
  char *buf, *buf2;
  char *path;
  
  g_return_val_if_fail (name, NULL);
  
  path = g_strdup_printf ("%s%s/Plugins/.Config", getenv ("HOME"), ROOT);
  if (!c2_file_exists (path)) {
    if (mkdir (path, 0700) < 0) {
      buf2 = g_strerror (errno);
      buf = g_strdup_printf (_("Couldn't create the directory %s: %s"), path, buf2);
      c2_free (path);
      gnome_dialog_run_and_close (GNOME_DIALOG (gnome_ok_dialog (buf)));
      return NULL;
    }
  }
  c2_free (path);
  path = g_strdup_printf ("%s%s/Plugins/.Config/%s.conf", getenv ("HOME"), ROOT, name);
  return path;
}

/**
 * c2_dynamic_module_copy_list
 * @head: A pointer to a C2DynamicModule object.
 *
 * Copies an entire linked list of C2DynamicModule's objects.
 *
 * Return Value:
 * A pointer to a C2DynamicModule object.
 **/
C2DynamicModule *
c2_dynamic_module_copy_list (const C2DynamicModule *head) {
  C2DynamicModule *copy = NULL;
  const C2DynamicModule *ptr;
  C2DynamicModule *ptr2, *ptr3 = NULL;
  
  if (!head) return NULL;

  for (ptr = head; ptr != NULL; ptr = ptr->next) {
    ptr2 = g_new0 (C2DynamicModule, 1);
    ptr2->fd		= ptr->fd;
    ptr2->filename	= g_strdup (ptr->filename);
    ptr2->name		= g_strdup (ptr->name);
    ptr2->version	= g_strdup (ptr->version);
    ptr2->author	= g_strdup (ptr->author);
    ptr2->url		= g_strdup (ptr->url);
    ptr2->description	= g_strdup (ptr->description);
    ptr2->next		= NULL;
    if (!ptr3) copy = ptr3 = ptr2;
    else {
      ptr3->next = ptr2;
      ptr3 = ptr2;
    }
  }

  return copy;
}

/**
 * c2_dynamic_module_free
 * @item: A pointer to a C2DynamicModule object.
 *
 * Frees a C2DynamicModule object.
 **/
void
c2_dynamic_module_free (C2DynamicModule **item) {
  g_return_if_fail (*item);

  c2_free ((*item)->filename);
  c2_free (*item);
  *item = NULL;
}

/**
 * c2_dynamic_module_free_list
 * @head: A pointer to a C2DynamicModule object.
 *
 * Frees an intire C2DynamicModule objects linked list.
 **/
void
c2_dynamic_module_free_list (C2DynamicModule **head) {
  C2DynamicModule *s, *next;

  g_return_if_fail (*head);
  
  for (s = next = *head; next; s = next) {
    next = s->next;
    c2_dynamic_module_free (&s);
  }
}

/**
 * c2_dynamic_module_autoload
 *
 * Loads all the modules in the ~/.CronosII/Plugins/ directory
 **/
void
c2_dynamic_module_autoload (void) {
  DIR *directory;
  struct dirent *dir;
  char *path;
  char *buf;

  path = g_strdup_printf ("%s" ROOT "/Plugins/", getenv ("HOME"));

  if ((directory = opendir (path)) == NULL) {
    if (errno == ENOENT) return;
    cronos_error (errno, _("Couldn't open the Plug-Ins directory"), ERROR_WARNING);
  }

  for (; (dir = readdir (directory)) != NULL;) {
    if (strneq (dir->d_name, ".", 1)) continue;
    buf = g_strconcat (path, dir->d_name, NULL);
    c2_dynamic_module_load (buf, &config->module_head);
    c2_free (buf);
  }
  c2_free (path);
  closedir (directory);
}

/**
 * c2_dynamic_module_signal_connect
 * @module_name: Name of the dynamic module calling this function.
 * @signal: Type of signal.
 * @func: Callback to be called when this signal is emited.
 *
 * Connects a callback function to be called when the signal @signal
 * is emited.
 **/
void
c2_dynamic_module_signal_connect (const char *module_name, C2DynamicModuleSignal signal,
    				  C2DynamicModuleSignalFunc func) {
  C2DynamicModuleSignalList *s;
  C2DynamicModuleSignalList *new;
  
  g_return_if_fail (module_name);
  g_return_if_fail (signal < C2_DYNAMIC_MODULE_LAST);
  g_return_if_fail (func);

  new = g_new0 (C2DynamicModuleSignalList, 1);
  new->func = func;
  new->owner = module_name;
  new->next = NULL;
  
  for (s = signals[signal]; s && s->next; s = s->next) {
    if (streq (s->owner, module_name)) {
      g_warning (_("The dynamic module %s has already registered a callback for the signal %d.\n"
	    	   "Each dynamic module can register just one callback for each signal.\n"),
	  		module_name, signal);
      c2_free (new);
      return;
    }
  }
  if (s) s->next = new;
  else signals[signal] = new;
}

/**
 * c2_dynamic_module_signal_disconnect
 * @module_name: Name of the dynamic module calling this function.
 * @signal: Type of signal.
 *
 * Disconnects a callback.
 **/
void
c2_dynamic_module_signal_disconnect(const char *module_name, C2DynamicModuleSignal signal) {
  C2DynamicModuleSignalList *s, *prev = NULL, *next;
  
  g_return_if_fail (module_name);
  g_return_if_fail (signal < C2_DYNAMIC_MODULE_LAST);
  
  for (s = signals[signal]; s; s = s->next) {
    if (streq (s->owner, module_name)) break;
    if (!prev) prev = signals[signal];
    else prev = prev->next;
  }

  if (!s) {
    g_warning (_("There's no callback function attached to the signam %d in the dynamic module %s.\n"),
			signal, module_name);
    return;
  }

  next = s->next;
  if (prev) prev->next = next;
  else signals[signal] = next;
  c2_free (s);
}

/**
 * c2_dynamic_module_signal_emit
 * @signal_name: A signal number.
 * @d1: Data 1. Any data that the function needs to pass to the callback.
 * @d2: Data 2. Any data that the function needs to pass to the callback.
 * @d3: Data 3. Any data that the function needs to pass to the callback.
 * @d4: Data 4. Any data that the function needs to pass to the callback.
 * @d5: Data 5. Any data that the function needs to pass to the callback.
 *
 * Emits a signal of the number @signal_name and calls all of the functions
 * attached to that signal.
 * Note that the function calling this function should be in its own thread,
 * since this is a blocking function and we don't want to block de gdk_thread.
 * 
 * This function calls all of the callbacks in the linked list of this signal
 * waiting for each function to finish before calling the next.
 **/
void
c2_dynamic_module_signal_emit				(C2DynamicModuleSignal signal_name,
    							 gpointer d1, gpointer d2, gpointer d3,
							 gpointer d4, gpointer d5) {
  C2DynamicModuleSignalList *s;
  
  g_return_if_fail (signal_name < C2_DYNAMIC_MODULE_LAST);
  
  for (s = signals[signal_name]; s; s = s->next) {
    s->func (d1, d2, d3, d4, d5);
  }
}

#endif
