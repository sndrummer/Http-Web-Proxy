#include <stdio.h>
#include <signal.h>
#include "csapp.h"
#include "sbuf.h"
#include "log.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define SBUFSIZE 100   /* number of slots in shared FIFO buffer */
#define LOGBUFSIZE 100 /* number of slots in shared Log message buffer */
#define NTHREADS 4     /* Thread pool size */

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";
static const char *endof_hdr = "\r\n";
static const int DEFAULT_PORT = 80;

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";
void *thread(void *vargp);
void *log_thread(void *vargp);

void reset_log();
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void create_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int init_server_conn(char *hostname, int port, char *http_header);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

sbuf_t sbuf;     /* Shared buffer of connected descriptors */
logbuf_t logbuf; /* Shared buffer/Queue of messages to be written to file */
cache_t cache;   /* Cache object, cache web data*/


int main(int argc, char **argv)
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    reset_log();
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    logbuf_init(&logbuf, LOGBUFSIZE);

    cache_init(&cache, &logbuf);

    for (i = 0; i < NTHREADS; i++) /* Create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);
    /* Launch logging thread */
    Pthread_create(&tid, NULL, log_thread, NULL);
    logbuf_insert(&logbuf, "main()", "Successfully started logging_thread...");
    /* Ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);
    //Signal(SIGINT, sigint_handler); /* ctrl-c */

    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        char message[MAXLINE];
        sprintf(message, "Accepted connection from (%s, %s)", hostname, port);
        logbuf_insert(&logbuf, "main()", message);
        sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }

    return 0;
}

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());

    while (1)
    {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
        doit(connfd);                    /* Service client */
        Close(connfd);
    }
    return NULL;
}

void *log_thread(void *vargp)
{
    Pthread_detach(pthread_self());
    pthread_t tid = pthread_self();
    unsigned long i = (long)tid;
    char thread_str[128];
    sprintf(thread_str, "thread %ld launched", i);
    logbuf_insert(&logbuf, "thread()", thread_str);
    FILE *proxy_log;
    proxy_log = fopen("proxy.log", "w");
    while (1)
    {
        char *log_msg = logbuf_remove(&logbuf);
        fprintf(proxy_log, "%s\n", log_msg);
        fflush(proxy_log);
        fflush(proxy_log);
    }

    fclose(proxy_log);

    return NULL;
}

/*
 * reset_log - initialize log file.
 * Make proxy.log empty file.
 */
void reset_log()
{
    FILE *file = fopen("proxy.log", "w");
    if (!file)
    {
        perror("Could not open file");
    }
    fclose(file);
}

void doit(int connfd)
{
    int server_fd; //fd for the server
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char server_http_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    char og_uri[MAXLINE];
    int port;
    rio_t rio, server_rio;
    // For cache
    char data[MAX_OBJECT_SIZE];
    int data_size = 0;
    //for debugging
    char dbg_msg[MAXLINE];

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); // read the client request line

    if (strcasecmp(method, "GET"))
    {
        clienterror(connfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    /* Check if the uri is cached */
    int cache_index = cache_lookup(&cache, uri);
    if (cache_index >= 0)
    {
        cache_get(&cache, uri, data, &data_size);
        sprintf(dbg_msg, "Cache hit! --> %s, %d bytes.", uri, data_size);
        logbuf_insert(&logbuf, "doit()", dbg_msg);
        Rio_writen(connfd, data, data_size);
        return;
    }

    sprintf(dbg_msg, "cache miss --> %s", uri);
    logbuf_insert(&logbuf, "doit()", dbg_msg);

    strcpy(og_uri, uri); //get copy of original uri
    parse_uri(uri, hostname, path, &port);
    create_http_header(server_http_header, hostname, path, port, &rio);

    //create a connection to the server
    server_fd = init_server_conn(hostname, port, server_http_header);
    if (server_fd < 0)
    {
        logbuf_insert(&logbuf, "doit()", "Server connection failed...");
        return;
    }

    Rio_readinitb(&server_rio, server_fd);
    Rio_writen(server_fd, server_http_header, strlen(server_http_header));
    size_t num_bytes;
    char *current = data;
    while ((num_bytes = Rio_readnb(&server_rio, buf, MAXLINE)))
    {
        data_size += num_bytes;
        if (data_size <= MAX_OBJECT_SIZE)
        {
            memcpy(current, buf, num_bytes);
            current += num_bytes;
        }
        Rio_writen(connfd, buf, num_bytes);
    }

    char dbg_msg2[MAXLINE];
    sprintf(dbg_msg2, "Bytes received: %d ", data_size);
    logbuf_insert(&logbuf, "doit()", dbg_msg2);
    /* Add to Cache */
    cache_uri(&cache, og_uri, data, data_size);

    sprintf(dbg_msg, "Cache size after add: %d ", cache.total_size);
    logbuf_insert(&logbuf, "doit()", dbg_msg);

    Close(server_fd);
}

void create_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
    {
        if (strcmp(buf, endof_hdr) == 0)
            break;

        if (!strncasecmp(buf, host_key, strlen(host_key)))
        {
            strcpy(host_hdr, buf);
            continue;
        }

        if (!strncasecmp(buf, connection_key, strlen(connection_key)) && !strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) && !strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_hdr, buf);
        }
    }
    if (strlen(host_hdr) == 0)
    {
        sprintf(host_hdr, "Host: %s\r\n", hostname);
    }
    sprintf(http_header, "%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            proxy_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return;
}

int init_server_conn(char *hostname, int port, char *http_header)
{
    char port_str[256];
    sprintf(port_str, "%d", port);
    return Open_clientfd(hostname, port_str);
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
    //default port
    *port = DEFAULT_PORT;

    char *cur_pos = strstr(uri, "//");
    cur_pos = cur_pos != NULL ? cur_pos + 2 : uri;

    //Check if there is a port number given
    char *cur_pos2 = strstr(cur_pos, ":");
    if (cur_pos2 != NULL)
    {
        *cur_pos2 = '\0';
        sscanf(cur_pos, "%s", hostname);
        sscanf(cur_pos2 + 1, "%d%s", port, path);
    }
    else
    {
        cur_pos2 = strstr(cur_pos, "/");
        if (cur_pos2 != NULL)
        {
            *cur_pos2 = '\0';
            sscanf(cur_pos, "%s", hostname);
            *cur_pos2 = '/';
            sscanf(cur_pos2, "%s", path);
        }
        else
        {
            sscanf(cur_pos, "%s", hostname);
        }
    }
    return;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor="
                  "ffffff"
                  ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
