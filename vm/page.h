#ifdef VM_PAGE_H
#define VM_PAGE_H

#define STACK_SIZE(8*(1<<20))

#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum supple_pte_type{
  SWAP = 001,
  FILE = 002,
  MMF = 004
};

union supple_pte_data{
  struct{
    struct file* file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
  }file_page;
  
  struct{
    struct file * file;
    off_t ofs;
    uint32_t read_bytes;
  }mmf_page;
};

struct supple_pte{
  void *uvaddr;
  enum supple_pte_type type;
  union supple_pte_data data;
  bool is_loaded;
  size_t swap_slot_idx;
  bool swap_writable;
  struct hash_elem elem;
};

void vm_page_init(void);
unsigned supple_pt_hash (const struct hash_elem *, void * UNUSED);
bool supple_pt_less (const struct hash_elem *, const struct hash_elem *,void * UNUSED);
bool supple_pt_insert_mmf (struct file *, off_t, uint8_t *, uint32_t);
struct supple_pte *get_supple_pte (struct hash *, void *);
void write_page_back_to_file_wo_lock (struct supple_pte *);
void free_supple_pt (struct hash *);
bool load_page (struct supple_pte *);
void grow_stack (void *);

#endif
