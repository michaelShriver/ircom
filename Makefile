CC		= gcc
CFLAGS	= -g
LIBS	= -lircclient -lpthread -lcrypto -lssl
PREFIX  = /usr/local

ircom: ircom.c handlers.c arguments.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom

install: ircom
	install -d $(PREFIX)/bin
	install -m 755 ircom $(PREFIX)/bin/ircom
