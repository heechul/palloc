CC=gcc
PGMS=mc-mapping mc-mapping-pagemap

CFLAGS=-Wall -O2

all: $(PGMS)

mc-mapping: mc-mapping.c
	$(CC) $< -O2 -o $@ -lrt -g

mc-mapping-pagemap: mc-mapping-pagemap.c
	$(CC) $< $(CFLAGS) -o $@ -lrt -lpthread -g
clean:
	rm *.o *~ $(PGMS)
