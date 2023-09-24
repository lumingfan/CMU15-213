#include "cachelab.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

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


    printSummary(hit, miss, evict);
    return 0;
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