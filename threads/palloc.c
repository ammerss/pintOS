#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */

#define MAX_ORDER 32;

struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. 비트개수, 비트들의배열가짐 */
    uint8_t *base;                      /* Base of pool. */
  };

size_t pre_page_idx;

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

static void init_pool (struct pool *, void *base, size_t page_cnt,
                       const char *name);
static bool page_from_pool (const struct pool *, void *page);

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void
palloc_init (size_t user_page_limit)
{
  /* Free memory starts at 1 MB and runs to the end of RAM. */
  uint8_t *free_start = ptov (1024 * 1024);
//free_start는 pintos의 4MB의 메모리 중 1MB 부분을 나타낸다.
//uint8_t 8비트(1바이트) 크기의 부호 없는 정수형 변수 선언
//ptov커널의 물리적 주소를 맵핑된 커널의 가상주소로 리턴해주는 함수이다.
  uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
  size_t free_pages = (free_end - free_start) / PGSIZE;
  size_t user_pages = free_pages / 2;
  size_t kernel_pages;
  if (user_pages > user_page_limit)
    user_pages = user_page_limit;
  kernel_pages = free_pages - user_pages;

  /* Give half of memory to kernel, half to user. */
  init_pool (&kernel_pool, free_start, kernel_pages, "kernel pool");
  init_pool (&user_pool, free_start + kernel_pages * PGSIZE,
             user_pages, "user pool");
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */

/*
First Fit으로 구현된 연속된 페이지 할당 정책에
Next Fit, Best Fit, 그리고 Buddy System 을 추가한다.
이는 User Pool과 Kernel Pool에 동시에 적용될 수 있어야 한다. 
*/

void *
palloc_get_multiple (enum palloc_flags flags, size_t page_cnt, int option)
{
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
//PAL_USER PAL_... flag argument 
  void *pages;
//범용포인터(Generic Pointer)
//범용이므로 사용전에 항상 적절한 캐스팅이 필요
  size_t page_idx;

  if (page_cnt == 0)
    return NULL;

  lock_acquire (&pool->lock);
//TODO select mode
  page_idx = first_fit(pool, page_cnt);

switch (option) {
    case 1: //first fit
	page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);; 
	break;
    case 2://next fit
	page_idx = bitmap_scan_and_flip (pool->used_map, pre_page_idx, page_cnt, false);
	if (page_idx == BITMAP_ERROR)
		page_idx = bitmap_scan_extend_and_flip (pool->used_map, 0, pre_page_idx, page_cnt, false);
	break;
    case 3://best fit
	page_idx = bitmap_scan_min_and_flip (pool->used_map, 0, page_cnt, false);
	break;
    case 4 ://buddy system
	page_idx = bitmap_scan_buddy_and_flip(pool->used_map, 0, page_cnt, false);
	break;

    default:
	break;
}
  pre_page_idx = page_idx;
//page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
//원하는 인덱스를 찾지 못할 경우 미리 정의된 BITMAP_ERROR 를 리턴
//각 풀의 용도는 풀의 페이지당 1비트인 비트맵으로 추적된다. n 페이지를 할당하라는 요청은 false로 설정된 n개의 연속 비트에 대한 비트맵을 스캔하여 해당 페이지가 자유롭다는 것을 표시한 다음 해당 비트를 true로 설정하여 사용한 것으로 표시한다.
  lock_release (&pool->lock);

  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;
//Returns a null pointer if the pages cannot be allocated.

  if (pages != NULL) 
    {
      if (flags & PAL_ZERO)
        memset (pages, 0, PGSIZE * page_cnt);
//어떤 메모리의 시작점부터 연속된 범위를 어떤 값으로(바이트 단위) 모두 지정하고 싶을 때 사용하는 함수
//할당된 페이지의 모든 바이트를 반환하기 전에 0으로 설정하십시오. 설정하지 않으면 새로 할당된 페이지의 내용은 예측할 수 없다.
    }
  else 
    {
      if (flags & PAL_ASSERT)
        PANIC ("palloc_get: out of pages");
//PAL_ASSERT : If the pages cannot be allocated, panic the kernel. This is only appropriate during kernel initialization. User processes should never be permitted to panic the kernel.
    }

  return pages;
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_page (enum palloc_flags flags) 
{
  return palloc_get_multiple (flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void
palloc_free_multiple (void *pages, size_t page_cnt) 
{
  struct pool *pool;
  size_t page_idx;

  ASSERT (pg_ofs (pages) == 0);
  if (pages == NULL || page_cnt == 0)
    return;

  if (page_from_pool (&kernel_pool, pages))
    pool = &kernel_pool;
  else if (page_from_pool (&user_pool, pages))
    pool = &user_pool;
  else
    NOT_REACHED ();

  page_idx = pg_no (pages) - pg_no (pool->base);

#ifndef NDEBUG
  memset (pages, 0xcc, PGSIZE * page_cnt);
#endif

  ASSERT (bitmap_all (pool->used_map, page_idx, page_cnt));
  bitmap_set_multiple (pool->used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
void
palloc_free_page (void *page) 
{
  palloc_free_multiple (page, 1);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
{
  /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
  if (bm_pages > page_cnt)
    PANIC ("Not enough memory in %s for bitmap.", name);
  page_cnt -= bm_pages;

  printf ("%zu pages available in %s.\n", page_cnt, name);

  /* Initialize the pool. */
  lock_init (&p->lock);
  p->used_map = bitmap_create_in_buf (page_cnt, base, bm_pages * PGSIZE);
  p->base = base + bm_pages * PGSIZE;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool
page_from_pool (const struct pool *pool, void *page) 
{
  size_t page_no = pg_no (page);
  size_t start_page = pg_no (pool->base);
  size_t end_page = start_page + bitmap_size (pool->used_map);

  return page_no >= start_page && page_no < end_page;
}
