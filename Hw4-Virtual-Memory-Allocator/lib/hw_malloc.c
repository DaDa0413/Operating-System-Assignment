#include "hw_malloc.h"
#define mmap_threshold 32768//32*KiB
void* heap = NULL;

void assign_chunk(chunk *temp_chunk, int size, int prev_size)
{
    //chunk link
    temp_chunk->prev = NULL;
    temp_chunk->next = NULL;
    //chunk info
    temp_chunk->info.prev_chunk_size = prev_size;
    temp_chunk->info.allocated = 0;
    temp_chunk->info.curr_chunk_size = size;
    temp_chunk->info.mmap = 0;
}

void assign_mmap_chunk(chunk *temp_chunk, int size)
{
    //chunk link
    temp_chunk->prev = NULL;
    temp_chunk->next = NULL;
    //chunk info
    temp_chunk->info.prev_chunk_size = 0;
    temp_chunk->info.allocated = 1;
    temp_chunk->info.curr_chunk_size = size;
    temp_chunk->info.mmap = 1;
}

void divide_chunk(ull addr, int i)
{
    //**first**
    ull front_half_addr = addr;
    int prev_size = ((chunk *)addr)->info.prev_chunk_size;
    assign_chunk((chunk *)front_half_addr, (i - 1 + 5), prev_size);
    Push((chunk *)front_half_addr, i - 1);
    //**second**
    ull rear_half_addr = addr + (1 << (i - 1 + 5));
    assign_chunk((chunk *)rear_half_addr, (i - 1 + 5), i - 1 + 5);
    Push((chunk *)rear_half_addr, i - 1);
    //adjust next chunk
    ull next_header_addr = addr + (1 << (i + 5));
    ((chunk *)next_header_addr)->info.prev_chunk_size = i - 1 + 5;

}

void merge(ull header_addr)
{
    chunk *header = (chunk *)header_addr;

    //biggest bin
    if(header->info.curr_chunk_size == 15)
        return;

    ull prev_header_addr = header_addr - (1 << header->info.prev_chunk_size);
    ull next_header_addr = header_addr + (1 << header->info.curr_chunk_size);

    chunk *prev_header = (chunk *)prev_header_addr;
    chunk *next_header = (chunk *)next_header_addr;

    if(header->info.prev_chunk_size != 0 &&
            prev_header->info.allocated == 0 &&
            prev_header->info.curr_chunk_size == header->info.curr_chunk_size) {
        // printf("merge:0x%08llx 0x%08llx\n", prev_header_addr - (ull)heap, header_addr - (ull)heap);
        // fflush(stdout);
        //adjust bin
        if(Pop_specific(prev_header_addr, prev_header->info.curr_chunk_size - 5) == 0)
            printf("pop prev_header error\n");
        if(Pop_specific(header_addr, header->info.curr_chunk_size - 5) == 0)
            printf("pop header error\n");
        prev_header->info.curr_chunk_size += 1;
        Push(prev_header, prev_header->info.curr_chunk_size - 5);
        //adjust next header
        next_header->info.prev_chunk_size = prev_header->info.curr_chunk_size;
        //reset header
        header->info.allocated = 0;
        //recursive
        merge(prev_header_addr);
    } else if(next_header_addr != (ull)heap + 64 * 1024 &&
              next_header->info.allocated == 0 &&
              header->info.curr_chunk_size == next_header->info.curr_chunk_size) {
        // printf("merge:0x%08llx 0x%08llx\n", header_addr - (ull)heap, next_header_addr - (ull)heap);
        // fflush(stdout);
        //adjust bin
        if(Pop_specific(header_addr, header->info.curr_chunk_size - 5) == 0)
            printf("pop header error\n");
        if(Pop_specific(next_header_addr, next_header->info.curr_chunk_size - 5) == 0)
            printf("pop next_header error\n");
        header->info.curr_chunk_size += 1;
        Push(header, header->info.curr_chunk_size - 5);
        //adjust next next header
        ull next_next_header_addr = next_header_addr + (1 << next_header->info.curr_chunk_size);
        if(next_next_header_addr != (ull)heap + 64 * 1024)
            ((chunk *)next_next_header_addr)->info.prev_chunk_size = header->info.curr_chunk_size;
        //reset next header
        next_header->info.allocated = 0;
        //recursive
        merge(header_addr);
    }
}

void *hw_malloc(size_t bytes)
{
    void *addr = NULL;
    if(bytes + 24 > mmap_threshold) {
        if((addr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == (void *)-1)
            perror("mmap");
        // printf("I got 0x%08llx\n", (ull)addr - (ull)heap);
        assign_mmap_chunk((chunk *)addr,bytes);
        Push_mmap((chunk *)addr);
    } else {
        int i;
        for (i = 0; i < 11; ++i)
            if((1 << (i + 5)) >= (bytes + sizeof(chunk)) && !isEmpty(i))
                break;
        while(1) {
            //i = 0 is the smallest block
            if((1 << (i - 1 + 5)) >= (bytes + sizeof(chunk)) && i != 0) {
                //cut bin[i] and modify header of bin[i] & bin[i - 1]
                ull to_be_divide_addr = Pop_lowest(i);
                //divide to 2 chunk
                divide_chunk(to_be_divide_addr, i);
                //next, please
                i--;
            } else {
                addr = (void *)Pop_lowest(i);
                //no need to adjsut chunk info
                ((chunk *)addr)->info.allocated = 1;
                break;
            }
        }
    }
    return (void *)((ull)addr + sizeof(chunk));
}

int hw_free(ull mem)
{
    ull header_addr = mem - sizeof(chunk);
    chunk *header = (chunk *)header_addr;
    if(header->info.curr_chunk_size > 32 * 1024) {
        if(Pop_mmap_specific(header_addr) == 0)
            return 0;
        if(munmap((void *)header_addr, header->info.curr_chunk_size) == -1) {
            perror("munmap:");
            return 0;
        }
    } else { //going to be modified to be else
        //fail to free
        if(header_addr < (ull)heap || header_addr > (ull)heap + 64 * 1024)
            return 0;
        else {
            header->info.allocated = 0;
            Push(header, header->info.curr_chunk_size - 5);
            merge(header_addr);
        }
    }
    return 1;
}

void *get_start_sbrk(void)
{
    return heap;
}

chunk* Q_HEAD[11];
chunk* Q_TAIL[11];
int Q_NUM[11] = {0};
chunk *mmap_alloc_list_HEAD = NULL;
chunk *mmap_alloc_list_TAIL = NULL;
int mmap_alloc_list_NUM;

void Push(chunk *data, int bin)
{
    //set Head and Tail
    chunk* HEAD = Q_HEAD[bin];
    chunk* TAIL = Q_TAIL[bin];
    int *NUM = &Q_NUM[bin];

    //Empty queue
    if(HEAD == NULL) {
        HEAD = data;
        HEAD->prev = HEAD;
        HEAD->next = HEAD;
        TAIL = HEAD;
    } else {
        data->prev = TAIL;
        data->next = HEAD;
        TAIL->next = data;
        HEAD->prev = data;
        TAIL = data;
    }
    (*NUM)++;
    // //test
    // printf("bin %d after Push:", bin);
    // chunk* ptr = HEAD;
    // if(HEAD != NULL) {
    //     do {
    //         printf("0x%08llx ", (ull)ptr - (ull)heap);
    //         ptr = ptr->next;
    //     } while(ptr != HEAD);
    // } else
    //     printf("Empty after pushing???\n");
    // printf("\n");
    // //end test

    //reset Head and Tail
    Q_HEAD[bin] = HEAD;
    Q_TAIL[bin] = TAIL;
}

void Push_mmap(chunk *data)
{
    //Empty queue
    if(mmap_alloc_list_HEAD == NULL) {
        mmap_alloc_list_HEAD = data;
        mmap_alloc_list_HEAD->prev = mmap_alloc_list_HEAD;
        mmap_alloc_list_HEAD->next = mmap_alloc_list_HEAD;
        mmap_alloc_list_TAIL = mmap_alloc_list_HEAD;
    } else {
        chunk *ptr = mmap_alloc_list_HEAD;
        if(ptr->info.curr_chunk_size < data->info.curr_chunk_size) {
            for (int i = 0; i < mmap_alloc_list_NUM; ++i, ptr = ptr->next)
                if(ptr->info.curr_chunk_size > data->info.curr_chunk_size)
                    break;
            ptr = ptr->prev;
            //change tail
            if(ptr == mmap_alloc_list_TAIL)
                mmap_alloc_list_TAIL = data;
            data->prev = ptr;
            data->next = ptr->next;
            ptr->next->prev = data;
            ptr->next = data;
        } else {
            data->prev = mmap_alloc_list_TAIL;
            data->next = mmap_alloc_list_HEAD;
            mmap_alloc_list_HEAD->prev = data;
            mmap_alloc_list_TAIL->next = data;
            //change head
            mmap_alloc_list_HEAD = data;
        }
    }
    mmap_alloc_list_NUM++;
    // //test
    // printf("mmap after Push:");
    // chunk* ptr = mmap_alloc_list_HEAD;
    // if(mmap_alloc_list_HEAD != NULL) {
    //     do {
    //         printf("0x%08llx ", (ull)ptr - (ull)heap);
    //         ptr = ptr->next;
    //     } while(ptr != mmap_alloc_list_HEAD);
    //     printf("\n");
    // } else
    //     printf("Empty after pushing???\n");
    // //end test
}

ull Pop_lowest(int bin)
{
    //check Empty
    if(isEmpty(bin))
        return 0;

    //set Head and Tail
    chunk* HEAD = Q_HEAD[bin];
    chunk* TAIL = Q_TAIL[bin];
    int *NUM = &Q_NUM[bin];

    ull MIN = ~0;
    chunk* ptr = HEAD;
    chunk* record = NULL;
    do {
        if((ull)ptr < MIN) {
            MIN = (ull)ptr;
            record = ptr;
        }
        ptr = ptr->next;
    } while(ptr != HEAD);

    if(record != NULL && MIN != ~0) {
        if(record == HEAD && record == TAIL) {
            HEAD = TAIL = NULL;
        } else {
            if(record == HEAD)
                HEAD = record->next;
            else if(record == TAIL)
                TAIL = record->prev;
            record->prev->next = record->next;
            record->next->prev = record->prev;
        }
        (*NUM)--;

        // //test
        // printf("bin %d after Pop:", bin);
        // chunk* ptr = HEAD;
        // if(HEAD != NULL) {
        //     do {
        //         printf("0x%08llx ", (ull)ptr - (ull)heap);
        //         ptr = ptr->next;
        //     } while(ptr != HEAD);
        // }
        // printf("\n");
        // //end test

    } else {
        printf("Nothing poped\n");
        return 0;
    }

    //reset Head and Tail
    Q_HEAD[bin] = HEAD;
    Q_TAIL[bin] = TAIL;
    return MIN;
}

int Pop_specific(ull addr, int bin)
{
    //check Empty
    if(isEmpty(bin))
        return 0;

    //set Head and Tail
    chunk* HEAD = Q_HEAD[bin];
    chunk* TAIL = Q_TAIL[bin];
    int *NUM = &Q_NUM[bin];

    chunk* ptr = HEAD;
    int found = 0;
    do {
        if((ull)ptr == addr) {
            found = 1;
            break;
        }
        ptr = ptr->next;
    } while(ptr != HEAD);

    if(found) {
        if(ptr == HEAD && ptr == TAIL) {
            HEAD = TAIL = NULL;
        } else {
            if(ptr == HEAD)
                HEAD = ptr->next;
            else if(ptr == TAIL)
                TAIL = ptr->prev;
            ptr->prev->next = ptr->next;
            ptr->next->prev = ptr->prev;
        }
        (*NUM)--;

        // //test
        // printf("bin %d after Pop:", bin);
        // chunk* ptr = HEAD;
        // if(HEAD != NULL) {
        //     do {
        //         printf("0x%08llx ", (ull)ptr - (ull)heap);
        //         ptr = ptr->next;
        //     } while(ptr != HEAD);
        // }
        // printf("\n");
        // //end test

    } else {
        printf("Nothing poped\n");
        return 0;
    }

    //reset Head and Tail
    Q_HEAD[bin] = HEAD;
    Q_TAIL[bin] = TAIL;
    return 1;
}

int Pop_mmap_specific(ull addr)
{
    //check Empty
    if(mmap_alloc_list_NUM == 0)
        return 0;

    chunk* ptr = mmap_alloc_list_HEAD;
    int found = 0;
    do {
        if((ull)ptr == addr) {
            found = 1;
            break;
        }
        ptr = ptr->next;
    } while(ptr != mmap_alloc_list_HEAD);

    if(found) {
        if(ptr == mmap_alloc_list_HEAD && ptr == mmap_alloc_list_TAIL) {
            mmap_alloc_list_HEAD = mmap_alloc_list_TAIL = NULL;
        } else {
            if(ptr == mmap_alloc_list_HEAD)
                mmap_alloc_list_HEAD = ptr->next;
            else if(ptr == mmap_alloc_list_TAIL)
                mmap_alloc_list_TAIL = ptr->prev;
            ptr->prev->next = ptr->next;
            ptr->next->prev = ptr->prev;
            fflush(stdout);
        }
        mmap_alloc_list_NUM--;

        // //test
        // printf("mmap after Pop:");
        // chunk* ptr = mmap_alloc_list_HEAD;
        // if(mmap_alloc_list_HEAD != NULL) {
        //     do {
        //         printf("0x%08llx ", (ull)ptr - (ull)heap);
        //         ptr = ptr->next;
        //     } while(ptr != mmap_alloc_list_HEAD);
        // }
        // printf("\n");
        // //end test

    } else {
        printf("Pop_mmap_specific not found\n");
        return 0;
    }
    return 1;
}

int isEmpty(int bin)
{
    int NUM = 0;
    NUM = Q_NUM[bin];

    if(NUM == 0) return 1;
    else return 0;
}

void print_all_bins()
{
    printf("********************\nprint all bins~\n");
    int from = 0, end = -1;
    for (int i = 0; i < 11; ++i) {
        from = end + 1;
        end = (1 << (i + 5)) - 1;
        printf("bin %d size:%d~%d:", i, from, end);
        chunk* ptr = Q_HEAD[i];
        if(Q_HEAD[i] != NULL) {
            do {
                // printf("hi\n");
                fflush(stdout);
                printf("0x%012llx ", (ull)ptr - (ull)heap);
                // printf("hi2\n");
                fflush(stdout);
                ptr = ptr->next;
            } while(ptr != Q_HEAD[i]);
        }
        printf("\n");
    }
    printf("********************\n");

}

void print_one_bin(int i)
{
    // printf("bin %d:", i);
    chunk* ptr = Q_HEAD[i];
    if(Q_HEAD[i] != NULL) {
        do {
            printf("0x%012llx--------%d\n", (ull)ptr - (ull)heap, (1 << ptr->info.curr_chunk_size));
            ptr = ptr->next;
        } while(ptr != Q_HEAD[i]);
    }
}

void print_mmap_alloc_list()
{
    // printf("mmap_alloc_list:");
    chunk* ptr = mmap_alloc_list_HEAD;
    if(mmap_alloc_list_HEAD != NULL) {
        do {
            printf("0x%012llx--------%d\n", (ull)ptr - (ull)heap, ptr->info.curr_chunk_size);
            ptr = ptr->next;
        } while(ptr != mmap_alloc_list_HEAD);
    }
}