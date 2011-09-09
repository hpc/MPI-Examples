/* File: pthread_metal_plate.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Simulate heat traveling through a plate using POSIX threads
   */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<string.h>
#include<math.h>
#include<time.h>
#define DEFAULT_NUM_THREADS 4
#define DEFAULT_PLATE_SIZE 500
#define DEFAULT_CYCLES 1000
#define BLOCK_LOW(id,p,n) ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) \
    (BLOCK_LOW((id)+1,p,n)-1)

/* Struct to hold configuration options */
typedef struct
{
    int num_threads,id, plate_size,num_cycles;
    pthread_barrier_t * barrier;
    pthread_mutex_t * lock;
    double ***sheet;
    double * min;
    double * max;
} Params;


/* Function to be executed by the workers */
static void *slave(void * param)
{  
    // get parameters
    Params * p = (Params *) param;
    int id = -1;
    
    // get thread id
    pthread_mutex_lock(p->lock);
    id = p->id;
    p->id = p->id +1;
    pthread_mutex_unlock(p->lock);

    // set up variables
    int t,i,j;
    double ** sheet = *p->sheet;
    double * max = p->max;
    double * min = p->min;
    int low = BLOCK_LOW(id,p->num_threads,p->plate_size);
    int high = BLOCK_HIGH(id,p->num_threads,p->plate_size);
    // temp storage
    double ** temp = (double **) malloc(sizeof(double*)*p->plate_size);
    for(i=0;i<p->plate_size;i++)
        temp[i] = (double *) malloc(sizeof(double)*p->plate_size);
    // Simulation loop
    for(t = 0; t < p->num_cycles; t++)
    {
        // Calculate new values
        for(i = low; i <= high; i++)
        {
            // For every discrete row entry, calculate the new value
            for(j = 1; j < p->plate_size; j++)
            {
                // these if statements handle boundary cases
                if(i == 0)
                    temp[i][j] = 0.25 * (sheet[i][j] + sheet[i+1][j] + sheet[i][j-1] + sheet[i][j+1]);
                else if(i == p->plate_size - 1)
                    temp[i][j] = 0.25 * (sheet[i-1][j] + sheet[i][j] + sheet[i][j-1] + sheet[i][j+1]);
                // non boundary case
                else
                    temp[i][j] = 0.25 * (sheet[i-1][j] + sheet[i+1][j] + sheet[i][j-1] + sheet[i][j+1]);
                // keep track of min/max
                if(temp[i][j] > max[id])
                    max[id] = temp[i][j];
                if(temp[i][j] < min[id])
                    min[id] = temp[i][j];
            }
        }
        // Update values
        pthread_barrier_wait(p->barrier);
        for(i = low; i <= high; i++)
            for(j = 1; j < p->plate_size - 1; j++)
                if(i != 0 && i!= p->plate_size-1)
                    sheet[i][j] = temp[i][j];

        // to print the array, change this comparison
        if(id ==-1)
        {
            for(i = 0; i < p->plate_size; i++)
            {
                for(j = 0; j < p->plate_size; j++)
                    fprintf(stderr,"%10f ",sheet[i][j]);
                fprintf(stderr,"\n");
            }
        }
        if(id == -1)
                fprintf(stderr,"\n");

    }
    pthread_exit(0);
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
    while((c = getopt(argc,argv,"hn:t:")) != -1)
    {
        switch(c)
        {
            case 'h':
                if(p->id == 0) usage();
                exit(0);
            case 'n':
                p->num_threads = atoi(optarg);
                break;
            case 't':
                p->num_cycles = atoi(optarg);
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

    // set up parameters
    Params p;
    p.num_threads = DEFAULT_NUM_THREADS;
    p.plate_size = DEFAULT_PLATE_SIZE;
    p.num_cycles = DEFAULT_CYCLES;

    struct timespec start,end,elapsed;
    pthread_barrier_t barrier;
    pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;
    p.lock = &id_lock;
    p.barrier = &barrier;
    parse_args(argc,argv,&p);
    pthread_barrier_init(&barrier,NULL,p.num_threads);

    // storage 
    double ** sheet = (double**) malloc(sizeof(double*)*p.plate_size);
    double * min = (double *) malloc(sizeof(double)*p.num_threads);
    double * max = (double *) malloc(sizeof(double)*p.num_threads);
    int i,j;

    // set up problem
    for(i = 0; i < p.plate_size; i++)
    {
        sheet[i] = (double*) malloc(sizeof(double)*p.plate_size);
        for(j = 0; j < p.plate_size; j++)
        {
            if(i == 0 || i == p.plate_size -1 || j == 0 || j == p.plate_size -1)
            {
                sheet[i][j] = 232;
            }
            else
            {
                sheet[i][j] = 97;
            }
        }
    }
    for(i = 0; i < p.num_threads; i++)
    {
        max[i] = 0;
        min[i] = 232;
    }
    p.sheet = &sheet;
    p.id = 0;
    p.max = max;
    p.min = min;

    // create threads, start the clock
    pthread_t * threads = (pthread_t *) malloc(sizeof(pthread_t)*p.num_threads);
    clock_gettime(CLOCK_REALTIME, &start);
    for(i = 0; i < p.num_threads; i++)
    {
        // run simulation
        if(pthread_create(&threads[i],NULL,slave, (void*)&p)<0)
            perror("pthread_create");
    }
    for(i = 0;i < p.num_threads; i++)
        if(pthread_join(threads[i],NULL))
            perror("pthread_join");

    // stop teh clock, print results
    clock_gettime(CLOCK_REALTIME, &end);
    elapsed.tv_sec = end.tv_sec - start.tv_sec;
    elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    if(elapsed.tv_nsec < 0)
    {
        elapsed.tv_sec -= 1;
        elapsed.tv_nsec += 1000000000;
    }
    printf("Elapsed time: %ld.%ld\n",elapsed.tv_sec,elapsed.tv_nsec);
    fflush(stdout);
    double global_min = 232, global_max = 0;
    for(i = 0; i < p.num_threads; i++)
    {
        if(global_min > min[i])
            global_min = min[i];
        if(global_max < max[i])
            global_max = max[i];
        
    }
    fprintf(stderr,"Max temperature: %f Min temperature: %f\n",global_max,global_min);
    return 0;
}
