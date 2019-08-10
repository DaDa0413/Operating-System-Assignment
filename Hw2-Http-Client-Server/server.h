#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>

// #define PORT     8080
#define MAXLINE 10000
#define queue_size 20

extern char *queue[queue_size];
extern int sock_queue[queue_size];
extern pthread_mutex_t mutex1;
extern int queueHead;
extern int queueTail;
extern int PORT;
extern char *root;
extern pthread_cond_t cond;

struct extn {
    char *file;
    char *content;
};
struct extn extensions[] = {
    {"htm", "text/html"},
    {"html", "test/html"},
    {"css", "text/css"},
    {"h", "text/x-h"},
    {"hh", "text/x-h"},
    {"c", "text/x-c"},
    {"cc", "text/x-c"},
    {"json", "application/json"},
    {0, 0}
};

enum {
    OK = 0,
    BAD_REQUEST,
    NOT_FOUND,
    METHOD_NOT_ALLOWED,
    UNSUPPORT_MEDIA_TYPE
};

const int status_code[] = {
    200,	//OK
    400,	//Bad Request
    404,	//Not Found
    405,	//Method Not Allowed
    415	//Unsupported Media Type
};


int return_status(char *recv_msg, char *query)
{
    int status = 0;
    char *recv_msg_saveptr = NULL;
    char *method = __strtok_r(recv_msg, " ", &recv_msg_saveptr);
    strcpy(query, root);
    char *pathname_without_root = __strtok_r(NULL, " ", &recv_msg_saveptr);
    strcat(query, pathname_without_root);
    // free(recv_msg_saveptr);

    char *redundant = (char *)malloc(sizeof(char) * (strlen(query) + 1));
    char *redundant_saveptr = NULL;
    strcpy(redundant, query);
    __strtok_r(redundant, ".", &redundant_saveptr);
    char *filetype = __strtok_r(NULL, "\0/", &redundant_saveptr);
    free(redundant);
    // free(redundant_saveptr);
    redundant = NULL;

    if(query[strlen(query) - 1] == '/')
        query[strlen(query) - 1] = '\0';
    // printf("root query:%s\n", query);
    // printf("filetype:%s\n", filetype);

    //BAD_REQUEST
    if(query[0] != '/') {
        status = BAD_REQUEST;
        printf("%s %d\n", "BAD_REQUEST", 400);
    } else
        strcpy(query, &query[1]);

    //METHOD_NOT_ALLOWED
    if(!status && strcmp(method, "GET")) {
        status = METHOD_NOT_ALLOWED;
        printf("%s %d\n", "METHOD_NOT_ALLOWED", 405);
    }

    //UNSUPPORT_MEDIA_TYPE
    char flag = 0;
    if(!status && filetype != NULL) {
        for (int i = 0; i <= 7; ++i)
            if(!strcmp(extensions[i].file, filetype))
                flag = 1;

        if(!flag) {
            status = UNSUPPORT_MEDIA_TYPE;
            printf("%s %d\n", "UNSUPPORT_MEDIA_TYPE", 415);
        }
    }

    return status;
}
//before msg send to here, it has to be initialize with '\0'!!!!!
void DFS(char *pathname, char *send_msg)
{
    struct stat sb;
    strcpy(send_msg, "HTTP/1.x ");

    //we found a directory
    if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode)) {

        strcat(send_msg, "200 OK\r\nContent-Type:directory\r\nServer: httpserver/1.x\r\n\r\n");

        DIR *d;
        struct dirent *dir = NULL;
        d = opendir(pathname);
        if (d)
            while ((dir = readdir(d)) != NULL) {
                if((strcmp(dir->d_name, ".") == 0) || (strcmp(dir->d_name, "..") == 0))
                    continue;
                strcat(send_msg, dir->d_name);
                strcat(send_msg, " ");
            }
        strcat(send_msg, "\r\n");
        closedir(d);
    }
    //we found a file
    else if(stat(pathname, &sb) == 0 && S_ISREG(sb.st_mode)) {

        strcat(send_msg, "200 OK\r\nContent-Type:");
        for (int i = 0; i <= 7; ++i) {
            char comp[7];
            sprintf(comp, ".%s", extensions[i].file);
            if(strstr(pathname, comp) != NULL) {
                strcat(send_msg, extensions[i].content);
                break;
            }
        }
        strcat(send_msg, "\r\nServer: httpserver/1.x\r\n\r\n");
        FILE *fp = fopen(pathname, "r");
        char buff[1000];
        while(fgets(buff, 100, fp) != NULL)
            strcat(send_msg, buff);

        fclose(fp);
    } else if(stat(pathname, &sb)!= 0) {
        strcat(send_msg, "404 NOT_FOUND");
        strcat(send_msg, "\r\nContent-Type:\r\nServer: httpserver/1.x\r\n\r\n");
    }
    return;
}

//if I do nothing inside while, child stuck
void *create_child_thread()
{
    int recv_sock;
    char *recv_msg;
    printf("hello from child thread\n");
    char send_msg[1000];

    while(1) {
        // printf("");

        //critical section
        pthread_mutex_lock(&mutex1);
        while(queueTail <= queueHead) {
            pthread_cond_wait(&cond, &mutex1);
        }
        ++(queueHead);
        //copy msg
        recv_msg = (char*)malloc(sizeof(char) * (strlen(queue[queueHead % queue_size]) + 1));
        memset(recv_msg, 0, sizeof(char) * (strlen(queue[queueHead % queue_size]) + 1));
        strcpy(recv_msg, queue[queueHead % queue_size]);

        printf("----------------\nGet %d\n", queueHead);
        printf("Receive len:%d\n", (int)strlen(recv_msg));
        printf("Receive msg:%s\n", recv_msg);

        //copy socket
        recv_sock = sock_queue[queueHead % queue_size];
        //free resources and delete number
        free(queue[queueHead % queue_size]);
        queue[queueHead % queue_size] = NULL;
        sock_queue[queueHead % queue_size] = 0;
        pthread_mutex_unlock(&mutex1);
        //end critical section

        char *query = (char *)malloc(sizeof(char) * 100);
        memset(query, 0, sizeof(char) * 100);
        int status = return_status(recv_msg, query);
        printf("Status:%d\n", status);
        printf("Query%s\n", query);

        memset(send_msg, 0, sizeof(send_msg));
        if(status == 0) {
            DFS(query, send_msg);
        } else {
            strcpy(send_msg, "HTTP/1.x ");
            if(status == BAD_REQUEST)
                strcat(send_msg, "400 BAD_REQUEST");
            else if(status == METHOD_NOT_ALLOWED)
                strcat(send_msg, "405 METHOD_NOT_ALLOWED");
            else if(status == UNSUPPORT_MEDIA_TYPE)
                strcat(send_msg, "415 UNSUPPORT_MEDIA_TYPE");
            else
                printf("How can status code be wrong\n");
            strcat(send_msg, "\r\nContent-Type:\r\nServer: httpserver/1.x\r\n\r\n");
        }
        if(send(recv_sock, send_msg, strlen(send_msg),0) == -1)
            printf("send msg error\n");

        //free resources
        free(recv_msg);
        recv_msg = NULL;
        free(query);
        query = NULL;
    }
    close(recv_sock);
    pthread_exit(NULL);
}

void *create_main_thread()
{

    int sockfd, sock_for_client;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, recvCliaddr;
    socklen_t recv_len = sizeof(recvCliaddr);

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        printf("server socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&recvCliaddr, 0, sizeof(recvCliaddr));

    // Filling server information
    servaddr.sin_family    = PF_INET; //  protocol Family
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
        printf("bind failed\n");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);
    while(1) {
        sock_for_client = accept(sockfd, (struct sockaddr *)&recvCliaddr, &recv_len);
        memset(buffer, 0, sizeof(buffer));
        recv(sock_for_client, (char *)buffer, MAXLINE, 0);

        //critical section
        pthread_mutex_lock(&mutex1);
        ++(queueTail);
        //push buffer to queue
        queue[queueTail % queue_size] = (char *)malloc(sizeof(char) * (strlen((char *)buffer) + 1));
        memset(queue[queueTail % queue_size], 0, sizeof(char) * (strlen((char *)buffer) + 1));
        strcpy(queue[queueTail % queue_size], (char *)buffer);
        //push socket to queue
        sock_queue[queueTail % queue_size] = sock_for_client;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex1);
    }

    close(sockfd);
    pthread_exit(NULL);
}

#endif
