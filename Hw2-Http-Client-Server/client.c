// Client side implementation of UDP client-server model
#define _POSIX_C_SOURCE 200112L

#include "client.h"
char *PORT_str = NULL;

// Driver code
int main(int argc, char *argv[])
{
    // char *query = "GET / HTTP/1.x\r\nHOST:127.0.0.1:8080\r\n\r\n";
    // char *query = "GET /secfolder/ HTTP/1.x\r\nHOST:127.0.0.1:8080\r\n\r\n";
    // char *query = "GET /secfolder/db.json HTTP/1.x\r\nHOST:127.0.0.1:8080\r\n\r\n";
    // char *query = "GET /secfolder/trifolder HTTP/1.x\r\nHOST:127.0.0.1:8080\r\n\r\n";
    // char *query = "GET /example.html HTTP/1.x\r\nHOST:127.0.0.1:8080\r\n\r\n";

    if(argc != 7) {
        printf("wrong configuration\n");
        return 1;
    }
    char *path = argv[2];
    char *port = argv[6];
    char query[100];
    memset(query, 0, sizeof(query));
    strcpy(query, "GET ");
    strcat(query, path);
    strcat(query, " HTTP/1.x\r\nHOST:127.0.0.1:");
    strcat(query, port);
    strcat(query, "\r\n\r\n");

    PORT_str = (char *)malloc(sizeof(char) * strlen(argv[6]));
    strcpy(PORT_str, argv[6]);
    pthread_t DFS_thread;
    pthread_create(&DFS_thread, NULL, DFS, query);
    pthread_join(DFS_thread, NULL);

    // printf("return from DFS_thread\n");
    free(PORT_str);
    return 0;
}
