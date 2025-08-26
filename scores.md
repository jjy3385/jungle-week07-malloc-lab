# malloc-lab 점수판
* 활용도 + 처리량
* 활용도는 메모리 이용도를 말하는 것으로 내부/외부 단편화 해결이 관건
* 처리량은 속도 문제로 탐색이 길거나 불필요한 복사/병합이 많은 경우

## 묵시적 가용 리스트

### 📌 find_fit() 개선

**1. first_fit**
* 57 = 44 + 13

**2. next_fit**
* 84 = 44 + 40 

**3. best_fit**
* 57 = 45 + 12

### 📌 realloc() 개선

* 수정하려는 사이즈가 기존 블록의 사이즈보다 작으면 굳이 블록의 크기를 줄이지 않도록 수정
    * 오히려 활용도 떨어짐
    * next_fit 은 -1 , 나머지는 그대로...(83 = 43 + 40)
    * 병합 추가 시 사이즈를 줄이지 않고 그냥 두는 이 코드가 오류를 유발해서 관리하기 힘들어서 지워버림
* 다음블록 병합 처리 넣어봄
    * first_fit 활용도 + 6, 처리량 + 1 (64 = 50 + 14)
    * best _fit 처리량 + 2 (59 = 45 + 14)
    * next_fit 활용도 + 2 (86 = 46 + 40)
    * 왜 이런 결과를 얻얼을까?
    * 기존에는 무조건 malloc() 을 호출하고 malloc() 에 extend_heap() 이 있기 때문에 적절한 블록을 찾지 못하면 메모리를 더 요청받았음
    * 그런데 이제 병합처리를 할 수 있는 경우에 바로 메모리 요청을 더 하는게 아니라 병합을 하기 떄문에 활용도가 올라갈 수 있음
        * 근데 best_fit 은 활용도 안올라감 (왜?)
    * 처리량같은 경우에도 메모리를 새로 할당받고 복사하는 처리보다는 병합처리가 더 빠른 연산이라 처리량이 약간 올라간 것으로 보인다.

```C

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
```

### 📌 경계태그 최적화
* 얜 패스 

## 명시적 가용 리스트
* 이건 페이로드 안에 포인터를 두는 형태임
* 근데 헤더 푸터는 그대로 있음
* 그리고 배열이 아니라 연결리스트를 써야됨
* 근데 어짜피 비트배열은 똑같은거고 안에 포인터를 둔다는게 다른거 아닌가?
* 포인터 주소는 뭘 갖고 있어야되는거지....?
* bp겠지 뭐
* 그러면 뭐가 바뀔지 생각해보자
    * mm_init()
        * 



