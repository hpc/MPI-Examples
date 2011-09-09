/* File: golberg.c
    Authors: Jharrod LaFon, Michael Harris
    Date: Spring 2011
    Purpose: Test Goldberg's Conjecture
*/
#include<stdio.h>
#include<stdlib.h>
#include<omp.h>
#include<sys/time.h>
#include<math.h>
#include<time.h>
#include<unistd.h>
#define MAX 500
#define MIN 2

// returns 1 if number p is prime, 0 otherwise
int prime(int p)
{
    int result = 1;
    int i;
    for(i = 2; i < sqrt(p); i++)
	if(p % i == 0)
	{
	    result = 0;
	    break;
	}
    return result;
}

// returns the number of combinations of primes that equal p
int gold(int p)
{
    int result = 0;
    int i,j;
    for(i=0;i<p;i++)
	if(prime(i))
	    for(j=0;j<p;j++)
		if(prime(j) && (i+j) == p)
		    result++;
    return result;
}
void usage()
{
    printf("Usage: goldberg [ARGS]\n\
	    -n <int>\t\tNumber of threads\n\
	    -s <int>\t\tSchedule.\n\
	    \t\t\tStatic\t1\n\
	    \t\t\tDynamic\t2\n\
	    \t\t\tGuided\t3\n\
	    \t\t\tAuto\t4\n\
	    -m <int>\t\tSchedule modifier\n\
	    \t\t\tFor Dynamic,guided & static this is the chunk size\n\
	    -h\t\t\tPrint this message\n");
}
int main(int argc, char*argv[])
{
    int max = MAX;
    int min = MIN;
    int size = (max-min)/2;
    int * array = (int*) malloc(sizeof(int)*size);
    int i;
    struct timespec start, end,elapsed;
    int c = 0;
    int num_threads = 1;
    int modifier = 0;
    omp_sched_t schedule = 1;
    while((c = getopt(argc,argv,"n:s:m:h")) != -1)
    {
        switch(c)
        {
            case 'n':
                num_threads = atoi(optarg);
                break;
	    case 's':
		schedule = atoi(optarg);
		break;
	    case 'm':
		modifier = atoi(optarg);
		break;
	    case 'h':
		usage();
		exit(0);
            default:
                break;
        }
    }
    omp_set_num_threads(num_threads);
    omp_set_schedule(schedule,modifier);
    clock_gettime(CLOCK_REALTIME, &start);
    for(i = 1; i < size; i++)
	array[i] = 0;
#pragma omp parallel for
    for(i = 1; i < size; i++)
	array[i] = gold(2*i);
#pragma omp parallel for
    for(i = 1; i < size; i++)
	if(array[i] == 0)
	    fprintf(stdout,"Violated at %d\n",2*i);
    clock_gettime(CLOCK_REALTIME, &end);
    elapsed.tv_sec = end.tv_sec - start.tv_sec;
    elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    if(elapsed.tv_nsec < 0)
    {
	elapsed.tv_sec -= 1;
	elapsed.tv_nsec += 1000000000;
    }
    printf("%ld.%ld\n",elapsed.tv_sec,elapsed.tv_nsec);
    return 0;
}
