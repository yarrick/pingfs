all: pingfs

OBJS=icmp.o
LDFLAGS = -lanl

pingfs: $(OBJS) pingfs.o

.PHONY = clean
clean:
	@rm -f *.o pingfs

