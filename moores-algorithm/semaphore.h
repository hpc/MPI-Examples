#ifndef SEMAPHORE_H
#define SEMAPHORE_H
/*
 * File: semaphore.h
 * Author: Jharrod LaFon
 * Date: Fall 2010
 * Purpose: Implement SYSV semaphore API so I don't hate them.
   This implementation removes the tedious stuff involved from my
   webserver implementation. Also contained in here is SYSV shared
   memory semantic stuff.  I was going to use it to transfer data
   between processes.  Then I came up with a better design for the 
   program that didn't require data transfer. */
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
struct msgbuf
{
    long mtype;
    int request;
    int vertex;
    int distance;
};

/* Function to open an IPC message queue */

int open_queue(key_t key)
{
    int id;
    if((id = msgget(key,IPC_CREAT | IPC_PRIVATE | 0660)) < 0)
        return -1;
    return id;
}

/* Function to send an IPC message */
int send_msg(int id, struct msgbuf * buf)
{
    int result, len;
    len = sizeof(struct msgbuf) - sizeof(long);
    if((result = msgsnd(id,buf,len,0)) == -1)
    {
        fprintf(stderr,"[%s][%d] id: %d buf: %p len: %d\n",__FILE__,__LINE__,id,buf,len);
        return -1;
    }
    return result;
}
/* Function to unlock a semaphore, with given ID and element number. */
int sem_unlock(int sid, int num, int inc);

/* Function to open a SYSV socket pair for IPC */
int get_sock_pair(int * fd)
{
    #ifdef DEBUG
	fprintf(stderr, "[%s][%d]\tCreating socket pair.\n",__FILE__,__LINE__);
    #endif 
    int status = socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
    if(status < 0)
	perror("Failed to create socket pair");
    return status;
}

/* Function to receive a message over a unix domain socket */
int recv_sockfd(int srcfd, int * recvfd)
{
    char buf[1];
    
    struct msghdr rmsg;
    struct iovec vec[1];
    ssize_t n;
    rmsg.msg_name = NULL;
    rmsg.msg_namelen = 0;
 
    vec[0].iov_base = &buf[0];
    vec[0].iov_len = 1;
    rmsg.msg_iov = vec;
    rmsg.msg_iovlen = 1;
    if (( n = recvmsg(srcfd, &rmsg, 0)) <= 0)
	perror("Unable to receive message on Unix domain socket");
    return n;
}
/* Function to send a unix domain socket from 
   one process to another. */
ssize_t send_sockfd(int dest, int sockfd)
{
    #ifdef DEBUG
	fprintf(stderr, "[%s][%d]\tSending fd %d to destination %d.\n",__FILE__,__LINE__,dest,sockfd);
    #endif 

    char buf[1];
    buf[0] = '1';

    struct msghdr smsg;
    struct iovec vec[1];
    smsg.msg_name = NULL;
    smsg.msg_namelen = 0;

    vec[0].iov_base = &buf[0];
    vec[0].iov_len = 1;
    smsg.msg_iov = vec;
    smsg.msg_iovlen = 1;

    int status = sendmsg(dest, &smsg, 0);
    if(status < 0)
	perror("Unable to send file descriptor");
    return status;
}

/* Function to return the current value of a semaphore. */
int get_sem_value(int sid, int num)
{
     #ifdef DEBUG
         fprintf(stderr,"[%s][%d]\tReading semaphore value with id,num %d,%d\n",__FILE__,__LINE__,sid,num);
     #endif
     int val = semctl(sid, num, GETVAL);
     if(val < 0)
	 perror("Unable to get semaphore value");
     return val;
}

/* Function to remove a semaphore */
int rmsem(int sid)
{
     #ifdef DEBUG
    	fprintf(stderr,"[%s][%d]\tRemoving semaphore %d.\n",__FILE__,__LINE__,sid);
     #endif
	int val = semctl(sid,0,IPC_RMID);
	if(val < 0)
	    perror("Unable to remove semaphore");
	else
     #ifdef DEBUG
	fprintf(stderr,"[%s][%d]\tSemaphore removed.\n",__FILE__,__LINE__);
     #else
	;
     #endif
	return 0;
}

/* Function to get a new semaphore.  Returns the semaphore id on success, -1 otherwise. 
   num is the number of semaphores in the set, val is the initial value they are set to.
   */
int open_sem(key_t key,int num, int val)
{
	int id = -1;
        /* Make sure the key is somewhat unique */
	#ifdef DEBUG
	    fprintf(stderr,"[%s][%d]\tGetting semaphore with key %d, num %d\n",__FILE__,__LINE__,key,num);
	#endif
	/* Get a new semaphore with read/write permissions */
	id = semget( key, num, IPC_CREAT |  0660 );
	if(id < 0)
	{
	    perror("Failed to get semaphore");
	    return -1;
	}
	#ifdef DEBUG
	    fprintf(stderr,"[%s][%d]\tInitializing semaphore %d\n",__FILE__,__LINE__,id);
	#endif
	/* The following lines are used to set the initial value of the semaphore
	   to val, including all semaphores within the set of semaphores.
	 */
	union semun 
	{
		int		 val;
		struct semid_ds *buf;
		unsigned short *array;
		struct seminfo *__buf;
	} args;
	unsigned short * buf = (unsigned short *) malloc(sizeof(unsigned short)*num);
	int i = 0;
	for(;i < num; i++)
	    buf[i] = val;
	args.array = buf;
	int status = 0;
	status = semctl(id, 0, SETALL,args);
	if(status == -1)
	{
		perror("Unable to initialize semaphore");
		fprintf(stderr,"Error code: %d\n",status);
	}
	#ifdef DEBUG
    	    fprintf(stderr,"[%s][%d]\tSemaphore %d opened with dimension %d, value: %d\n", __FILE__,__LINE__,id,num,get_sem_value(id,(num-1)));
	#endif
	return id;
}
/* Function to lock (decrement) as semaphore.
   sid is the id of the semaphore as returned by open_sem. 
   num is the element in the semaphore set to lock. 
   Returns 0 on success, -1 otherwise.
 */
int sem_lock(int sid, int num, int dec)
{
	if( num < 0)
	{
	    fprintf(stderr,"Failed to lock semaphore: %d out of bounds\n", sid);
	    return -1;
	}
	int d = -1 * dec;
	struct sembuf sem_lock = { num, d, 0 };
#ifdef DEBUG
    	fprintf(stderr,"[%s][%d]\tLocking semaphore %d id %d, value: %d\n", __FILE__,__LINE__,num,sid,get_sem_value(sid,num));
#endif	
	if((semop(sid,(struct sembuf *) &sem_lock,1) == -1))
	{
	    perror("Failed to lock semaphore");
	    return -1;
	}
	#ifdef DEBUG
    	    fprintf(stderr,"[%s][%d]\tSemaphore %d locked, value: %d\n", __FILE__,__LINE__,num,get_sem_value(sid,num));
	#endif
	return 0;
}
/* Function to unlock (increment) as semaphore.
   sid is the id of the semaphore as returned by open_sem. 
   num is the element in the semaphore set to unlock. 
   Returns 0 on success, -1 otherwise.
 */
int sem_unlock(int sid, int num, int inc)
{
	#ifdef DEBUG
    	    fprintf(stderr,"[%s][%d]\tUnlocking semaphore %d num %d\n",__FILE__,__LINE__,sid,num);
	#endif
        struct sembuf sem_unlock = { num, inc, 0 };
	if((semop(sid, &sem_unlock, 1) == -1))
	    perror("Failed to unlock semaphore");
	#ifdef DEBUG
    	    fprintf(stderr,"[%s][%d]\tSemaphore %d unlocked, value: %d\n", __FILE__,__LINE__,num,get_sem_value(sid,num));
	#endif
	return 0;
}
#endif
