CC		= gcc
CFLAGS	= 
LIBS	= -lircclient -lpthread

ircom: ircom.c handlers.c functions.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom
