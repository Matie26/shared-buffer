CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic
LDFLAGS=-lpthread -lrt

sharedBuffer: sharedBuffer.o
	$(CC) $(CFLAGS) sharedBuffer.o -o sharedBuffer $(LDFLAGS)

sharedBuffer.o: sharedBuffer.c
	$(CC) $(CFLAGS) -c sharedBuffer.c -o sharedBuffer.o

clean:
	rm sharedBuffer.o sharedBuffer