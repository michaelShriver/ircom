CFAGS	= 
LIBS	= -lircclient -lpthread

ircom: ircom.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm ircom
