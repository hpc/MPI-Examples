/*  File:  pthread_moore.c
    Author: Jharrod LaFon
    Date: Spring 2011
    Purpose: A POSIX thread implementation of Moore's algorithm for graph exploration
    */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/time.h>
#include<time.h>
#include<pthread.h>
#include<sys/wait.h>
#include<unistd.h>
#define INFINITY 2147483647
#define DEFAULT_NUM_PROCS 2
enum status { REQUEST, QUEUE, TERMINATE,VERTEX };


typedef struct vertex
{
    int name;
    struct vertex * next;
} vertex;

typedef struct vertex_queue
{
    int count;
    int request_count;
    vertex * head;
} vertex_queue;

typedef struct workinfo
{
    int num_procs;
    int count;
    int * graph;
    vertex_queue * queue;
    unsigned int * dist;
    int master;
    int terminate;
    pthread_mutex_t * dist_lock;
    pthread_mutex_t * queue_lock;
    pthread_cond_t * termination_cond;
} workinfo;
static void *do_slave(void *arg);
static void *do_master(void *arg);

static void *do_slave(void *arg)
{
    workinfo * info = (workinfo *) arg;
    vertex temp;
    int * graph = (int*) info->graph;
    int * dist = (int*) info->dist;
    int count = info->count;
    unsigned int temp_dist;
    int j;
    vertex_queue * queue = (vertex_queue*)info->queue;
    while(!info->terminate)
    {
        pthread_mutex_lock(info->queue_lock);
               // fprintf(stderr,"Queue: %d request_count: %d\n",queue->count,queue->request_count);
        // queue is empty
        if(queue->count == 0)
        {
            queue->request_count = queue->request_count + 1;
            pthread_mutex_unlock(info->queue_lock);
            continue;
        }
        // queue is non empty
        else if(queue->count > 0)
        {
            temp.name = queue->head->name;
            queue->count = queue->count - 1;
            if(queue->count > 1)
                queue->head = queue->head->next;
        }
        pthread_mutex_unlock(info->queue_lock);
        for(j = 1; j < count; j++)
        {
            temp_dist = graph[temp.name*count+j];
            if(temp_dist != INFINITY)
            {
                temp_dist += dist[temp.name];
                if(temp_dist < dist[j])
                {
                    pthread_mutex_lock(info->queue_lock);
                    dist[j] = temp_dist;
                    vertex * temp = (vertex *) malloc(sizeof(vertex));
                    temp->name = j;
                    temp->next = queue->head;
                    queue->count = queue->count+1;
                    queue->head = temp;
                    if(queue->request_count > 0)
                        queue->request_count = queue->request_count - 1;
                    pthread_mutex_unlock(info->queue_lock);
                }
            }
        }
    }
pthread_exit(0);
}
static void *do_master(void *arg)
{
    workinfo * info = (workinfo *) arg;
    while(1)
    {
        pthread_mutex_lock(info->queue_lock);
        if(info->queue->count == 0 && info->queue->request_count >= info->num_procs-1)
        {
            info->terminate = 1;
            pthread_mutex_unlock(info->queue_lock);
            break;
        }
        pthread_mutex_unlock(info->queue_lock);

    }
    pthread_exit(0);
}
int main(int argc, char * argv[])
{
    // variable declarations:
    // timing variables
    struct timespec start,end,elapsed;
    // loop counters, etc
    int i = 0, j = 0;
    
    // array to store graph
    int * graph;
    

    // array to store distances
    unsigned int * dist; 

    // size of graph as a string
    char char_count[12];
    
    // size of graph as int
    int count = 0;
    
    // input file name;
    char filename[256] = "graph";

    // input file descriptor
    FILE * input;


    // Number of processes
    int num_procs = DEFAULT_NUM_PROCS;


    // variables used for parsing
    char * line;
    char * ptr;

    // Parse arguments, if any
    while((i = getopt(argc,argv,"n:f:")) != -1)
    {
        switch(i)
        {
            case 'n':
                num_procs = atoi(optarg);
                break;
            case 'f':
                strncpy(filename,optarg,256);
                break;
            default:
                break;
        }
    }

    // Open input file
    input = fopen(filename,"r");
    if(input == NULL)
    {
        perror("Unable to open file");
        exit(-1);
    }
    // get vertex count
    fgets(&char_count[0],12,input);
    count = atoi(char_count);

    graph = (int*) malloc(count*count*sizeof(int));
    dist = (unsigned int*) malloc(sizeof(unsigned int)*count);

    // Read in graph
    // instead of storing the graph matrix as a 2d array, I use a 1d array
    // so graph[i][j] becomes graph[i*count+j] where count is the length of a row
    i = 0;
    //fprintf(stderr,"Graph has %d vertices\n",atoi(char_count));
    line = (char*) malloc(sizeof(char)*11*(count+1));
    while(i < count && fgets(line, (count*12), input) != (char*)EOF)
    {
          j = 0;
          ptr = (char * ) strtok(line," ");
          while(ptr != NULL)
          {
              graph[i*count + j++] = (unsigned int )atoi(ptr);
              ptr = strtok(NULL, " ");
          }
          i++;
    }
    // Begin at node zero, so get distance from 0 to every other node
    // note: this array is shared
    for(i = 0; i < count; i++)
        dist[i] = graph[i];

    
    // start the clock
    clock_gettime(CLOCK_REALTIME, &start);
    vertex_queue queue;
    queue.count = 0;
    queue.request_count = 0;
    vertex root;
    root.name = 0;
    queue.head = &root;
    pthread_mutex_t dist_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t termination_cond;

    workinfo info;
    info.terminate = 0;
    info.count = count;
    info.graph = graph;
    info.dist = dist;
    info.master = 0;
    info.queue_lock = &queue_lock;
    info.dist_lock = &dist_lock;
    info.termination_cond = &termination_cond;
    info.num_procs = num_procs;
    info.queue = &queue;

    vertex * temp;
    for(i = 0; i < count; i++)
        // if the distance is not INFINITY, add the node to the queue
        if(dist[i] != INFINITY)
        {
            temp = (vertex *) malloc(sizeof(vertex));
            temp->next = queue.head;
            temp->name = i;
            queue.count = queue.count + 1;
            queue.head = temp;
        }
        
    pthread_t * threads = (pthread_t *) malloc(sizeof(pthread_t)*num_procs);
    if(pthread_create(&threads[0],NULL,do_master, (void*)&info)<0)
        perror("pthread_create");

    for(i = 1;i < num_procs; i++)
        if(pthread_create(&threads[i],NULL,do_slave, (void*)&info)<0)
            perror("pthread_create");
    for(i = 0;i < num_procs; i++)
        if(pthread_join(threads[i],NULL))
            perror("pthread_join");

    clock_gettime(CLOCK_REALTIME, &end);
    elapsed.tv_sec = end.tv_sec - start.tv_sec;
    elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    if(elapsed.tv_nsec < 0)
    {
        elapsed.tv_sec -= 1;
        elapsed.tv_nsec += 1000000000;
    }
    printf("Elapsed time: %ld.%ld\n",elapsed.tv_sec,elapsed.tv_nsec);
    // clean up
    fclose(input);
    return 0;
}
