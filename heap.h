//
// Created by wikto on 04.12.2022.
//

#ifndef INC_2_5_FUNC_H
#define INC_2_5_FUNC_H

#define F_SIZE 4
#define PAGE_SIZE 4096

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

struct memory_manager_t
{
    void *memory_start;
    size_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    int free;
    long hash;
};

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);

void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);

int how_many_free(void);
int size_of_heap(void);
void set_fences();
void join_chunks(struct memory_chunk_t *mem_to_free);
long valid_struct_hush(struct memory_chunk_t* chunk);
void  heap_free(void* memblock);
int heap_validate(void);
int heap_setup(void);
void heap_clean(void);
size_t   heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);

#endif //INC_2_5_FUNC_H
