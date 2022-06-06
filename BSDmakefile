CC      = gcc
CFLAGS  = -g
LIBS    = -lircclient -lpthread -lcrypto -lssl -largp
PREFIX  = /usr/local

OS     := $(shell uname -s)

.if $(OS)=="DARWIN"
	CFLACS += -L/opt/homebrew/lib -I/opt/homebrew/include
.endif  

ircom: ircom.c arguments.c handlers.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom

install: ircom
	install -d $(PREFIX)/bin
	install -m 755 ircom $(PREFIX)/bin/ircom