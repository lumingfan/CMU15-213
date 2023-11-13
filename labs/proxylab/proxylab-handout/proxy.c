#include <stdio.h>
#include <string.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_BACKLOG 1024
#define MAX_LINE_LEN 64
#define MAX_REQUEST_LEN 4096
#define THREAD_NUM 4
#define FDBUF_SIZE 16
#define MAX_TRANSMIT_SIZE (1 << 31)

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


struct sbuf_t {
    int fdbuf[FDBUF_SIZE];
    int head;
    int tail;

    sem_t lock;
    sem_t remain;
    sem_t available;
};
typedef struct sbuf_t sbuf_t;
static void init_sbuf(sbuf_t *buf);
static void insert_sbuf(sbuf_t *buf, int fd);
static int remove_sbuf(sbuf_t *buf);


struct cache_node_t {
    struct cache_node_t *next;
    struct cache_node_t *prev;
    char *content;  
    char tag[MAX_LINE_LEN];
    int size;
};

typedef struct cache_node_t cache_node_t;
static void create_node(cache_node_t *node, 
                        const char *content, 
                        const char *tag);
static void delete_node(cache_node_t *node);

struct cache_t {
    cache_node_t *sentinel;  
    int total_size;
};
typedef struct cache_t cache_t;
static void init_cache(cache_t *cache);
static void insert_cache(cache_t *cache, const char *content, const char *tag);
static void insert_node(cache_t *cache, cache_node_t *node);
static void remove_cache(cache_t *cache);  // remove the LRU item from cache
static void remove_node(cache_node_t *node);
static void free_cache(cache_t *cache);

// used for thread argument passing
typedef struct {
    sbuf_t *sbuf;
    cache_t *cache;
} sbufcache_t;

// using LRU 
static void replace_cache(cache_t *cache, const char *content, const char *tag);
static char *find_cache(cache_t *cache, const char *tag);


static void process_client(int clientfd, cache_t *cache);
static int process_http_header(rio_t *rp, char **pte, 
                        char *method, char *hostName, char *port,
                        char *path, char *version);
static int process_request_header(rio_t *rp, char **pte, const char *hostName);
static int process_url(const char *url, char *hostName, char *port, char *path);

static void forwarding(char *message, size_t requestLen, char *hostName, 
                        char *port, char *path, int clientfd, cache_t *cache); 

static void *thread_func(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port", argv[0]);
        exit(-1);
    }
    printf("%s", user_agent_hdr);

    // get listen fd of server
    int listenfd = Open_listenfd(argv[1]);

    sbuf_t buf;
    cache_t cache;
    init_sbuf(&buf);
    init_cache(&cache);
    sbufcache_t arg;
    arg.sbuf = &buf;
    arg.cache = &cache;

    pthread_t threadspool[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; ++i) {
        Pthread_create(&threadspool[i], NULL, thread_func, &arg);
    }


    while (1) {
        struct sockaddr client;
        socklen_t clientLen = sizeof(client); 

        // connect with client
        int connectfd = Accept(listenfd, &client, &clientLen);

        // print client information
        char host[MAX_LINE_LEN], serv[MAX_LINE_LEN];
        Getnameinfo(&client, clientLen, 
                    host, MAX_LINE_LEN, 
                    serv, MAX_LINE_LEN, 0);
        printf("connect to %s: %s\n", host, serv);

        // insert new connected file descriptor to fd buffer
        insert_sbuf(&buf, connectfd);
    }

    free_cache(&cache);
}

void process_client(int clientfd, cache_t *cache) {
    // initial rio buffer
    rio_t rio; 
    Rio_readinitb(&rio, clientfd);

    // process http request message from client, then send proxyRequet
    // to origin server
    // request_pointer record the end of proxyRequest
    char proxyRequest[MAX_REQUEST_LEN];
    char *request_pointer = proxyRequest;

    char method[MAX_LINE_LEN], hostName[MAX_LINE_LEN], port[MAX_LINE_LEN], 
         path[MAX_LINE_LEN], http_version[MAX_LINE_LEN];

    // http Header
    if (!process_http_header(&rio, &request_pointer, 
                        method, hostName, port, path, http_version)) {
        // not GET method
        fprintf(stderr, "URL format error or" 
            " Doesn't support method: %s\n", method);
        return ;
    }
    // add blank line: \r\n
    int cnt = sprintf(request_pointer, "\r\n");
    request_pointer += cnt;
    // request Header
    if (!process_request_header(&rio, &request_pointer, hostName)) {
        fprintf(stderr, "Header format is error\n");
        return ;
    }

    printf("%s\n", proxyRequest);

    // forward client request to origin server and get returned object
    // if size of returned object is bigger than MAX_OBJECT_SIZE, then 
    // truncate it
    size_t requestLen = request_pointer - &proxyRequest[0];
    forwarding(proxyRequest, requestLen, 
            hostName, port, path, clientfd, cache); 
}


void forwarding(char *message, size_t requestLen, char *hostName, 
                char *port, char *path, int clientfd, cache_t *cache) {

    char tag[MAX_LINE_LEN];
    strcat(tag, hostName);
    strcat(tag, ":");
    strcat(tag, port);
    strcat(tag, path);
    char *content;
    printf("%s\n", tag);
    
    // find content in cache
    if ((content = find_cache(cache, tag)) != NULL) {
        Rio_writen(clientfd, content, strlen(content));
        return ;
    }
 
    // connect to default http port
    int connectfd = open_clientfd(hostName, port);    
    // connect error
    if (connectfd < 0) {
        fprintf(stderr, "Open_clientfd error\n");
        return ;
    }

    rio_t rio;
    Rio_readinitb(&rio, connectfd);

    // sent request
    Rio_writen(connectfd, message, requestLen); 

    // get response
    int cnt;
    int total_bytes = 0;
    char usrbuf[MAX_OBJECT_SIZE];
    char cachebuf[MAX_OBJECT_SIZE];
    int flag = 0;

    while ((cnt = Rio_readnb(&rio, usrbuf, MAX_OBJECT_SIZE))) {
        total_bytes += cnt;
        if (total_bytes > MAX_OBJECT_SIZE) {
            flag = 1;
        } else {
            strncat(cachebuf, usrbuf, cnt);
        }
        Rio_writen(clientfd, usrbuf, cnt);
    }

   
    if (!flag ) {
        if (cache->total_size + total_bytes < MAX_CACHE_SIZE) {
            // insert
            insert_cache(cache, cachebuf, tag);
        } else {
            // replace
            replace_cache(cache, cachebuf, tag);
        }
    }
    close(connectfd);
}

int process_http_header(rio_t *rp, char **pte, 
                    char *method, char *hostName, char *port,
                    char *path, char *version) {
    char usrbuf[MAX_LINE_LEN];
    ssize_t len = Rio_readlineb(rp, usrbuf, MAX_LINE_LEN);
    usrbuf[len - 2] = ' ';
    
    char *arr = usrbuf;
    char *p = strchr(arr, ' ');
    *p = '\0';
    strcpy(method, arr); // get method

    // don't support other method
    if (strcmp(method, "GET")) {
        return 0;
    }

    char url[MAX_LINE_LEN];
    arr = p + 1;
    p = strchr(arr, ' ');
    *p = '\0';
    strcpy(url, arr); // get url
    
    // using url fill hostName, port and path
    // if wrong format, return 0
    if (!process_url(url, hostName, port, path)) {
        return 0;
    }

    arr = p + 1;
    p = strchr(arr, ' ');
    *p = '\0';
    strcpy(version, arr);
    
    version[strlen(version) - 1] = '0';

    int cnt = sprintf(*pte, "%s", method); 
    *pte += cnt;
    cnt = sprintf(*pte, " %s", path);
    *pte += cnt;
    cnt = sprintf(*pte, " %s\r\n", version);
    *pte += cnt;
    
    // return normally
    return 1;
}

int process_request_header(rio_t *rp, char **pte, const char *hostName) {
    char usrbuf[MAX_LINE_LEN];
    int nbytes;
    int hostFlag = 0;
    while ((nbytes = Rio_readlineb(rp, usrbuf, MAX_LINE_LEN))) {
        int cnt;
        if (nbytes == 2 && !strcmp(usrbuf, "\r\n")) { // blank line
            cnt = sprintf(*pte, "%s", usrbuf);
            *pte += cnt;
            break;
        }

        char header[MAX_LINE_LEN], content[MAX_LINE_LEN]; 
        char *p = usrbuf;
        char *delimeter = strchr(p, ':'); 

        if (delimeter == NULL) {
            // not a valid header, exit with failure
            return 0;
        }

        *delimeter = '\0';
        strcpy(header, p);
        p = delimeter + 1;
        strcpy(content, p);
        
        if (!strcmp(header, "Host")) {
            hostFlag = 1;
            cnt = sprintf(*pte, "%s:%s", header, content);
            *pte += cnt;
        } else if (!strcmp(header, "User-Agent")) {
            cnt = sprintf(*pte, "%s", user_agent_hdr); 
            *pte += cnt;
        } else if (!strcmp(header, "Connection")) {
            cnt = sprintf(*pte, "%s: close", header); 
            *pte += cnt;
        } else if (!strcmp(header, "Proxy-Connection")) {
            cnt = sprintf(*pte, "%s: close", header); 
            *pte += cnt;
        } else { // other headers, forward them unchanged
            cnt = sprintf(*pte, "%s:%s", header, content); 
            *pte += cnt;
        }
    }

    if (!hostFlag) {
        // brower doesn't send Host header, add default one
        int cnt = sprintf(*pte, "Host: %s", hostName);
        *pte += cnt;
    }

    return 1; // return normally
}


int process_url(const char *url, char *hostName, char *port, char *path) {
    char urlcopy[MAX_LINE_LEN]; 
    strcpy(urlcopy, url);
    char *p = urlcopy;
    p = strstr(p, "http://");
    if (p == NULL) {
        // not a valid http url
        return 0; // 
    }
    p += strlen("http://");
    
    // hostName and port
    char *del_colon = strchr(p, ':');
    char *del_slash = strchr(p, '/');

    // invalid http url
    if (del_slash == NULL) {
        return 0;
    }
    
    if (del_colon == NULL) {
        // default port 80
        strcpy(port, "80");
        *del_slash = '\0';
        strcpy(hostName, p);
    } else {
        *del_colon = '\0';
        strcpy(hostName, p);
        p = del_colon + 1;
        *del_slash = '\0';
        strcpy(port, p);
    }

    p = del_slash + 1;
    path[0] = '/';
    path[1] = '\0';
    strncat(path, p, MAX_LINE_LEN - 2);
    return 1;
}


void init_sbuf(sbuf_t *buf) {
    buf->head = 0;
    buf->tail = 0;

    Sem_init(&buf->lock, 0, 1);
    Sem_init(&buf->remain, 0, 0);
    Sem_init(&buf->available, 0, FDBUF_SIZE);
}

void insert_sbuf(sbuf_t *buf, int fd) {
    P(&buf->available);

    P(&buf->lock);
    buf->fdbuf[buf->tail] = fd;
    buf->tail = (buf->tail + 1) % FDBUF_SIZE;
    V(&buf->lock);

    V(&buf->remain);
}

int remove_sbuf(sbuf_t *buf) {
    int retv;
    P(&buf->remain);
     
    P(&buf->lock);
    retv = buf->fdbuf[buf->head];
    buf->head = (buf->head + 1) % FDBUF_SIZE;
    V(&buf->lock);

    V(&buf->available);
    return retv;
}

void *thread_func(void *arg) {
    Pthread_detach(Pthread_self());
    sbufcache_t t = *(sbufcache_t*)arg;
    while (1) {
        int connectfd = remove_sbuf(t.sbuf);
        process_client(connectfd, t.cache);
        Close(connectfd);
    }
    return NULL;
}

void create_node(cache_node_t *node, 
                 const char *content, 
                 const char *tag) {
    node->size = strlen(content);
    node->content = (char*)malloc(node->size + 1);
    strcpy(node->content, content);
    strcpy(node->tag, tag);
    node->next = NULL;
    node->prev = NULL;
}

void delete_node(cache_node_t *node) {
    free(node->content);
}

void init_cache(cache_t *cache) {
    cache->sentinel = (cache_node_t*) malloc(sizeof(cache_node_t)); 
    cache->sentinel->next = cache->sentinel; 
    cache->sentinel->prev = cache->sentinel;
    cache->total_size = 0;
}

void insert_cache(cache_t *cache, const char *content, const char *tag) {
    cache_node_t *node = (cache_node_t*) malloc(sizeof(cache_node_t));
    create_node(node, content, tag);
    insert_node(cache, node);
    cache->total_size += node->size;
}

void insert_node(cache_t *cache, cache_node_t *node) {
    node->next = cache->sentinel->next;
    node->prev = cache->sentinel;
    cache->sentinel->next->prev = node;
    cache->sentinel->next = node;
}


void remove_cache(cache_t *cache) {
    cache_node_t *node = cache->sentinel->prev; 
    remove_node(node);
    cache->total_size -= node->size;
    delete_node(node);
    free(node);
}

void remove_node(cache_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

void free_cache(cache_t *cache) {
    while (cache->total_size != 0) {
        remove_cache(cache);
    }
    free(cache->sentinel);
}

void replace_cache(cache_t *cache, const char *content, const char *tag) {
    remove_cache(cache);
    insert_cache(cache, content, tag);
}

char *find_cache(cache_t *cache, const char *tag) {
    cache_node_t *node = cache->sentinel->next;
    while (node != cache->sentinel) {
        if (!strcmp(node->tag, tag)) {
            // move this node to the head of list
            remove_node(node);
            insert_node(cache, node);
            return node->content;
        } 
        node = node->next;
    }
    return NULL;
}







