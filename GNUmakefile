CC      = gcc
CFLAGS  = # -g
LIBS    = -lircclient -lpthread -lcrypto -lssl
PREFIX  = /usr/local

OS     := $(shell uname -s)
HOST   := $(shell hostname)

ifeq ($(OS),Linux)
	ifeq ($(HOST),ma.sdf.org)
		CFLAGS += -I$(HOME)/.local/include -L$(HOME)/.local/lib
		PREFIX  = $(HOME)/.local
	endif
endif
ifeq ($(OS),Darwin)
	CFLAGS += -L/opt/homebrew/lib -I/opt/homebrew/include
	LIBS   += -largp
endif
ifeq ($(OS),FreeBSD)
	LIBS   += -largp
endif
ifeq ($(OS),NetBSD)
	DOMAIN := $(shell domainname)
	ifeq ($(DOMAIN),SDF)
		CFLAGS += -I$(HOME)/.local/include -L$(HOME)/.local/lib
		PREFIX  = $(HOME)/.local
	endif
	LIBS   += -largp
endif
ifeq ($(OS),OpenBSD)
	LIBS   += -largp
endif
ifeq ($(OS),UNIX)
	LIBS   += -largp
endif

ircom: ircom.c arguments.c handlers.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom

install: ircom
	install -d $(PREFIX)/bin
	install -m 755 ircom $(PREFIX)/bin/ircom