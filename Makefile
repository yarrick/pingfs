all: pingfs

OBJS=icmp.o pingfs.o

pingfs: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.c.o: $<
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -f *.o pingfs

