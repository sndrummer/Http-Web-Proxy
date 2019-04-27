#include "csapp.h"
#include "cache.h"
#include <string.h>

/*
    P = wait() 
    V = signal() 
*/

/* READ */
int read_cnt;  /* Initially = 0 */
int write_cnt; /* Initially = 0 */

static void pre_read();
static void post_read();
static void pre_write();
static void post_write();

/* Read/Write Protection, Initially = 1 */
sem_t outerQ;
sem_t rsem;
sem_t rmutex;
sem_t wmutex;
sem_t wsem;

logbuf_t *logger; /* Shared buffer/Queue of messages to be written to file */

/* Cache Functions */
void cache_init(cache_t *cache, logbuf_t *logbuf)
{
    logger = logbuf;

    read_cnt = 0;
    write_cnt = 0;

    Sem_init(&outerQ, 0, 1);
    Sem_init(&rsem, 0, 1);
    Sem_init(&rmutex, 0, 1);
    Sem_init(&wmutex, 0, 1);
    Sem_init(&wsem, 0, 1);

    cache->total_size = 0;
    cache->block_count = 0;
    cache->head = NULL;
}

void cache_cleanup(cache_t *cache)
{
    data_block_t *current = cache->head;
    data_block_t *next;

    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }

    /* deref head to affect the real head back 
      in the caller. */
    cache->head = NULL;
}

int cache_lookup(cache_t *cache, char *uri)
{
    pre_read();
    /* Read */
    int i;
    int found = -1;
    data_block_t *block;

    for (i = 0, block = cache->head; block != NULL; block = block->next, i++)
    {
        if (!strcmp(block->uri, uri)) /* uri matches */
        {
            found = i;
        }
    }
    /* End Read */
    post_read();

    return found;
}

void cache_get(cache_t *cache, char *uri, char *data, int *data_size)
{
    char dbg_msg[MAXLINE];
    sprintf(dbg_msg, "Retrieving uri: %s", uri);
    logbuf_insert(logger, "cache_get()", dbg_msg);

    pre_write();
    /* Write  */
    data_block_t *block;
    for (block = cache->head; block != NULL; block = block->next)
    {
        if (!strcmp(block->uri, uri)) // uri matches, copy cache block to data buf
        {
            memcpy(data, block->data, block->size);
            *data_size = block->size;
            block->LRU = 0;
        }
        else
        {
            block->LRU += 1;
        }
    }
    /* End Write */
    post_write();
}

void cache_add(cache_t *cache, char *uri, char *data, int size)
{
    char dbg_msg[MAXLINE];
    sprintf(dbg_msg, "Adding --> %s, to cache, %d bytes.", uri, size);
    logbuf_insert(logger, "cache_add1()", dbg_msg);

    pre_write();
    /* Write  */
    data_block_t *block = Malloc(sizeof(data_block_t));
    block->uri = Malloc(strlen(uri) + 1);
    block->data = Malloc(size);
    strcpy(block->uri, uri);
    memcpy(block->data, data, size);
    block->size = size;
    /* Insert new block at the head */
    block->next = cache->head;
    cache->head = block;
    cache->total_size += size;
    cache->block_count++;
    update_LRU(cache);
    block->LRU = 0;
    /* End Write */
    post_write();

    char dbg_msg2[MAXLINE];
    sprintf(dbg_msg2, "Cache size after adding --> %d.", cache->total_size);
    logbuf_insert(logger, "cache_add2()", dbg_msg2);
}

void cache_evict(cache_t *cache, char *uri, char *data, int size)
{
    logbuf_insert(logger, "cache_evict()", "EVICTING!!");
    pre_write();
    /* Write  */
    data_block_t *block = cache->head;
    data_block_t *prev = NULL;
    data_block_t *prev_LRU = NULL;
    data_block_t *LRU_block;
    int cur_LRU = -1;

    /* Get LRU_block */
    while (block != NULL)
    {
        if (block->LRU > cur_LRU)
        {
            LRU_block = block;
            cur_LRU = block->LRU;
            prev_LRU = prev;
        }
        prev = block;
        block = block->next;
    }
    if (prev_LRU != NULL)
    {
        prev_LRU->next = LRU_block->next;
    }
    else
    {
        cache->head = NULL;
    }

    cache->total_size = cache->total_size - LRU_block->size;
    cache->block_count--;
    free(LRU_block->uri);
    free(LRU_block->data);
    free(LRU_block);
    LRU_block = NULL;

    /* End Write */
    post_write();

    char dbg_msg2[MAXLINE];
    sprintf(dbg_msg2, "Cache size after evicting --> %d.", cache->total_size);
    logbuf_insert(logger, "cache_evict2()", dbg_msg2);
}

void cache_uri(cache_t *cache, char *uri, char *data, int size)
{
    if (size <= MAX_OBJECT_SIZE)
    {   
        /* Make room for new data */
        while (size + cache->total_size > MAX_CACHE_SIZE)
        {
            cache_evict(cache, uri, data, size);
        }

        cache_add(cache, uri, data, size);
    }
    else
    {
        char dbg_msg[MAXLINE];
        sprintf(dbg_msg, "Object too large to cache --> %s, %d bytes.", uri, size);
        logbuf_insert(logger, "cache_add()", dbg_msg);
    }
}

void update_LRU(cache_t *cache)
{
    data_block_t *block = cache->head;
    while (block != NULL)
    {
        block->LRU++;
        block = block->next;
    }
}

/* End of cache Functions */

static void pre_read()
{
    P(&outerQ);
    P(&rsem);
    P(&rmutex);
    read_cnt++;
    if (read_cnt == 1)
        P(&wsem);
    V(&rmutex);
    V(&rsem);
    V(&outerQ);
}

static void post_read()
{
    P(&rmutex);
    read_cnt--;
    if (read_cnt == 0)
        V(&wsem);
    V(&rmutex);
}

static void pre_write()
{
    P(&wsem);
    write_cnt++;
    if (write_cnt == 1)
        P(&rsem);
    V(&wsem);
    P(&wmutex);
}

static void post_write()
{
    V(&wmutex);
    P(&wsem);
    write_cnt--;
    if (write_cnt == 0)
        V(&rsem);
    V(&wsem);
}
