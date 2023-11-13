#include <stdio.h>
#include <string.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_BACKLOG 1024
#define MAX_LINE_LEN 64
#define MAX_REQUEST_LEN 4096

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void process_client(int clientfd);
int process_http_header(rio_t *rp, char **pte, 
                        char *method, char *hostName, char *port,
                        char *path, char *version);
int process_request_header(rio_t *rp, char **pte, const char *hostName);
int process_url(const char *url, char *hostName, char *port, char *path);

ssize_t forwarding(char *message, size_t requestLen, 
                   char *hostName, char *port, 
                   char *object, int max_object_size); 

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port", argv[0]);
        exit(-1);
    }
    printf("%s", user_agent_hdr);

    // get listen fd of server
    int listenfd = Open_listenfd(argv[1]);

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

        // process requests of client
        process_client(connectfd);  
        // free fd
        Close(connectfd);
    }
}

void process_client(int clientfd) {
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
        fprintf(stderr, "Doesn't support method: %s\n", method);
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
    char object[MAX_OBJECT_SIZE];
    size_t requestLen = request_pointer - &proxyRequest[0];
    ssize_t responseLen = forwarding(proxyRequest, requestLen, 
                        hostName, port, object, MAX_OBJECT_SIZE); 

    // write response to connectfd
    Rio_writen(clientfd, object, responseLen);
}


ssize_t forwarding(char *message, size_t requestLen, 
                   char *hostName, char *port, 
                    char *object, int max_object_size) {

    // connect to default http port
    int connectfd = Open_clientfd(hostName, port);    

    rio_t rio;
    Rio_readinitb(&rio, connectfd);

    // sent request
    Rio_writen(connectfd, message, requestLen); 

    // get response
    ssize_t len = Rio_readnb(&rio, object, max_object_size);
    close(connectfd);
    return len;
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


