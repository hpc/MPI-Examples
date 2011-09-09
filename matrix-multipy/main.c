/* File: main.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: A simple matrix multiplication program
*/
#include<stdio.h>  
#include<stdlib.h> 		/* exit() */
#include<getopt.h> 		/* getopt() */
#include<sys/resource.h> 	/* getrusage() */
#include<sys/time.h> 		/* For Solaris */
#include<time.h>   		/* time() */

/* Function to print program usage.  Gets printed when a user doesn't supply
   the proper arguments. Terminates the program. */
void usage()
{
    printf("Usage: mm -n <size> <option>\n" \
	   "\tFlag\tTiming Function\n" \
	   "\t-t\ttime()\n" \
	   "\t-g\tclock_gettime()\n" \
	   "\t-u\tgetrusage()\n");
#if defined (__SVR4) && defined (__sun)
	  printf("\t-r\tgethrtime()\n");
#else
	  
#endif
    exit(1);
}

int row_col_mult(int * a, int ** b, int col, int n)
{
    int sum = 0,i = 0;
    for(i = 0; i < n; i++)
	    sum += a[i]*b[i][col];
    return sum;
}

/* Function to multiply two matrices of the same dimensions (a and b), the result
   being stored in result. */
void matrix_multiply(int ** a, int ** b, int n, int ** result)
{
    int i,j;
    for(i = 0; i < n; i++)
	for(j = 0; j < n; j++)
	{
	    /* The function row_col_mult returns the correct value for the [i][j]
	       entry in the resulting matrix */
	    result[i][j] = row_col_mult(a[i],b,j,n);
	}
}

/* Function to print a matrix to the screen */
void print_matrix(int ** m, int n)
{
    int i,j;
    printf("%20d",0);
    for(i = 0; i < n; i++)
    	printf("%20d",i);
    printf("\n");
    for(i = 0; i < n; i++)
    {
	printf("%20d",i);
	for(j = 0; j < n; j++)
	    printf("%20d",m[i][j]);
	printf("\n");
    }
    printf("\n");

}


int main(int argc, char** argv)
{
    /* Used to choose the instrumentation vehicle */
    enum { TIME, CLOCKGETTIME, GETRUSAGE, GETHRTIME };
    int n = 0;
    int c = 0;
    int flag = 0;
    int time_flag = 0;
    /* Parse command line arguments */
    while ((c = getopt( argc, argv, "gutrn:")) != -1)
	switch(c)
	{
	    case 'n':
		n = atoi(optarg);
		flag++;
		break;
	    case 't':
		time_flag = TIME;
		flag++;
		break;
	    case 'g':
		time_flag = CLOCKGETTIME;
		flag++;
		break;
	    case 'u':
		time_flag = GETRUSAGE;
		flag++;
		break;
	    case 'r':
/* If the system is Solaris Unix, we allow this flag.  Otherwise,
   gethrtime() is not available. This should be semi portable. */
#if defined (__SVR4) && defined (__sun)
		time_flag = GETHRTIME;
		flag++;
#else
		fprintf(stderr,"gethrtime() Not available on this system.\n");
		usage();
#endif
		break;		
	    default: usage();
	}
    /* Make sure we got all the arguments we need */
    if(flag <= 1 || !n)
	usage();

    /* Calculate memory consumption.  This is one int for every entry in an 
       n X n array, times 3 (for a,b,result). */
    double memsize = (double) n * n * sizeof(int) * 3;
    char unit[2] = " B";
    if(memsize > 1024)
    {
	memsize = memsize/1024;
	unit[0] = 'K';
    }
    if(memsize > 1024)
    {
	memsize = memsize/1024;
	unit[0] = 'M';
    }
    if(memsize > 1024)
    {
	memsize = memsize/1024;
	unit[0] = 'G';
    }        
    printf("Allocating %.2f %s of memory.\n", memsize,unit);

    /* Allocate memory for the matrices.  They are stored
       as an array of pointers, each one pointing to an array itself. 
       These lines allocate memory for the 1st dimension of the table. */
    int ** a = (int **) malloc (sizeof(int**) * n);
    int ** b = (int **) malloc (sizeof(int**) * n);
    int ** r = (int **) malloc (sizeof(int**) * n);

    /* Allocate memory for the 2nd dimension of each array. There is a list
       corresponding to each value of n. */
    int i,j;
    for(i = 0; i < n; i++)
    {
	a[i] = (int*) malloc(sizeof(int) * n);
	b[i] = (int*) malloc(sizeof(int) * n);
	r[i] = (int*) malloc(sizeof(int) * n);
    }

    /* Populate the arrays (a,b) with random numbers, modulo 100.  This is done
       to try and keep from overflowing the integer entries in the resulting array. */
    srand(time(0));
    for(i = 0;  i < n; i++)
	for(j = 0; j < n; j++)
	{
	    a[i][j] = rand() % 100;
	    b[i][j] = rand() % 100;
	    r[i][j] = 0;
	}


    /* Timing code */
    struct timespec start_time,end_time,elapsed, user,kernel;
    struct rusage start_usage,end_usage;
    /* Here we just determine what the user's choice was for instrumentation. 
       How the resulting values get printed depend on the choice.  Some values have
       multiple components. */
    if(time_flag == CLOCKGETTIME)
    {
	clock_gettime(CLOCK_REALTIME, &start_time);
	/* Multiply the matrices */
	matrix_multiply(a,b,n,r); 
	clock_gettime(CLOCK_REALTIME, &end_time);
    }
    else if(time_flag == TIME)
    {
	start_time.tv_nsec = end_time.tv_nsec = 0;
	time(&start_time.tv_sec);
	/* Multiply the matrices */
	matrix_multiply(a,b,n,r); 
	time(&end_time.tv_sec);
    }
    else if(time_flag == GETRUSAGE)
    {
	getrusage(RUSAGE_SELF, &start_usage);
	/* Multiply the matrices */
	matrix_multiply(a,b,n,r); 
	getrusage(RUSAGE_SELF, &end_usage);
	user.tv_sec = end_usage.ru_utime.tv_sec - start_usage.ru_utime.tv_sec;
	user.tv_nsec = 1000*(end_usage.ru_utime.tv_usec - start_usage.ru_utime.tv_usec);
	kernel.tv_sec = end_usage.ru_stime.tv_sec - start_usage.ru_stime.tv_sec;
	kernel.tv_nsec = 1000*(end_usage.ru_stime.tv_usec - start_usage.ru_stime.tv_usec);
	printf("%10lu.%09lu s user time\n",user.tv_sec,user.tv_nsec); 
	printf("%10lu.%09lu s kernel time\n",kernel.tv_sec,kernel.tv_nsec); 
    }
#if defined (__SVR4) && defined (__sun)
    else if(time_flag == GETHRTIME)
    {
	hrtime_t start = gethrtime();
	/* Multiply the matrices */
	matrix_multiply(a,b,n,r);
	hrtime_t end = gethrtime();
        printf("Elapsed time: %f s\n",((double)end - start)/1000000000.0);
    }
#endif
    if(time_flag != GETRUSAGE && time_flag != GETHRTIME)
    {
	elapsed.tv_sec = end_time.tv_sec - start_time.tv_sec;
	elapsed.tv_nsec = end_time.tv_nsec - start_time.tv_nsec;
	if(elapsed.tv_nsec < 0)
	{
	    elapsed.tv_sec -= 1;
	    elapsed.tv_nsec += 1000000000;
	}
	/* Print results */
	printf("Elapsed time: %ld.%ld \n", elapsed.tv_sec , elapsed.tv_nsec);
    }
    /* Print the first and last value in the array. */
    printf("Element [0][0] = %d\nElement [%d][%d] = %d\n",r[0][0],n,n,r[n-1][n-1]);
    exit(0); 
}
