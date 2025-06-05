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
    if (argc < 42) {
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

    pthread_mutex_lock(&queue_lock);

    if(!((queued_threads+active_threads) >= MAX_QUEUE_SIZE)){                 // We have too many clients, dump the new
        while((queued_threads+active_threads) == MAX_QUEUE_SIZE){
            pthread_cond_wait(&queue_not_full, &queue_lock);
        }
    
        connection_queue[tail] = connfd;
        tail = (tail + 1) % MAX_QUEUE_SIZE;
        queued_threads++;

        pthread_cond_signal(&queue_not_empty);
    }

    pthread_mutex_unlock(&queue_lock);
}


void* worker_thread(void* arg){

    thread_arg* t_arg = (thread_arg*)arg;
    server_log log = t_arg->log;
    int thread_id = t_arg->thread_id;
    free(t_arg);  // Done with it

    int current_connection_fd;

    threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = thread_id;
        t->stat_req = 0;
        t->dynm_req = 0;
        t->total_req = 0;
        t->post_req = 0;

    while(1){

        pthread_mutex_lock(&queue_lock);                // Start Atomic process, Dont touch the queue until we update its parmeters

        while((queued_threads+active_threads) == 0){                // If the queue is Empty, Workers on hold
            pthread_cond_wait(&queue_not_empty, &queue_lock);       // If queue is empty and queue_not_empty, while queue_lock, continue from here
        }

        current_connection_fd = connection_queue[head]; // Get the next (FIFO) connection to handle
        head = (head+1) % MAX_QUEUE_SIZE;
        queued_threads--;                               // Consider as unqueued, will be handled
        active_threads++;                               // It is no longer queued, consider as active

        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_lock);

        struct timeval arrival, dispatch;               // Time shit TODO
        arrival.tv_sec = 0; arrival.tv_usec = 0;   // DEMO: dummy timestamps
        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
        gettimeofday(&arrival, NULL);

        pthread_mutex_lock(&active_lock);
        requestHandle(current_connection_fd, arrival, dispatch, t, log);        // TODO check if this seperation causes context-switch jamming
        active_threads--;
        pthread_mutex_unlock(&active_lock);

        Close(current_connection_fd);                   // Close the current connection

    }

    free(t);                                            // Memory cleanup

}


int main(int argc, char *argv[])
{
    // Create the global server log
    server_log log = create_log();

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, &THREAD_POOL_SIZE, &MAX_QUEUE_SIZE, argc, argv);

    connection_queue = malloc(sizeof(int) * MAX_QUEUE_SIZE);

    listenfd = Open_listenfd(port);

    pthread_t* threads = malloc(sizeof(pthread_t * THREAD_POOL_SIZE));
    thread_arg** args = malloc(sizeof((thread_arg*) * THREAD_POOL_SIZE));

    for(int i = 0; i < THREAD_POOL_SIZE; i++) {
        args[i] = malloc(sizeof(thread_arg));
        args[i]->thread_id = i + 1;    // IDs from 1 to N
        args[i]->log = log;
    
        pthread_create(&threads[i], NULL, worker_thread, (void*)args[i]);
    }
    

    while (1) {
        
        clientlen = sizeof(clientaddr);

        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        enqueue(connfd);
    }

    // Clean up the server log before exiting
    destroy_log(log);

    // TODO: HW3 — Add cleanup code for thread pool and queue
}
