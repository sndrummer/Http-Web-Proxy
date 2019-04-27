/*
 * log.h - thread safe logging
 */
/* $begin log.h */

#ifndef __LOG_H__
#define __LOG_H__

/* 
You should also create a logging thread to log web accesses and requests to
a file. It should dequeue messages that are put into a logging queue by the
threads handling requests.
*/

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct
{
    char* tag;
    char *data;
} msg_t;

typedef struct
{
    msg_t *msgs;    /* Message buffer -- HOLDS THE MESSAGES TO BE DEQUED */
    int n;          /* Maximum number of slots */
    int front;      /* buf[(front+1)%n] is first message */
    int rear;       /* buf[rear%n] is last message */
    sem_t mutex;    /* Protects accesses to buf */
    sem_t slots;    /* Counts available slots */
    sem_t messages; /* Counts available messages */
} logbuf_t;


void logbuf_init(logbuf_t *lbuf, int n);
void logbuf_deinit(logbuf_t *lbuf);
void logbuf_insert(logbuf_t *lbuf, char *tag, char *message);
char *logbuf_remove(logbuf_t *lbuf);
msg_t convert_message(char *tag, char *message);
char *get_message(msg_t *message);

#endif /* __LOG_H__ */