all: pingfs

OBJS=icmp.o host.o pingfs.o
LDFLAGS=-lanl
CFLAGS+=--std=c99 -Wall -pedantic -g

pingfs: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

