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

#define MAX_QUEUE_SIZE 64
#define THREAD_POOL_SIZE 8

int connection_queue[MAX_QUEUE_SIZE];
int head = 0;
int tail = 0;
int count = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
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

    while(count == MAX_QUEUE_SIZE){
        pthread_cond_wait(&queue_not_full, &queue_lock);
    }

    connection_queue[tail] = connfd;
    tail = (tail + 1) % MAX_QUEUE_SIZE;
    count++;

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_lock);
}


void* worker_thread(void* arg){
    (void)arg;                      // We ignore it now, TODO use the args

    threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = 0;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->total_req = 0;      // Total request count

    while(1){

        int current_connection_fd;

        pthread_mutex_lock(&queue_lock);

        while(count == 8){
            pthread_cond_wait(&queue_not_empty, &queue_lock);
        }

        current_connection_fd = connection_queue[head];
        head = (head+1) % MAX_QUEUE_SIZE;
        count--;

        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_lock);

        struct timeval arrival, dispatch;               // Time shit TODO
        arrival.tv_sec = 0; arrival.tv_usec = 0;   // DEMO: dummy timestamps
        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
        // gettimeofday(&arrival, NULL);

        requestHandle(connfd, arrival, dispatch, t, log);
        Close(connfd);                                  // Close the current connection

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
        pthread_create(&threads[i], NULL, worker_thread, NULL);
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
