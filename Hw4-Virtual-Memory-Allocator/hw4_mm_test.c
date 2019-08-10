#include "lib/hw_malloc.h"
#include "hw4_mm_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define KiB 1024
void test();
void init_heap();

typedef unsigned long long ull;
int main(int argc, char *argv[])
{
    init_heap();
    // printf("Start Address:%08llX\n", (ull)get_start_sbrk());
    char temp[30];
    while(fgets(temp, 30, stdin) != NULL) {
        char *choice = (char *)malloc(sizeof(char) * 30);
        strcpy(choice, temp);
        if(strstr(choice, "alloc ") != NULL) {
            strtok(choice, " ");
            char *num_str = strtok(NULL, " \0\n");
            ull num = strtoull(num_str, NULL, 0);
            // printf("malloc:%lld\n", num);
            ull addr = (ull)hw_malloc(num);
            printf("0x%012llx\n", addr - (ull)heap);
        } else if(strstr(choice, "free ") != NULL) {
            strtok(choice, " ");
            char *num_str = strtok(NULL, " \0\n");
            ull num = strtoull(num_str, NULL, 0);
            // printf("free:%llx\n", num);
            // fflush(stdout);
            printf("%s\n", (hw_free(num + (ull)heap) == 1? "success" : "fail"));
            // print_all_bins();
        } else if(strstr(choice, "print bin") != NULL) {
            strtok(choice, "[");
            char *num_str = strtok(NULL, "]");
            int bin = atoi(num_str);
            // printf("print bin[%d]:\n", bin);
            print_one_bin(bin);
        }
        //mmap_alloc_list
        else if(strstr(choice, "print mmap_alloc_list") != NULL) {
            print_mmap_alloc_list();
        } else if(strstr(choice, "print all") != NULL) {
            print_all_bins();
        } else {
            printf("Wrong command\n");
        }
        // else if(strstr(choice, "print mmap_alloc_list"))
        free(choice);
    }
    return 0;
}

void init_heap()
{
    heap = sbrk(64 * KiB);
    assign_chunk((chunk *)heap, 15, 0);
    Push(((chunk *)heap), 10);
    assign_chunk((chunk *)((ull)heap + 32 * KiB), 15, 15);
    Push((chunk *)((ull)heap + 32 * KiB), 10);
}

void test()
{
    printf("hw_malloc\n");
    ull address[4];
    printf("%llx\n", (address[0] = (ull)hw_malloc(16)) - (ull)heap);
    printf("%llx\n", (address[1] = (ull)hw_malloc(16)) - (ull)heap);
    printf("%llx\n", (address[2] = (ull)hw_malloc(16)) - (ull)heap);
    printf("%llx\n", (address[3] = (ull)hw_malloc(16)) - (ull)heap);
    printf("hw_free\n");
    printf("%s\n", (hw_free(address[0]) == 1? "success" : "fail"));
    printf("%s\n", (hw_free(address[1]) == 1? "success" : "fail"));
    printf("%s\n", (hw_free(address[2]) == 1? "success" : "fail"));
    printf("%s\n", (hw_free(address[3]) == 1? "success" : "fail"));
    printf("%s\n", (hw_free(34578) == 1? "success" : "fail"));
}