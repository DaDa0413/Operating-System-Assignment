#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include "simple_pstree.h"

#define NETLINK_USER 31

#define MAX_PAYLOAD 10000

int sock;
struct msghdr msg;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct sockaddr_nl src_addr, dest_addr;

int main(int argc, char* argv[])
{
    //socket
    if ((sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER)) == -1) {
        printf("open sock failed\n");
        return -1;
    }

    //sockaddr_nl for client
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); /* self pid */
    src_addr.nl_groups = 0; /* not in mcast groups */

    //bind
    if (bind(sock, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        printf("bind failed\n");
        close(sock);
        return -1;
    }

    //sockaddr_nl for Linux kernel
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;/* unicast */

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    /* Fill the netlink message header */
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid(); /* self pid */
    nlh->nlmsg_flags = 0;
    /* Fill in the netlink message payload */
    char *str;
    if (argc == 2) {
        char *str2 = (char *)malloc(sizeof(char) * 20);
        // choose(&str2, argv[1], getpid());
        choose(str2, argv[1], getpid());
        str = (char*)malloc(sizeof(char) * (strlen(str2) + 1));
        strcpy(str, str2);
        free(str2);
    } else {
        str = (char *)malloc(sizeof(char) * 4); //"a", " ", "1", "/0"
        strcpy(str, "c 1");
    }
    // printf("send \"%s\" to kernel\n", str);


    strcpy(NLMSG_DATA(nlh), str);

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(sock, &msg, 0) == -1) {
        printf("sendmsg error\n");
        free(str);
        close(sock);
        return -1;
    }

    // printf("Waiting for message from kernel\n");

    /* Read message from kernel */
    recvmsg(sock, &msg, 0);
    // printf("Received message payload:\n%s\n", (char*)NLMSG_DATA(msg.msg_iov->iov_base));
    printf("%s", (char*)NLMSG_DATA(msg.msg_iov->iov_base));

    free(str);
    close(sock);
    return 0;
}
