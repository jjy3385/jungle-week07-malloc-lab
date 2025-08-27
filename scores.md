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
        * 프롤로그랑 에필로그... 필요한가?? - 필요 없을거 같음, 그냥 앞 포인터가 NULL 이면 1번인거고 뒷포인터가 NULL 이면 맨 뒤인거잖아
        * 그러면 할일은? 아무것도 없고 extend_heap() 호출, 다만 가장 첫번째 블록의 리스트 페이로드를 가르키 heap_listp 처리를 extend_heap()으로 넘김?
        * 그러면 안됨, 이러면 매번 heap 늘릴 때마다 첫번째 블록 노드가 바뀌는 거라 잘못됨
        * 그럼 mm_init() 에서 첫번째 블록헤더를 생성한다
        * 아니 이런거 생각하기 전에 그냥 연결 리스트 초기화를 어떻게 했더라??
        * 걍 루트 포인터 NULL 처리하고 끝이잖아
        * 음... 근데 명시적 가용 리스트라는게 가용 상태인 리스트만 연결리스트로 만드는거고 나머지 블록은 그냥 똑같은거 아닌가?
        * 그러면 프롤로그 헤더랑 에필로그 헤더 넣는게 맞을거 같네
        * 그러면 mm_init() 은 건드릴게 없음

    * extend_heap()
        * 이것도 할 거 없음, 본인이 첫번째 가용블록이고 프롤로그 헤더는 첫번째 블록이 아님
        * 근데 루트 포인터를 하나 만들어서 첫번째 노드를 가르키게 해야 되는거 아닌가? 근데 이 함수에서 새로 생성된 블록이 항상 첫번째 노드는 아님
        * 그렇다고 하면 아예 구조 자체가 다 뒤집어진다는건가??
        * 기존에 선언했던 헬퍼함수들도 다 바뀌는건가??

    * 이렇게 함수단위로 생각하는 방식은 잘못됨


### 고민했던 것
- 프롤로그 헤더/푸터 , 에필로그 헤더 필요한지 안필요한지?
    - 있는게 좋다, 왜? => 얘네 역할은 find_fit() 이나 coalesce() 시 조건문을 단순하게 만들기 위한 것이다. 즉, 실제 가용 블록의 연결 리스트와는 관련이 없기 때문에 굳이 없앨 필요는 없고 그냥 두는게 코드를 코드가 깔끔해짐

- 초기화 처리를 어떻게 해야되는건지?
    - mm_init() 에서 초기 가용블록까지 넣어줘야 하나?
        - 처음엔 초기 가용블록까지 넣는 형태로 짰으나 나중에 연결 완성한 후 필요없다고 판단해서 제거
    - 루트포인터는 어떻게 해야되지?
        - NULL 로 처리하도록 수정
        - 처음엔 프롤로그 헤더를 가르킨 상태 그대로 뒀다
        - 하지만 초기화 직후 첫번째 가용 블록을 추가할 때 문제가 있다는 걸 알 수 있었음
        - LIFO 이기 때문에 루트포인터는 새로운 가용 블록을 가르키도록 바꿔줘야 되고 이전에 루트포인터가 가르키는 블록을 새로운 가용 블록의 다음 블록으로 처리해줘야 하는데 NULL 처리를 안하면 새로운 가용 블록의 다음블록이 프롤로그가 되버림
        즉, 가용이 아닌 블록을 연결리스트에 넣게 되는 오류가 발생하여 NULL 처리함

        ```C
        int mm_init(void)
        {
            if ((free_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
                return -1;
            }
            PUT(free_listp,0);
            PUT(free_listp + (1*WSIZE), PACK(DSIZE,1));  //프롤로그 헤더
            PUT(free_listp + (2*WSIZE), PACK(DSIZE,1));  //프롤로그 푸터
            PUT(free_listp + (3*WSIZE),PACK(0,1));      //에필로그 헤더
            free_listp = NULL;

            if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
                return -1;
            }
            return 0;
        }        
        ```
        
- 그 외 트러블슈팅
    - delete_free_list 시 이전 블록 / 다음 블록 찾아갈 때 segmentation fault 발생
    - place() 함수 할당 및 분할 처리 시 전체 할당 시 잘못된 사이즈로 인해 발생한 오류였음
    - 왜 그렇게 될까?
    - place() 호출전에 asize 는 최소블록크기(24) 이상으로 모두 처리를 하고 있음, 이건 문제없음
    - 근데 저렇게 하면 헤더가 덮어써지고 남은 푸터는 헤더없는 푸터가 됨
    - 이러면 free 나 coaleace 처리할 때 짝에 맞는 헤더 푸터 못찾음 => 구조 다 깨짐


    ```C
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
            PUT(HDRP(bp), PACK(size,1));    // 📌 여기에 asize 라고 넣어놨었음
            PUT(FTRP(bp), PACK(size,1));        
        }
    }
    ```



### 📌 배치정책 - LIFO 
* 82 = 42 + 40 

### 📌 realloc() 개선
* 83 = 43 + 40 
    * 활용도 1점밖에 안오름
    * Ran out of memory 오류 났던건... place() 를 그대로 쓸려고 해서였음
    * 그대로 못 쓰는 이유는 내 코드에서 place()가 이미 할당된 블록에 대해서 호출되는 경우는 없는 형태라 그럼, 즉 가용블록인 경우에만 호출되어야 한다는 건데 왜냐하면 delete_free_list() 때문임


    




