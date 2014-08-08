all: pingfs

OBJS=icmp.o
LDFLAGS=-lanl
CFLAGS+=--std=c99 -Wall -pedantic

pingfs: $(OBJS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

