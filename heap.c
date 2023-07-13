#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "custom_unistd.h"

struct memory_manager_t memory_manager;

int setup = 0;

int flaga = 0;

size_t last_block_size = 0;

int alignment(int size) {
    int res = (size + 4096 - 1) & ~(4096 - 1);
    return res;
}

void printHeap(void)
{
    struct memory_chunk_t *ptr = memory_manager.first_memory_chunk;
    printf("===============\n");
    int counter = 0;
    int free_blocks = 0;
    while(ptr)
    {
        printf("%d - Size: %zu | Free: %d | Address: %lu\n", counter,ptr->size, ptr->free, (intptr_t)ptr);
        if(ptr->free == 0)
        {
            free_blocks++;
        }
        ptr = ptr->next;
        counter++;
    }
    printf("===============\n");
    printf("Ilosc blokow: %d\n", counter);
    printf("Ilosc niezwolnionych blokow: %d\n", free_blocks);
}

void* heap_malloc_aligned(size_t count)
{
    if((int)count<=0) return  NULL;

    if(custom_sbrk((intptr_t) alignment((int)count)) == (void*)-1)
        return NULL;

    memory_manager.memory_size += alignment((int)count);

    struct memory_chunk_t *tmp;

    if(memory_manager.first_memory_chunk == NULL)
    {
        tmp = (void*)((char*)memory_manager.memory_start + PAGE_SIZE - sizeof(struct memory_chunk_t) - F_SIZE);
        tmp->size = count;
        tmp->free = 0;
        tmp->prev = NULL;
        tmp->next = NULL;

        memory_manager.first_memory_chunk = tmp;
        set_fences();
        tmp->hash = valid_struct_hush(tmp);
        last_block_size = tmp->size;
        return (void*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE);
    }

    for(tmp = memory_manager.first_memory_chunk; tmp; tmp=tmp->next)
    {
        if(tmp->next)
        {
            if(tmp->free == 1)
            {
                tmp->size=(char*)tmp->next - (char*)tmp - sizeof(struct memory_chunk_t) - 2*F_SIZE;
                tmp->hash = valid_struct_hush(tmp);
                if(tmp->size >= count)
                {
                    tmp->size = count;
                    tmp->free = 0;
                    set_fences();
                    tmp->hash = valid_struct_hush(tmp);
                    if(flaga == 1)
                    {
                        bzero((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE,count);
                        flaga = 2;
                    }
                    long page_dif = ((intptr_t)tmp & (intptr_t)(PAGE_SIZE - 1));

                    if((int)(PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE) < 0)
                        tmp = (struct memory_chunk_t*)((char*)tmp + 2*PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);
                    else
                        tmp = (struct memory_chunk_t*)((char*)tmp + PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);

                    return (void*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE);
                }
            }
        }
        else if(tmp->size >= count && tmp->free == 1)
        {
            tmp->size = count;
            tmp->free = 0;
            set_fences();
            tmp->hash = valid_struct_hush(tmp);
            long page_dif = ((intptr_t)tmp & (intptr_t)(PAGE_SIZE - 1));
            if((int)(PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE) < 0)
                tmp = (struct memory_chunk_t*)((char*)tmp + 2*PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);
            else
                tmp = (struct memory_chunk_t*)((char*)tmp + PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);

            return (void*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE);
        }
    }

    tmp = memory_manager.first_memory_chunk;
    while(tmp->next)
    {
        tmp = tmp->next;
    }

    struct memory_chunk_t *new = (struct memory_chunk_t*)((char*)tmp + sizeof(struct memory_chunk_t) + tmp->size + 2*F_SIZE);

    long page_dif = ((intptr_t)new & (intptr_t)(PAGE_SIZE - 1));

    if((int)(PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE) < 0)
        new = (struct memory_chunk_t*)((char*)new + 2*PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);
    else
        new = (struct memory_chunk_t*)((char*)new + PAGE_SIZE - page_dif - sizeof(struct memory_chunk_t) - F_SIZE);

    new->size = count;
    new->free = 0;
    new->next = NULL;
    new->prev = tmp;
    tmp->next = new;
    set_fences();
    tmp->hash = valid_struct_hush(tmp);
    new->hash = valid_struct_hush(new);
    last_block_size = new->size;
    return (void*)((char*)new + sizeof(struct memory_chunk_t) + F_SIZE);
}
void* heap_calloc_aligned(size_t number, size_t size)
{
    if((int)number<=0 || (int)size<=0) return NULL;

    flaga = 1;
    void* ptr=heap_malloc_aligned(number*size);

    if(!ptr) return NULL;

    if(flaga != 2)
    {
        bzero(ptr,number*size);
        flaga = 0;
    }

    return ptr;
}

void* heap_realloc_aligned(void* memblock, size_t size)
{
    if(setup == 0)
    {
        heap_setup();
        return NULL;
    }

    if((int)size<0) return NULL;

    if(memblock == NULL)
    {
        struct memory_chunk_t* tmp = heap_malloc_aligned(size);
        return tmp;
    }
    if(size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    for(struct memory_chunk_t* tmp = memory_manager.first_memory_chunk; tmp; tmp=tmp->next)
    {
        if((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE == memblock)
        {
            if(size <= tmp->size)
            {
                tmp->size = size;
                set_fences();
                tmp->hash = valid_struct_hush(tmp);
                return memblock;
            }
            if(tmp->next)
            {
                size_t real_whole_size = 0;
                if(tmp->next->next && tmp->next->next->free != 1)
                {
                    real_whole_size = (char*)tmp->next->next - (char*)tmp - sizeof(struct memory_chunk_t) - 3*F_SIZE;
                }
                if(tmp->next->free == 1 && real_whole_size >= size)
                {
                    tmp->size = size;
                    struct memory_chunk_t* next_tmp = tmp->next;

                    while(next_tmp->free == 1)
                    {
                        if(next_tmp->next==NULL) break;

                        next_tmp = next_tmp->next;
                    }
                    if(next_tmp != tmp) {
                        next_tmp->prev = tmp;
                        next_tmp->hash = valid_struct_hush(next_tmp);
                        tmp->next = next_tmp;
                        tmp->hash = valid_struct_hush(tmp);
                    }
                    set_fences();
                    tmp->hash = valid_struct_hush(tmp);
                    return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
                }
                else if(tmp->free != 1 && PAGE_SIZE > size)
                {
                    tmp->size = size;
                    set_fences();
                    tmp->hash = valid_struct_hush(tmp);
                    return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
                }
                else
                {
                    char* res = heap_malloc_aligned(size);
                    if(res == NULL)
                        return NULL;

                    memcpy(res,(char*)tmp+sizeof(struct memory_chunk_t) + F_SIZE,tmp->size);

                    struct memory_chunk_t* next_tmp = tmp->next;

                    while(next_tmp->free == 1)
                    {
                        if(next_tmp->next==NULL) break;

                        next_tmp = next_tmp->next;
                    }
                    if(tmp->prev)
                    {
                        tmp = tmp->prev;
                        if(next_tmp != tmp) {
                            next_tmp->prev = tmp;
                            next_tmp->hash = valid_struct_hush(next_tmp);
                            tmp->next = next_tmp;
                            tmp->hash = valid_struct_hush(tmp);
                        }
                    }
                    return res;
                }

            }
            if(tmp==memory_manager.first_memory_chunk)
            {
                tmp->size = size;
                tmp->hash = valid_struct_hush(tmp);
                set_fences();
                return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
            }
            if(size <= last_block_size)
            {
                tmp->size = size;
                tmp->hash = valid_struct_hush(tmp);

                set_fences();
                return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
            }
            else
            {
                return NULL;
            }
        }
    }

    return NULL;
}

void heap_free(void* memblock) {
    if (memblock == NULL || get_pointer_type(memblock) != pointer_valid) return;

    struct memory_chunk_t *tmp;

    if ((char *) memory_manager.first_memory_chunk == memblock && memory_manager.first_memory_chunk->next == NULL) {
        memory_manager.first_memory_chunk = NULL;
        return;
    }

    for (tmp = memory_manager.first_memory_chunk; tmp != NULL; tmp = tmp->next)
    {
        if ((char *) tmp + sizeof(struct memory_chunk_t) + F_SIZE == memblock)
        {
            if (tmp != memory_manager.first_memory_chunk)
            {
                tmp->free = 1;
                tmp->hash = valid_struct_hush(tmp);

                if (size_of_heap() == how_many_free()) break;

                if (tmp->prev->free == 0)
                {
                    if (tmp->next != NULL)
                    {
                        if (tmp->next->free == 1)
                        {
                            join_chunks(tmp);
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                join_chunks(tmp);
                break;
            }
            else
            {
                tmp->free = 1;
                join_chunks(tmp);
                tmp->hash = valid_struct_hush(tmp);
                break;
            }
        }
    }
    while(tmp)
    {
        if(tmp->free != 1) return;
        if(tmp->prev)
            if(tmp->prev->free != 1) return;
        tmp = tmp->next;
    }

    memory_manager.first_memory_chunk = NULL;

}

int main(void) {return 0;}

void* heap_calloc(size_t number, size_t size)
{
    if((int)number<=0 || (int)size<=0) return NULL;

    void* ptr=heap_malloc(number*size);

    if(!ptr) return NULL;

    bzero(ptr,number*size);

    return ptr;
}

void* heap_malloc(size_t size)
{
    if((int)size<=0) return  NULL;

    if(custom_sbrk((intptr_t)size) == (void*)-1) return NULL;

    memory_manager.memory_size += size;

    struct memory_chunk_t *tmp;

    if(memory_manager.first_memory_chunk == NULL)
    {
        tmp = memory_manager.memory_start;
        tmp->size = size;
        tmp->free = 0;
        tmp->prev = NULL;
        tmp->next = NULL;

        memory_manager.first_memory_chunk = tmp;
        set_fences();
        tmp->hash = valid_struct_hush(tmp);
        last_block_size = tmp->size;
        return (void*)((char*)memory_manager.memory_start + sizeof(struct memory_chunk_t) + F_SIZE);
    }
    else
    {
        for(tmp = memory_manager.first_memory_chunk; tmp; tmp=tmp->next)
        {
            if(tmp->next)
            {
                if(tmp->free == 1)
                {
                    tmp->size=(char*)tmp->next - (char*)tmp - sizeof(struct memory_chunk_t) - 2*F_SIZE;
                    tmp->hash = valid_struct_hush(tmp);
                    if(tmp->size >= size)
                    {
                        tmp->size = size;
                        tmp->free = 0;
                        set_fences();
                        tmp->hash = valid_struct_hush(tmp);
                        return (void*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE);
                    }
                }
            }
            else if(tmp->size >= size && tmp->free == 1)
            {
                tmp->size = size;
                tmp->free = 0;
                set_fences();
                tmp->hash = valid_struct_hush(tmp);
                return (void*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE);
            }
        }

        tmp = memory_manager.first_memory_chunk;
        while(tmp->next)
        {
            tmp = tmp->next;
        }

        struct memory_chunk_t *new = (struct memory_chunk_t*)((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE + tmp->size + F_SIZE);

        new->size = size;
        new->free = 0;
        new->next = NULL;
        new->prev = tmp;
        tmp->next = new;
        set_fences(new);
        tmp->hash = valid_struct_hush(tmp);
        new->hash = valid_struct_hush(new);

        last_block_size = new->size;

        return (void*)((char*)new + sizeof(struct memory_chunk_t) + F_SIZE);
    }
}
void* heap_realloc(void* memblock, size_t count)
{
    if((int)count<0 || setup == 0) return NULL;

    if(memblock == NULL)
    {
        struct memory_chunk_t* tmp = heap_malloc(count);
        return tmp;
    }
    if(count == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    for(struct memory_chunk_t* tmp = memory_manager.first_memory_chunk; tmp; tmp=tmp->next)
    {
        if((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE == memblock)
        {
            if(count <= tmp->size)
            {
                tmp->size = count;
                tmp->hash = valid_struct_hush(tmp);
                set_fences();
                return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
            }
            if(tmp->next)
            {
                size_t real_whole_size;
                if(tmp->next->next && tmp->next->next->free != 1)
                {
                    real_whole_size = (char*)tmp->next->next - (char*)tmp - sizeof(struct memory_chunk_t) - 3*F_SIZE;
                }
                if(tmp->next->free == 1 && real_whole_size >= count)
                {
                    tmp->size = count;
                    struct memory_chunk_t* next_tmp = tmp->next;

                    while(next_tmp->free == 1)
                    {
                        if(next_tmp->next==NULL) break;

                        next_tmp = next_tmp->next;
                    }
                    if(next_tmp != tmp) {
                        next_tmp->prev = tmp;
                        next_tmp->hash = valid_struct_hush(next_tmp);
                        tmp->next = next_tmp;
                        tmp->hash = valid_struct_hush(tmp);
                    }
                    set_fences();
                    tmp->hash = valid_struct_hush(tmp);
                    return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
                }
                else
                {
                    char* res = heap_malloc(count);
                    if(res == NULL)
                        return NULL;

                    memcpy(res,(char*)tmp+sizeof(struct memory_chunk_t) + F_SIZE,tmp->size);

                    struct memory_chunk_t* next_tmp = tmp->next;

                    while(next_tmp->free == 1)
                    {
                        if(next_tmp->next==NULL) break;

                        next_tmp = next_tmp->next;
                    }
                    if(tmp->prev)
                    {
                        tmp = tmp->prev;
                        if(next_tmp != tmp) {
                            next_tmp->prev = tmp;
                            next_tmp->hash = valid_struct_hush(next_tmp);
                            tmp->next = next_tmp;
                            tmp->hash = valid_struct_hush(tmp);
                        }
                    }
                    return res;
                }

            }
            if(tmp==memory_manager.first_memory_chunk)
            {
                tmp->size = count;
                tmp->hash = valid_struct_hush(tmp);
                set_fences();
                return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
            }
            if(count <= last_block_size)
            {
                tmp->size = count;
                tmp->hash = valid_struct_hush(tmp);

                set_fences();
                return (char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE;
            }
            else
            {
                return NULL;
            }

        }
    }

    return NULL;
}


int size_of_heap(void)
{
    struct memory_chunk_t *tmp = memory_manager.first_memory_chunk;
    int counter = 0;
    while(tmp)
    {
        counter++;
        tmp=tmp->next;
    }
    return counter;
}
long valid_struct_hush(struct memory_chunk_t* chunk)
{
    long hash = 0;

    for (int i = 0; i < 32; i++)
    {
        hash += ((unsigned char*)chunk)[i];
    }

    return hash;
}
int how_many_free(void)
{
    struct memory_chunk_t *tmp = memory_manager.first_memory_chunk;
    int counter = 0;
    while(tmp)
    {
        if(tmp->free == 1)
            counter++;
        tmp=tmp->next;
    }
    return counter;
}
int heap_setup(void)
{
    memory_manager.memory_start = custom_sbrk(4096);

    memory_manager.memory_size = 4096;

    memory_manager.first_memory_chunk = NULL;

    if(memory_manager.memory_start == NULL) return -1;

    setup = 1;

    return 0;
}
void heap_clean(void)
{
    memset(memory_manager.memory_start,0,memory_manager.memory_size);

    custom_sbrk((intptr_t)(-memory_manager.memory_size));

    setup = 0;
}
void set_fences()
{
    for(struct memory_chunk_t* tmp = memory_manager.first_memory_chunk; tmp; tmp=tmp->next)
    {
        memset((char*)tmp + sizeof(struct memory_chunk_t),'#',F_SIZE);

        memset((char*)tmp + sizeof(struct memory_chunk_t) + F_SIZE + tmp->size,'#',F_SIZE);
    }
}
void join_chunks(struct memory_chunk_t *mem_to_free)
{
    if(size_of_heap() == 1 && mem_to_free == memory_manager.first_memory_chunk)
        return;

    struct memory_chunk_t* tmp = mem_to_free;
    size_t all_sizes=0;
    int how_many_blocks_to_free = 0;
    while(1)
    {
        if(tmp->free!=1)
        {
            if(tmp->prev)
                tmp=tmp->prev;
            break;
        }
        if(tmp->next==NULL)break;

        tmp = tmp->next;
    }
    while(1)
    {
        if(tmp->free != 1)
        {
            tmp=tmp->next;
            break;
        }
        all_sizes+=tmp->size;

        if(tmp->prev==NULL) break;

        tmp = tmp->prev;
        how_many_blocks_to_free++;
    }
    if(how_many_blocks_to_free > 1)
    {
        tmp->size = all_sizes + (how_many_blocks_to_free-1) * sizeof(struct memory_chunk_t) + (how_many_blocks_to_free-1)*2*F_SIZE;
        tmp->hash = valid_struct_hush(tmp);
    }
    else
    {
        tmp->size = (char*)tmp->next - (char*)tmp - sizeof(struct memory_chunk_t) - 2*F_SIZE;
        tmp->hash = valid_struct_hush(tmp);
    }

    struct memory_chunk_t* next_tmp = tmp;

    while(next_tmp->free == 1)
    {
        if(next_tmp->next==NULL) break;

        next_tmp = next_tmp->next;
    }
    if(next_tmp != tmp) {
        next_tmp->prev = tmp;
        next_tmp->hash = valid_struct_hush(next_tmp);
        tmp->next = next_tmp;
        tmp->hash = valid_struct_hush(tmp);
    }
}
int heap_validate(void)
{
    if(memory_manager.memory_start == NULL || setup == 0)
        return 2;

    char* ptr;

    for(struct memory_chunk_t* tmp = memory_manager.first_memory_chunk; tmp!=NULL; tmp=tmp->next)
    {
        if(valid_struct_hush(tmp) != tmp->hash)
            return 3;

        if(tmp->free == 1) continue;

        ptr = (void*)((char*)tmp + sizeof(struct memory_chunk_t));
        for(int i=0; i<F_SIZE; i++)
        {
            if(ptr[i]!='#')
                return 1;
        }
        ptr = (void*)((char*)tmp  + sizeof(struct memory_chunk_t) + F_SIZE + tmp->size);

        for(int i=0; i<F_SIZE; i++)
        {
            if(ptr[i]!='#')
                return 1;
        }

    }
    return 0;
}
size_t heap_get_largest_used_block_size(void)
{
    if(setup == 0)
        heap_setup();
    if(memory_manager.first_memory_chunk == NULL || heap_validate() != 0)
        return (size_t)0;

    size_t max=0;
    for(struct memory_chunk_t *tmp=memory_manager.first_memory_chunk; tmp!=NULL; tmp=tmp->next)
    {
        if(tmp->size>max && tmp->free != 1)
            max = tmp->size;
    }
    return max;
}
enum pointer_type_t get_pointer_type(const void* const pointer){
    if(pointer == NULL)
        return pointer_null;

    if(heap_validate() == 1 || heap_validate() == 2)
        return pointer_heap_corrupted;

    if(memory_manager.first_memory_chunk == NULL)
        return pointer_unallocated;

    struct memory_chunk_t *tmp = memory_manager.first_memory_chunk, *old;
    int flaga_pointer = 0;
    for(; tmp!=NULL; tmp = tmp->next)
    {
        old = tmp;
        if(tmp > (struct memory_chunk_t*)pointer)
        {
            tmp=tmp->prev;
            flaga_pointer = 1;
            break;
        }
        if((char*)tmp + F_SIZE + sizeof(struct memory_chunk_t) == pointer)
        {
            if(tmp->free == 1)
                return pointer_unallocated;
            else
                return pointer_valid;
        }
    }

    if(tmp == NULL)
    {
        if((char*)old + F_SIZE + sizeof(struct memory_chunk_t) + old->size + F_SIZE <= (char*)pointer)
            return pointer_unallocated;
    }

    if(flaga_pointer == 1)
    {
        old = tmp;
    }
    if((char*)old + sizeof(struct memory_chunk_t) + F_SIZE + old->size + F_SIZE > (char*)pointer)
    {
        if((char*)old + sizeof(struct memory_chunk_t) + F_SIZE + old->size > (char*)pointer)
        {
            if((char*)old + sizeof(struct memory_chunk_t) + F_SIZE > (char*)pointer)
            {
                if((char*)old + sizeof(struct memory_chunk_t)  > (char*)pointer)
                {
                    if(old->free == 1)
                        return pointer_unallocated;
                    else
                        return pointer_control_block;
                }
                if(old->free == 1)
                    return pointer_unallocated;
                else
                    return pointer_inside_fences;
            }
            if(old->free == 1)
                return pointer_unallocated;
            else
                return pointer_inside_data_block;
        }
        if(old->free == 1)
            return pointer_unallocated;
        else
            return pointer_inside_fences;
    }
    return pointer_unallocated;

}



