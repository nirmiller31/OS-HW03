#include "segel.h"
#include "request.h"
#include "log.h"

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// TODO, handle: Proper error checking and robust synchronization (no race condition, no deadlocks) are required.

int MAX_QUEUE_SIZE;
int THREAD_POOL_SIZE;

typedef struct {
    int thread_id;
    server_log log;
} thread_arg;

int *connection_queue;
int head = 0;
int tail = 0;
int queued_threads = 0;
int active_threads = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

// Parses command-line arguments
void getargs(int *port, int *threads, int *queue_size, int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
}
// TODO: HW3 — Initialize thread pool and request queue
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.

void enqueue(int connfd) {
    int rc;
    if((rc = pthread_mutex_lock(&queue_lock)) != 0){
        posix_error(rc,"pthread_mutex_lock failed");
    }

    while ((queued_threads + active_threads) == MAX_QUEUE_SIZE) {
        if((rc = pthread_cond_wait(&queue_not_full, &queue_lock)) != 0){           // BLOCK until space
            posix_error(rc,"pthread_cond_wait failed");
        }
    }

    connection_queue[tail] = connfd;
    tail = (tail + 1) % MAX_QUEUE_SIZE;
    queued_threads++;

    if((rc = pthread_cond_signal(&queue_not_empty)) != 0){                         // wake a worker
        posix_error(rc,"pthread_cond_signal failed");
    }
    if((rc = pthread_mutex_unlock(&queue_lock)) != 0){
        posix_error(rc,"pthread_mutex_unlock failed");
    }
}

void* worker_thread(void* arg){

    thread_arg* t_arg = (thread_arg*)arg;
    server_log log = t_arg->log;
    int thread_id = t_arg->thread_id;
    free(t_arg);                                                    // Done with it

    int current_connection_fd;
    int rc;
    threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = thread_id;
        t->stat_req = 0;
        t->dynm_req = 0;
        t->total_req = 0;
        t->post_req = 0;

    while(1){

        if((rc = pthread_mutex_lock(&queue_lock)) != 0){                           // Start Atomic process, Dont touch the queue until we update its parmeters
            posix_error(rc,"pthread_mutex_lock failed");
        }
        while(queued_threads == 0){                                 // If the queue is Empty, Workers on hold
            if((rc = pthread_cond_wait(&queue_not_empty, &queue_lock)) != 0){       // If queue is empty and queue_not_empty, while queue_lock, continue from here
                posix_error(rc,"pthread_cond_wait failed");
            }
        }

        current_connection_fd = connection_queue[head];             // Get the next (FIFO) connection to handle
        head = (head+1) % MAX_QUEUE_SIZE;
        queued_threads--;                                           // Consider as unqueued, will be handled
        active_threads++;                                           // It is no longer queued, consider as active

        if((rc = pthread_mutex_unlock(&queue_lock)) != 0){
            posix_error(rc,"pthread_mutex_unlock failed");
        }

        struct timeval arrival, dispatch;                           // Time shit TODO
        gettimeofday(&arrival, NULL);
        gettimeofday(&dispatch, NULL);                              // TODO verify correct location for dispatch/arrival
        requestHandle(current_connection_fd, arrival, dispatch, t, log);        // TODO check if this seperation causes context-switch jamming
        Close(current_connection_fd);

        if((rc = pthread_mutex_lock(&queue_lock)) != 0){
            posix_error(rc,"pthread_mutex_lock failed");
        }
        active_threads--;
        if((rc = pthread_cond_signal(&queue_not_full)) != 0){
            posix_error(rc,"pthread_cond_signal failed");
        }
        if((rc = pthread_mutex_unlock(&queue_lock)) != 0){
            posix_error(rc,"pthread_mutex_unlock failed");
        }  
    }
    free(t);                                                        // Memory cleanup
}


int main(int argc, char *argv[])
{
    server_log log = create_log();                                  // Create the global server log

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    int rc;
    getargs(&port, &THREAD_POOL_SIZE, &MAX_QUEUE_SIZE, argc, argv);

    connection_queue = malloc(sizeof(int) * MAX_QUEUE_SIZE);
    if(connection_queue == NULL){
        perror("malloc failed");
    }
    listenfd = Open_listenfd(port);

    pthread_t* threads = malloc(sizeof(pthread_t) * THREAD_POOL_SIZE);
    if(threads == NULL){
        perror("malloc failed");
    }
    thread_arg** args = malloc(sizeof(thread_arg*) * THREAD_POOL_SIZE);
    if(args == NULL){
        perror("malloc failed");
    }
    for(int i = 0; i < THREAD_POOL_SIZE; i++) {                     // Create THREAD_POOL_SIZE thread amount
        args[i] = malloc(sizeof(thread_arg));
        if(args[i] == NULL){
            perror("malloc failed");
        }
        args[i]->thread_id = i + 1;                                 // IDs from 1 to N
        args[i]->log = log;
                                                                    // Send each thread to worker_thread
        if((rc = pthread_create(&threads[i], NULL, worker_thread, (void*)args[i])) != 0){
            posix_error(rc,"pthread_create failed");
        }
    }
    
    while (1) {
        
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        enqueue(connfd);
    }

    destroy_log(log);                                               // Clean up the server log before exiting

    // TODO: HW3 — Add cleanup code for thread pool and queue verify it is enough
}
