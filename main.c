#include <stdio.h>
#include "heap.h"

int main(void) {

    heap_setup();

    char *ptr[10];

    for(int i = 0; i<5; i++)
    {
        ptr[i] = heap_malloc(50);
    }
    heap_free(ptr[1]);
    heap_free(ptr[4]);
    heap_free(ptr[2]);
    
    ptr[5] = heap_malloc_aligned(2254);
    ptr[6] = heap_malloc_aligned(8000);
    ptr[7] = heap_malloc_aligned(12000);

    heap_free(ptr[6]);

    if(((intptr_t)ptr[5] & (intptr_t)(PAGE_SIZE - 1)) == 0){
        printf("12 least significant bits of the address in ptr are set to zero\n");
    }

    printHeap();

    int status = heap_validate();
    
    if(status == 1 || status == 2 || status == 3){
        printf("Heap is invalid\n");
        return -1;
    }
    return 0;
}


