
SUBDIRS = intl po src images pixmaps doc man plugins

bin_SCRIPTS = cronosII-config

cronosincludedir = $(includedir)/cronosII
cronosinclude_HEADERS = config.h

EXTRA_DIST =			\
	cronosII.spec		\
	cronosII.desktop		\
	c2addrbook.desktop	\
	cronosII-gmc.desktop	\
	c2addrbook-gmc.desktop	\
	AUTHORS		\
	COPYING		\
	FEATURES		\
	NEWS			\
	README		\
	TODO			\
	ChangeLog		\
	sample.so		\
	filters.so		\
	fortune.so		\
	nohtml.so		\
	README-developers.Changelog

Internetdir = $(datadir)/gnome/apps/Internet
Internet_DATA = cronosII.desktop
Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = c2addrbook.desktop
GMCdir = $(datadir)/mc/templates
GMC_DATA = cronosII-gmc.desktop c2addrbook-gmc.desktop

plugindir = $(pkgdatadir)/plugins
plugin_DATA = 	\
	 filters.so	\
	fortune.so  \
	sample.so	\
	nohtml.so
