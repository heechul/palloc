CC=gcc
PGMS=mc-mapping mc-mapping-pagemap

CFLAGS=-Wall

all: $(PGMS)

mc-mapping: mc-mapping.o
	$(CC) $< -O2 -o $@ -lrt -g

mc-mapping-pagemap: mc-mapping-pagemap.o
	$(CC) $< -O2 -o $@ -lrt -g

clean:
	rm *.o *~ $(PGMS)
