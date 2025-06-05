#include <stdlib.h>
#include <string.h>
#include "segel.h"
#include "log.h"

// Opaque struct definition
struct Server_Log {
    
    char* log_buffer;
    size_t log_size;
    size_t log_capacity;

    pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t read_allowed = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t write_allowed = PTHREAD_MUTEX_INITIALIZER;
    
    int readers_inside;
    int writers_inside;
    int writers_waiting;
};

// Creates a new server log instance (stub)
server_log create_log() {

    server_log log = malloc(sizeof(struct Server_Log));
    if(!log){
        perror("Couldn't malloc a log struct");
        return NULL;
    }

    log -> log_size = 0;
    log -> log_capacity = segel::MAXBUF;
    log -> log_buffer = malloc(log -> log_capacity);     // Initial size for a buffer
    if(!log -> buffer){
        perror("Couldn't malloc a log");
        free(log);
        return NULL;
    }
    log -> buffer = '\0';

    log -> readers_inside = 0;
    log -> writers_inside = 0;
    log -> writers_waiting = 0;
    mutex_init(&log -> log_lock, NULL);
    cond_init(&log -> read_allowed, NULL);
    cond_init(&log -> write_allowed, NULL);

    return log;
    // TODO: Allocate and initialize internal log structure
    // return (server_log)malloc(sizeof(struct Server_Log));
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
    while(log->writers_inside + log->readers_inside > 0) {
        cond_wait(&log->write_allowed, &log->log_lock);
    }
    (log->writers_waiting)--;
    (log->writers_inside)++;
    mutex_unlock(&log->log_lock);

    if(log->size + 1 > log->log_capacity) {                 // Case we need to increase the log
        size_t new_log_capcity = (log->log_capacity)*2;
        while(new_log_capcity < log->log_size + data_len + 1){
            new_log_capcity *= 2;
        }
        char* new_log_buffer = realloc(log->log_buffer, new_log_capcity);
        if(!new_log_buffer) {
            fprintf(stderr, "ERROR, Failed to realloc new log");
        }
        else{
            log->log_buffer = new_log_buffer;               // Update to the new buffer (sizing)
            log->log_capacity = new_log_capcity;            // Update new capacity

            memcpy(log->log_buffer + log->log_size, data, data_len);        // Copy the new data to the pointer of log_buffer+log_size (end of previous log)
            log->log_size += data_len;                      // Increase the log_size by the size we added
            log->log_buffer[log->log_size] = '\0';          // End the new log
        }
    }
    else {                                                  // Case the old capacity is enough
        memcpy(log->log_buffer + log->log_size, data, data_len);        // Copy the new data to the pointer of log_buffer+log_size (end of previous log)
        log->log_size += data_len;                          // Increase the log_size by the size we added
        log->log_buffer[log->log_size] = '\0';              // End the new log    
    }

    mutex_lock(&log->log_lock);
    writers_inside--;
    if(writers_inside == 0) {
        cond_broadcast(&read_allowed);
        cond_signal(&write_allowed);
    }
    mutex_unlock(&log_lock);
}
