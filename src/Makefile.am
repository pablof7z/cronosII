DEFS=@DEFS@ -DLOCALEDIR=\"${localedir}\"
INCLUDES = -I$(top_srcdir) -I$(includedir) \
	`gnome-config --cflags gnome` \
	 -DG_LOG_DOMAIN=\"CronosII\" -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	 -I../intl -I$(top_srcdir)/intl $(GNOME_INCLUDEDIR) -DDATADIR=\"${datadir}\"

cronosincludedir = $(includedir)/cronosII

bin_PROGRAMS = cronos c2addrbook

cronos_SOURCES =			\
	account.c			\
	addrbook.c			\
	autocompletition.c		\
	check.c				\
	error.c				\
	exit.c				\
	gnomefilelist.c			\
	gui-about.c			\
	gui-addrbook.c			\
	gui-decision_dialog.c		\
	gui-export.c			\
	gui-import.c			\
	gui-logo.c			\
	gui-mailbox_tree.c		\
	gui-preferences.c		\
	gui-print.c			\
	gui-select_file.c		\
	gui-select_mailbox.c		\
	gui-window_checking.c		\
	gui-composer.c			\
	gui-window_main.c		\
	gui-window_main_callbacks.c	\
	gui-window_message.c		\
	gui-utils.c			\
	index.c				\
	init.c				\
	mailbox.c			\
	main.c				\
	message.c			\
	net.c				\
	plugin.c			\
	pop.c				\
	rc.c				\
	search.c			\
	smtp.c				\
	spool.c				\
	utils.c				\
	version.c			\
					\
					\
					\
	account.h			\
	addrbook.h			\
	autocompletition.h		\
	check.h				\
	debug.h				\
	error.h				\
	exit.h				\
	gnomefilelist.h			\
	gui-about.h			\
	gui-addrbook.h			\
	gui-composer.h			\
	gui-decision_dialog.h		\
	gui-export.h			\
	gui-import.h			\
	gui-logo.h			\
	gui-mailbox_tree.h		\
	gui-preferences.h		\
	gui-print.h			\
	gui-select_file.h		\
	gui-select_mailbox.h		\
	gui-window_checking.h		\
	gui-window_main.h		\
	gui-window_main_callbacks.h	\
	gui-window_message.h		\
	gui-utils.h			\
	index.h				\
	init.h				\
	mailbox.h			\
	main.h				\
	message.h			\
	net.h				\
	plugin.h			\
	pop.h				\
	rc.h				\
	search.h			\
	smtp.h				\
	spool.h				\
	utils.h				\
	version.h 

cronos_LDADD = $(LIBS) $(INTLLIBS)
cronos_LDFLAGS = $(LIBS) $(INTLLIBS)

c2addrbook_SOURCES = 			\
	account.c			\
	addrbook.c			\
	bin-addrbook.c			\
	error.c				\
	gui-addrbook.c			\
	init.c				\
	mailbox.c			\
	message.c			\
	plugin.c			\
	rc.c				\
	search.c			\
	utils.c				\
	version.c			\
	account.h			\
	addrbook.h			\
	error.h				\
	gui-addrbook.h			\
	init.h				\
	mailbox.h			\
	message.h			\
	plugin.h			\
	rc.h				\
	utils.h				\
	search.h			\
	version.h

c2addrbook_LDADD = $(LIBS) $(INTLLIBS)
c2addrbook_LDFLAGS = $(LIBS) $(INTLLIBS)

cronosinclude_HEADERS =		\
	account.h		\
	addrbook.h		\
	check.h			\
	debug.h			\
	error.h			\
	exit.h			\
	gnomefilelist.h		\
	gui-about.h		\
	gui-addrbook.h		\
	gui-composer.h		\
	gui-decision_dialog.h	\
	gui-export.h		\
	gui-import.h		\
	gui-logo.h		\
	gui-mailbox_tree.h	\
	gui-preferences.h	\
	gui-print.h		\
	gui-select_file.h	\
	gui-select_mailbox.h	\
	gui-utils.h		\
	gui-window_checking.h	\
	gui-window_main.h	\
	gui-window_main_callbacks.h	\
	gui-window_message.h	\
	index.h			\
	init.h			\
	mailbox.h		\
	main.h			\
	message.h		\
	net.h			\
	plugin.h		\
	pop.h			\
	rc.h			\
	search.h		\
	smtp.h			\
	spool.h			\
	utils.h

include_HEADERS = cronosII.h
	

EXTRA_DIST = \
		xpm/drafts.xpm		\
		xpm/folder.xpm		\
		xpm/forward.xpm		\
		xpm/inbox.xpm		\
		xpm/mark.xpm		\
		xpm/mini_about.xpm	\
		xpm/mini_error.xpm	\
		xpm/new.xpm		\
		xpm/outbox.xpm		\
		xpm/queue_mbox.xpm	\
		xpm/read.xpm		\
		xpm/reply.xpm		\
		xpm/sendqueue.xpm	\
		xpm/trash.xpm		\
		xpm/unread.xpm
