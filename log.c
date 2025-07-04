#include <stdlib.h>
#include <string.h>
#include "segel.h"
#include "log.h"

#include <unistd.h>

// Opaque struct definition
struct Server_Log {
    
    char* log_buffer;
    size_t log_size;
    size_t log_capacity;

    pthread_mutex_t log_lock;
    pthread_cond_t read_allowed;
    pthread_cond_t write_allowed;
    
    int readers_inside;
    int writers_inside;
    int writers_waiting;
};

// Creates a new server log instance (stub)
server_log create_log() {
    int rc;
    server_log log = malloc(sizeof(struct Server_Log));
    if(!log){
        perror("Couldn't malloc a log struct");
        return NULL;
    }
    log -> log_size = 0;
    log -> log_capacity = MAXBUF;
    log -> log_buffer = malloc(log -> log_capacity);     // Initial size for a buffer
    if(!log -> log_buffer){
        perror("Couldn't malloc a log");
        free(log);
        return NULL;
    }
    log->log_buffer[0] = '\0';

    log -> readers_inside = 0;
    log -> writers_inside = 0;
    log -> writers_waiting = 0;
    if((rc = pthread_mutex_init(&log -> log_lock, NULL)) != 0){
        posix_error(rc,"pthread_mutex_init Error");
    }
    if((rc = pthread_cond_init(&log -> read_allowed, NULL)) != 0){
        posix_error(rc,"pthread_cond_init Error");
    }
    if((rc = pthread_cond_init(&log -> write_allowed, NULL)) != 0){
        posix_error(rc,"pthread_cond_init Error");
    }

    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    int rc;
    if (!log) return;
    free(log->log_buffer);
    if((rc = pthread_mutex_destroy(&log->log_lock)) != 0){
        posix_error(rc,"pthread_mutex_destroy Error");

    }
    if((rc = pthread_cond_destroy(&log->read_allowed)) != 0){
        posix_error(rc,"pthread_cond_destroy Error");

    }
    if((rc =pthread_cond_destroy(&log->write_allowed)) != 0){
        posix_error(rc,"pthread_cond_destroy Error");    
    }
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // This function should handle concurrent access
    int rc;
    if((rc = pthread_mutex_lock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_lock Error");
    }
    while((log->writers_inside) > 0 || (log->writers_waiting) > 0){
        if((rc = pthread_cond_wait(&(log->read_allowed), &(log->log_lock))) != 0){
            posix_error(rc,"pthread_cond_wait Error");
        }
    }
    (log->readers_inside)++;
    if((rc =pthread_mutex_unlock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_unlock Error");
    }

    size_t len = log->log_size;
    if(len > 0 && log->log_buffer[len - 1] == '\n'){
        len--;
    }

    *dst = malloc(len + 1);
    if(*dst == NULL){                                       // Handle with a failed reading
        perror("Couldn't malloc get_log");
        if((rc = pthread_mutex_lock(&(log->log_lock))) != 0){
            posix_error(rc,"pthread_mutex_lock Error");
        }
        (log->readers_inside)--;
        if((log->readers_inside) == 0){
            if((rc = pthread_cond_signal(&(log->write_allowed))) != 0){
                posix_error(rc,"pthread_cond_signal Error");
            }
        }
        if((rc = pthread_mutex_unlock(&(log->log_lock))) != 0){
            posix_error(rc,"pthread_mutex_unlock Error");
        }
        return 0;
    }

    memcpy(*dst, log->log_buffer, len);
    (*dst)[len] = '\0';

    if((rc = pthread_mutex_lock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_lock Error");
    }
    (log->readers_inside)--;
    if((log->readers_inside) == 0){
        if((rc = pthread_cond_signal(&(log->write_allowed))) != 0){
            posix_error(rc,"pthread_cond_signal Error");
        }
    }
    if((rc = pthread_mutex_unlock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_unlock Error");
    }
    return (int)len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    int rc;
    if((rc = pthread_mutex_lock(&log->log_lock)) != 0){
        posix_error(rc,"pthread_mutex_lock Error");
    }
    (log->writers_waiting)++;
    while((log->writers_inside + log->readers_inside) > 0) {
        if((rc = pthread_cond_wait(&log->write_allowed, &log->log_lock)) != 0){
                posix_error(rc,"pthread_cond_wait Error");
        }
    }
    (log->writers_waiting)--;
    (log->writers_inside)++;
    usleep(200000);    

    if((rc = pthread_mutex_unlock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_unlock Error");
    }

    if (log->log_size + data_len + 1 > log->log_capacity) { // Case we need to increase the log
        size_t new_log_capacity = (log->log_capacity)*2;
        while(new_log_capacity < log->log_size + data_len + 1){
            new_log_capacity *= 2;
        }
        char* new_log_buffer = realloc(log->log_buffer, new_log_capacity);
        if(!new_log_buffer) {
            fprintf(stderr, "ERROR, Failed to realloc new log");
        }
        else{
            log->log_buffer = new_log_buffer;               // Update to the new buffer (sizing)
            log->log_capacity = new_log_capacity;           // Update new capacity

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

    if((rc = pthread_mutex_lock(&log->log_lock)) != 0){
        posix_error(rc,"pthread_mutex_lock Error");
    }

    (log->writers_inside)--;
    if(log->writers_inside == 0){
        if(log->writers_waiting > 0){
		    if((rc = pthread_cond_signal(&(log->write_allowed))) != 0){
                posix_error(rc,"pthread_cond_signal Error");
            }
        }
        else{
            if((rc = pthread_cond_broadcast(&(log->read_allowed))) != 0){
                posix_error(rc,"pthread_cond_broadcast Error");
            }
        }
	    
    }
    if((rc = pthread_mutex_unlock(&(log->log_lock))) != 0){
        posix_error(rc,"pthread_mutex_unlock Error");
    }
}

