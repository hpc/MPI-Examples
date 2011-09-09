/* File: bucket_sort.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Parallel implementation of bucket sort
   */
#include "memwatch.h"
#include<stdio.h>
#include<stdlib.h>
//#include<string.h>
#include<unistd.h>
#include<mpi.h>
#include<math.h>
#include<time.h>
#define DEFAULT_ARRAY_SIZE 10
#define MAX_NUM_SIZE 100

/* Struct to hold configuration options */
typedef struct
{
    int rank,size, array_size,slice,max_num,verbose, print_array, print_ranges;
} Params;

/* Struct defining a bucket, that contains index items */
typedef struct
{
    int * array;
    int index;
} Bucket;

/* Inserts x into the bucket, and updates the index */
void bucket_insert(Bucket * b, int x)
{
    b->array[b->index] = x;
    b->index = b->index +1;
}

/* Function to compare two values for sorting, needed for quick sort */
int compare( const void * n1, const void * n2)
{
    return (*(int*)n1 - *(int*)n2);
}

/* Checks a bucket for sortedness */
int check_bucket(Bucket * b)
{
    int i, last = b->array[0];
    for(i = 0; i < b->index; i++)
    {
	if(b->array[i] < last)
	    return -1;
	last = b->array[i];
    }
    return 0;
}

/* Function to be executed by the workers */
void slave(Params * p)
{  
    /* Timer */
    double start, end;
    
    /* Loop Counter */
    int i;

    /* Number of large buckets, also the number of ranks */
    int num_buckets = p->size;
    
    /* Size of one large bucket, that is the array size/(number of ranks) */
    int large_bucket_size = ceil(p->array_size/(float)p->size);
    
    /* Size of small bucket size, that is a large bucket divided by the number of ranks */
    int small_bucket_size = large_bucket_size;
    
    /* Pointer to the resulting array, only malloc'd by the master rank */
    int * array;

    /* To be executed by the master rank only, sets up the large array to be scattered */
    if(p->rank == 0)
    {
        /* Start Timer */
        start = MPI_Wtime();
        
        /* Master array */
        array = (int *) malloc(sizeof(int)*p->array_size);
        
        /* Seed the random number generator */
        srand(time(NULL));
        
        /* Initialize array with random numbers, from 0 to max_num */
        for(i = 0; i < p->array_size; i++)
            array[i] = rand()%(p->max_num);
        if(p->print_array)
            for(i = 0; i < p->array_size; i++)
                fprintf(stderr,"Array[%d] = %d\n",i,array[i]);

        
        /* Print problem parameters if the user wants */
        if(p->verbose)
            fprintf(stdout,"Number of buckets: %d\n\
                Large bucket size:\t%d\n\
                Small bucket size:\t%d\n\
                Array size:\t\t%d\n\
                Largest number:\t%d\n",num_buckets,large_bucket_size,small_bucket_size,p->array_size,p->max_num);
    }

    /* Array of small buckets.
       If the number of ranks is P, each rank has P smaller buckets. */
    Bucket ** buckets = (Bucket **) malloc(sizeof(Bucket*)*num_buckets);
    for(i = 0; i < num_buckets; i++)
    {
        /* Malloc pointer to the bucket */
        buckets[i] = (Bucket *) malloc(sizeof(Bucket));
        /* Malloc pointer to the bucket's array */
        buckets[i]->array = (int*) malloc(sizeof(int)*small_bucket_size*2.0);
        /* Index points to the next location to insert an element */
        buckets[i]->index = 0;
    }

    /* Large bucket to hold numbers from the initial scatter. */
    Bucket large_bucket;

    /* NOTE: The overestimation of bucket sizes is due to rand() not producing a true uniform distribution. 
       If the numbers are not uniformly distributed, some buckets will be larger than others. */
    int * my_bucket_array = (int*) malloc(sizeof(int)*large_bucket_size*4.0);
    large_bucket.array = my_bucket_array;
    large_bucket.index = 0;
    
    /* Print range of each bucket */
    int range_min = (p->max_num*p->rank)/p->size, range_max = (p->max_num*(p->rank+1))/p->size;

    if(p->print_ranges)
        fprintf(stderr,"Bucket [%d] %d => %d\n",p->rank,range_min,range_max);
    
    /* Scatter array into large buckets */
    MPI_Scatter(array,large_bucket_size,MPI_INT,large_bucket.array,large_bucket_size,MPI_INT,0,MPI_COMM_WORLD);
    int dest;
    /* Place numbers from large bucket into small buckets */
    for(i = 0; i < large_bucket_size; i++)
    {
        /* Calculate the destination bucket by mapping a number from 0 to max_num to
           a bucket from 0 to num_buckets */
        dest = (large_bucket.array[i] * num_buckets)/p->max_num;
        /* If the destination is to my bucket, then insert it into my local bucket directly */
        if(dest == p->rank)
                bucket_insert(&large_bucket,large_bucket.array[i]);
            /* Otherwise insert the number into someone else bucket */
        else
                bucket_insert(buckets[dest],large_bucket.array[i]);
    }
    MPI_Request * requests = (MPI_Request *) malloc(sizeof(MPI_Request) * p->size);
    /* Send each bucket to it's owner, non blocking so that there is no deadlock */
    for(i = 0; i < p->size; i++)
       /* Make sure I'm not sending to myself */
       if(i != p->rank)
           MPI_Isend(buckets[i]->array,small_bucket_size*2,MPI_INT,i,buckets[i]->index,MPI_COMM_WORLD,&requests[i]);
    MPI_Status status;
    /* Current is the location at which to begin inserting elements into my large bucket.  
       My large bucket already contains my numbers, so begin there */
    int current = large_bucket.index;
    /* Receive small buckets from other ranks */
    for(i = 0; i < p->size-1; i++)
    {
        MPI_Recv(&large_bucket.array[current],small_bucket_size*2,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
        if(p->verbose)
            fprintf(stderr,"[%d] Received %d elements from %d total now %d\n",p->rank, status.MPI_TAG, status.MPI_SOURCE,current+status.MPI_TAG);
        current+=status.MPI_TAG;
    }
    large_bucket.index = current;
  
    /* Sort my bucket */
    qsort(&large_bucket.array[0],current,sizeof(int),compare);
    
    /* Gather array sizes */
    int * sizes = (int *) malloc(sizeof(int)*p->size);
    MPI_Gather(&current,1,MPI_INT,sizes,1,MPI_INT,0,MPI_COMM_WORLD);
    
    
    /* Determine the displacement in the final array */
    int * disp = (int *) malloc(sizeof(int)*p->size);
    if(p->rank == 0)
    {
        disp[0] = 0;
        for(i = 1; i < p->size+1; ++i)
            disp[i] = disp[i-1] + sizes[i-1];
    }

    /* For verbose mode */
    if(p->rank == 0 && p->verbose)
        for(i = 0; i < p->size; ++i)
        {
            range_min = disp[i], range_max = disp[i]+sizes[i];
            fprintf(stderr,"Rank [%d] [%d] => [%d] Disp = %d Size = %d\n",i,range_min,range_max, disp[i],sizes[i]);
        }
    
    /* Gather results */
    MPI_Gatherv(large_bucket.array,current,MPI_INT,array,sizes,disp,MPI_INT,0,MPI_COMM_WORLD);
    
    /* Verbose Mode */
    if(p->rank == 0)
    {
        end = MPI_Wtime();
        fprintf(stderr,"array size: %d\telapse time: %f\n",p->array_size,end-start);
        if(p->print_array)
            for(i = 0; i < p->array_size; i++)
                fprintf(stdout,"array[%d] = %d\n",i,array[i]);
        free(array);
    }
    return;
}

void usage()
{
    printf("bucket_sort\n\
            MPI Program to sort an array using bucket sort.\n\
            Jharrod LaFon 2011\n\
            Usage: bucket_sort [args]\n\
            -r\t\tPrint bucket ranges\n\
            -s <size>\tSpecify array size\n\
            -m <max>\tSpecify largest number in array\n\
            -p\t\tPrint the array\n\
            -v\t\tBe verbose\n\
            -h\t\tPrint this message\n");
}
/* Parse user arguments */
void parse_args(int argc, char ** argv, Params * p)
{
    int c = 0;
    while((c = getopt(argc,argv,"rs:hm:vp")) != -1)
    {
        switch(c)
        {
            case 'r':
                p->print_ranges = 1;
                break;
            case 's':
                p->array_size = atoi(optarg);
                break;
            case 'm':
                p->max_num = atoi(optarg);
                break;
            case 'h':
                if(p->rank == 0) usage();
                exit(0);
            case 'p':
                p->print_array = 1;
                break;
            case 'v':
                p->verbose = 1;
                break;
            default:
                break;
        }
    }
    return;
}

/* Main */
int main(int argc, char *argv[])
{
    int rank;
    int size;
    /* Initialize MPI */
    if(MPI_Init(&argc, &argv) != MPI_SUCCESS)
    {
        fprintf(stderr, "Unable to initialize MPI!\n");
        return -1;
    }
    /* Get rank and size */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    /* Populate the parameters with default values */
    Params p;

    /* Don't be verbose by default */
    p.verbose = 0;
    p.print_array = 0;
    p.print_ranges = 0;
    p.rank = rank;
    p.size = size;
    p.array_size = DEFAULT_ARRAY_SIZE;
    p.max_num = MAX_NUM_SIZE;
    /* Check for user options */
    parse_args(argc,argv,&p);
    if(p.array_size % p.size != 0)
    {
        if(rank == 0) fprintf(stderr,"Error: Array size must be multiple of number of ranks.\n");
        exit(1);
    }
    slave(&p);
    MPI_Finalize();
    return 0;

}
