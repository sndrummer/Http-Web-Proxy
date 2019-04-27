#include "csapp.h"
#include "log.h"

/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin logbuf_init */

void logbuf_init(logbuf_t *lbuf, int n)
{
    lbuf->msgs = Malloc(n * sizeof(msg_t));
    lbuf->n = n;                     /* Buffer holds max of n messages */
    lbuf->front = lbuf->rear = 0;    /* Empty buffer iff front == rear */
    Sem_init(&lbuf->mutex, 0, 1);    /* Binary semaphore for locking */
    Sem_init(&lbuf->slots, 0, n);    /* Initially, msgs has n empty slots */
    Sem_init(&lbuf->messages, 0, 0); /* Initially, msgs has zero data messages */
}
/* $end logbuf_init */

/* Clean up buffer lbuf */
/* $begin logbuf_deinit */
void logbuf_deinit(logbuf_t *lbuf)
{
    Free(lbuf->msgs);
}
/* $end logbuf_deinit */

/* Insert message onto the rear of shared buffer lbuf */
/* $begin logbuf_insert */
void logbuf_insert(logbuf_t *lbuf, char *tag, char *message)
{
    msg_t msg = convert_message(tag, message);
    P(&lbuf->slots);                           /* Wait for available slot */
    P(&lbuf->mutex);                           /* Lock the buffer */
    lbuf->rear = (lbuf->rear + 1) % (lbuf->n); /* Increment rear */
    lbuf->msgs[lbuf->rear] = msg;              /* Insert the message */
    V(&lbuf->mutex);                           /* Unlock the buffer */
    V(&lbuf->messages);                        /* Announce available message */
}

msg_t convert_message(char *tag, char *message)
{
    msg_t msg;
    msg.tag = tag;
    msg.data = message;
    return msg;
}
/* $end logbuf_insert */

/* Remove and return the first message from buffer lbuf */
/* $begin logbuf_remove */
char *logbuf_remove(logbuf_t *lbuf)
{
    msg_t message;
    P(&lbuf->messages);                          /* Wait for available message */
    P(&lbuf->mutex);                             /* Lock the buffer */
    lbuf->front = (lbuf->front + 1) % (lbuf->n); /* Increment front */
    message = lbuf->msgs[lbuf->front];           /* Remove the message */
    V(&lbuf->mutex);                             /* Unlock the buffer */
    V(&lbuf->slots);                             /* Announce available slot */

    return get_message(&message);
}

char *get_message(msg_t *message)
{
    static char message_f[256];
    time_t now;
    time(&now);
    sprintf(message_f, "%s [%s]: %s\n", ctime(&now), message->tag, message->data);
    return message_f;
}
