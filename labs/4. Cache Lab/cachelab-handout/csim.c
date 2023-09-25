#define _GNU_SOURCE
#include "cachelab.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <math.h>

// cacheLine is used to simulator the cache (2D array)
typedef long long unsigned Addr;

struct cacheLine{
    int dirty;
    Addr tag;
    int length;
    struct cacheLine *next;
    struct cacheLine *prev;
};

typedef struct cacheLine cacheLine;
typedef cacheLine Node;

static cacheLine *cache;
static int cacheRows;
static int cacheCols;

// malloc s sets cache, each cache consists of linked list
void mallocCache();
void freeCache();
int addNode(Addr sIndex, Addr tag);
int findNode(Addr sIndex, Addr tag);
Node *createNode(Addr tag);
void headInsertNode(Node *header, Addr tag);
void deleteNode(Node *header);

// record the hit/miss/evict times of one line
struct CacheBehavior {
    int hitTimes;
    int missTimes;
    int evictTimes;
};

// determine the hit/miss/evict behavior of given line, if vlag is set
// enable the verbose output
struct CacheBehavior classifyLine(
    char *line, ssize_t len, 
    int s, int b, int vlag
);

// caculate the hit/miss/evict times given sIndex, tag 
struct CacheBehavior loadCache(Addr sIndex, Addr tag);
struct CacheBehavior saveCache(Addr sIndex, Addr tag);

// print verbose info about cache behavior when vflag is set
void printVerbose(char opt, Addr address, int offset,
                  int missTimes, int evictTimes, int hitTimes);

// pow standard library in math.h
double pow(double x, double y);  

// make the help output behaves like csim-ref
void printHelpMessage();        

int main(int argc, char *argv[]) {
    int sflag = 0, eflag = 0, bflag = 0, tflag = 0;    
    int hflag = 0, vflag = 0;

    int s = 0, e = 0, b = 0;
    char *tracefile = NULL;
    
    int hit = 0, miss = 0, evict = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h': hflag = 1; break;
        case 'v': vflag = 1; break;

        case 's': sflag = 1; s = atoi(optarg); break;
        case 'E': eflag = 1; e = atoi(optarg); break;
        case 'b': bflag = 1; b = atoi(optarg); break; 

        case 't': tflag = 1; tracefile = optarg; break;
        case '?': 
            printHelpMessage();
            return 1;
        default:
            abort();
        }
    }

    if (hflag) {
        printHelpMessage();
        return 0;
    }

    if (!sflag || !eflag || !bflag || !tflag) {
        fprintf(stderr, "Missing required command line argument\n");
        printHelpMessage();
        return 1;
    }

    cacheRows = (int) pow(2, s);
    cacheCols = e;
    mallocCache();

    FILE *file = fopen(tracefile, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: No such file or directory\n", tracefile);
        return 1;
    }

    char *line = NULL; 
    size_t len = 0;
    ssize_t nread;
    while ((nread = getline(&line, &len, file)) != -1) {
        struct CacheBehavior record = 
            classifyLine(line, nread, s, b, vflag);
        hit += record.hitTimes;
        miss += record.missTimes;
        evict += record.evictTimes;
    }

    free(line);
    fclose(file);
    freeCache();
    printSummary(hit, miss, evict);
    
    return 0;
}

struct CacheBehavior classifyLine (
    char *line, ssize_t len, 
    int s, int b, int vflag
) {
    struct CacheBehavior ret = {0, 0, 0};
    char space, opt;
    int offset;
    Addr address;
    sscanf(line, "%c%c%llx, %d", &space, &opt, &address, &offset);
    if (space != ' ') 
        return ret;

    Addr sIndex = (address & ~(~0 << (s + b))) >> b;
    Addr tag = (address & (~0 << (s + b))) >> (s + b); 
    struct CacheBehavior loadBehavior;
    struct CacheBehavior saveBehavior;

    switch (opt) {
    case 'L': 
        loadBehavior = loadCache(sIndex, tag);
        ret.missTimes = loadBehavior.missTimes;
        ret.evictTimes = loadBehavior.evictTimes;
        ret.hitTimes = loadBehavior.hitTimes;
        break;  
    case 'S':
        saveBehavior = saveCache(sIndex, tag);
        ret.missTimes = saveBehavior.missTimes;
        ret.evictTimes = saveBehavior.evictTimes;
        ret.hitTimes = saveBehavior.hitTimes;
        break;
    case 'M':
        loadBehavior = loadCache(sIndex, tag);
        saveBehavior = saveCache(sIndex, tag);
        ret.missTimes = loadBehavior.missTimes + saveBehavior.missTimes;
        ret.evictTimes = loadBehavior.evictTimes + saveBehavior.evictTimes;
        ret.hitTimes = loadBehavior.hitTimes + saveBehavior.hitTimes;
        break;
    default:
        break;
    }

    if (vflag) {
        printVerbose(
            opt, address, offset, ret.missTimes, ret.evictTimes, ret.hitTimes
        );
    }
    return ret;
}

struct CacheBehavior loadCache(Addr sIndex, Addr tag) {
    return saveCache(sIndex, tag);
}

struct CacheBehavior saveCache(Addr sIndex, Addr tag) {
    struct CacheBehavior ret = {0, 0, 0};
    if (findNode(sIndex, tag)) {
        ret.hitTimes = 1;
        /**********
        Pay attention: Hit also needs to update visited order    
        **********/
        return ret;
    }

    ret.missTimes = 1;
    if (addNode(sIndex, tag) == 1) {
        // miss and cache is full
        ret.evictTimes = 1;
    }
    return ret;
}


void printVerbose(char opt, Addr address, int offset,
                  int missTimes, int evictTimes, int hitTimes) {
    printf("%c %llx,%d", opt, address, offset);
    while (missTimes) {
        printf(" miss ");
        missTimes--;
    }
    while (evictTimes) {
        printf(" eviction ");
        evictTimes--;
    }
    while (hitTimes) {
        printf(" hit ");
        hitTimes--;
    }
    printf("\n");
}



void mallocCache() {
    cache = (cacheLine*) malloc(cacheRows * sizeof(cacheLine));
    if (cache == NULL) {
        fprintf(stderr, "No more free space\n");
        exit(1);
    }
    for (int i = 0; i < cacheRows; ++i) {
        cache[i].dirty = 0;
        cache[i].tag = 0;
        cache[i].length = 0;
        cache[i].next = NULL;
        cache[i].prev = NULL;
    }
}

void freeCache() {
    for (int i = 0; i < cacheRows; ++i) {
        for (int j = 0; j < cache[i].length; ++j)
            deleteNode(&cache[i]);
    }
    free(cache);
}

int findNode(Addr sIndex, Addr tag) {
    Node *header = &cache[sIndex];
    Node *node = header->next;
    for (int i = 0; i < header->length; ++i) {
        if (node->dirty && node->tag == tag) {
            if (i == 0)  // the finded node already is at the MRU location
                return 1;
            if (node->next != NULL)
                node->next->prev = node->prev;
            if (node->prev != NULL)
                node->prev->next = node->next;
            node->next = header->next;
            if (header->next != NULL)
                header->next->prev = node;
            header->next = node;
            
            // when header is at the LRU location
            // header->prev needs to be updated
            if (header->prev == node) 
                header->prev = node->prev;
            node->prev = NULL;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

void headInsertNode(Node *header, Addr tag) {
    Node *newNode = createNode(tag);
    newNode->next = header->next;
    if (header->length != 0)
        header->next->prev = newNode;
    else 
        header->prev = newNode;
    header->next = newNode;
}

void deleteNode(Node *header) {
    Node *evictedNode = header->prev;
    if (evictedNode != NULL) {
        header->prev = evictedNode->prev;
        if (header->prev != NULL)
            header->prev->next = NULL;
        free(evictedNode);
    }
}

int addNode(Addr sIndex, Addr tag) {
    Node *header = &cache[sIndex];
    int isEvict = (header->length == cacheCols);

    if (!isEvict) {
        headInsertNode(header, tag);
        header->length++;
    } else {
        headInsertNode(header, tag);
        deleteNode(header);
    }
    return isEvict;
}

Node *createNode(Addr tag) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->tag = tag;
    node->dirty = 1;
    node->next = NULL;
    node->prev = NULL;
    return node;
}


void printHelpMessage() {
    fprintf(stderr, 
        "Usage: ./csim-ref: [-hv] -s <num> -E <num> -b <num> -t <file>\n"
        "Options:\n"
        "  %-*s %s.\n"
        "  %-*s %s.\n"
        "  %-*s %s.\n"
        "  %-*s %s.\n"
        "  %-*s %s.\n"
        "  %-*s %s.\n\n"
        "Examples:\n"
        "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
        "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
                    10, "-h", "Print this help message", 
                    10, "-v", "Optional verbose flag",
                    10, "-s <num>", "Number of set index bits",
                    10, "-E <num>", "Number of lines per set",
                    10, "-b <num>", "Number of block offset bits",
                    10, "-t <file>", "Trace file");
}