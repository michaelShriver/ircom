CC      = gcc
CFLAGS  = # -g
LIBS    = -lircclient -lpthread -lcrypto -lssl -largp
PREFIX  = /usr/local

ircom: ircom.c arguments.c handlers.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom

install: ircom
	install -d $(PREFIX)/bin
	install -m 755 ircom $(PREFIX)/bin/ircom
