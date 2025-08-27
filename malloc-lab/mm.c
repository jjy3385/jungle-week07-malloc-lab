/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "week7",
    /* First member's full name */
    "juyeong jin",
    /* First member's email address */
    "jjy3386@gmail.com",
    /* Second member's full name (leave blank if none) */
    "seyeong jeon",
    /* Second member's email address (leave blank if none) */
    "jeon@gmail.comm"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 기본 상수 및 매크로 정의 */
#define WSIZE   4
#define DSIZE   8
#define CHUNKSIZE (1<<12)   /*4KB = 4096B*/

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size,alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 함수 전방선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 전역 변수 */
static char *heap_listp = NULL;

#ifdef NEXT_FIT
static char *rover = NULL;
#endif

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (3*WSIZE),PACK(0,1));
    heap_listp += (2*WSIZE);

    #ifdef NEXT_FIT
    rover = NEXT_BLKP(heap_listp);
    #endif

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    // 짝수 * WSIZE(4) 라서 항상 8의 배수임을 보장할 수 있음
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0 ));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // CASE 1
    if (prev_alloc && next_alloc) {
        return bp;
    }
    // CASE 2
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }
    // CASE 3
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    // CASE 4
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }

    #ifdef NEXT_FIT
    rover = bp;
    #endif
    
    return bp;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp,asize);
        return bp;
    }

    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *find_fit(size_t asize)
{
    
    #ifdef FIRST_FIT

    void *bp;      

    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        size_t alloc = GET_ALLOC(HDRP(bp));
        size_t size = GET_SIZE(HDRP(bp));
        if (alloc == 0 && size >= asize) {
            return bp;
        }
    }
    return NULL;

    #elif defined(NEXT_FIT)
    void *bp = NULL;  

    for (bp = rover; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        size_t alloc = GET_ALLOC(HDRP(bp));
        size_t size = GET_SIZE(HDRP(bp));
        if (alloc == 0 && size >= asize) {
            return bp;
        }
    }

    for (bp = NEXT_BLKP(heap_listp); bp != rover; bp = NEXT_BLKP(bp)) {
        size_t alloc = GET_ALLOC(HDRP(bp));
        size_t size = GET_SIZE(HDRP(bp));
        if (alloc == 0 && size >= asize) {
            return bp;
        }        
    }
    return NULL;

    #else 

    void *bp;      
    void *best_bp = NULL;
    size_t min_diff = __SIZE_MAX__;

    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        size_t alloc = GET_ALLOC(HDRP(bp));
        size_t size = GET_SIZE(HDRP(bp));
        // 딱맞는 사이즈의 블록이 있을 때 return 
        if (alloc == 0 && size == asize) {
            return bp;   
        // 차이가 최소인 bp를 따라가는 포인터 갱신
        } else if(alloc == 0 && size >= asize ) {
            if (min_diff > size - asize) {
                min_diff = size - asize;
                best_bp = bp;
            }          
        }
    }
    return best_bp;

    #endif

}

static void place(void *bp, size_t asize)
{
    size_t ori_size = GET_SIZE(HDRP(bp));

    //최소 블록크기
    if (ori_size - asize >= (2*DSIZE)) {
        //할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //분할
        PUT(HDRP(NEXT_BLKP(bp)),PACK(ori_size - asize,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(ori_size - asize,0));

    } else {
        //할당(분할X)
        PUT(HDRP(bp), PACK(ori_size, 1));
        PUT(FTRP(bp), PACK(ori_size, 1));    
    } 

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free

 */

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    size_t ori_blk_size = GET_SIZE(HDRP(oldptr));
    size_t ori_payload_size = ori_blk_size - DSIZE;

    // 다음 블록 병합이 가능한 경우
    size_t asize;
    //최소 사이즈 처리
    if (size <= DSIZE) asize = 2*DSIZE;
    // 8의 배수로 맞추기 
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);    

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
    size_t add_size = ori_blk_size + next_blk_size;
    
    //미할당 + realloc 사이즈 < 합친블록 사이즈 
    if (!next_alloc && asize <= add_size) {
        //합친블록 사이즈만큼 블록을 미할당으로 합치고
        PUT(HDRP(oldptr), PACK(add_size,0));
        PUT(FTRP(oldptr), PACK(add_size,0));
        //할당처리(분할도 발생할 수 있음)
        place(oldptr,asize);
        return oldptr;
    } 

    // 새로 할당(기존 코드)
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    // 기존코드에 SIZE_T_SIZE, 이게 뭔지 모르겠어서 바꿈
    copySize = (size < ori_payload_size) ? size : ori_payload_size;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;     
    
}