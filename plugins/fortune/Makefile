LDFLAGS = -ggdb
CFLAGS = -Wall
CC = gcc
OBJECTS = fortune.o
BIN = fortune.so
PREFIX=`cronosII-config --prefix`
DATADIR=`cronosII-config --datadir`

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -I../../ -DSKIPCONFIG -shared -Wl,-soname,fortune.so -o $@ $(OBJECTS) -lc

fortune.o : fortune.c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` fortune.c -c

clean :
	rm -f core $(OBJECTS) $(BIN)

install : $(BIN)
	$(PREFIX)/bin/install -c -m 644 $(BIN) $(DATADIR)/plugins
