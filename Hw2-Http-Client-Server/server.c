// Server side implementation of UDP client-server model
#include "server.h"

char *queue[queue_size];
int sock_queue[queue_size];
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int queueHead = 0;
int queueTail = 0;
int PORT = 0;
char *root = NULL;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int number_of_thread;

int main(int argc, char *argv[])
{
    // if(argc != 7 || !strcmp(argv[1], "-r") ||
    //     !strcmp(argv[3], "-p") || !strcmp(argv[5], "-n"))
    if(argc != 7) {
        printf("Wrong configuration\n");
        return 1;
    }
    root = (char*)malloc(sizeof(char) * strlen(argv[2]));
    if(!strcmp(argv[2], "/") || !strcmp(argv[2], "./"))
        strcpy(root, "");
    else {
        strcpy(root, "/");
        strcat(root, argv[2]);
    }
    PORT = atoi(argv[4]);
    number_of_thread = atoi(argv[6]);

    printf("PORT:%d Number of thread:%d\n", PORT, number_of_thread);
    printf("Root:%s\n", root);
    pthread_t main_thread;
    pthread_t *thread_pool = (pthread_t *)malloc(sizeof(pthread_t) * number_of_thread);
    int main_thread_number;
    int *thread_order = (int *)malloc(sizeof(int) * number_of_thread);

    // create threadpool
    for (int i = 0; i < number_of_thread; ++i) {
        thread_order[i] = pthread_create(&(thread_pool[i]), NULL, create_child_thread, NULL);
        if(thread_order[i] != 0)
            printf("thread %d wrong\n", thread_order[i]);
    }

    //create main thread
    main_thread_number = pthread_create(&main_thread, NULL, create_main_thread, NULL);
    if(main_thread_number != 0)
        printf("main_thread wrong %d\n", main_thread_number);

    pthread_join(main_thread, NULL);

    for (int i = 0; i < number_of_thread; ++i) {
        pthread_join(thread_pool[i], NULL);
    }
    free(root);
    root = NULL;
    printf("return from main_thread\n");
    return 0;
}
