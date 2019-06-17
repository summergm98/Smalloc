#include <unistd.h>
#include <stdio.h>
#include "smalloc.h" 

sm_container_ptr sm_first = 0x0 ;
sm_container_ptr sm_last = 0x0 ;
sm_container_ptr sm_unused_containers = 0x0 ;
sm_container_ptr unused_last = 0x0 ;

void sm_container_split(sm_container_ptr hole, size_t size)
{
	sm_container_ptr remainder = hole->data + size ;

	remainder->data = ((void *)remainder) + sizeof(sm_container_t) ;
	remainder->dsize = hole->dsize - size - sizeof(sm_container_t) ;
	remainder->status = Unused ;

	remainder->next_unused = 0x0;
	hole->next = 0x0;
	sm_last = hole;
	
	if(sm_unused_containers == 0x0){
		sm_unused_containers = remainder;
		unused_last = remainder;
		return;
	}else{
		unused_last->next_unused = remainder;
		unused_last = remainder;
		return;
	}
}

void * sm_retain_more_memory(int size)
{
	sm_container_ptr hole ;
	int pagesize = getpagesize() ;
	int n_pages = 0 ;

	n_pages = (sizeof(sm_container_t) + size + sizeof(sm_container_t)) / pagesize  + 1 ;
	hole = (sm_container_ptr) sbrk(n_pages * pagesize) ;
	if (hole == 0x0)
		return 0x0 ;

	hole->data = ((void *) hole) + sizeof(sm_container_t) ;
	hole->dsize = n_pages * getpagesize() - sizeof(sm_container_t) ;
	hole->status = Unused ;

	return hole ;
}

void * smalloc(size_t size) 
{
	size_t gap = 99999;
	sm_container_ptr temp = 0x0;
	sm_container_ptr before = 0x0;
	sm_container_ptr hole = 0x0 ;
	sm_container_ptr itr = 0x0 ;

	if(sm_unused_containers != 0x0){
		if(sm_unused_containers->dsize == size){
			sm_unused_containers->status = Busy;
			sm_last->next = sm_unused_containers;
			sm_last = sm_last->next;
			sm_unused_containers = sm_unused_containers->next_unused;
			sm_last->next = 0x0;
			return sm_last->data;
		}else if (size + sizeof(sm_container_t) < sm_unused_containers->dsize){
			if(gap > sm_unused_containers->dsize - (size + sizeof(sm_container_t))){
				gap = sm_unused_containers->dsize - (size + sizeof(sm_container_t));
				temp = sm_unused_containers;
				before = 0x0;
			}
		}
	}

	for(itr = sm_unused_containers; itr != 0x0 && itr->next_unused != 0x0; itr = itr->next_unused){
		if(size == itr->next_unused->dsize){
			itr->next_unused->status = Busy;
			sm_last->next = itr->next_unused;
			sm_last = sm_last->next;
			itr->next_unused = itr->next_unused->next_unused;
			sm_last = 0x0;
			return itr->next_unused->data;
		}else if (size + sizeof(sm_container_t) < itr->next_unused->dsize){
			if(gap > itr->next_unused->dsize - (size + sizeof(sm_container_t))){
				gap = itr->next_unused->dsize - (size + sizeof(sm_container_t));
				temp = itr->next_unused;
				before = itr;
			}
		}
	}

	hole = temp;

	if(hole == 0x0){
		hole = sm_retain_more_memory(size);
	
		if(hole == 0x0)
			return 0x0;
	
		if(sm_first == 0x0){
			sm_first = hole;
			sm_last = hole;
			hole->next = 0x0;
		}
		else{
			sm_last->next = hole;
			sm_last = hole;
			hole->next = 0x0;
		}
	}else{
		if(before == 0x0){
			sm_unused_containers = hole->next_unused;
			sm_last->next = hole;
			sm_last = hole;
			hole->next = 0x0;
		}else{
			before->next_unused = hole->next_unused;
			sm_last->next = hole;
			sm_last = hole;
			hole->next = 0x0;
		}
	}	

	sm_container_split(hole, size) ;
	hole->dsize = size ;
	hole->status = Busy ;

	return hole->data ;
}

void merge(){
	sm_container_ptr itr;
	sm_container_ptr itr2;

	for(itr = sm_unused_containers; itr != 0x0; itr = itr->next_unused){

		void * temp =  itr->data + itr->dsize + sizeof(sm_container_t);
		
		if(sm_unused_containers->data == temp && sm_unused_containers->status == Unused){
			itr->dsize = itr->dsize + sizeof(sm_container_t) + sm_unused_containers->dsize;
			sm_unused_containers = sm_unused_containers->next_unused;
		}
		
		for(itr2 = sm_unused_containers; itr2->next_unused != 0x0; itr2 = itr2->next_unused){
			if(temp == itr2->next_unused->data && itr2->next_unused->status == Unused){
				itr->dsize = itr->dsize + sizeof(sm_container_t) + itr2->next_unused->dsize;
				itr2->next_unused = itr->next_unused->next_unused;
			}
		}
	}	
}
	
void sfree(void * p)
{
	sm_container_ptr itr;

	if(sm_first->data == p){
		sm_first->status = Unused;
		unused_last->next_unused = sm_first;
		sm_first = sm_first->next;
		unused_last = unused_last->next_unused;
		unused_last->next_unused = 0x0;
	}

	for(itr = sm_first; itr->next != 0x0; itr = itr->next){
		if(itr->next->data == p){
			itr->next->status = Unused;
			unused_last->next_unused = itr->next;
			itr->next = itr->next->next;
			unused_last = unused_last->next_unused;
			unused_last->next_unused = 0x0;
			break;
		}
	}
		
	merge();
	return;
}

void print_sm_containers()
{
	sm_container_ptr itr ;
	int i = 0 ;

	printf("==================== sm_containers ====================\n") ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next, i++) {
		char * s ;
		printf("%3d:%p:%s:", i, itr->data, itr->status == Unused ? "Unused" : "  Busy") ;
		printf("%8d:", (int) itr->dsize) ;

		for (s = (char *) itr->data ;
			 s < (char *) itr->data + (itr->dsize > 8 ? 8 : itr->dsize) ;
			 s++) 
			printf("%02x ", *s) ;
		printf("\n") ;
	}
	printf("=======================================================\n") ;
}

void print_sm_unused_containers(){
	sm_container_ptr itr ;
	int i = 0 ;

	printf("================= unused_containers====================\n") ;
	for (itr = sm_unused_containers ; itr != 0x0 ; itr = itr->next_unused, i++) {
		char * s ;
		printf("%3d:%p:%s:", i, itr->data, itr->status == Unused ? "Unused" : "  Busy") ;
		printf("%8d:", (int) itr->dsize) ;

		for (s = (char *) itr->data ;
			 s < (char *) itr->data + (itr->dsize > 8 ? 8 : itr->dsize) ;
			 s++) 
			printf("%02x ", *s) ;
		printf("\n") ;
	}
	printf("=======================================================\n") ;
}

void print_sm_uses(){
	sm_container_ptr itr;
	size_t un_allocated = 0;
	size_t allocated = 0;

	for(itr = sm_first; itr != 0x0; itr = itr->next)
		allocated += itr->dsize;
	for(itr = sm_unused_containers; itr != 0x0; itr = itr->next_unused)
		un_allocated += itr->dsize;

	printf("=================== sm_memory_status ==================\n") ;
	printf("All retained memory size: %lu\n", un_allocated + allocated);
	printf("Allocated memory size: %lu\n", allocated);
	printf("Unused memory size: %lu\n", un_allocated);
	printf("=======================================================\n") ;
}
