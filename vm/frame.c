
#include <list.h>
#include <thread.h>
static struct list frame_list //list of frames
static struct list_elem *clock_ptr;


struct frame { //frame table entries
	void *upage; //pointer to page, user virtual page
	void *uaddr; //pointer to user virtual address
	struct thread *t; //thread the page belongs to
	struct list_elem elem; //element in frame_list
};

void vm_frame_init() {
	list_init(&frame_list); 
	//clock_ptr = NULL;
}

void* vm_frame_allocate(enum palloc_flags flags, void *upage) {//프레임 추가
	void *frame = palloc_get_page(PAL_USER | flags); //page userpool에서 page 가져옴

	if (free_page== NULL) { //page allocate를 받을수 없는경우 
		vm_evict_frame();
	}
	if(free_page!=NULL){ //successfully allocated page
		vm_add_frame(upage,frame);
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
	
	ef = frametoevict(); 
	if (ef == NULL) //스왑할 페이지를 못찾음
		PANIC("No frame to evict");
	sef = save_evicted_frame(ef); //스왑할 페이지 저장
	if (!sef) //스왑할 페이지 저장실패
		PANIC("can't save evicted frame");

	//페이지 비우기
	ef->t = thread_current();
	ef->upage = NULL;
	ef->uaddr = NULL;

	return &ef;
}

struct frame* frametoevict() {//clock algorithm ???????????????
	struct frame *f;
	struct thread *t;
	struct list_elem *e;

	int cnt = 0;
	for (e = list_begin(&frame_list); e != list_end(&frame_list) && cnt != 1; e = list_next(e)) {

		t = thread_get_by_id(e->t);

		//clock algorithm
		if( pagedir_is_accessed(t->pagedir,e->uaddr )){
			return e;
		}
		else {
			pagedir_set_accessed(t->pagedir, e->uaddr, false);
		}		

		//2바퀴 돌도록 
		if (is_tail(list_next(e)) && cnt == 0) {
			e = list_begin(&frame_list);
			cnt++;
		}
	}
}

void vm_remove_frame(void *upage) {
	struct frame *f;
	struct list_elem *e;

	e = list_head(&frame_list);
	while ((e = list_next(e)) != list_tail(&frame_list)) {
		f = list_entry(e,struct frame, elem);
		if (f->upage == upage) {
			list_remove(e);
			free(f);
			break;
		}	
	}
}
