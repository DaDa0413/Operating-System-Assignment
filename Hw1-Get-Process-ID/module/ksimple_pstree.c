#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/pid.h>
#include <linux/list.h>
#include "ksimple_pstree.h"

#define NETLINK_USER 31
#define msg_size 10000
struct sock *nl_sk = NULL;

static void kernel_nl_recvmsg(struct sk_buff *skb)
{

    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int res;

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

    nlh = (struct nlmsghdr *)skb->data;
    printk(KERN_INFO "Netlink received msg payload:%s\n", (char *)nlmsg_data(nlh));
    pid = nlh->nlmsg_pid; /*pid of sending process */

    skb_out = nlmsg_new(msg_size, 0);                                   //to-be-modified
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

    //convert pid(from msg user sent) to int
    char *received_msg = (char *)nlmsg_data(nlh);
    int task_pid;
    if(kstrtoint( &(((char *)nlmsg_data(nlh))[2]), 10, &task_pid) != 0) {
        printk(KERN_ERR "Failed to convert string\n");
        return;
    }

    printk(KERN_INFO "%c %d\n", received_msg[0], task_pid);

    //TMD!!!!!!!     Savage!!!!!!!!
    struct task_struct *task;
    task = pid_task(find_get_pid(task_pid), PIDTYPE_PID);      //init

    char *msg = (char *)vmalloc(sizeof(char) * msg_size);   //new
    memset(msg, 0, msg_size);
    if(task == NULL) {
        printk(KERN_ERR "pid not found\n");
        // strcpy(msg, "pid not found\n");
    } else

    {
        //choose to print child, parent or sibling
        if(received_msg[0] == 'c') { //-c
            strcat(msg, task->comm);
            strcat(msg, "(");

            char str_temp[20];
            sprintf(str_temp, "%d", task_pid_nr(task));
            strcat(msg, str_temp);

            strcat(msg, ")\n");
            recursive_child_msg(msg, task, 1);
        } else if(received_msg[0] == 'p') //-s
            receive_parent_msg(msg, task);
        else if(received_msg[0] == 's')  //-p
            receive_sibling_msg(msg, task);
        else
            printk(KERN_ERR "Wrong received_msg\n");
    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);                         //to-be-modified
    strncpy(nlmsg_data(nlh), msg, msg_size);

    res = nlmsg_unicast(nl_sk, skb_out, pid);
    vfree(msg);

    if (res < 0)
        printk(KERN_INFO "Error while sending back to user\n");
}

static int __init kernel_init(void)
{

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

    struct netlink_kernel_cfg cfg = {
        .input = kernel_nl_recvmsg,
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ERR "Error creating socket.\n");
        return -10;
    }
    printk(KERN_INFO "init complete.\n");

    return 0;
}

static void __exit kernel_exit(void)
{

    printk(KERN_INFO "exiting kernel module\n");
    netlink_kernel_release(nl_sk);
}

module_init(kernel_init);
module_exit(kernel_exit);

MODULE_LICENSE("GPL");