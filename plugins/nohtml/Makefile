LDFLAGS = -ggdb
CFLAGS = -Wall
CC = gcc
OBJECTS = nohtml.o
BIN = nohtml.so
PREFIX=`cronosII-config --prefix`
DATADIR=`cronosII-config --datadir`

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -I../../ -DSKIPCONFIG -shared -Wl,-soname,nohtml.so -o $@ $(OBJECTS) -lc

nohtml.o : nohtml.c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` nohtml.c -c

configure.o : configure.c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` configure.c -c

clean :
	rm -f core $(OBJECTS) $(BIN)

install : $(BIN)
	$(PREFIX)/bin/install -c -m 644 $(BIN) $(DATADIR)/plugins
