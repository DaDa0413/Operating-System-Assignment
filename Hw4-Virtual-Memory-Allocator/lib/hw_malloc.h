#ifndef HW_MALLOC_H
#define HW_MALLOC_H
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned long long ull;

struct chunk_info {
    unsigned int prev_chunk_size : 31;
    unsigned int allocated : 1;
    unsigned int curr_chunk_size : 31;
    unsigned int mmap : 1;
};

typedef struct _chunk {
    struct _chunk* prev;
    struct _chunk* next;
    struct chunk_info info;
} chunk;

//chunk operation
void assign_chunk(chunk *temp_chunk, int size, int prev_size);
void assign_mmap_chunk(chunk *temp_chunk, int size);
void divide_chunk(ull addr, int i);

//homework API
void *hw_malloc(size_t bytes);
int hw_free(ull mem);
void *get_start_sbrk(void);

//queue operation
void Push(chunk *data, int bin);
void Push_mmap(chunk * data);
ull Pop_lowest(int bin);
int Pop_specific(ull addr, int bin);
int Pop_mmap_specific(ull addr);
int isEmpty(int bin);
extern chunk* Q_HEAD[];
extern chunk* Q_TAIL[];
extern int Q_NUM[];

//printing operation
void print_all_bins();
void print_one_bin(int i);
void print_mmap_alloc_list();
//start position of heap
extern void* heap;
#endif
