LDFLAGS = -ggdb
CFLAGS = -Wall -Wstrict
CC = gcc
OBJECTS = filters.o
BIN = filters.so
PREFIX=`cronosII-config --prefix`
DATADIR=`cronosII-config --datadir`

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -I../../ -DSKIPCONFIG -shared -Wl,-soname,filters.so -o $@ $(OBJECTS) -lc

filters.o : filters.c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` filters.c -c

clean :
	rm -f core $(OBJECTS) $(BIN)

install : $(BIN)
	$(PREFIX)/bin/install -c -m 644 $(BIN) $(DATADIR)/plugins
