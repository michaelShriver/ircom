CC		= gcc
CFLAGS	= -g
LIBS	= -lircclient -lpthread # -lcrypto -lssl

ircom: ircom.c handlers.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom
