CC = gcc
CFLAGS = -g -Wall -std=c99 -fsanitize=address,undefined -pthread

PROGRAMS = rpsd rc

all: $(PROGRAMS)

rpsd: rpsd.o network.o
	$(CC) $(CFLAGS) -o $@ $^

rc: rc.o network.o
	$(CC) $(CFLAGS) -o $@ $^

network.o rpsd.o rc.o: network.h

clean:
	rm -rf *.o $(PROGRAMS)
