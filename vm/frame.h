
#include "threads/thread.h"

struct frame { //frame table entries
	void *upage; //pointer to page, user virtual page
	void *uaddr; //pointer to user virtual address
	struct thread *t; //thread the page belongs to
	struct list_elem elem; //element in frame_list
};


static struct list frame_list; //list of frames





void vm_frame_init();

void* vm_frame_allocate(enum palloc_flags flags, void *upage);

void vm_add_frame(void *upage, void *frame);

void* vm_evict_frame();

struct frame* evict_by_clock();


struct frame* evict_by_lru();

struct frame* evict_by_second_chance();

bool save_evicted_frame(struct frame *f);

void vm_remove_frame(void *frame);
void vm_free_frame(void *frame);
void vm_set_frame(void*, void *, void *);
