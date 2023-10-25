/*
 * mm-implicit.c  
 * Block Structure:      
 *      | hdr(size)| hdr(alloc bit) |  payload | ftr(similar to hdr) |
 * Heap Structure:
 *      | Pro(hdr): 8/1 | Pro(ftr): 8/1 | blocks | Epilogue(hdr): 0/1 | 
 *
 * Allocate: Using first-fit to find the first suitable block (with splitting)
 *               
 * Free: Immediately Coalesce 
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

#define MM_IMPLICIT_C
#include "macros.h"

// #define CHECKHEAP mm_checkheap(0);
#define CHECKHEAP

/**
#define CHECKPLACE printf("Chech heap after place:\n"); \
                  CHECKHEAP \
                  printf("\n\n");
                  */
#define CHECKPLACE 

/**
#define CHECKFREE printf("Check heap after free\n");\
                 CHECKHEAP\
                 printf("\n\n"); 
                 */
#define CHECKFREE



/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


// private helper function

// return the pointer to the payload on success, (void *)-1 on error
static void *findFreeBlock(size_t size);
static void *place(void *ptrToblk, size_t newsize);
static void *coalesce(void *ptrToblk);

// Always point to the footer of prologue 
PTR proPtr;

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void)
{
    proPtr = mem_sbrk(ALIGN(DSIZE + WSIZE));
    if ((long)proPtr < 0) { 
        return -1;
    } else {
        proPtr += WSIZE;     // padding 4 bytes at beginning 
                        // becaues the aligment of 8

        // prologue
        SET(proPtr, PACK(8, 1)); // header: 8/1
        proPtr += WSIZE;
        SET(proPtr, PACK(8, 1)); // footer

        // epilogue
        SET(proPtr + WSIZE, PACK(0, 1));        

        CHECKHEAP
        // | Prologue| Epilogue|, now is no allocated request
        return 0;
    }
}

/*
 * malloc - Allocate a block if have free block (with splitting)
 *          Call mem_sbrk if not
 *          NULL if no more heap space
 * 
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size)
{
    if (size == 0) { // Ignore spurious requests 
        return NULL;
    }

    
    size_t newsize = ALIGN(size + DSIZE); // payload + header + footer
    
    PTR ptrToblk;
    if ((ptrToblk = findFreeBlock(newsize)) != (PTR)((void *)-1)) { // find
        return place(ptrToblk, newsize); 

    } else { // not find, call mem_sbrk

        size_t allocSize = MAX(newsize, CHUNKSIZE);
        PTR newChunk = mem_sbrk(allocSize); 
        if ((long)newChunk < 0) {
            return NULL;
        }
            
        SET(HDR(newChunk), PACK(allocSize, 0)); // header
        SET(FTR(newChunk), PACK(allocSize, 0)); // footer

        SET(FTR(newChunk) + WSIZE, PACK(0, 1)); // new epilogue
        newChunk = coalesce(newChunk);

        return place(newChunk, newsize);
    }
}

/*
 * free - immediate coalesce  
 */

void free(void *ptr) {
    if (ptr == NULL) {
        return ;
    }

    DEALLOCATE(HDR(ptr));
    DEALLOCATE(FTR(ptr));
    ptr = coalesce(ptr); // ptr = ... to make gcc quiet
    CHECKFREE
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size)
{
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0) {
    free(oldptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if(oldptr == NULL) {
    return malloc(size);
  }

  newptr = malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if(!newptr) {
    return 0;
  }

  /* Copy the old data. */
  oldsize = BSIZE(HDR(oldptr));
  if(size < oldsize) oldsize = size;
  memcpy(newptr, oldptr, oldsize);

  /* Free the old block. */
  free(oldptr);

  return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size)
{
  size_t bytes = nmemb * size;
  void *newptr;

  newptr = malloc(bytes);
  memset(newptr, 0, bytes);

  return newptr;
}

/*
 * mm_checkheap - 
 * - Check the heap
 *      - epilogue and prologue blocks.
 *      - block’s address alignment.
 *      - heap boundaries. 
 *      - each block’s header and footer: 
 *              - size (minimum size, alignment), 
 *              - prev/next allocate/free bit consistency
 *              - header and footer matching each other
 *      - coalescing: no two consecutive free blocks in the heap. 
 */
void mm_checkheap(int verbose){
    // make gcc quiet
    verbose = verbose;    

    // prologue blocks:
    if (BSIZE(HDR(proPtr)) != 8 && BALLOC(HDR(proPtr)) != 1 &&
        BSIZE(FTR(proPtr)) != 8 && BALLOC(FTR(proPtr)) != 1) {
        printf("prologue blocks check errors\n");
    }

    // epilogue blocks:
    void *ptrEnd = mem_sbrk(0);
    if (BSIZE(HDR(ptrEnd)) != 0 && BALLOC(HDR(ptrEnd)) != 1) {
        printf("epilogue blocks check errors\n");
    }


    void *ptr = proPtr;
    while (ptr < ptrEnd) {
        printf("block at %p, header at %p: %d\\%d, footer at %p: %d\\%d\n", 
                ptr, HDR(ptr), BSIZE(HDR(ptr)), BALLOC(HDR(ptr)), 
                     FTR(ptr), BSIZE(FTR(ptr)), BALLOC(FTR(ptr)));
        ptr = PTRNEXTBLK(ptr);
    }

}

// private helper function
static void *place(void *ptrToblk, size_t newsize) {
    size_t blockSize = BSIZE(HDR(ptrToblk));
    // reminder part is equal or smaller than minimum size of a block
    if (blockSize - newsize <= DSIZE) {  
        // don't split, directly return this pointer
        ALLOCATE(HDR(ptrToblk)); 
        ALLOCATE(FTR(ptrToblk));
        CHECKPLACE
        return ptrToblk;
    }

    // split

    // allocated block
    SET(HDR(ptrToblk), PACK(newsize, 1)); // header
    SET(FTR(ptrToblk), PACK(newsize, 1)); // footer

    size_t rSize = blockSize - newsize; // size of reminder part
    PTR ptrToRmd = PTRNEXTBLK(ptrToblk); // pointer to the reminder block

    // reminder free block
    SET(HDR(ptrToRmd), PACK(rSize, 0)); // header
    SET(FTR(ptrToRmd), PACK(rSize, 0)); // footer

    CHECKPLACE

    return ptrToblk;
}

static void *coalesce(void *ptr) {
    void *next = PTRNEXTBLK(ptr);
    void *prev = PTRPREVBLK(ptr);
    
    size_t tsize = BSIZE(HDR(ptr)); // record the total size of free block 
    
    if (BALLOC(HDR(prev)) && BALLOC(HDR(next))) { // no coalescing
        return ptr;
    } else if (!BALLOC((HDR(prev))) && BALLOC(HDR(next))) { 
        // coalescing with prev block
        tsize += BSIZE(HDR(prev)); 
        SET(HDR(prev), PACK(tsize, 0));
        SET(FTR(ptr), PACK(tsize, 0));
        return prev;

    } else if (BALLOC(HDR(prev)) && !BALLOC(HDR(next))) { 
        // coalescing with next block
        tsize += BSIZE(HDR(next));
        SET(HDR(ptr), PACK(tsize, 0));
        SET(FTR(next), PACK(tsize, 0));
        return next;

    } else { // coalescing with prev and next block
        tsize += BSIZE(HDR(prev));
        tsize += BSIZE(HDR(next));
        SET(HDR(prev), PACK(tsize, 0));
        SET(FTR(next), PACK(tsize, 0));
        return prev;

    }

}

// first fit
static void *findFreeBlock(size_t size) {
    PTR ptr = proPtr;
    PTR epiPtr = mem_sbrk(0); 
    while (ptr < epiPtr) {
        if (!BALLOC(HDR(ptr)) && BSIZE(HDR(ptr)) >= size) {
            return ptr;
        }
        ptr = PTRNEXTBLK(ptr);
    }
    return (void *)(-1);
}