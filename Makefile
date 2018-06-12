CC=gcc
CXX=g++

PGMS=mc-mapping mc-mapping-pagemap cache_attack

CFLAGS=-Wall -O2 -std=c11
CXXFLAGS=-Wall -O2 -std=c++11

all: $(PGMS)

mc-mapping: mc-mapping.c
	$(CC) $< -O2 -o $@ -lrt -g

mc-mapping-pagemap: mc-mapping-pagemap.c
	$(CC) $< $(CFLAGS) -o $@ -lrt -lpthread -g

cache_attack: cache_attack.cc
	$(CXX) -std=c++11 $< $(CXXFLAGS) -o $@ -lrt -lpthread -g
clean:
	rm *.o *~ $(PGMS)
