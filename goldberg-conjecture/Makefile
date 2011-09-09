LIBS    += -lm -lrt -fopenmp
CFLAGS  += -Wall

all: goldberg

goldberg: goldberg.c
	$(CC) $(INCLUDE) $(LIBS) $(CFLAGS) $@.c -o $@ 

clean:
	- /bin/rm -f goldberg.c
	- /bin/rm -f *.o
	- /bin/rm -f *.x

