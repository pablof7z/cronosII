LDFLAGS = -ggdb
CFLAGS = -Wall
CC = gcc
OBJECTS = sample.o
BIN = sample.so
PREFIX=`cronosII-config --prefix`
DATADIR=`cronosII-config --datadir`

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -I../../ -DSKIPCONFIG -shared -Wl,-soname,sample.so -o $@ $(OBJECTS) -lc

sample.o : sample.c
	$(CC) `gnome-config --cflags gnome gnomeui` sample.c -c

configure.o : configure.c
	$(CC) `gnome-config --cflags gnome gnomeui` configure.c -c

clean :
	rm -f core $(OBJECTS) $(BIN)

install : $(BIN)
	$(PREFIX)/bin/install -c -m 644 $(BIN) $(DATADIR)/plugins
