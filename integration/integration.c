/* File: integration.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Compute the approximation of the integral of f(x)
   */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<mpi.h>
#include<math.h>

/* Integration Interval */
#define INTERVAL_MIN 0.0
#define INTERVAL_MAX 1.0
/* Starting number of intervals */
#define NUM_INTERVALS 1
/* Default Error Threshold */
#define ERROR_THRESHOLD 0.00001

/* Struct to hold configuration options */
typedef struct
{
    int intervals,rank,size;
    double delta,region;
    double error;
    double min,max;
    double (*func)(double start, double end, double delta);
} Params;

/* Function f(x) to be integrated */
double f(double x)
{
    return sqrt(1-x*x); 
}

/* Approximation of f(x) using the rectangle rule */
double rectangle_rule(double start, double end, double delta)
{
    double area = 0.0;
    double x;
    for(x = start; x <= end; x+= delta)
        area += f(x)*delta;
    return area;
}

/* Approximation of f(x) using the trapezoid rule */
double trapezoid_rule(double start, double end, double delta)
{
        double area = 0.0;
        area = 0.5 * ( f(start) + f(end) );
        double x  = 0.0;
        for(x = start + delta; x <= end; x+= delta)
        {
            area += f(x);
        }
        area = area * delta;
        return area;
}

/* Function to be executed by the master rank */
void master(Params * p)
{   
    double last = 0.0;
    double area = p->error+1,local;
    double start, end;
    /* Loop until the difference of results is less than the error */
    while(fabs(last - area) > p->error)
    {
        last = area;
        /* Start the clock */
        start = MPI_Wtime();
        /* Broadcast the intervals */
        MPI_Bcast(&p->intervals,1,MPI_INT,0,MPI_COMM_WORLD);
        /* Reduce the result */
        MPI_Reduce(&local,&area,1,MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        /* Stop the clock */
        end = MPI_Wtime();
        /* Calculate delta */
        p->delta = (p->max-p->min)/p->intervals;
        /* Print results */
        fprintf(stderr,"Intervals = %8d\tDelta = %f\tResult = %f\tError=%f\tTime = %f\n",p->intervals,p->delta,area,fabs(last-area),end-start);
        /* Double the number of intervals */
        p->intervals = p->intervals*2;
    } 
    /* Send all ranks term signal */
    p->intervals = -1;
    MPI_Bcast(&p->intervals,1,MPI_INT,0,MPI_COMM_WORLD);
    return;
}

/* Function to be executed by all the slaves */
void slave(Params *p)
{
    double start, end, area;
    /* Loop until done */
    while(1)
    {
        /* Receive the interval count */
        MPI_Bcast(&p->intervals,1,MPI_INT,0,MPI_COMM_WORLD);
        /* Terminate if no more work to do */
        if(p->intervals == -1)
            return;
        /* Calculate the region */
        p->region = (p->max-p->min)/(double)p->size;
        /* Calculate start of region */
        start = p->min + p->region * (p->rank-1);
        /* Calculate end of region */
        end = start + p->region;
        /* Calculate step (delta) */
        p->delta = (p->max-p->min)/p->intervals;
        /* Use either the trapezoid or rectangle rule */
        area = p->func(start,end,p->delta);
        /* Return results */
        MPI_Reduce(&area,&area,1,MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        }
}

void usage()
{
    printf("integration\n\
            MPI Program to approximate an integral.\n\
            Jharrod LaFon 2011\n\
            Usage: integration [args]\n\
            a <start>\t\tStarting point of interval to be integrated\n\
            b <end>\t\tEnding point of interval to be integrated\n\
            e <error>\t\tMaximum error threshold\n\
            t\t\tUse the trapezoid rule\n\
            r\t\tUse the rectangle rule\n\
            h\t\tPrint this message\n");
}

/* Parse user arguments */
void parse_args(int argc, char ** argv, Params * p)
{
    int c = 0;
    while((c = getopt(argc,argv,"a:b:e:trh")) != -1)
    {
        switch(c)
        {
            case 'a':
                p->min = atof(optarg);
                break;
            case 'b':
                p->max = atof(optarg);
                break;
            case 'e':
                p->error = atof(optarg);
                break;
            case 't':
                p->func = trapezoid_rule;
                break;
            case 'r':
                p->func = rectangle_rule;
                break;
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
    p.min = INTERVAL_MIN;
    p.max = INTERVAL_MAX;
    p.error = ERROR_THRESHOLD;
    p.intervals = NUM_INTERVALS;
    p.rank = rank;
    p.size = size-1;
    p.func = rectangle_rule;
    
    /* Check for user options */
    parse_args(argc,argv,&p);
    
    if(rank == 0)
        master(&p);
    else
        slave(&p);
    MPI_Finalize();
    return 0;
}
