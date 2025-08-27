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
    "jeon@gmail.com"};

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

/* 현재 블록의 헤더로 다음 bp 찾기 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
/* 이전 블록의 푸터로 이전 bp 찾기 */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
/* 연결리스트 포인터 */
/* bp가 가르키는 값이 포인터주소이기 때문에 **bp 사용 */
#define PRED(bp) (*(void **)(bp))   
#define SUCC(bp) (*(void **)((char *)(bp) + DSIZE))
// 최소크기(24byte) = 헤더(4) + 푸터(4) + PRED(8) + SUCC(8)
#define MINIBLOCK (3*DSIZE)
#define SEG_LIST_SIZE 12

/* 함수 전방선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_list(void *bp);
static void delete_free_list(void *bp);
static int get_list_index(size_t asize);

/* 가용 리스트 첫번째 블록 bp */
static void *seg_free_lists[SEG_LIST_SIZE];
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *bp; 
    if ((bp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(bp,0);
    PUT(bp + (1*WSIZE), PACK(DSIZE,1));  //프롤로그 헤더
    PUT(bp + (2*WSIZE), PACK(DSIZE,1));  //프롤로그 푸터
    PUT(bp + (3*WSIZE),PACK(0,1));      //에필로그 헤더

    // 가용리스트 배열 초기화 처리
    for(int i = 0 ; i < SEG_LIST_SIZE; i++) {
        seg_free_lists[i] = NULL;
    } 

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    //더블 워드 정렬 유지를 위해 words 를 짝수로 만들고 * WSIZE 
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));    // 새 에필로그 헤더
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        insert_free_list(bp);   // 가용블록 연결리스트에 추가
        return bp;
    } else if (prev_alloc && !next_alloc) {
        delete_free_list(NEXT_BLKP(bp));    // 합쳐지는 다음 블록을 가용블록 연결리스트에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    } else if (!prev_alloc && next_alloc) {
        delete_free_list(PREV_BLKP(bp));   // 합쳐지는 이전 블록을 가용블록 연결리스트에서 제거(bp를 합쳐지는 이전 블록으로 변경 후 insert_free_list(bp) 호출함)
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    } else {
        delete_free_list(NEXT_BLKP(bp));    
        delete_free_list(PREV_BLKP(bp)); 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    insert_free_list(bp); // 병합 처리 후 가용블록 연결리스트에 추가
    return bp;
}


// 사이즈에 맞는 리스트 찾아서 삽입(LIFO)
static void insert_free_list(void *bp) 
{
    size_t size = GET_SIZE(HDRP(bp));
    int i = get_list_index(size);

    PRED(bp) = NULL;
    SUCC(bp) = seg_free_lists[i];

    if (seg_free_lists[i] != NULL) {
        PRED(seg_free_lists[i]) = bp;
    }
    seg_free_lists[i] = bp;
}

//가용 리스트에서 제거하는 함수
static void delete_free_list(void *bp)
{
    int index = get_list_index(GET_SIZE(HDRP(bp)));     
    void *pred = PRED(bp);
    void *succ = SUCC(bp);

    //이전 블록이 있으면
    if (pred != NULL) {
        // 이전 블록의 다음 블록은 현재 블록의 다음 블록
        SUCC(pred) = succ;
    } else {
        //이전 블록이 없으면 현재 블록이 루트이므로 다음 블록을 루트로 변경
        seg_free_lists[index] = succ;
    }

    //다음 블록이 있으면
    if (succ != NULL) {
        // 다음 블록의 이전 블록은 현재 블록의 이전 블록
        PRED(succ) = pred;
    }
    PRED(bp) = SUCC(bp) = NULL;
}

//어떤 클래스에 해당되는지 인덱스 찾는 함수
static int get_list_index(size_t asize) {
    int i = 0;
    // 📌 최소 클래스 크기는 16바이트(32,64,128,256,512.... 이렇게 나갈 수 있게)
    size_t size = 2 * DSIZE;
    while (i < SEG_LIST_SIZE - 1 && asize > size) {
        size <<= 1;
        i++;
    }
    return i;
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

    if (size == 0){
        return NULL;
    }

    if (size <= 2 * DSIZE) {
        asize = MINIBLOCK;  // 최소 블록 크기
    } else {
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);    // 8의 배수로 올림 처리
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp,asize);    // 할당
        return bp;
    }

    // 할당할 수 있는 블록이 없을 경우 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
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
    int index = get_list_index(asize);

    // 가용 리스트 배열의 각 원소값 = 해당 클래스의 첫번째 가용 블록
    void *bp = seg_free_lists[index];
    while ( index < SEG_LIST_SIZE) {
        bp = seg_free_lists[index];
        while (bp != NULL) {
            // first_fit 
            if (asize <= GET_SIZE(HDRP(bp))) {
                return bp;
            }
            //다음 가용 블록으로 진행
            bp = SUCC(bp);
        }
        // asize 가 해당되는 가용 블록 리스트 중 블록 사이즈가 전부 asize 보다 작은 경우도 있어서 다음 진행이 필요함
        index++;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    // 가용블록 연결리스트에서 제거
    delete_free_list(bp);   
    
    // 현재 블록 사이즈
    size_t size = GET_SIZE(HDRP(bp));   

    //최소 블록 사이즈보다 크면 분할
    if ((size - asize) >= MINIBLOCK) {
        // 할당
        PUT(HDRP(bp), PACK(asize,1));
        PUT(FTRP(bp), PACK(asize,1));
        // 분할
        PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize,0));
        insert_free_list(NEXT_BLKP(bp));    //분할된 가용 블록을 연결리스트에 삽입
    } else {
        // 아니면 전체 할당(약간의 내부단편화)
        PUT(HDRP(bp), PACK(size,1));
        PUT(FTRP(bp), PACK(size,1));        
    }
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free

 */

void *mm_realloc(void *ptr, size_t size)
{
    void *old_ptr = ptr;
    size_t old_blk_size = GET_SIZE(HDRP(old_ptr));
    size_t old_payload_size = old_blk_size - DSIZE;
    
    size_t asize;
    if (size <= 2 * DSIZE) {
        asize = MINIBLOCK;  // 최소 블록 크기
    } else {
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);    // 8의 배수로 올림 처리
    }

    //1.사이즈 줄이는 경우
    if (asize <= old_payload_size) {
        return old_ptr;
    }

    //2.다음 블록 병합이 가능한 경우 처리        
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(old_ptr)));
    size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(old_ptr)));
    size_t add_size = old_blk_size + next_blk_size;    

    //미할당 + realloc 사이즈 < 합친블록 사이즈 
    // if (!next_alloc && asize <= add_size) {
    //     //합친블록 사이즈만큼 블록을 미할당으로 합치고
    //     delete_free_list(NEXT_BLKP(old_ptr));
    //     PUT(HDRP(old_ptr), PACK(add_size,0));
    //     PUT(FTRP(old_ptr), PACK(add_size,0));

    //     //최소 블록 사이즈보다 크면 분할
    //     if ((add_size - asize) >= MINIBLOCK) {
    //         // 할당
    //         PUT(HDRP(old_ptr), PACK(asize,1));
    //         PUT(FTRP(old_ptr), PACK(asize,1));
    //         // 분할
    //         PUT(HDRP(NEXT_BLKP(old_ptr)), PACK(add_size - asize,0));
    //         PUT(FTRP(NEXT_BLKP(old_ptr)), PACK(add_size - asize,0));
    //         insert_free_list(NEXT_BLKP(old_ptr));    //분할된 가용 블록을 연결리스트에 삽입
    //     } else {
    //         // 아니면 전체 할당(내부단편화 감수)
    //         PUT(HDRP(old_ptr), PACK(add_size,1));
    //         PUT(FTRP(old_ptr), PACK(add_size,1));   
    //     }
    //     return old_ptr;
    // }
    
    if (!next_alloc && asize <= add_size) {
        // delete_free_list(old_ptr);
        delete_free_list(NEXT_BLKP(old_ptr));
        PUT(HDRP(old_ptr), PACK(add_size,1));
        PUT(FTRP(old_ptr), PACK(add_size,1));       
        return old_ptr; 
    }

    // 새로 할당
    void *new_ptr;
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