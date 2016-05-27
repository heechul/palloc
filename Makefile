CC=gcc
PGMS=mc-mapping 

CFLAGS=-Wall

all: $(PGMS)

mc-mapping: mc-mapping.o
	$(CC) $< -O2 -o $@ -lrt -g

clean:
	rm *.o *~ $(PGMS)
