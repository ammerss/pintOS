
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include <debug.h>
#include <stdio.h>
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "vm/frame.h"


static struct list_elem *clock_ptr;

void vm_frame_init() {
	list_init(&frame_list);
	//clock_ptr = NULL;
}

void* vm_frame_allocate(enum palloc_flags flags, void *upage) {//프레임 추가
	void *frame = palloc_get_page(PAL_USER | flags); //page userpool에서 page 가져옴

	if (frame == NULL) { //page allocate를 받을수 없는경우 
		vm_evict_frame();
	}
	if (frame != NULL) { //successfully allocated page
		vm_add_frame(upage, frame);
	} 
}

void vm_add_frame(void *upage, void *frame) { //프레임테이블에 엔트리 추가
	struct frame *f = (struct frame*) malloc(sizeof(struct frame));
	f->t = thread_current();
	f->upage = upage;////
	f->uaddr = frame;////??????????????????????????????????????????맞는지모름
	list_push_back(&frame_list, &f->elem); //프레임 추가!!!
}

void* vm_evict_frame() { //페이지자리 하나 만들기

	struct frame *ef;//스왑할 페이지

	ef = evict_by_clock();
	if (ef == NULL) //스왑할 페이지를 못찾음
		PANIC("No frame to evict");
	if(!save_evicted_frame(ef))
		PANIC("can't save evicted frame");

	//페이지 비우기
	ef->t = thread_current();
	ef->upage = NULL;
	ef->uaddr = NULL;

	return ef;
}

struct frame* evict_by_clock() {//clock algorithm
	struct frame *f;
	struct thread *t;
	struct list_elem *e;
	void* last_use = pagedir_lru_list_get_head(false);
	if(last_use != NULL){
		for(e=list_begin(&frame_list); e!=list_end(&frame_list); e=list_next(e)){
			f = list_entry(e, struct frame, elem);
			if(f->uaddr == last_use){
				e = f;
			}
		}
	}
	struct frame *f_class0 = NULL;

	int cnt = 0;
	bool found = false;

	
	while (!found) {
		for (; e != list_end(&frame_list) && cnt < 2; e = list_next(e)) {

			f = list_entry(e, struct frame, elem);
			t = f->t;

			//clock algorithm
			if (!pagedir_is_accessed(t->pagedir, f->uaddr)) {
				f_class0 = f;
				list_remove(e);
				list_push_back(&frame_list, e);
				break;
			}
			else {
				pagedir_set_accessed(t->pagedir, f->uaddr, false);
			}
		}
		if (f_class0 != NULL) found = true;
		else if (cnt++ == 2) found = true;
	}

	pagedir_lru_list_remove(f_class0->uaddr);

	return f_class0;
}

struct frame* evict_by_lru() {
	struct frame *f;
	struct list_elem *e;
	void* evict_page = pagedir_lru_list_get_head(true);
	
	if(evict_page == NULL){
		PANIC("No page in LRU list");
	}
	
	for(e = list_begin(&frame_list); e!=list_end(&frame_list); e = list_next(e)){
		f = list_entry(e, struct frame, elem);
		
		if(f->uaddr == evict_page){
			return f;
		}
	}
	
	return NULL;
}

struct frame* evict_by_second_chance() {
	struct frame *f;
	struct thread *t;
	struct list_elem *e;

	struct frame *f_class0 = NULL;

	int cnt = 0;
	bool found = false;

	while (!found) {
		for (e = list_begin(&frame_list); e != list_end(&frame_list) && cnt < 2; e = list_next(e)) {

			f = list_entry(e, struct frame, elem);
			t = f->t;

			//clock algorithm
			if (!pagedir_is_accessed(t->pagedir, f->uaddr)) {
				f_class0 = f;
				list_remove(e);
				list_push_back(&frame_list, e);
				break;
			}
			else {
				pagedir_set_accessed(t->pagedir, f->uaddr, false);
			}
		}
		if (f_class0 != NULL) found = true;
		else if (cnt++ == 2) found = true;
	}

	return f_class0;
}

bool save_evicted_frame(struct frame *f) {
	
	struct thread *t;
	size_t swap_slot_idx;
	
	t = f->t;
	
	if(pagedir_is_dirty(t->pagedir, f->uaddr)){
		
		swap_slot_idx = vm_swap_out(f->uaddr);
		if(swap_slot_idx == SWAP_ERROR)
			return false;
	}
	
	memset(f, 0, PGSIZE);
	
	pagedir_clear_page(t->pagedir, f->uaddr);
	
	return true;
}

void vm_remove_frame(void *frame) {
	struct frame *f;
	struct list_elem *e;

	e = list_head(&frame_list);
	while ((e = list_next(e)) != list_tail(&frame_list)) {
		f = list_entry(e, struct frame, elem);
		if (f == frame) {
			list_remove(e);
			free(f);
			break;
		}
	}
}
void vm_free_frame(void *frame) {
	vm_remove_frame(frame);
	palloc_free_page(frame);
}
void vm_set_frame(void *fr, void *pte, void *upage){
	struct frame *f, *find;
	struct list_elem *e;
	f=fr;
	e=list_head(&frame_list);
	while((e=list_next(e)) != list_tail (&frame_list)){
		find=list_entry(e, struct frame, elem);
		if(find==f){
			break;
		}
		find=NULL;
	}
	if(find != NULL){
		find->uaddr = pte;
		find->upage = upage;
	}
}
