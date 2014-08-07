all: pingfs

OBJS=icmp.o
LDFLAGS=-lanl

pingfs: $(OBJS)

.PHONY = clean all
clean:
	@rm -f *.o pingfs

