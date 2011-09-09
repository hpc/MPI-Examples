/* File: metal_plate.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Simulate heat traveling through a plate
   */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<mpi.h>
#include<math.h>
#include<time.h>
#include "memwatch.h"
#define DEFAULT_PLATE_SIZE 500
#define CYCLES 10000
#define BLOCK_LOW(id,p,n) ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) \
    (BLOCK_LOW((id)+1,p,n)-1)

/* Struct to hold configuration options */
typedef struct
{
    int rank,size, plate_size;
} Params;



/* Function to be executed by the workers */
void slave(Params * p)
{  
    mwInit();
    /* Timer */
    double start, end;
    int t,i,j,index;
    int flag = 0;
    int count = 0;
    MPI_Request * requests = (MPI_Request *) malloc(sizeof(MPI_Request)*5);
    MPI_Status  * statuses = (MPI_Status *) malloc(sizeof(MPI_Status)*5);
    double ** sheet = (double**) malloc(sizeof(double*)*p->plate_size);
    double ** temp = (double**) malloc(sizeof(double*)*p->plate_size);
    for(i = 0; i < p->plate_size; i++)
    {
        sheet[i] = (double*) malloc(sizeof(double)*p->plate_size);
        temp[i] = (double*) malloc(sizeof(double)*p->plate_size);
        for(j = 0; j < p->plate_size; j++)
        {
            if(i == 0 || i == p->plate_size -1 || j == 0 || j == p->plate_size -1)
            {
                sheet[i][j] = 232;
                temp[i][j] = 232;
            }
            else
            {
                sheet[i][j] = 97;
                temp[i][j] = 97;
            }
        }
    }
    double * above = (double *) malloc(sizeof(double)*p->plate_size);
    double * below = (double *) malloc(sizeof(double)*p->plate_size);
    int low = BLOCK_LOW(p->rank,p->size,p->plate_size);
    if(low == 0)
        low++;
    int high = BLOCK_HIGH(p->rank,p->size,p->plate_size);
    for(i = 0; i < p->plate_size; i++)
    {
        if(low == 0 || high == p->plate_size - 1)
        {
            above[i] = 232;
            below[i] = 232;
        }
        else
        {
            above[i] = 97;
            below[i] = 97;
        }
    }
    above[0] = 232;
    below[0] = 232;
    above[p->plate_size] = 232;
    below[p->plate_size] = 232;
    start = MPI_Wtime();
    // Simulation loop
    for(t = 0; t < CYCLES; t++)
    {
        if(p->rank ==-1)
        {
            for(i = 0; i < p->plate_size; i++)
            {
                for(j = 0; j < p->plate_size; j++)
                    fprintf(stderr,"%10f ",sheet[i][j]);
                fprintf(stderr,"\n");
            }
        }
        if(p->rank == -1)
                fprintf(stderr,"\n");

        // Calculate new values
        for(i = low; i < high; i++)
        {
            // For every discrete row entry, calculate the new value
            for(j = 1; j < p->plate_size; j++)
                if(i == 0)
                    temp[i][j] = 0.25 * (sheet[i][j] + sheet[i+1][j] + sheet[i][j-1] + sheet[i][j+1]);
                else if(i == p->plate_size - 1)
                    temp[i][j] = 0.25 * (sheet[i-1][j] + sheet[i][j] + sheet[i][j-1] + sheet[i][j+1]);
                else
                    temp[i][j] = 0.25 * (sheet[i-1][j] + sheet[i+1][j] + sheet[i][j-1] + sheet[i][j+1]);
        }
        // Update values
        for(i = low; i < high; i++)
            for(j = 1; j < p->plate_size - 1; j++)
                sheet[i][j] = temp[i][j];

        MPI_Barrier(MPI_COMM_WORLD);        
        // Exchange values
        if((p->rank %2) == 0 && p->rank > 0 && p->rank < p->size - 1)
        {
            // Send my first row to the previous rank
            MPI_Isend(&sheet[low][0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[0]);
            // Receive last row from previous rank
            MPI_Irecv(&below[0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[1]);
            // Send my last row to the next rank
            MPI_Isend(&sheet[high][0], p->plate_size, MPI_DOUBLE, p->rank+1, 0, MPI_COMM_WORLD, &requests[2]);
            // Receive first row from next rank
            MPI_Irecv(&above[0], p->plate_size, MPI_DOUBLE, p->rank+1,0,MPI_COMM_WORLD, &requests[3]);
            count = 4;
        }
        else if(p->rank > 0 && p->rank < p->size - 1)
        {
            // Receive last row from previous rank
            MPI_Irecv(&below[0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[0]);
            // Send my first row to the previous rank
            MPI_Isend(&sheet[low][0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[1]);
            // Receive first row from next rank
            MPI_Irecv(&above[0], p->plate_size, MPI_DOUBLE, p->rank+1,0,MPI_COMM_WORLD, &requests[2]);
            // Send my last row to the next rank
            MPI_Isend(&sheet[high][0], p->plate_size, MPI_DOUBLE, p->rank+1, 0, MPI_COMM_WORLD, &requests[3]);
            count = 4;
        }
        else if(p->rank == 0)
        {
            // Receive first row from next rank
            MPI_Irecv(&above[0], p->plate_size, MPI_DOUBLE, p->rank+1,0,MPI_COMM_WORLD, &requests[0]);
            // Send my last row to the next rank
            MPI_Isend(&sheet[high][0], p->plate_size, MPI_DOUBLE, p->rank+1, 0, MPI_COMM_WORLD, &requests[1]);
            count = 2;
        }
        else if(p->rank == p->size -1)
        {
            // Receive last row from previous rank
            MPI_Irecv(&below[0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[0]);
            // Send my first row to the previous rank
            MPI_Isend(&sheet[low][0], p->plate_size, MPI_DOUBLE, p->rank-1, 0, MPI_COMM_WORLD, &requests[1]);
            count = 2;
        }

        MPI_Barrier(MPI_COMM_WORLD);
        // Update values
        if(p->rank > 0 && p->rank < p->size - 1)
            for(i = 0; i < p->plate_size; i++)
            {
                sheet[low-1][i] = below[i];
                sheet[high+1][i] = above[i];
            }
    }
    end = MPI_Wtime();
    if(p->rank == 0)
        fprintf(stderr,"Elapsed time: %f\n",end-start);
    mwTerm();
    return;
}

void usage()
{
    printf("metal_plate\n\
            MPI Program to simulate heat moving through a plate.\n\
            Jharrod LaFon 2011\n\
            Usage: metal_plate [args]\n\
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
            case 'h':
                if(p->rank == 0) usage();
                exit(0);
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
    p.rank = rank;
    p.size = size;
    p.plate_size = DEFAULT_PLATE_SIZE;
    /* Check for user options */
    parse_args(argc,argv,&p);
    slave(&p);
    MPI_Finalize();
    return 0;

}
