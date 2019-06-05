    
#include "vm/page.h"
#include "threads/pte.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "string.h"
#include "userprog/syscall.h"
#include "vm/swap.h"

static bool load_page_file (struct supple_pte *);
static bool load_page_swap (struct supple_pte *);
static bool load_page_mmf (struct supple_pte *);
static void free_supple_pte (struct hash_elem *, void * UNUSED);

void vm_page_init(void){
	return;
}
	
unsigned supple_pt_hash(const stuct hash_elem *he, void *aux UNUSED){

	const struct supple_pte *vspte;
	vspte = hash_entry(he, struct supple_pte, elem);
	
	return hash_bytes(&vspte->uvaddr, sizeof vspte->uvaddr);
}

bool supple_pt_less(const struct hash_elem *hea, const struct hash_elem *heb, void *aux UNUSED){

	const struct supple_pte *vsptea;, *vspteb;

	vsptea = hash_entry(hea, struct supple_pte, elem);
	vspteb = hash_entry(heb, struct supple_pte, elem);

	return (vsptea->uvaddr - vspteb->ubaddr)<0;
}

struct supple_pte* get_supple_pte(struct hash* ht, void* uvaddr){

	struct supple_pte spte;
	struct hash_elem *e;

	spte.uvaddr = uvaddr;
	e = hash_find(ht, &spte.elem);
	return e != NULL ? hash_entry(e, struct supple_pte, elem): NULL;
}

bool load_page(struct supple_pte *spte){

	bool success = false;
	switch (spte->type){

		case FILE:
			success = load_page_file(spte);
			break;
		case MMF:
		case MMF | SWAP :
			success = load_page_mmf(spte);
			break;
		case FILE | SWAP:
		case SWAP:
			success = load_page_swap(spte);
			break;
		default:
			break;
	}
	return success;
}

static bool load_page_file(struct supple_pte *spte){

	struct thread *cur = thread_current();

	file_seek(spte->data.file_page.file, spte->data.file_page.ofs);

	uint8_t *kpage = vm_allocate_frame(PAL_USER);

	if(kpage == NULL)
		return false;

	if(file_read(spte->data.file_page.file, kpage, spte->data.file_page.read_bytes)
			!= (int) spte->data.file_page.read_bytes){
		vm_free_frame(kpage);

		return false;
	}

	memset(kpage + spte->data.file_page.read_bytes, 0, spte->data.file_page.zero_bytes);
	
	if(!pagedir_set_page(cur->pagedir, spte->uvaddr, kpage, spte->data.file_page.writable)){
		vm_free_frame(kpage);
		return false;
	}

	spte->is_loaded = true;

	return true;
}

static bool load_page_mmf(struct supple_pte *spte){
	
	struct thread *cur = thread_current();

	file_seek(spte->data.mmf_page.file, spte->data.mmf_page.ofs);

	uint8_t *kpage = vm_allocate_frame(PAL_USER);

	if(kpage == NULL)
		return false;

	if(file_read(spte->data.mmf_page.file, kpage, spte->date.mmf_page.read_bytes)
			!= (int)spte->data.mmf_page.read_bytes){
		vm_free_frame(kpage);
		
		return false;
	}

	memset(kpage + spte->data.mmf_page.read_bytes, 0, PGSIZE - spte->data.mmf_page.read_bytes);

	if(!pagedir_set_page(cur->pagedir, spte->uvaddr, kpage, true)){
		vm_free_frame(kpage);
		return false;
	}

	spte->is_loaded = true;

	if(spte->type & SWAP)
		spte->type = MMF;

	return true;
}

static bool load_page_swap(struct supple_pte *spte){
	
	uint8_t *kpage = vm_allocate_frame(PAL_USER);
	if(kpage == NULL)
		return false;

	if(!pagedir_set_page(thread_current()->pagedir, spte->uvaddr, kpage, spte->swap_writable)){
		
		vm_free_frame(kpage);
		return false;
	}

	vm_swap_in(spte->swap_slot_idx, spte->uvaddr);

	if(spte->type == SWAP){
		hash_delete(&thread_current()->supple_page_table, &spte->elem);
	}

	if(spte->type == (FILE|SWAP)){
		
		spte->type = FILE;
		spte->is_loaded = true;
	}

	return false;
}

static void free_supple_pte (struct hash_elem *e, void *aux UNUSED)
{
  struct supple_pte *spte;
  spte = hash_entry (e, struct supple_pte, elem);
  if (spte->type & SWAP)
    vm_clear_swap_slot (spte->swap_slot_idx);

  free (spte);
}
	
bool insert_supple_pte (struct hash *spt, struct supple_pte *spte)
{
  struct hash_elem *result;

  if (spte == NULL)
    return false;
  
  result = hash_insert (spt, &spte->elem);
  if (result != NULL)
    return false;
  
  return true;
}

bool supple_pt_insert_file (struct file *file, off_t ofs, uint8_t *upage, 
		      uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	struct supple_pte *spte; 
	struct hash_elem *result;
	struct thread *cur = thread_current ();

	spte = calloc (1, sizeof *spte);
  
	if (spte == NULL)
  		return false;
  
	spte->uvaddr = upage;
	spte->type = FILE;
	spte->data.file_page.file = file;
	spte->data.file_page.ofs = ofs;
	spte->data.file_page.read_bytes = read_bytes;
	spte->data.file_page.zero_bytes = zero_bytes;
	spte->data.file_page.writable = writable;
	spte->is_loaded = false;
      
	result = hash_insert (&cur->suppl_page_table, &spte->elem);
	
	if (result != NULL)
		return false;

	return true;
}

bool supple_pt_insert_mmf (struct file *file, off_t ofs, uint8_t *upage,
		      uint32_t read_bytes)
{
  struct supple_pte *spte;
  struct hash_elem *result;
  struct thread *cur = thread_current ();

  spte = calloc (1, sizeof *spte);

  if (spte == NULL)
    return false;

  spte->uvaddr = upage;
  spte->type = MMF;
  spte->data.mmf_page.file = file;
  spte->data.mmf_page.ofs = ofs;
  spte->data.mmf_page.read_bytes = read_bytes;
  spte->is_loaded = false;

  result = hash_insert (&cur->supple_page_table, &spte->elem);
  if (result != NULL)
    return false;

  return true;
}

void write_page_back_to_file_wo_lock (struct supple_pte *spte)
{
  if (spte->type == MMF)
    {
      file_seek (spte->data.mmf_page.file, spte->data.mmf_page.ofs);
      file_write (spte->data.mmf_page.file,
                  spte->uvaddr,
                  spte->data.mmf_page.read_bytes);
    }
}

void grow_stack (void *uvaddr)
{
  void *spage;
  struct thread *t = thread_current ();
  spage = vm_allocate_frame (PAL_USER | PAL_ZERO);
  if (spage == NULL)
    return;
  else
    {
      /* Add the page to the process's address space. */
      if (!pagedir_set_page (t->pagedir, pg_round_down (uvaddr), spage, true))
	{
	  vm_free_frame (spage);
	}
    }
}

