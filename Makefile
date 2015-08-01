all: pingfs

OBJS=icmp.o host.o pingfs.o fs.o net.o chunk.o
LDFLAGS=-lanl -lrt `pkg-config fuse --libs`
CFLAGS+=--std=c99 -Wall -Wshadow -pedantic -g `pkg-config fuse --cflags`
CFLAGS+=-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809 -D_XOPEN_SOURCE

pingfs: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

