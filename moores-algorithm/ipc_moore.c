/*  File:  ipc_moore.c
    Author: Jharrod LaFon
    Date: Spring 2011
    Purpose: A shared memory implementation of Moore's algorithm for graph exploration
    */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/wait.h>
#include<unistd.h>
#include "semaphore.h"
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
    vertex * head;
} vertex_queue;


int main(int argc, char * argv[])
{
    // variable declarations:

    // timing variables
    struct timespec start,end,elapsed;
    // loop counters, etc
    int i = 0, j = 0;
    
    // array to store graph
    int * graph;
    
    // temp vertex
    vertex * temp;

    // array to store distances
    unsigned int * dist; 

    // size of graph as a string
    char char_count[12];
    
    // size of graph as int
    int count = 0;
    
    // input file name;
    char filename[256] = "graph10";

    // input file descriptor
    FILE * input;

    // pid used for forking
    int * pid;

    // Number of processes
    int num_procs = DEFAULT_NUM_PROCS;

    // boolean
    int master = 1;

    // keys used for ipc stuff
    char * path = "/tmp";
    key_t shared_vkey = ftok(path,0);
    key_t smsg_key = ftok(path,1);
    key_t rmsg_key = ftok(path,2);
    key_t shared_dist_key = ftok(path,3);
    
    // message queue ids, one is for slave to master comm, the other for master to slave comm
    int qid,rqid;
    
    // shared memory ids
    int did,gid;

    // needed for ipc accounting
    struct msqid_ds dsbuf[2];
    struct msgbuf rmsg;
    
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

    // Set up shared memory stuff
    qid = open_queue(smsg_key);
    if(qid < 0)
    {
            perror("open_queue");
            exit(-1);
    }
    rqid = open_queue(rmsg_key);
    if(rqid  < 0)
    {
            perror("open_queue");
            exit(-1);
    }

    // clear any messages out of both queues
    msgctl(qid, IPC_STAT, &dsbuf[0]);
    for(i = 0; i < dsbuf[0].msg_qnum; i++)
            msgrcv(qid, &rmsg, sizeof(rmsg)-sizeof(long),0,0);
    msgctl(rqid, IPC_STAT, &dsbuf[1]);
    for(i = 0; i < dsbuf[1].msg_qnum; i++)
            msgrcv(rqid, &rmsg, sizeof(rmsg)-sizeof(long),0,0);

    // get ids for shared memory regions
    // shared graph region
    gid = shmget(shared_vkey,(count*count*sizeof(int)),IPC_CREAT | 0660);
    // shared distance region
    did = shmget(shared_dist_key,sizeof(int)*count,IPC_CREAT | 0660);

    if(gid == -1 || did == -1)
    {
        fprintf(stderr,"gid: %d did: %d\n",gid,did);
        perror("shmget");
        exit(-1);
    }

    // attach to shared memory regions
    graph = shmat(gid, NULL,0);
    dist = (unsigned int *) shmat(did,NULL,0);
    if(graph == (void*)-1 || dist == (void*)-1)
    {
        perror("shmat");
        exit(-1);
    }

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
    // fork off the correct amount of processes
    int proc_count = 1;
    pid = (int *) malloc(sizeof(int)*num_procs);
    i = 0;
    while(proc_count < num_procs && master)
    {
        pid[i++] = fork();
        proc_count++;
        if(pid[i-1] == 0)
            master = 0;
    }
    // master only
    if(master)
    {
        // the first node on the queue is the root, or vertex 0
        vertex root;
        root.name = 0;
        root.next = NULL;
        vertex_queue queue;
        queue.count = 1;
        queue.head = &root;
        // algorithm 
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
        // variable to keep track of requests
        int request_count = 0;
        while(queue.count > 0 && request_count < num_procs-1)
        {
            // receive message
            if(msgrcv(qid, &rmsg, sizeof(rmsg)-sizeof(long),0,0) < 0)
            {
                fprintf(stderr,"[%s][%d][%d] msgrcv\n",__FILE__,__LINE__,getpid());
                perror("msgrcv");
                exit(-1);
            }
            // received work request
            if(rmsg.request == REQUEST)
            {
                // if we have no vertices in the queue, log the request
                if(queue.count == 0)
                    request_count++;
                // if we have a vertex in the queue, send it
                else
                {
                    rmsg.mtype = 2;
                    rmsg.request = VERTEX;
                    rmsg.vertex = queue.head->name;
                    queue.count = queue.count - 1;
                    if(queue.count > 0)
                    {
                        vertex * t = queue.head;
                        queue.head = queue.head->next;
                        free(t);
                    }
                    if(send_msg(rqid, &rmsg)<0)
                    {
                        fprintf(stderr,"[%s][%d] rqid: %d\n",__FILE__,__LINE__,rqid);
                        perror("send_msg");
                        exit(-1);
                    }
                }
            }
            // received result
            else if(rmsg.request == QUEUE)
            {
                // is the result better?
                if(rmsg.distance < dist[rmsg.vertex])
                {
                    // update the distance
                    dist[rmsg.vertex] = rmsg.distance;
                    
                    // check for outstanding requests, add the vertex to the queue if there are none
                    if(request_count == 0)
                    {
                        vertex * temp = (vertex *) malloc(sizeof(vertex));
                        temp->name = rmsg.vertex;
                        temp->next = queue.head;
                        queue.head = temp;
                        queue.count = queue.count + 1;
                    }
                    // if there is at least once outstanding request, send out the message
                    else
                    {
                        rmsg.mtype = 2;
                        rmsg.request = VERTEX;
                        if(send_msg(rqid, &rmsg)<0)
                        {
                            fprintf(stderr,"[%s][%d] rqid: %d\n",__FILE__,__LINE__,rqid);
                            perror("send_msg");
                            exit(-1);
                        }
                        request_count--;
                    }
                }
            }
        }
        // terminate
        rmsg.request = TERMINATE;
        for(i = 0; i < num_procs; i++)
            if(send_msg(rqid, &rmsg)<0)
            {
                fprintf(stderr,"[%s][%d] rqid: %d\n",__FILE__,__LINE__,rqid);
                perror("send_msg");
                exit(-1);
            }
    }
    // slave
    else
    do 
    {
        // this is a different temp, does not need to be dynamically allocated
        vertex temp;
        // get vertex
        struct msgbuf msg;
        msg.mtype = 1;
        msg.request = REQUEST;
        // request vertex
        if((send_msg(qid,&msg)) == -1)
        {
            fprintf(stderr,"[%s][%d] qid: %d\n",__FILE__,__LINE__,qid);
            perror("send_msg");
            exit(-1);
        }
        // receive response
        if(msgrcv(rqid, &rmsg, sizeof(rmsg)-sizeof(long),2,0) < 0)
        {
            perror("msgrcv");
            exit(-1);
        }
        // copy result 
        temp.name = rmsg.vertex;
        if(rmsg.request != TERMINATE)
        {
            // algorithm
            for(j = 1; j < count; j++)
            {
                unsigned int temp_dist = graph[temp.name*count+j];
                if(temp_dist != INFINITY)
                {
                    temp_dist += dist[temp.name];
                    if(temp_dist < dist[j])
                    {
                        msg.distance = temp_dist;
                        msg.vertex = j;
                        msg.request = QUEUE;
                        if((send_msg(qid,&msg)) == -1)
                        {
                            fprintf(stderr,"[%s][%d] qid: %d\n",__FILE__,__LINE__,qid);
                            perror("send_msg");
                            exit(-1);
                        }
                    }
                }
            }
        }
    }while(rmsg.request != TERMINATE);
    // kill children
    if(!master)
    {
        exit(0);
    }
    else
    {
        i = 0;
        int status = 0;
        while(i++ < proc_count-1)
        {
            wait(&status);
        }
    }
        
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
    for(i=0;i<count;i++)
        fprintf(stderr,"[%d] %d\n",i,dist[i]);
    // clean up
    msgctl(qid, IPC_RMID,&dsbuf[0]); 
    msgctl(rqid, IPC_RMID,&dsbuf[1]); 
    shmdt(graph);
    shmdt(dist);
    fclose(input);
    return 0;
}
