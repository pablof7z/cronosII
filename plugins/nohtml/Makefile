LDFLAGS = -ggdb
CFLAGS = -Wall
CC = gcc
NAME = nohtml
OBJECTS = $(NAME).o
BIN = $(NAME).so
PREFIX=`cronosII-config --prefix`
DATADIR=`cronosII-config --datadir`

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(CFLAGS) -I../../ -DSKIPCONFIG -shared -Wl,-soname,$(NAME).so -o $@ $(OBJECTS) -lc

$(NAME).o : $(NAME).c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` $(NAME).c -c

configure.o : configure.c
	$(CC) -fPIC `gnome-config --cflags gnome gnomeui` configure.c -c

clean :
	rm -f core $(OBJECTS) $(BIN)

install : $(BIN)
	$(PREFIX)/bin/install -c -m 644 $(BIN) $(DATADIR)/plugins
