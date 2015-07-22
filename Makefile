all: pingfs

OBJS=icmp.o host.o pingfs.o fs.o
LDFLAGS=-lanl -lrt `pkg-config fuse --libs`
CFLAGS+=--std=c99 -Wall -pedantic -g `pkg-config fuse --cflags`

pingfs: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY=clean all
clean:
	rm -f *.o pingfs

