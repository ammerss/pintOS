#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <round.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h" //option에 따라 다른 할당 방식을 사용하기 위해 추가
#ifdef FILESYS
#include "filesys/file.h"
#endif

/* Element type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of elem_type (defined above) that
   simulates an array of bits. */
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
  };

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx (size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask (size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask (const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Creates and returns a pointer to a newly allocated bitmap with room for
   BIT_CNT (or more) bits.  Returns a null pointer if memory allocation fails.
   The caller is responsible for freeing the bitmap, with bitmap_destroy(),
   when it is no longer needed. */
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf (size_t bit_cnt, void *block, size_t block_size UNUSED)
{
  struct bitmap *b = block;
  
  ASSERT (block_size >= bitmap_buf_size (bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  bitmap_set_all (b, false);
  return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size (size_t bit_cnt) 
{
  return sizeof (struct bitmap) + byte_cnt (bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by bitmap_create_in_buf(). */
void
bitmap_destroy (struct bitmap *b) 
{
  if (b != NULL) 
    {
      free (b->bits);
      free (b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size (const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set (struct bitmap *b, size_t idx, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  if (value)
    bitmap_mark (b, idx);
  else
    bitmap_reset (b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] |= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the OR instruction in [IA32-v2b]. */
  asm ("orl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] &= ~mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the AND instruction in [IA32-v2a]. */
  asm ("andl %1, %0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] ^= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the XOR instruction in [IA32-v2b]. */
  asm ("xorl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test (const struct bitmap *b, size_t idx) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i, value_cnt;

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  value_cnt = 0;
  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      value_cnt++;
  return value_cnt;
}
/*
between START and START + CNT value인 비트 개수 반환
*/

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}
/*
하나라도 할당된(true)면 true 하나도 할당안되어있으면 false 반환
*/


/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any (const struct bitmap *b, size_t start, size_t cnt) 
{
  return bitmap_contains (b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  static size_t latest = 0; // 직전에 할당한 위치에 대한 정보를 담고 있는 변수이다.
  static size_t check_cnt = 0; // 실행 작업을 위해 해당 함수가 호출되는 횟수를 담고 있는 변수이다.
  static size_t check_size = 0; // 실행 작업을 위해 할당되는 공간의 크기를 담고 있는 변수이다.

  if (cnt <= b->bit_cnt) 
    {
      size_t last = b->bit_cnt - cnt;
      size_t i;
      if(option == 0){ //원래 pintos에서 구현되어 있던 first fit
	for (i = start; i <= last; i++){
          if (!bitmap_contains (b, i, cnt, !value))
            return i;
	}

      }else if(option == 1){ //next fit
	// 제일 최근에 할당한 위치에서부터 검색
        for (i = latest; i <= last; i++){
          if (!bitmap_contains (b, i, cnt, !value))
          {
            latest = i; //제일 최근 할당 위치 갱신
            return i;
          }
	}
	// 앞선 반복문에서 할당할 수 있는 위치를 찾지 못한 경우 메모리의 시작 지점부터 직전에 할당한 위치까지 검색
        for (i = start; i <= latest; i++){
          if (!bitmap_contains (b, i, cnt, !value))
          {
            latest = i; //제일 최근 할당 위치 갱신
            return i;
          }
	}

      }else if(option == 2){ //best fit
	size_t min_idx = BITMAP_ERROR; //내부 단편화가 가장 작은 페이지 위치를 저장하는 변수
	for (i = start; i <= last; i++){
          if (!bitmap_contains (b, i, cnt, !value))
            return min_idx = i;
	} //first fit으로 맨 처음 위치로 부터 할당할 수 있는 페이지 위치를 저장 
	if(min_idx == BITMAP_ERROR){
      	  return min_idx; // 할당할 수 있는 페이지 자체가 없는 경우
	}else{
	  size_t fragment; // 사용 가능 하지만 사용하지 않는 남는 공간의 크기를 저장하는 변수
	  //fragment 가 가장 적은 페이지 위치를 반환하는 것이 best fit
	  for(i = 1; i <= last - (min_idx + cnt) ; i++){ //i의 크기를 증가시켜가며, 남은 공간을 측정
	    if (bitmap_test (b, min_idx + cnt + i) != false){
	      i--;
              break;
	    }
	  }
	  fragment = i;

	  for (i = min_idx + cnt + fragment + 1; i <= last; i++){
	    //첫 번째 찾은 페이지 이후로
	    if (!bitmap_contains (b, i, cnt, !value)){
	      // 할당 가능한 페이지를 찾으면
	      for(size_t j = 1; j <= last - (i + cnt) ; j++){
		if (bitmap_test (b, i + cnt + j) != false){
		  j--;
          	  break;
	        }
	      }
	      // 그 페이지의 남은 공간을 측정하고
	      if(fragment > j){	
	      //fragment보다 작으면 min_idx를 갱신한다
		min_idx = i;
		fragment = j;
	      }
	    }
	  }
	  return min_idx; //끝까지 검색하고 min_idx 반환 
	}
      }else if(option == 3){ //buddy system
	
	size_t max = 512; // Buddy System 기법을 사용하였을 때, 할당할 수 있는 최대 공간 크기이다.
        size_t bound = 512; // 요청 크기에 가장 가까운 빈 공간의 크기를 찾는데 도움을 주는 변수이다.
        size_t offset = 0; // 할당되어 있는 공간의 2^k 크기를 구하는 변수이다.
        size_t j = 0; // 해당 위치가 빈 공간인지 아닌지 알고자 반복문에서 사용하는 변수이다.

        if(check_cnt < 3)
        {
          i = 0; // 요청 크기에 가장 가까운 빈 공간의 위치를 담고 있는 변수이다.
          check_cnt++;
        }
        else
        {
          i = check_size;
        }

        if(cnt > max)
        { // 요청 크기가 할당할 수 있는 최대 크기 공간보다 클 경우에 대한 예외 처리이다.
          return BITMAP_ERROR;
        }

        while(true)
        { // 요청 크기에 가장 가까운 빈 공간을 찾는 반복문이다.
          bound = bound / 2;

          if(cnt > bound)
          {
            while(true)
            {
              if(i >= max + check_size)
              { // 요청 크기에 가장 가까운 빈 공간을 찾을 수 없을 경우에 대한 예외 처리이다.
                return BITMAP_ERROR;
              }

              if(bitmap_test (b, i) == true)
              { // 해당 위치가 할당되어 있을 경우, 할당되어 있는 공간의 2^k 크기를 구한다.
                j = i;
                while(true)
                { // 할당되어 있는 공간의 크기를 구하는 반복문이다.
                  if(j >= max + check_size)
                  { // 빈 공간이 없는 경우에 대한 예외 처리이다.
                    return BITMAP_ERROR;
                  }

                  if(bitmap_test (b, j) == true)
                  {
                    offset++;
                  }
                  else
                  {
                    break;
                  }

                  j++;
                }

                if(offset > 256)
                { // 빈 공간이 없는 경우에 대한 예외 처리이다.
                  offset = 512;
                  return BITMAP_ERROR;
                }
                else if(offset > 128)
                { // 해당 if else 문들은 offset 변수의 값을 보다 큰 2^k 로 바꾼다.
                  offset = 256;
                }
                else if(offset > 64)
                {
                  offset = 128;
                }
                else if(offset > 32)
                {
                  offset = 64;
                }
                else if(offset > 16)
                {
                  offset = 32;
                }
                else if(offset > 8)
                {
                  offset = 16;
                }
                else if(offset > 4)
                {
                  offset = 8;
                }
                else if(offset > 2)
                {
                  offset = 4;
                }
                else if(offset > 1)
                {
                  offset = 2;
                }
                else
                {
                  offset = 1;
                }

                if(offset > bound * 2)
                {
                  i = i + offset;
                }
                else
                {
                  i = i + bound * 2;
                }
              }
              else
              {
                if(bound != 0)
                {
                  if(!bitmap_contains (b, i, bound * 2, !value))
                  {
                    printf("할당된 위치 i: %10d,    ", i-3);
                    printf("요청 크기 cnt: %10d\n", cnt);
                    return i;
                  }

                  i = i + bound * 2;
                }
                else
                {
                  if(bitmap_test (b, i) == false)
                  {
                    printf("할당된 위치 i: %10d,    ", i-3);
                    printf("요청 크기 cnt: %10d\n", cnt);
                    return i;
                  }

                  i = i + 1;
                }
              }
            }
          }
        }
      }
    }
  return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}

/* File input and output. */

#ifdef FILESYS
/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}

/* Reads B from FILE.  Returns true if successful, false
   otherwise. */
bool
bitmap_read (struct bitmap *b, struct file *file) 
{
  bool success = true;
  if (b->bit_cnt > 0) 
    {
      off_t size = byte_cnt (b->bit_cnt);
      success = file_read_at (file, b->bits, size, 0) == size;
      b->bits[elem_cnt (b->bit_cnt) - 1] &= last_mask (b);
    }
  return success;
}

/* Writes B to FILE.  Return true if successful, false
   otherwise. */
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
#endif /* FILESYS */

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void
bitmap_dump (const struct bitmap *b) 
{
  hex_dump (0, b->bits, byte_cnt (b->bit_cnt), false);
}

