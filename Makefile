all: pingfs

OBJS=icmp.o
LDFLAGS=-lanl
CPPFLAGS=--std=c99

pingfs: $(OBJS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

