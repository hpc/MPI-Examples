#!/bin/bash
for i in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384
do
	echo "# Test Size $i";
        echo "time()"
	./mm_solaris -n $i -t
        echo "clock_gettime()"
	./mm_solaris -n $i -g
	echo "getrusage()"
	./mm_solaris -n $i -u
	echo "gethrtime()"
	./mm_solaris -n $i -r
	echo "Done"
done

