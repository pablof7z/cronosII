# This file is not standard but should work with or with the db2 tools or gnomedoc stuff.
# It installs in all the correct gnome places. --pete feb 2002.
cronosII_helpdir = $(datadir)/gnome/help/cronosII/C

cronosII_help_DATA =         \
        topic.dat

SGML_FILES =                    \
        cronosII.sgml		\
	 accounts.sgml	\
	 addressbook.sgml		\
	 authors.sgml	\
	bugs.sgml	\
	gtk-brief.sgml	\
	latest.sgml	\
	plugins.sgml	\
	shortcuts_table.sgml	


# files that aren't in a binary/data/library target have to be listed here
# to be included in the tarball when you 'make dist'
EXTRA_DIST = 	\
        topic.dat               \
        $(SGML_FILES)	\
	cronosII-htmldocs.tar.gz


## The - before the command means to ignore it if it fails.  that way
## people can still build the software without the docbook tools

all: cronosII/index.html

cronosII/index.html: $(SGML_FILES)
#	-db2html cronosII.sgml	left out as a work around gnomedocs.dssl probs. *&^%# --pete feb2002

## when we make dist, we include the generated HTML so people don't
## have to have the docbook tools
dist-hook:
	mkdir $(distdir)/cronosII
	-cp cronosII/*.html cronosII/*.css $(distdir)/cronosII
	mkdir $(distdir)/cronosII/stylesheet-images
	-cp cronosII/stylesheet-images/* $(distdir)/cronosII/stylesheet-images/


install-data-local: cronosII/index.html
# this tar added --pete Feb 2002 (try keeping things nice building an rpm without it !)
	tar zxvf  $(srcdir)/cronosII-htmldocs.tar.gz ;	
	$(mkinstalldirs) $(cronosII_helpdir)/stylesheet-images	#/ 
#	-for file in $(srcdir)/cronosII/*.html  $(srcdir)/cronosII/stylesheet-images/* $(srcdir)/cronosII/*.css ;
#	do	
#	basefile=`basename $$file`; \
#	$(INSTALL_DATA) $(srcdir)/$$file $(cronosII_helpdir)/$$basefile; \
#	done
#while that should have done it the following covers some other situations and should not hurt.
	cp -vf $(srcdir)/cronosII/*.html  $(cronosII_helpdir)/;	
	-cp -vf $(srcdir)/cronosII/stylesheet-images/*  $(cronosII_helpdir)/stylesheet-images/ ;	
	-cp -vf $(srcdir)/cronosII/images/*  $(cronosII_helpdir)/;

 #added above cp's feb 2002 this covers the two most common scenarios and should only copy what is required. 
# next we get rid of the html directory (don't need it any more and it will be a minor hassle for people using sgml tools)
	-rm -rf $(srcdir)/cronosII/ ; 

cronosII.ps: cronosII.sgml
	-db2ps $<

cronosII.rtf: cronosII.sgml       
	-db2rtf $<
