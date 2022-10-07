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
#include <stdint.h>

#include "config.h"
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "csa",
    /* First member's full name */
    "Shouao Chen",
    /* First member's email address */
    "chenshouao@foxmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define THRESHOLD       (1024)
#define SEPARATE_GAP    (1024 * 16)
#define LIST_SIZE       (MAX_HEAP / ALIGNMENT / SEPARATE_GAP + THRESHOLD)

void* m_root[LIST_SIZE];

int get_root_ind(size_t size) {
    if (size < THRESHOLD) {
        return size;
    } else {
        return THRESHOLD + (size - THRESHOLD) / SEPARATE_GAP;
    }
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    memset(m_root, 0x00, sizeof(m_root));
    memset(mem_heap_lo(), 0xff, MAX_HEAP);
    return 0;
}

size_t* p_h_size(void* b_ptr) {
    return b_ptr + SIZE_T_SIZE;
}

size_t* p_t_size(void* b_ptr) {
    return b_ptr + *p_h_size(b_ptr) - SIZE_T_SIZE;
}

void* next(void* b_ptr) {
    return *(void**)b_ptr;
}

void insert(void* b_ptr) {
    int ind = get_root_ind(*p_h_size(b_ptr) / ALIGNMENT);
    *p_t_size(b_ptr) = *p_h_size(b_ptr);
    assert(b_ptr != m_root[ind]);
    *(void**)b_ptr = m_root[ind];
    m_root[ind] = b_ptr;
}

void erase(void* b_ptr) {
    int ind = get_root_ind(*p_h_size(b_ptr) / ALIGNMENT);
    void* cur = m_root[ind];
    void* pre = NULL;
    while (cur) {
        if (cur == b_ptr) {
            if (pre) {
                *(void **) pre = next(cur);
            } else {
                m_root[ind] = next(cur);
            }
            break;
        }
        pre = cur;
        cur = next(cur);
    }
}

void print_list(void* cur) {
    if (cur == NULL) return;
    printf("%p", cur);
    cur = next(cur);
    while (cur) {
        assert(p_h_size(cur) == p_t_size(cur));
        printf("->%p", cur);
        cur = next(cur);
    }
    printf("\n");
}

/**
 *  1.free  : next + size + data + size
 *  2.using : next + size + data + 0x00000111
 **/

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE * 3);
    void* p = NULL;

    for (int ind = get_root_ind(newsize / ALIGNMENT); ind < LIST_SIZE && p == NULL; ++ind) {
//        static int time = 0;
//        if (m_root[ind]) {
//            printf("%d %d:", ind, time);
//        }
//        print_list(m_root[ind]);
        if (m_root[ind] != NULL) {
            void* cur = m_root[ind];
            while (cur != NULL) {
                if (*p_h_size(cur) >= newsize) {
                    p = cur;
//                    printf("%p %u\n", p, *p_h_size(p));
//                    printf("|%p %u\n", p_t_size(p), *p_t_size(p));
//                    printf("||%d %u\n", ind, newsize);
                    erase(p);
                    assert(((*p_t_size(p)) & 0x1) == 0);
                    if (*p_h_size(p) - newsize >= 32) {
                        *p_h_size((char*)p + newsize) = *p_h_size(p) - newsize;
                        *p_t_size((char*)p + newsize) = *p_h_size((char*)p + newsize);
                        insert(p + newsize);
                    } else {
                        newsize = *p_h_size(p);
                    }
                    break;
                }
                cur = next(cur);
            }
        }
    }
    if (p == NULL) {
        p = mem_sbrk(newsize);
        if (p == (void *) -1)
            return NULL;
    }
    *p_h_size(p) = newsize;
    *p_t_size(p) |= 0x1;
    return (char*)p + SIZE_T_SIZE * 2;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    assert(ptr != NULL);
    ptr = (char*)ptr - SIZE_T_SIZE * 2;

    void* pr = NULL;
    void* ne = NULL;
    if (ptr - 32 >= mem_heap_lo()) {
        pr = ptr - *(size_t*)(ptr - SIZE_T_SIZE);
        if (*(size_t*)(ptr - SIZE_T_SIZE) & 0x1) {
            pr = NULL;
        }
    }
    if (ptr + *p_h_size(ptr) + 32 <= mem_heap_hi()) {
        if ((*p_h_size(ptr) & 0x1) == 0) {
            ne = ptr + *p_h_size(ptr);
        }
        if (*p_t_size(ne) & 0x1) {
            ne = NULL;
        }
    }

    size_t new_size = *p_h_size(ptr);
    if (pr != NULL) {
        new_size += *p_h_size(pr);
        erase(pr);
        ptr = pr;
    }
    if (ne != NULL) {
        new_size += *p_h_size(ne);
        erase(ne);
    }
    *p_h_size(ptr) = new_size;
    *p_t_size(ptr) = new_size;
    insert(ptr);
    assert(*p_h_size(ptr) == *p_t_size(ptr));
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
    }
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *p_h_size(oldptr - 2 * SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














