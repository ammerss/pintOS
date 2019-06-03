#include <bitmap.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/*Block device for swap*/
struct block *swap_device;

/*Bitmap of swap slot avaliavlity and corresponding lock*/
static struct bitmap *swap_map;

static size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_size_in_page( void );

/*Initialize swap_device and swap_map*/
void
vm_swap_init(){

	swap_device = block_get_role(BLOCK_SWAP);
	if(swap_device == NULL )
		PANIC("no swap device found, can't initialize swap");

	swap_map = bitmap_create(swap_size_in_page());
	if(swap_map == NULL)
		PANIC("swap bitmap creation failed");

	bitmap_set_all(swap_map, true);

}

void
vm_swap_free( size_t swap_idx ){

	if(bitmap_test(swap_map, swap_idx) == true){
		
		PANIC("ERROR: invalid free request to unassigned swap block");
	}
	bitmap_set(swap_map, swap_idx, true)
}

size_t
vm_swap_out(const void *uva){

	/*find a swap slot and mark it in use*/
	size_t swap_idx = bitmap_scan_and_flip(swap_map, 0, 1, true);

	if(swap_idx == BITMAP_ERROR)
		return SWAP_ERROR;

	/*write the page of data to the swap slot*/
	size_t counter = 0;
	while(counter < SECTORS_PER_PAGE){
		
		block_write(swap_device, swap_idx * SECTORS_PER_PAGE + counter, uva + counter*BLOCK_SECTOR_SIZE);
		counter++;
	}
	return swap_idx;

}

void
vm_swap_in(size_t swap_idx, void *uva){
	
	size_t count = 0;
	while( counter < SECTORS_PER_PAGE ){

		blokc_read( swap_device, swap_idx * SECTORS_PER_PAGE + counter, uva + counter*BLOCK_SECTOR_SIZE);
		counter++;
	}
	bitmap_filp(swap_map, swap_idx);
}

static size_t
swap_size_in_page(){
	
	return block_size(swap_device)/SECTORS_PER_PAGE;
}
