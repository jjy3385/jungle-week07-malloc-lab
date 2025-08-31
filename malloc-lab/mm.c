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
    "jeon@gmail.com"};

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
/* 이전/다음 블록 포인터 */
#define PRED(bp) (*(void **)(bp))
#define SUCC(bp) (*(void **)((char *)(bp) + DSIZE))
/* 최소크기(24byte) = 헤더(4) + 푸터(4) + PRED(8) + SUCC(8) */
#define MINIBLOCK (3 * DSIZE)
/* 분리 가용 리스트 배열 크기 */
#define SEG_LIST_SIZE 12

/* 함수 전방선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_list(void *bp);
static void delete_free_list(void *bp);
static int get_list_index(size_t asize);

/* 가용 리스트의 배열 */
static void *seg_free_lists[SEG_LIST_SIZE];

/* 할당기 초기화 */
int mm_init(void) {
  char *bp;
  if ((bp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
    return -1;
  }
  PUT(bp, 0);
  PUT(bp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(bp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(bp + (3 * WSIZE), PACK(0, 1));

  for (int i = 0; i < SEG_LIST_SIZE; i++) {
    seg_free_lists[i] = NULL;
  }

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
    return -1;
  }
  return 0;
}

/* 메모리 공간 추가 */
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

  if ((long)(bp = mem_sbrk(size)) == -1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그 헤더
  return coalesce(bp);
}

/* 가용 블록 연결 */
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  // case 1 : 연결 X
  if (prev_alloc && next_alloc) {
    insert_free_list(bp);
    return bp;

    // case 2 : 다음
  } else if (prev_alloc && !next_alloc) {
    delete_free_list(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // case 3 : 이전
  } else if (!prev_alloc && next_alloc) {
    delete_free_list(PREV_BLKP(bp));
    size += GET_SIZE(FTRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);

    // case 4 : 이전 + 다음
  } else {
    delete_free_list(NEXT_BLKP(bp));
    delete_free_list(PREV_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  insert_free_list(bp);
  return bp;
}

/* 가용 리스트에 블록 추가(LIFO) */
static void insert_free_list(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));
  int i = get_list_index(size);

  PRED(bp) = NULL;
  SUCC(bp) = seg_free_lists[i];

  if (seg_free_lists[i] != NULL) {
    PRED(seg_free_lists[i]) = bp;
  }
  seg_free_lists[i] = bp;
}

/* 가용 리스트에서 블록 제거 */
static void delete_free_list(void *bp) {
  int index = get_list_index(GET_SIZE(HDRP(bp)));
  void *pred = PRED(bp);
  void *succ = SUCC(bp);

  // 이전 블록
  if (pred != NULL) {
    SUCC(pred) = succ;
  } else {
    seg_free_lists[index] = succ;
  }

  // 다음 블록
  if (succ != NULL) {
    PRED(succ) = pred;
  }
  PRED(bp) = SUCC(bp) = NULL;
}

/* 연결리스트 인덱스 */
static int get_list_index(size_t asize) {
  int i = 0;
  // 16바이트부터 시작
  size_t size = 2 * DSIZE;
  while (i < SEG_LIST_SIZE - 1 && asize > size) {
    size <<= 1;
    i++;
  }
  return i;
}

/* 동적 메모리 할당 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;

  if (size == 0) {
    return NULL;
  }

  if (size <= 2 * DSIZE) {
    asize = MINIBLOCK;
  } else {
    asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
  }

  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize); // 할당
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

/* 가용 블록 탐색(first fit) */
static void *find_fit(size_t asize) {
  int index = get_list_index(asize);
  // 가용 리스트 배열의 각 원소값 = 해당 클래스의 첫번째 가용 블록주소
  void *bp = seg_free_lists[index];
  while (index < SEG_LIST_SIZE) {
    bp = seg_free_lists[index];
    while (bp != NULL) {
      if (asize <= GET_SIZE(HDRP(bp))) {
        return bp;
      }
      bp = SUCC(bp);
    }
    index++;
  }
  return NULL;
}

/* 블록 배치(할당) */
static void place(void *bp, size_t asize) {
  delete_free_list(bp);
  size_t size = GET_SIZE(HDRP(bp));

  if ((size - asize) >= MINIBLOCK) {
    // 할당
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    // 분할
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
    insert_free_list(NEXT_BLKP(bp));
  } else {
    // 전체 할당
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
  }
}

/* 할당된 메모리 블록의 크기 변경 */
void *mm_realloc(void *ptr, size_t size) {

  size_t asize;
  if (size <= 2 * DSIZE) {
    asize = MINIBLOCK;
  } else {
    asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
  }

  void *new_ptr = ptr;

  size_t old_blk_size = GET_SIZE(HDRP(ptr));
  size_t old_payload_size = old_blk_size - DSIZE;

  // CASE 1.사이즈 줄임
  if (asize <= old_payload_size) {
    return ptr;
  }

  // CASE 2.블록 병합(제자리 확장)
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
  size_t prev_blk_size = GET_SIZE(FTRP(PREV_BLKP(ptr)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
  size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

  // 2-1.다음 블록 병합
  if (prev_alloc && !next_alloc && asize <= old_blk_size + next_blk_size) {
    delete_free_list(NEXT_BLKP(ptr));
    PUT(HDRP(ptr), PACK(old_blk_size + next_blk_size, 1));
    PUT(FTRP(ptr), PACK(old_blk_size + next_blk_size, 1));
    return ptr;
    
  }
  // 2-2.이전 블록 병합
  if (!prev_alloc && next_alloc && asize <= old_blk_size + prev_blk_size) {
    new_ptr = PREV_BLKP(ptr);
    delete_free_list(new_ptr);
    PUT(FTRP(ptr), PACK(old_blk_size + prev_blk_size, 1));
    // 새 블록으로 데이터 복사
    memcpy(new_ptr, ptr, old_payload_size);
    PUT(HDRP(new_ptr), PACK(old_blk_size + prev_blk_size, 1));
    return new_ptr;
  }

  // 2-3 이전/다음 블록 병합
  if (!prev_alloc && !next_alloc &&
      asize <= old_blk_size + prev_blk_size + next_blk_size) {
    new_ptr = PREV_BLKP(ptr);
    delete_free_list(NEXT_BLKP(ptr));
    delete_free_list(new_ptr);
    PUT(FTRP(NEXT_BLKP(ptr)),
        PACK(old_blk_size + prev_blk_size + next_blk_size, 1));
    // 새 블록으로 데이터 복사
    memcpy(new_ptr, ptr, old_payload_size);
    PUT(HDRP(new_ptr), PACK(old_blk_size + prev_blk_size + next_blk_size, 1));
    return new_ptr;
  }

  // 4.새로 할당
  new_ptr = mm_malloc(size);
  if (new_ptr == NULL) {
    return NULL;
  }

  size_t copy_size = GET_SIZE(HDRP(ptr)) - DSIZE;
  if (size < copy_size) {
    copy_size = size;
  }

  // 새 블록으로 데이터 복사
  memcpy(new_ptr, ptr, copy_size);
  // 기존 블록 반환
  mm_free(ptr);
  return new_ptr;
}