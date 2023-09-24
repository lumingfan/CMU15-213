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
typedef long long unsigned cacheLine;
// malloc s x E cache 
cacheLine **mallocCache(int s, int E);
void freeCache(cacheLine **cache, int s, int E);

// record the hit/miss/evict times of one line
struct CacheBehavior {
    int hitTimes;
    int missTimes;
    int evictTimes;
};

// determine the hit/miss/evict behavior of given line, if vlag is set
// enable the verbose output
struct CacheBehavior classifyLine(
    cacheLine **cahce, char *line, ssize_t len, int vlag
);

// print verbose info about cache behavior when vflag is set
void printVerbose(char *line, int missTimes, int evictTimes, int hitTimes);

// pow standard library in math.h
double pow(double x, double y);  

// make the help output behaves like csim-ref
void printHelpMessage();        

int main(int argc, char *argv[]) {
    int sflag = 0, eflag = 0, bflag = 0, tflag = 0;    
    int hflag = 0, vflag = 0;

    int s = 0, e = 0;
    char *tracefile = NULL;
    
    int hit = 0, miss = 0, evict = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h': hflag = 1; break;
        case 'v': vflag = 1; break;

        case 's': sflag = 1; s = (int) pow(2, atoi(optarg)); break;
        case 'E': eflag = 1; e = (int) pow(2, atoi(optarg)); break;
        case 'b': bflag = 1; break; 

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

    cacheLine **cache = mallocCache(s, e);

    FILE *file = fopen(tracefile, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: No such file or directory\n", tracefile);
        return 1;
    }

    char *line = NULL; 
    size_t len = 0;
    ssize_t nread;
    while ((nread = getline(&line, &len, file)) != -1) {
        struct CacheBehavior record = classifyLine(cache, line, nread, vflag);
        hit += record.hitTimes;
        miss += record.missTimes;
        evict += record.evictTimes;
    }

    free(line);
    fclose(file);
    freeCache(cache, s, e);
    printSummary(hit, miss, evict);
    
    return 0;
}

struct CacheBehavior classifyLine (
    cacheLine **cache, char *line, ssize_t len, int vflag
) {
    struct CacheBehavior ret = {0, 0, 0};
    char space, opt;
    long long unsigned address;
    sscanf(line, "%c%c%llx", &space, &opt, &address);
    if (space != ' ') 
        return ret;

    switch (opt) {
    case 'L': 
        break;
    case 'S':
        break;
    case 'M':
        break;
    default:
        break;
    }


    if (vflag) {
        printVerbose(
            line, ret.missTimes, ret.evictTimes, ret.hitTimes
        );
    }
    return ret;
}

void printVerbose(char *line, int missTimes, int evictTimes, int hitTimes) {

}


cacheLine **mallocCache(int s, int E) {
    cacheLine **retCache = (cacheLine**) malloc(s * sizeof(cacheLine*));
    for (int i = 0; i < s; ++i) {
        retCache[i] = (cacheLine *) malloc(E * sizeof(cacheLine));
        for (int j = 0; j < E; ++j) 
            retCache[i][j] = 0;
    }

    return retCache;
}

void freeCache(cacheLine **cache, int s, int E) {
    for (int i = 0; i < s; ++i) {
        free(cache[i]);
    }
    free(cache);
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