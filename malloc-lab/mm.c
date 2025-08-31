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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

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
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) /*4KB = 4096B*/

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

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

/* 할당기 초기화 */
int mm_init(void) {

  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
    return -1;
  }

  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
  heap_listp += (2 * WSIZE);

#ifdef NEXT_FIT
  rover = NEXT_BLKP(heap_listp);
#endif

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
    return -1;
  }
  return 0;
}

/* 메모리 공간 추가 */
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  /* 짝수 * WSIZE(4바이트) => 8의 배수 보장 */
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  return coalesce(bp);
}

/* 가용 블록 연결 */
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  // CASE 1
  if (prev_alloc && next_alloc) {
    return bp;
  }
  // CASE 2 : 다음 블록 연결
  else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  // CASE 3 : 이전 블록 연결
  else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  // CASE 4 : 이전 / 다음 블록 연결
  else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

#ifdef NEXT_FIT
  rover = bp;
#endif

  return bp;
}

/* 동적 메모리 할당 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size == 0) {
    return NULL;
  }

  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }

  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  // 가용 블록 없으면 메모리 공간 추가
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

/* 할당 해제 */
void mm_free(void *ptr) {
  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
}

/* 가용 블록 탐색 */
static void *find_fit(size_t asize) {
  void *bp = NULL;

#ifdef FIRST_FIT

  for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    size_t alloc = GET_ALLOC(HDRP(bp));
    size_t size = GET_SIZE(HDRP(bp));
    if (alloc == 0 && size >= asize) {
      return bp;
    }
  }
  return NULL;

#elif defined(NEXT_FIT)

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
  void *best_bp = NULL;
  size_t min_diff = __SIZE_MAX__;

  for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    size_t alloc = GET_ALLOC(HDRP(bp));
    size_t size = GET_SIZE(HDRP(bp));
    if (alloc == 0 && size == asize) {
      return bp;
    } else if (alloc == 0 && size >= asize) {
      if (min_diff > size - asize) {
        min_diff = size - asize;
        best_bp = bp;
      }
    }
  }
  return best_bp;

#endif
}
/* 블록 배치(할당) */
static void place(void *bp, size_t asize) {
  size_t ori_size = GET_SIZE(HDRP(bp));

  // 최소 블록크기
  if (ori_size - asize >= (2 * DSIZE)) {
    // 할당
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    // 분할
    PUT(HDRP(NEXT_BLKP(bp)), PACK(ori_size - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(ori_size - asize, 0));

  } else {
    // 할당(분할X)
    PUT(HDRP(bp), PACK(ori_size, 1));
    PUT(FTRP(bp), PACK(ori_size, 1));
  }
}

/* 할당된 메모리 블록의 크기 변경 */
void *mm_realloc(void *ptr, size_t size) {
  void *old_ptr = ptr;
  size_t ori_blk_size = GET_SIZE(HDRP(old_ptr));
  size_t ori_payload_size = ori_blk_size - DSIZE;

  size_t asize;
  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }

  // 다음 가용 블록 연결
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(old_ptr)));
  size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(old_ptr)));
  size_t add_size = ori_blk_size + next_blk_size;

  if (!next_alloc && asize <= add_size) {
    PUT(HDRP(old_ptr), PACK(add_size, 0));
    PUT(FTRP(old_ptr), PACK(add_size, 0));
    place(old_ptr, asize);
    return old_ptr;
  }

  // 새로 할당
  void *new_ptr;
  size_t copy_size;

  new_ptr = mm_malloc(size);
  if (new_ptr == NULL)
    return NULL;
  copy_size = (size < ori_payload_size) ? size : ori_payload_size;
  if (size < copy_size)
    copy_size = size;
  memcpy(new_ptr, old_ptr, copy_size);
  mm_free(old_ptr);
  return new_ptr;
}