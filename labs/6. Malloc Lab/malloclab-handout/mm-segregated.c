/*
 * mm-segregated.c  
 *
 * Free List Array Structure (total MAXIDX entry):
 *      | size <= MINBLKSIZE | 1 * MINBLKSIZE < size <= 3 * MINBLKSIZE |
 *      | 3 < s <= 7| 7 < s <= 15| 15 < s <= 31 | 31 < s <= 63 | 63 < s <= 127 |
 *      | 127 < s <= 255 | ... | 2^(MAXIDX - 1) - 1< s |
 * Block Structure:
 *      - free      
 *          | hdr| prev| next| free space | ftr|
 *      - allocated
 *          | hdr| payload | ftr |
 * Heap Structure:
 *      | Pro(hdr): sizeOfPrologue/1 | Pro(prev)[0] | Pro(next)[0] |...| 
 *      |Pro(prev)[MAXIDX] | Pro(next)[MAXIDX] | Pro(ftr): sizeOfPrologue/1 | 
 *      |blocks | Epilogue(hdr): 0/1 |  
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

#define MM_SEGREGATED_C
#include "macros.h"

// #define CHECKINIT checkInit();
#define CHECKINIT 

// #define CHECKPLACE(ptr, size) checkPlace(ptr, size);
#define CHECKPLACE(ptr, size)

// #define CHECKFREE(ptr) checkFree(ptr);
#define CHECKFREE(ptr)



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

static inline void insertBLK(void *freeList, void *ptrToblk);
static inline void deleteBLK(void *ptrToblk);

static inline void checkInit() {
    printf("Cheap heap after init:\n"); 
    mm_checkheap(0); 
    printf("\n\n");
}
static inline void checkPlace(void *ptr, size_t size) {
    printf("Check heap after place: %p with %ld bytes\n", ptr, size);
    mm_checkheap(0);
    printf("\n\n"); 
}
static inline void checkFree(void *ptr) {
    printf("Check heap after free: %p\n", ptr);
    mm_checkheap(0);
    printf("\n\n"); 
}

// given the array of free list sentinels and index
// return the corresponding sentinel
inline void *getListHdr(ADDR* listArr, int idx) {
    return (listArr + 2 * idx);
}

inline int getIdx(size_t size) {
    size_t s = 2;
    for (int i = 0; i <= MAXIDX; ++i) {
        if (size <= (s - 1) * MINBLKSIZE) {
            return i;
        }
        s <<= 1;
    }
    return MAXIDX;
}


// Always point to the first prev pointer of prologue
PTR proPtr;

// point to the sentinel of free list
ADDR *freeListArray;

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void)
{
    // padding + prologue + epilogue
    size_t prologueSize = DSIZE + PTRSIZE * 2 * (MAXIDX + 1);
    proPtr = mem_sbrk(ALIGN(WSIZE + prologueSize + WSIZE)); 
    if ((long)proPtr < 0) { 
        return -1;
    } 
    proPtr += WSIZE;     // padding 4 bytes at beginning 
                            // becaues of the aligment of 8

    // prologue
    SET(proPtr, PACK(prologueSize, 3)); // header 

    proPtr += WSIZE;

    for (int i = 0; i <= MAXIDX; ++i) { // set [0, MAXIDX] free list sentinel
        for (int j = 0; j <= 1; ++j) { // 0: prev, 1: next
            SETPTR(proPtr + (2 * i + j) * PTRSIZE, proPtr + 2 * i * PTRSIZE);
        }
    }
    /** 
        *  Example:
        *    SETPTR(proPtr, proPtr); // 1st prev
        *    SETPTR(proPtr + PTRSIZE, proPtr); // 1st next
        *    SET(proPtr + 2 * PTRSIZE, proPtr + 2 * PTRSIZE); // 2nd prev
        *    SET(proPtr + 3 * PTRSIZE, proPtr + 2 * PTRSIZE); // 2nd next
    */

    size_t offset = (2 * (MAXIDX + 1)) * PTRSIZE;
    SET(proPtr + offset, PACK(prologueSize, 3)); // footer 

    // epilogue: always 0/0xX1
    // X = 0: prev block is free
    // X = 1: prev block is allocated
    SET(proPtr + offset + WSIZE, PACK(0, 3)); 

    freeListArray = (ADDR*)proPtr;

    CHECKINIT
    // | Prologue| Epilogue|, now is no allocated request
    return 0;
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

    
    size_t newsize;
    // payload can be hold in MINBLKSIZE
    // pointer filed and footer filed can be used in allocated blocks
    if (size <= MINBLKSIZE - WSIZE) 
        newsize = MINBLKSIZE; 
    else 
        newsize = ALIGN(size + WSIZE); // payload + header + footer
    
    PTR ptrToblk;
    if ((ptrToblk = findFreeBlock(newsize)) != (PTR)((void *)-1)) { // find
        return place(ptrToblk, newsize); 

    } else { // not find, call mem_sbrk

        size_t allocSize = MAX(newsize, CHUNKSIZE);
        PTR newChunk = mem_sbrk(allocSize); 
        if ((long)newChunk < 0) {
            return NULL;
        }
            
        // using epilogue prev alloc bit to mask new header and footer
        unsigned int mask;
        if (PREBALLOC(HDR(newChunk))) {
            mask = 2;
        } else {
            mask = 0;
        }

        SET(HDR(newChunk), PACK(allocSize, mask)); // header
        SET(FTR(newChunk), PACK(allocSize, mask)); // footer
        SET(FTR(newChunk) + WSIZE, PACK(0, 1)); // new epilogue

        newChunk = coalesce(newChunk); 
        // // coalescing only in free
        // insertBLK(getListHdr(freeListArray, getIdx(newsize)), newChunk);

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
    SETPREVALLOC(HDR(PTRNEXTBLK(ptr)));
    void *newptr = coalesce(ptr); 
    newptr = newptr; // make gcc quiet
    CHECKFREE(ptr);
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
    printf("epilogue at %p\n", HDR(ptrEnd));
    if (BSIZE(HDR(ptrEnd)) != 0 && BALLOC(HDR(ptrEnd)) != 1) {
        printf("epilogue blocks check errors\n");
    }


    void *ptr = proPtr;
    while (ptr < ptrEnd) {
        printf("block at %p, header at %p: %d\\%d", 
                ptr, HDR(ptr), BSIZE(HDR(ptr)), BINFO(HDR(ptr)));

        // free block or initially sentinel, print the prev/next pointer
        if (!BALLOC(HDR(ptr))) { 
            printf(", footer at %p: %d\\%d, ", 
                FTR(ptr), BSIZE(FTR(ptr)), BINFO(FTR(ptr)));
            printf("prev: %p, next: %p", PREVFREEBLK(ptr), NEXTFREEBLK(ptr));
        
            if (NEXTFREEBLK(PREVFREEBLK(ptr)) != ptr) {
                fprintf(stderr, "\nprev pointer error\n");
                exit(1);
            }
            if (PREVFREEBLK(NEXTFREEBLK(ptr)) != ptr) {
                fprintf(stderr, "\nnext pointer error\n");
                exit(1);
            }
        }
        printf("\n");
        
        if (ptr == proPtr) {
            for (int i = 0; i <= MAXIDX; ++i) {
                void *sentinel = getListHdr(freeListArray, i);
                printf("prev[%d]: %p, next[%d]: %p\n", 
                    i + 1, PREVFREEBLK(sentinel), i + 1, NEXTFREEBLK(sentinel));
            }
            printf("\n");
        }

        ptr = PTRNEXTBLK(ptr);

    }

}

// private helper function
static void *place(void *ptrToblk, size_t newsize) {
    size_t blockSize = BSIZE(HDR(ptrToblk));
    deleteBLK(ptrToblk);  // remove allocated block from free list

    // reminder part is equal or smaller than minimum size of a block
    if (blockSize - newsize <= MINBLKSIZE) {  
        // don't split, directly return this pointer
        // set allocated
        ALLOCATE(HDR(ptrToblk)); 
        ALLOCATE(FTR(ptrToblk));
        
        // set the prev allocated bit of next block
        SETPREVALLOC(HDR(PTRNEXTBLK(ptrToblk)));

        CHECKPLACE(ptrToblk, newsize);
        return ptrToblk;
    }

    // split

    // allocated block
    SET(HDR(ptrToblk), PACK(newsize, PREBALLOC(HDR(ptrToblk)) | 1)); // header
    SET(FTR(ptrToblk), PACK(newsize, PREBALLOC(HDR(ptrToblk)) | 1)); // footer

    size_t rSize = blockSize - newsize; // size of reminder part
    PTR ptrToRmd = PTRNEXTBLK(ptrToblk); // pointer to the reminder block

    // reminder free block
    SET(HDR(ptrToRmd), PACK(rSize, 0)); // header
    SET(FTR(ptrToRmd), PACK(rSize, 0)); // footer
    SETPREVALLOC(HDR(ptrToRmd));

    // add this new splitted block to free list
    insertBLK(getListHdr(freeListArray, getIdx(rSize)), ptrToRmd); 

    CHECKPLACE(ptrToblk, newsize);

    return ptrToblk;
}

static void *coalesce(void *ptr) {
    void *next = PTRNEXTBLK(ptr);
    void *prev = NULL;
    if (!PREBALLOC(HDR(ptr))) {
        prev = PTRPREVBLK(ptr);
    }

    size_t tsize = BSIZE(HDR(ptr)); // record the total size of free block 
    
    if (!prev && BALLOC(HDR(next))) { // no coalescing
        insertBLK(getListHdr(freeListArray, getIdx(tsize)), ptr);
        return ptr;
    } else if (prev && BALLOC(HDR(next))) { 
        // coalescing with prev block
        tsize += BSIZE(HDR(prev)); 

        SET(HDR(prev), PACK(tsize, PREBALLOC(HDR(prev))));
        SET(FTR(ptr), PACK(tsize, PREBALLOC(HDR(prev))));

        deleteBLK(prev);
        insertBLK(getListHdr(freeListArray, getIdx(tsize)), prev);

        return prev;

    } else if (!prev && !BALLOC(HDR(next))) { 
        // coalescing with next block
        tsize += BSIZE(HDR(next));

        SET(HDR(ptr), PACK(tsize, PREBALLOC(HDR(ptr))));
        SET(FTR(next), PACK(tsize, PREBALLOC(HDR(ptr))));

        deleteBLK(next);
        insertBLK(getListHdr(freeListArray, getIdx(tsize)), ptr);

        return next;

    } else { // coalescing with prev and next block
        tsize += BSIZE(HDR(prev));
        tsize += BSIZE(HDR(next));

        SET(HDR(prev), PACK(tsize, PREBALLOC(HDR(prev))));
        SET(FTR(next), PACK(tsize, PREBALLOC(HDR(prev))));

        deleteBLK(prev);
        deleteBLK(next);
        insertBLK(getListHdr(freeListArray, getIdx(tsize)), prev);

        return prev;

    }

}

// next fit
static void *findFreeBlock(size_t size) {
    int idx = getIdx(size);
    while (idx <= MAXIDX) {
        ADDR sentinel = getListHdr(freeListArray, idx);
        ADDR ptr = NEXTFREEBLK(sentinel);
        while (ptr != sentinel) { // tranver over the list from the front of free list
            if (!BALLOC(HDR(ptr)) && BSIZE(HDR(ptr)) >= size) {
                return ptr;
            }
            ptr = NEXTFREEBLK(ptr);
        }
        ++idx;
    }

    // not find
    return (void *)(-1);
}

// insert the new free block to the front of list 
static inline void insertBLK(void *freeList,void *ptrToblk) {
    // ptrToblk->next = freeList->next
    // ptrToblk->prev = freeList
    SETNEXTPTR(ptrToblk, NEXTFREEBLK(freeList));
    SETPREVPTR(ptrToblk, freeList);

    // freeList->next->prev = ptrToblk
    // freeList->next = ptrToblk
    SETPREVPTR(NEXTFREEBLK(freeList), ptrToblk);
    SETNEXTPTR(freeList, ptrToblk);
}

static inline void deleteBLK(void *ptrToblk) {
    // ptrToblk->next->prev = ptrToblk->prev
    // ptrToblk->prev->next = ptrToblk->next

    SETPREVPTR(NEXTFREEBLK(ptrToblk), PREVFREEBLK(ptrToblk));
    SETNEXTPTR(PREVFREEBLK(ptrToblk), NEXTFREEBLK(ptrToblk));
}