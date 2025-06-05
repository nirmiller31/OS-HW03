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

#define MAX_QUEUE_SIZE 64
#define THREAD_POOL_SIZE 8

int connection_queue[MAX_QUEUE_SIZE];
int head = 0;
int tail = 0;
int queued_threads = 0;
int active_threads = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

// Parses command-line arguments
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
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

    server_log log = (server_log)arg;

    int current_connection_fd;

    threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = 0;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->total_req = 0;      // Total request count

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

    getargs(&port, argc, argv);

    listenfd = Open_listenfd(port);

    pthread_t threads[THREAD_POOL_SIZE];
    for(int i=0 ; i<THREAD_POOL_SIZE ; i++){
        pthread_create(&threads[i], NULL, worker_thread, (void*)log);
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
