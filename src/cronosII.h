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
#ifndef __CRONOS_II_H__
#define __CRONOS_II_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <cronosII/config.h>
#include <cronosII/account.h>
#include <cronosII/check.h>
#include <cronosII/debug.h>
#include <cronosII/error.h>
#include <cronosII/exit.h>
#include <cronosII/gnomefilelist.h>
#include <cronosII/gui-about.h>
#include <cronosII/gui-composer.h>
#include <cronosII/gui-decision_dialog.h>
#include <cronosII/gui-export.h>
#include <cronosII/gui-import.h>
#include <cronosII/gui-logo.h>
#include <cronosII/gui-mailbox_tree.h>
#include <cronosII/gui-preferences.h>
#include <cronosII/gui-print.h>
#include <cronosII/gui-select_file.h>
#include <cronosII/gui-select_mailbox.h>
#include <cronosII/gui-utils.h>
#include <cronosII/gui-window_checking.h>
#include <cronosII/gui-window_main.h>
#include <cronosII/gui-window_main_callbacks.h>
#include <cronosII/gui-window_message.h>
#include <cronosII/index.h>
#include <cronosII/init.h>
#include <cronosII/mailbox.h>
#include <cronosII/main.h>
#include <cronosII/message.h>
#include <cronosII/net.h>
#include <cronosII/pop.h>
#include <cronosII/rc.h>
#include <cronosII/search.h>
#include <cronosII/smtp.h>
#include <cronosII/spool.h>
#include <cronosII/utils.h>
#include <cronosII/version.h>

#ifdef BUILD_ADDRESS_BOOK
#  include <cronosII/addrbook.h>
#  include <cronosII/gui-addrbook.h>
#endif

#ifdef USE_PLUGINS
#  include <cronosII/plugins.h>
#endif

#ifdef __cplusplus
}
#endif

#endif
