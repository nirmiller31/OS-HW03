#include <stdlib.h>
#include <string.h>
#include "segel.h"
#include "log.h"

pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t read_allowed = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t write_allowed = PTHREAD_MUTEX_INITIALIZER;
int readers_inside, writers_inside, writers_waiting;

// Opaque struct definition
struct Server_Log {
    // TODO: Implement internal log storage (e.g., dynamic buffer, linked list, etc.)
};

// Creates a new server log instance (stub)
server_log create_log() {
    
    readers_inside = 0;
    writers_inside = 0;
    writers_waiting = 0;
    mutex_init(&log_lock, NULL);
    cond_init(&read_allowed, NULL);
    cond_init(&write_allowed, NULL);

    // TODO: Allocate and initialize internal log structure
    return (server_log)malloc(sizeof(struct Server_Log));
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    // TODO: Free all internal resources used by the log
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access

    mutex_lock(&log_lock);
    while(writers_inside > 0 || writers_waiting > 0) {
        cond_wait(&read_allowed, &log_lock);
    }
    readers_inside++;
    mutex_unlock(&log_lock);

    // TODO add the logging

    mutex_lock(&log_lock);
    readers_inside--;
    if(readers_inside == 0){
        cond_signal(&write_allowed);
    }
    mutex_unlock(&log_lock);

    // const char* dummy = "Log is not implemented.\n";
    // int len = strlen(dummy);
    // *dst = (char*)malloc(len + 1); // Allocate for caller
    // if (*dst != NULL) {
    //     strcpy(*dst, dummy);
    // }
    // return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {

    mutex_lock(&log_lock);
    writers_waiting++;
    while(writers_inside + readers_inside > 0) {
        cond_wait(&write_allowed, &log_lock);
    }
    writers_waiting--;
    writers_inside++;
    mutex_unlock(&log_lock);

    // TODO add the setter

    mutex_lock(&log_lock);
    writers_inside--;
    if(writers_inside == 0) {
        cond_broadcast(&read_allowed);
        cond_signal(&write_allowed);
    }
    mutex_unlock(&log_lock);
}
