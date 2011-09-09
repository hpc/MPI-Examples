/* File: bucket_sort.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Pipeline implementation of bucket sort
   */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<mpi.h>
#include<math.h>
#include<time.h>
#include "memwatch.h"
#define TERM_TAG 5
#define NUM_TAG 10
#define DEFAULT_ARRAY_SIZE 500
#define MAX_NUM_SIZE 30
#define BLOCK_LOW(id,p,n) ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) \
    (BLOCK_LOW((id)+1,p,n)-1)

/* Struct to hold configuration options */
typedef struct
{
    int rank,size, array_size,slice,max_num,verbose;
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


/* Function to be executed by the workers */
void slave(Params * p)
{  
    srand(2);
    /* Timer */
    double start, end;
    int i;
    int * array;
    array = (int *) malloc(sizeof(int) * p->array_size);
    MPI_Status status;
    int count = 0;
    int done = 0;
    int temp = 0;
    int low = BLOCK_LOW(p->rank-1,p->size,p->max_num+1);
    int high = BLOCK_HIGH(p->rank-1,p->size,p->max_num+1);
    start = MPI_Wtime();
    // Repeat until done
    while(!done)
    {
        // Master work
        if(p->rank == 0)
        {
            // Increase count until array_size
            if(count++ < p->array_size)
            {
                // Add random number to list, send it up the pipeline to rank 1
                temp = rand() % p->max_num;
                MPI_Send(&temp, 1, MPI_INT, 1, NUM_TAG, MPI_COMM_WORLD);
            }
            else
            {
                // Signal end of the list
                MPI_Send(&temp, 1, MPI_INT, 1, TERM_TAG, MPI_COMM_WORLD);
                done = 1;
            }
        }
        // Slave work
        else
        {
            // receive a number from the previous stage
            MPI_Recv(&temp, 1, MPI_INT, p->rank-1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            // check for termination
            if(status.MPI_TAG == TERM_TAG)
            {
                // make sure I'm sending to a valid rank and foward termination
                done = 1;
                if(p->rank != p->size)
                {
                    MPI_Send(&temp, 1, MPI_INT, p->rank+1, TERM_TAG, MPI_COMM_WORLD);
                }
            }
            // got number
            else if(status.MPI_TAG == NUM_TAG)
            {
                // check to see if I keep it
                if(temp >= low && temp <= high)
                    array[count++] = temp;
                else
                    // pass it on
                    MPI_Send(&temp, 1, MPI_INT, p->rank+1, NUM_TAG,MPI_COMM_WORLD); 
            }

        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    int * sizes = (int *) calloc(p->size+1,sizeof(int));
    MPI_Gather(&count, 1, MPI_INT, sizes, 1, MPI_INT, 0 , MPI_COMM_WORLD);
    int * disp = (int *) calloc(p->size+1,sizeof(int));
    if(p->rank == 0)
    {
        sizes[0] = 0;
        disp[0] = 0;
        for(i = 1; i < p->size+1; ++i)
        {
            disp[i] = disp[i-1] + sizes[i-1];
        }

    }
    // sort the array, except for the master
    if(p->rank != 0) 
        qsort(array,count,sizeof(int),compare);
    if(p->rank == 0) count = 0;
    // gather results
    MPI_Gatherv(array, count, MPI_INT, &array[0], sizes, disp, MPI_INT,0,MPI_COMM_WORLD);
    // stop the clock, print results
    end = MPI_Wtime();
    if(p->rank ==0) fprintf(stderr,"[%d] Elapsed time: %f\n",p->rank,end-start);
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
    while((c = getopt(argc,argv,"s:hm:v")) != -1)
    {
        switch(c)
        {
            case 's':
                p->array_size = atoi(optarg);
                break;
            case 'm':
                p->max_num = atoi(optarg);
                break;
            case 'h':
                if(p->rank == 0) usage();
                exit(0);
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
    p.rank = rank;

    // Used for cleaner code later on
    p.size = size - 1;
    
    p.array_size = DEFAULT_ARRAY_SIZE;
    p.max_num = MAX_NUM_SIZE;
    /* Check for user options */
    parse_args(argc,argv,&p);
    slave(&p);
    MPI_Finalize();
    return 0;

}
