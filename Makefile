all: pingfs

OBJS=icmp.o host.o
LDFLAGS=-lanl
CFLAGS+=--std=c99 -Wall -pedantic -g

pingfs: $(OBJS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

