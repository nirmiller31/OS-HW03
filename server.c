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

typedef struct {
    int connfd;
    struct timeval arrival_time;
} connection_t;

connection_t *connection_queue;
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
    if (argc < 4) {
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

void enqueue(int connfd, struct timeval arrival_time) {

    pthread_mutex_lock(&queue_lock);

    while ((queued_threads + active_threads) == MAX_QUEUE_SIZE) {
        pthread_cond_wait(&queue_not_full, &queue_lock);            // BLOCK until space
    }

    connection_queue[tail].connfd = connfd;
    connection_queue[tail].arrival_time = arrival_time;
    tail = (tail + 1) % MAX_QUEUE_SIZE;
    queued_threads++;

    pthread_cond_signal(&queue_not_empty);                          // wake a worker
    pthread_mutex_unlock(&queue_lock);

}

void* worker_thread(void* arg){

    thread_arg* t_arg = (thread_arg*)arg;
    server_log log = t_arg->log;
    int thread_id = t_arg->thread_id;
    free(t_arg);                                                    // Done with it

    connection_t current_connection_fd;

    threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = thread_id;
        t->stat_req = 0;
        t->dynm_req = 0;
        t->total_req = 0;
        t->post_req = 0;

    while(1){

        pthread_mutex_lock(&queue_lock);                            // Start Atomic process, Dont touch the queue until we update its parmeters
        
        while(queued_threads == 0){                                 // If the queue is Empty, Workers on hold
            pthread_cond_wait(&queue_not_empty, &queue_lock);       // If queue is empty and queue_not_empty, while queue_lock, continue from here
        }

        current_connection_fd = connection_queue[head];             // Get the next (FIFO) connection to handle
        head = (head+1) % MAX_QUEUE_SIZE;
        queued_threads--;                                           // Consider as unqueued, will be handled
        active_threads++;                                           // It is no longer queued, consider as active

        pthread_mutex_unlock(&queue_lock);

	struct timeval now, dispatch;
	long sec_tmp, usec_tmp;

	gettimeofday(&now, NULL);  // current time (when worker picks up request)

// Compute interval = now - arrival
	sec_tmp = now.tv_sec - current_connection_fd.arrival_time.tv_sec;
	usec_tmp = now.tv_usec - current_connection_fd.arrival_time.tv_usec;
	if (usec_tmp < 0) {
    		sec_tmp -= 1;
    		usec_tmp += 1000000;
	}

	dispatch.tv_sec = sec_tmp;
	dispatch.tv_usec = usec_tmp;
        requestHandle(current_connection_fd.connfd, current_connection_fd.arrival_time, dispatch, t, log);        // TODO check if this seperation causes context-switch jamming
        Close(current_connection_fd.connfd);

        pthread_mutex_lock(&queue_lock);
        active_threads--;
        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_lock);
        
    }
    free(t);                                                        // Memory cleanup
}


int main(int argc, char *argv[])
{
    server_log log = create_log();                                  // Create the global server log

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, &THREAD_POOL_SIZE, &MAX_QUEUE_SIZE, argc, argv);

    connection_queue = malloc(sizeof(connection_t) * MAX_QUEUE_SIZE);

    listenfd = Open_listenfd(port);

    pthread_t* threads = malloc(sizeof(pthread_t) * THREAD_POOL_SIZE);
    thread_arg** args = malloc(sizeof(thread_arg*) * THREAD_POOL_SIZE);

    for(int i = 0; i < THREAD_POOL_SIZE; i++) {                     // Create THREAD_POOL_SIZE thread amount
        args[i] = malloc(sizeof(thread_arg));
        args[i]->thread_id = i + 1;                                 // IDs from 1 to N
        args[i]->log = log;
                                                                    // Send each thread to worker_thread
        pthread_create(&threads[i], NULL, worker_thread, (void*)args[i]);
    }
	struct timeval arrival;                           // Time shit TODO
    
    while (1) {
        
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
	    gettimeofday(&arrival, NULL);
        enqueue(connfd, arrival);
    }

    destroy_log(log);                                               // Clean up the server log before exiting

    // TODO: HW3 — Add cleanup code for thread pool and queue verify it is enough
}
