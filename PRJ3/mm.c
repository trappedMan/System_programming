/*
 * mm.c - malloc package with Implicit list & Next fit search
 *
 * In this approach, a block is allocated in a free block which is located by Next fit method.
 * For detail, Searching starts on nowpos ptr. And traverse whole allocated space.
 * There are header and footer tags. It is packed with allocated bit in LSB.
 * (It's possible because block size is aligned to 2 words)
 * When a block is freed, it coalesced into more big block if neighbor block is free block.
 * Realloc is implemented directly using mm_malloc and mm_free.
 * (if realloc size is smaller than older one, no use mm-malloc or mm-free)
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"


team_t team = {
    /* Your student ID */
    "20181650",
    /* Your full name*/
    "DoHyun Ahn",
    /* Your email address */
    "enjoybeawall@gmail.com",
};

#define WORDSIZE 4 
#define DWORDSIZE 8
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)    
#define BASE  (1<<13)
#define PACK(size, alloc) ((size) | (alloc))
#define HDRP(bp)       ((char *)(bp) - WORDSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DWORDSIZE)
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(HDRP(bp) - WORDSIZE))
#define PRV_ALLOC(bp)  (GET_ALLOC(HDRP(PREV_BLKP(bp))))
#define NXT_ALLOC(bp)  (GET_ALLOC(HDRP(NEXT_BLKP(bp))))

static char* imp_list_start = 0;
static char* nowpos;
static int mm_check();
static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);

static int mm_check() {
    char* p;
    for (p = imp_list_start; GET_SIZE(HDRP(p)) > 0; p = NEXT_BLKP(p)) {
        if (GET_SIZE(p) % 8) {
            printf("not doubleword aligned : %p\n", p);
            return 0;
        }
        if (GET(HDRP(p)) != GET(FTRP(p))) {
            printf("payload overlapped(maybe header footer mismatch)\n");
            return 0;
        }
    }
    return 1;
}

int mm_init(void)
{
    if ((imp_list_start = mem_sbrk(4 * WORDSIZE)) == (void*)-1)
        return -1;
    PUT(imp_list_start, 0);
    PUT(imp_list_start + (1 * WORDSIZE), PACK(DWORDSIZE, 1));
    PUT(imp_list_start + (2 * WORDSIZE), PACK(DWORDSIZE, 1));
    PUT(imp_list_start + (3 * WORDSIZE), PACK(0, 1));
    imp_list_start += (2 * WORDSIZE);
    nowpos = imp_list_start;

    if (extend_heap(BASE / WORDSIZE) == NULL)
        return -1;
    return 0;
}

void* mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    if (imp_list_start == 0) {
        if (mm_init() == -1)
            return NULL;
    }
    size_t extendsize;
    char* p;

    size_t asize = size <= DWORDSIZE ? 2 * DWORDSIZE : DWORDSIZE * ((size + 2*DWORDSIZE - 1) / DWORDSIZE);

    if ((p = find_fit(asize)) == NULL) {
        extendsize = asize > BASE ? asize : BASE;
        if ((p = extend_heap(extendsize / WORDSIZE)) == NULL)
            return NULL;
    }
    place(p, asize);
    return p;
}


void mm_free(void* p)
{
    if (p == 0)
        return;

    size_t size = GET_SIZE(HDRP(p));
    if (imp_list_start == 0) {
        mm_init();
    }

    PUT(HDRP(p), PACK(size, 0));
    PUT(FTRP(p), PACK(size, 0));
    coalesce(p);
}

static void* coalesce(void* bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    if (PRV_ALLOC(bp) && NXT_ALLOC(bp)) {//case 1
        return bp;
    }

    else if (PRV_ALLOC(bp) && !NXT_ALLOC(bp)) {//case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!PRV_ALLOC(bp) && NXT_ALLOC(bp)) {//case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {//case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    if ((nowpos > (char*)bp) && (nowpos < NEXT_BLKP(bp)))
        nowpos = bp;

}

void* mm_realloc(void* ptr, size_t size)
{
    size_t oldsize;
    void* newptr;

    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(ptr));
    size_t asize = size <= DWORDSIZE ? 2 * DWORDSIZE : DWORDSIZE * ((size + 2 * DWORDSIZE - 1) / DWORDSIZE);
    if (asize <= oldsize) {
        PUT(FTRP(ptr), PACK(oldsize, 0));
        place(ptr, asize);
        return ptr;
    }
        
    newptr = mm_malloc(size);
    if (!newptr) {
        return 0;
    }
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);

    return newptr;
}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WORDSIZE : words * WORDSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DWORDSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void* find_fit(size_t asize)
{
    char* oldpos = nowpos;

    for (; GET_SIZE(HDRP(nowpos)) > 0; nowpos = NEXT_BLKP(nowpos))
        if (!GET_ALLOC(HDRP(nowpos)) && (asize <= GET_SIZE(HDRP(nowpos))))
            return nowpos;

    for (nowpos = imp_list_start; nowpos < oldpos; nowpos = NEXT_BLKP(nowpos))
        if (!GET_ALLOC(HDRP(nowpos)) && (asize <= GET_SIZE(HDRP(nowpos))))
            return nowpos;

    return NULL;
}

