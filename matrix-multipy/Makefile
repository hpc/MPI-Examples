CC = gcc
LDFLAGS = -lrt

main: main.c
	$(CC) -c main.c

linux: main.o
	$(CC) $(LDFLAGS) main.o -o mm

solaris: main.o
	$(CC) $(LDFLAGS) main.o -o mm_solaris

clean:
	rm *.o mm mm_solaris
