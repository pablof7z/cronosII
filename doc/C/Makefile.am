cronosII_helpdir = $(datadir)/gnome/help/cronosII/C

cronosII_help_DATA =         \
        topic.dat

SGML_FILES =                    \
        cronosII.sgml

# files that aren't in a binary/data/library target have to be listed here
# to be included in the tarball when you 'make dist'
EXTRA_DIST = \
        topic.dat               \
        $(SGML_FILES)


## The - before the command means to ignore it if it fails.  that way
## people can still build the software without the docbook tools

all: cronosII/index.html

cronosII/index.html: $(SGML_FILES)
	-db2html cronosII.sgml

## when we make dist, we include the generated HTML so people don't
## have to have the docbook tools
dist-hook:
	mkdir $(distdir)/cronosII
	-cp cronosII/*.html cronosII/*.css $(distdir)/cronosII
	mkdir $(distdir)/cronosII/stylesheet-images
	-cp cronosII/stylesheet-images/* $(distdir)/cronosII/stylesheet-images/

install-data-local: cronosII/index.html
	$(mkinstalldirs) $(cronosII_helpdir)/images
	-for file in $(srcdir)/cronosII/*.html $(srcdir)/cronosII/*.css $(srcdir)/cronosII/stylesheet-images/*; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(cronosII_helpdir)/$$basefile; \
	done

cronosII.ps: cronosII.sgml
	-db2ps $<

cronosII.rtf: cronosII.sgml       
	-db2rtf $<

