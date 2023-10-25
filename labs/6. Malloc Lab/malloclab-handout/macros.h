/**
 * This file contains the all pre-defined macros for mm.c 
 * #define MM_IMPLICIT_C is for mm-implicit.c, 
 * #define MM_EXPLICIT_C is for mm-explicit.c and so on
 * so it must be include after #define MM_IMPLICIT_C and so on
*/

#define WSIZE 4     // single word size
#define DSIZE 8     // double word size         
#define CHUNKSIZE 1 << 12    // Extend heap by this amount 

#define PTR unsigned char *  // pointer to unsigned char, make sure
                            // the pointer arithmetic is same as math arithmetic
#define WPTR unsigned int *  // used to access the header or footer

// rounds up to the nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + DSIZE - 1) & ~(DSIZE - 1)) 

// get/set value from pointer p
#define GET(p) (*(WPTR)(p)) 
#define SET(p, val) (*((WPTR)(p)) = (val))

// set/clear allocated bit
#define ALLOCATE(p) (*(WPTR)(p) |= 1)
#define DEALLOCATE(p) (*(WPTR)(p) &= ~0x1)

// get size / allocated bit given the pointer to header/footer
#define BSIZE(p) (GET(p) & ~(DSIZE - 1))
#define BALLOC(p) (GET(p) & 0x1)

// Pack size and allocated bit
#define PACK(size, alloc) ((size) | (alloc))



#define HDR(p) ((PTR)(p) - WSIZE)
#define FTR(p) (HDR(p) + BSIZE(HDR(p)) - WSIZE)

// find next/prev block pointer given now block pointer
#define PTRNEXTBLK(p) ((PTR)(p) + BSIZE(HDR(p)))
#define PTRPREVBLK(p) ((PTR)(p) - BSIZE(HDR(p) - WSIZE))

#ifdef MM_EXPLICIT_C

#define MINBLKSIZE DSIZE * 2 + WSIZE * 2
#define PTRSIZE sizeof(long)
#define ADDR unsigned long *
#define UL unsigned long

// get next/prev free block in the free list
#define NEXTFREEBLK(p) (ADDR)(*((ADDR)((PTR)(p) + PTRSIZE)))
#define PREVFREEBLK(p) (ADDR)(*(ADDR)(p))

#define SETNEXTPTR(p, val) (*((ADDR)((PTR)(p) + PTRSIZE)) = (UL)(val))
#define SETPREVPTR(p, val) (*(ADDR)(p) = (UL)(val))

#define SETPTR(p, val) (*((ADDR)(p)) = (UL)(val)) 
#endif

#define MAX(a, b) ((a) > (b) ? (a): (b))




