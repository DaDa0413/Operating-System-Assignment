#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

#define MAXLINE 10000
extern char *PORT_str;

void *DFS(void *data_from_parent)
{
    int sockfd;
    char *buffer = (char *)malloc(sizeof(char) * MAXLINE);
    char *first_address_of_buffer = buffer;
    char *query = (char *)data_from_parent;
    struct sockaddr_in servaddr;

    memset(buffer, 0, sizeof(char) * MAXLINE);

    //address family, protocol for TCP
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("client socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    int PORT = atoi(PORT_str);

    memset(&servaddr, 0, sizeof(servaddr));
    // Filling server information
    servaddr.sin_family = PF_INET;	//protocol family
    servaddr.sin_port = htons(PORT);	//adjust port accroding to Endian
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1) {
        printf("connect unsuccessful\n");
        exit(EXIT_FAILURE);
    }

    if(send(sockfd, (const char *)query, strlen(query), 0) == -1) {
        perror("send failed");
        exit(EXIT_FAILURE);
    }
    // printf("Query message sent.\n");
    // printf("query len:%d\n", strlen(query));
    // printf("send:%s\n", query);

    int n;
    if((n = recv(sockfd, (char *)buffer, MAXLINE, 0)) == -1) {
        perror("not normally receive");
        exit(EXIT_FAILURE);
    }

    printf("%s\n", (char *)buffer);
    //status:accepted
    if(strstr((char *)buffer, "HTTP/1.x 200 OK") != NULL) {
        //get a directory from server
        if(strstr((char *)buffer, "Content-Type:directory") != NULL) {

            //count how many file in dir, and copy then to array
            char *copy_buffer = (char *)malloc(sizeof(char) * (strlen((char *)buffer) + 1));
            memset(copy_buffer, 0, sizeof(char) * (strlen((char *)buffer) + 1));
            strcpy(copy_buffer, (char *)buffer);
            int number_of_query = 0;
            char *filename[100];
            char *copy_buffer_saveptr = NULL;
            //tricky solution= =, filter the first four lines
            __strtok_r(copy_buffer, "\n", &copy_buffer_saveptr);
            __strtok_r(NULL, "\n", &copy_buffer_saveptr);
            __strtok_r(NULL, "\n", &copy_buffer_saveptr);
            __strtok_r(NULL, "\n", &copy_buffer_saveptr);

            //count and push file in dir to array
            char *temp;
            while((temp = __strtok_r(NULL, " \r\n", &copy_buffer_saveptr)) != NULL)
                filename[number_of_query++] = temp;


            // printf("get how many file:%d\n", number_of_query);

            //cut filepath from query
            char *copy_query = (char *)malloc(sizeof(char) * (strlen(query) + 1));
            char *copy_query_saveptr = NULL;
            memset(copy_query, 0, sizeof(char) * (strlen(query) + 1));
            char *filepath;
            strcpy(copy_query, query);
            // printf("copy_query:%s\n", copy_query);
            __strtok_r(copy_query, " ", &copy_query_saveptr);
            filepath = __strtok_r(NULL, " ", &copy_query_saveptr);

            //create threads and send msgs
            pthread_t *thread_pool = (pthread_t *)malloc(sizeof(pthread_t) * number_of_query);
            // printf("filepath len:%d\n", strlen(filepath));
            // printf("filepath:%s\n", filepath);
            for (int i = 0; i < number_of_query; ++i) {
                // printf("------GET query start-----------\n%s\n", filename);
                char *DFS_query = (char *)malloc(sizeof(char) * (strlen(query) + 10));
                memset(DFS_query, 0, sizeof(char) * (strlen(query) + 10));
                strcpy(DFS_query, "GET ");
                strcat(DFS_query, filepath);

                if(filepath[strlen(filepath) - 1] == '/')
                    strcat(DFS_query, filename[i]);
                else {
                    strcat(DFS_query, "/");
                    strcat(DFS_query, filename[i]);
                }
                strcat(DFS_query, " HTTP/1.x\r\nHOST:127.0.0.1:");
                strcat(DFS_query, PORT_str);
                strcat(DFS_query, "\r\n\r\n");

                pthread_create(&(thread_pool[i]), NULL, DFS, DFS_query);
            }

            for (int i = 0; i < number_of_query; ++i)
                pthread_join(thread_pool[i], NULL);

            free(copy_buffer);
            copy_buffer = NULL;
            // free(copy_buffer_saveptr);
            free(copy_query);
            copy_query = NULL;
            // free(copy_query_saveptr);
        }
        //get a file from server
        else {
            // printf("-------------------\nwe found a file\n");
            char *filepath = (char *)malloc(sizeof(char) * (strlen(query) + 1));
            char *filepath_saveptr = NULL;
            memset(filepath, 0, sizeof(char) * (strlen(query) + 1));
            char *layer = (char *)malloc(sizeof(char) * (strlen(query) + 1));
            memset(layer, 0, sizeof(char) * (strlen(query) + 1));
            strcpy(filepath, query);
            strcpy(layer, "output");

            //if output not exists, create one
            struct stat st = {0};
            if (stat(layer, &st) == -1) {
                // printf("folder doesn't exist\n");
                if(mkdir(layer, S_IRWXU) == -1) {
                    printf("layer\n");
                    printf("create dir failed\n");
                }
            }
            //discard query til path start
            __strtok_r(filepath, "/", &filepath_saveptr);
            char *folder_or_file_name;
            while((folder_or_file_name = __strtok_r(NULL, "/ ", &filepath_saveptr)) != NULL) {
                //recusiver copy its name to layer
                //this is for creating layers of directories
                strcat(layer, "/");
                strcat(layer, folder_or_file_name);
                //find file name
                if(strchr(folder_or_file_name, '.')) {
                    // printf("%s\n", layer);
                    FILE *fp = fopen(layer, "w");
                    //tricky solution= =, filter the first three lines
                    char count = 4;
                    while(count--) {
                        buffer = strchr((char *)&buffer[1], '\n');
                    }

                    fprintf(fp, "%s", (char *)&buffer[1]);
                    fclose(fp);
                    break;
                }
                //find directory name
                else {
                    struct stat st = {0};
                    // printf("%s\n", layer);
                    if (stat(layer, &st) == -1) {
                        // printf("folder doesn't exist\n");
                        if(mkdir(layer, S_IRWXU) == -1) {
                            printf("create dir failed\n");
                        }
                    }
                }
                // printf("%s\n", layer);
            }
            // printf("%s\n---------------\n", layer);
            free(filepath);
            free(layer);

        }
    }
    //wrong status, I guess......do nothing here
    else {
        // printf("something wrong\n");
    }

    // free(data_from_parent);
    // data_from_parent = NULL;
    free(first_address_of_buffer);
    first_address_of_buffer = NULL;
    close(sockfd);
    pthread_exit(NULL);
}


#endif
