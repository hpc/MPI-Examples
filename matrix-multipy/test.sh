#!/bin/bash
for i in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384
do
	echo "# Test Size $i";
        echo "time()"
	./mm -n $i -t
        echo "clock_gettime()"
	./mm -n $i -g
	echo "getrusage()"
	./mm -n $i -u
	echo "Done"
done

