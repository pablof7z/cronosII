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
#ifndef PLUGIN_H
#define PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gnome.h>
  
#define C2_DYNAMIC_MODULE_SIGNAL_FUNC(func)	((C2DynamicModuleSignalFunc) func)

typedef void (*C2DynamicModuleSignalFunc) 	(gpointer d1, gpointer d2, gpointer d3,
    						 gpointer d4, gpointer d5);
typedef struct _C2DynamicModule C2DynamicModule;

struct _C2DynamicModule {
  void *fd;
  char *filename;
  char *configfile;
  char *name;
  char *version;
  char *author;
  char *url;
  char *description;
  GtkWidget *(*configure) (C2DynamicModule *module);
  struct _C2DynamicModule *next;
};

typedef enum {
  C2_DYNAMIC_MODULE_CHECK_NEW_SESSION,
  C2_DYNAMIC_MODULE_CHECK_NEW_ACCOUNT,
  
  C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_POP,
  C2_DYNAMIC_MODULE_MESSAGE_DOWNLOAD_SPOOL,
  
  C2_DYNAMIC_MODULE_COMPOSER_SEND,
  C2_DYNAMIC_MODULE_COMPOSER_INSERT_SIGNATURE,
 
  /* Available windows: "main", "composer", "checker" */
  C2_DYNAMIC_MODULE_WINDOW_FOCUS,
  
  C2_DYNAMIC_MODULE_MESSAGE_OPEN,

  C2_DYNAMIC_MODULE_MESSAGE_DISPLAY,

  
  C2_DYNAMIC_MODULE_LAST
} C2DynamicModuleSignal;

typedef struct _C2DynamicModuleSignalList {
  C2DynamicModuleSignalFunc func;
  const char *owner;

  struct _C2DynamicModuleSignalList *next;
} C2DynamicModuleSignalList;

C2DynamicModuleSignalList *signals[C2_DYNAMIC_MODULE_LAST];

int
c2_dynamic_module_load					(const char *filename, C2DynamicModule **head);

int
c2_dynamic_module_unload				(const char *name, C2DynamicModule **head);

C2DynamicModule *
c2_dynamic_module_find					(const char *name, const C2DynamicModule *head);

char *
c2_dynamic_module_get_config_file			(const char *name);

C2DynamicModule *
c2_dynamic_module_copy_list				(const C2DynamicModule *head);

void
c2_dynamic_module_free					(C2DynamicModule **item);

void
c2_dynamic_module_free_list				(C2DynamicModule **head);

void
c2_dynamic_module_autoload				(void);

void
c2_dynamic_module_signal_connect			(const char *module_name,
    							 C2DynamicModuleSignal signal,
    							 C2DynamicModuleSignalFunc func);

void
c2_dynamic_module_signal_disconnect			(const char *module_name,
    							 C2DynamicModuleSignal signal);

void
c2_dynamic_module_signal_emit				(C2DynamicModuleSignal signal_name,
    							 gpointer d1, gpointer d2, gpointer d3,
							 gpointer d4, gpointer d5);

#endif

#if USE_PLUGINS
#  define PLUGIN_CHECK_USE_PLUGIN 1
#else
#  define PLUGIN_CHECK_USE_PLUGIN 0
#endif

#ifdef __cplusplus
}
#endif
