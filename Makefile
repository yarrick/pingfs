all: pingfs

OBJS=icmp.o
LDFLAGS = -lanl

pingfs: $(OBJS) pingfs.o
	$(CC) $(OBJS) pingfs.o -o $@ $(LDFLAGS)

.c.o: $<
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -f *.o pingfs

