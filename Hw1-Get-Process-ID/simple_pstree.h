#ifndef tree
#define tree

#include <ctype.h>
#include <string.h>


void pid_behind(char *msg, char c, int pid, char **strr)
{
    char *str = *strr;
    if(c == 'c') {
        if(strlen(msg) == 2)
            strcat(str, "1");
        else
            strcat(str, &msg[2]);
    } else {
        if(strlen(msg) == 2) {
            char str_temp[10];
            sprintf(str_temp, "%d", pid);
            // printf("str_temp:%s\n", str_temp);
            strcat(str, str_temp);
        } else
            strcat(str, &msg[2]);
    }
}

void choose(char *result, char *msg, int pid)
{
    // char *result = *str;
    if(msg[0] == '-') {
        if(msg[1] == 'c') {
            strcpy(result, "c ");
        } else if(msg[1] == 's') {
            strcpy(result, "s ");
        } else if(msg[1] == 'p') {
            strcpy(result, "p ");
        } else {
            printf("inccorect choice\n");
            return;
        }
        pid_behind(msg, msg[1], pid, &result);
    } else if(isdigit(msg[0])) {
        strcpy(result, "c ");
        strcat(result, msg);
    } else {
        printf("inccorect choice\n");
        return;
    }
}

#endif