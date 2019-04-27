#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"
#include "log.h"

#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400

struct data_block;
/*
    REMEMBER that P is wait and V is signal 
    ========================================
    V (for signal passing operation)─ derived from the word 'Verhogen' which means 'to increment'. 
    P semaphore function signals that the task requires a resource and if not available waits for it. 
    V semaphore function signals which the task passes to the OS that the resource is now free for the other users.
*/


typedef struct
{ 
    int total_size;
    int block_count;
    struct data_block *head;
} cache_t;

struct data_block
{
    char *uri;  /* uri of the cached data  */
    char *data; /* Cache data  */
    int size;   /* Size of Cache data  */
    int LRU;    /* Value that shows how recently the data block was used */
    struct data_block *next;
};

typedef struct data_block data_block_t;

/* Cache Functions */
void cache_init(cache_t *cache, logbuf_t *logbuf);
void cache_cleanup(cache_t *cache);

int cache_lookup(cache_t *cache, char *uri); /* Look for uri in cache, return -1 if not found or index of hit */
void cache_get(cache_t *cache, char *uri, char *data, int *data_size);
void cache_add(cache_t *cache, char *uri, char *data, int size);
void cache_evict(cache_t *cache, char *uri, char *data, int size);
void cache_uri(cache_t *cache, char *uri, char *data, int size);
void update_LRU(cache_t *cache);

#endif //__CACHE_H__