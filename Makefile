all: pinger

OBJS=icmp.o pinger.o

pinger: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.c.o: $<
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -f *.o pinger

