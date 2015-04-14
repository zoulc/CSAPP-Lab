/*
zouliangchuan
1300012962@pku.edu.cn
In this lab, I implemented a simple multithread proxy with a cache. When the proxy receives a request from browser,
it creates a new thread to manipulate it: if the requested URL was cached, then simply return the cached content to browser;
otherwise, send the same reequest to server and get the content, which will then be cached and sent back to browser.
The cache uses a Least-Recently-Used eviction stratety.
*/
#include <stdio.h>
#include "csapp.h"
#define NDEBUG
#include "assert.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *saccept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

struct cache_t {
    char *data, *hostname, *pathname;
    int time, len;
} cache[1000];            //data structure for caching web contents

int global_time = 0, cache_num = 0, cache_total = 0;
//the last time using cache, total number of cache entries, total length(in bytes) of cache content
sem_t mutex;              //global variable for multithread working(set lock)

void SIG_HANDLER(int sig) {              //to avoid being interrupted by SIGPIPE
    return;
}

void clienterror(int fd, char *cause, char *errnum,
                char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "%s: %s\r\n", errnum, shortmsg);
    sprintf(body, "%s%s: %s\r\n", body, longmsg, cause);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

//transform uri to hostname, pathname and port
int parse_uri(char* uri, char* hostname, char* pathname, int* port) {
    char* p;
    if (strncasecmp(uri, "http://", 7) != 0)
        return -1;
    strcpy(hostname, uri + 7);
    p = strpbrk(hostname, " :/\r\n\0");
    assert(p != 0);
    *port = 80;
    if (*p == ':')
        *port = atoi(p + 1);
    *p = '\0';
    p = strchr(uri + 7, '/');
    if (p == NULL)
        pathname[0] = '\0';
    else
        strcpy(pathname, p);
    return 0;
}

//find the index of cache table entry for the given hostname and pathname
int cache_find(char* hostname, char* pathname) {
    int ret = -1, i;
    for (i = 0; i < cache_num; ++i)
	if (strcmp(cache[i].hostname, hostname) == 0)
	    if (strcmp(cache[i].pathname, pathname) == 0) {
		ret = i;
		break;
	    }
    return ret;
}

//if there is spare space, return a new empty cache table entry's index;
//otherwise, choose the least recently used cache table entry for eviction.
int cache_evict(int len) {
    int i, minT = global_time + 1, minI = -1;
    if (len + cache_total <= MAX_CACHE_SIZE) {
	memset(&cache[cache_num], 0, sizeof(struct cache_t));
	return cache_num++;
    }
    for (i = 0; i < cache_num; ++i)
	if (cache[i].time < minT) {
	    minT = cache[i].time;
	    minI = i;
	}
    return minI;
}

//make cache table entry i the most recently used one.
void update_cachetime(int i) {
    cache[i].time = ++global_time;
}

void cache_insert(char* data, int len, char* hostname, char* pathname) {
    int i = cache_evict(len);
    //if cache[i] is not a new empty one, free the space occupied by it
    if (cache[i].len != 0) {
	cache_total -= cache[i].len;
	if (cache[i].data)
	    Free(cache[i].data);
	if (cache[i].hostname)
	    Free(cache[i].hostname);
	if (cache[i].pathname)
	    Free(cache[i].pathname);
    }
    cache[i].data = calloc(len + 1, sizeof(char));
    cache[i].hostname = calloc(strlen(hostname) + 1, sizeof(char));
    cache[i].pathname = calloc(strlen(pathname) + 1, sizeof(char));
    memcpy(cache[i].data, data, len);
    memcpy(cache[i].hostname, hostname, strlen(hostname));
    memcpy(cache[i].pathname, pathname, strlen(pathname));
    cache[i].len = len;
    update_cachetime(i);
    cache_total += len;
    return;
}

//executed in a new thread; handle a request from web browser
void* doit(void* vargp) {
    rio_t rio, rio_client;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];
    int port, fd, clientfd, size, totalsize;
    char* now;

    Pthread_detach(pthread_self());
    fd = *((int *)vargp);
    Free(vargp);
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);

    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Not Implemented");
        return NULL;
    }

    if (parse_uri(uri, hostname, pathname, &port) < 0) {
        clienterror(fd, method, "400", "Bad Request", "URI Cannot Be Parsed");
        return NULL;
    }

    fprintf(stderr, "URI parsed successfully\n");
    P(&mutex);
    int i = cache_find(hostname, pathname);
    //the requested content was cached, return it immeditately
    if (i != -1) {
	update_cachetime(i);
	Rio_writen(fd, cache[i].data, cache[i].len);
	V(&mutex);
	close(fd);
	return NULL;
    }
    V(&mutex);
    now = calloc(100000, sizeof(char));
    totalsize = 0;
    fprintf(stderr, "%s %d\n", hostname, port);
    //connect to server and get the content
    clientfd = open_clientfd(hostname, port);
    if (clientfd == -1) {
	close(fd);
	return NULL;
    }
    Rio_readinitb(&rio_client, clientfd);
    sprintf(buf, "%s %s %s\r\n", method, pathname, version);
    Rio_writen(clientfd, buf, strlen(buf));
    fprintf(stderr, "%s", buf);
    while (Rio_readlineb(&rio, buf, MAXLINE) > 2) {
        if (strstr(buf, "Proxy-Connection"))
            strcpy(buf, "Proxy-Connection: close\r\n");
        else if (strstr(buf, "Connection"))
            strcpy(buf, "Connection: close\r\n");
        else if (strstr(buf, "Accept-Encoding"))
            strcpy(buf, accept_encoding);
        else if (strstr(buf, "Accept"))
            strcpy(buf, saccept);
        else if (strstr(buf, "User-Agent"))
            strcpy(buf, user_agent);
        fprintf(stderr, "%s", buf);
        Rio_writen(clientfd, buf, strlen(buf));
    }
    Rio_writen(clientfd, "\r\n", 2);
    fprintf(stderr, "%s", buf);
    while (Rio_readlineb(&rio_client, buf, MAXLINE) > 2) {
        fprintf(stderr, "%s", buf);
        //Rio_writen(fd, buf, strlen(buf));
	memcpy(now + totalsize, buf, strlen(buf));
	totalsize += strlen(buf);
    }
    strcpy(buf, "\r\n");
    fprintf(stderr, "%s", buf);
    //Rio_writen(fd, buf, strlen(buf));
    memcpy(now + totalsize, buf, strlen(buf));
    totalsize += strlen(buf);

    while ((size = Rio_readnb(&rio_client, buf, MAXLINE)) > 0) {
        memcpy(now + totalsize, buf, size);
        totalsize += size;
    }
    //send the content back to browser and cache it
    Rio_writen(fd, now, totalsize);
    if (totalsize <= MAX_OBJECT_SIZE)
        cache_insert(now, totalsize, hostname, pathname);
    fprintf(stderr, "Get HTTP contents from server successfully\n");
    free(now);
    close(clientfd);
    close(fd);
    return NULL;
}

int main(int argc, char **argv)
{
    int listenfd, *connfdp, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    fprintf(stderr, "%s%s%s\n", user_agent, saccept, accept_encoding);
    port = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    listenfd = open_listenfd(port);
    sem_init(&mutex, 0, 1);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        Pthread_create(&tid, NULL, doit, connfdp);
    }
    return 0;
}
