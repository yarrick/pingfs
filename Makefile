all: pinger pingfs

OBJS=icmp.o
LDFLAGS = -lanl

pinger: $(OBJS) pinger.o
	$(CC) $(OBJS) pinger.o -o $@ $(LDFLAGS)

pingfs: pingfs.o
	$(CC) pingfs.o -o $@ $(LDFLAGS)

.c.o: $<
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -f *.o pinger pingfs

