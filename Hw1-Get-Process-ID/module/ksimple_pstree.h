#ifndef ktree
#define ktree
#include <linux/list.h>
#include <linux/sched.h>

char *recursive_child_msg(char *msg, struct task_struct *task, int seq)
{
    // printk(KERN_INFO "print child\n");
    struct list_head *listPtr = NULL;
    list_for_each(listPtr, &(task->children)) {
        int i;
        for (i = 0; i < seq; ++i)
            strcat(msg, "    ");

        //use of sibling is to locate the offset between the start and member
        struct task_struct *entry = list_entry(listPtr, struct task_struct, sibling);

        strcat(msg, entry->comm);

        char str_temp[20];
        sprintf(str_temp, "(%d)\n", task_pid_nr(entry));
        strcat(msg, str_temp);

        // printk(KERN_INFO "%s(%s)\n", entry->comm, str_temp);
        recursive_child_msg(msg, entry, seq + 1);
    }
    return msg;
}


char *receive_parent_msg(char *msg, struct task_struct *task)
{
    // printk(KERN_INFO "print parent\n");
    char **stack = (char **)vmalloc(sizeof(char*) * 10);

    int count = -1;
    struct task_struct *iter = task;
    stack[++count] = (char*)vmalloc(sizeof(char) * (strlen(task->comm) + 10));
    strcpy(stack[count], iter->comm);

    char str_temp[20];
    sprintf(str_temp, "(%d)\n", task_pid_nr(iter));
    strcat(stack[count], str_temp);

    /*if iter is not pointed to init
      iter points to its parent
      add its parent to stack*/
    while(task_pid_nr(iter) != 1 && task_pid_nr(iter) != 2) {
        iter = iter->parent;
        stack[++count] = (char*)vmalloc(sizeof(char) * (strlen(iter->comm) + 10));

        strcpy(stack[count], iter->comm);

        char str_temp[20];
        sprintf(str_temp, "(%d)\n", task_pid_nr(iter));
        strcat(stack[count], str_temp);
        // printk(KERN_INFO "%s(%s)\n", iter->comm, str_temp);
    }

    int i, j;
    for (i = count; i >= 0; --i) {
        for (j = 0; j < count - i; ++j) {
            strcat(msg, "    ");
        }
        strcat(msg, stack[i]);
    }
    return msg;
}

char *receive_sibling_msg(char *msg, struct task_struct *task)
{
    struct task_struct *parent = task->parent;

    struct list_head *listPtr = NULL;
    list_for_each(listPtr, &parent->children) {
        //use of sibling is to locate the offset between the start and member
        struct task_struct *entry = list_entry(listPtr, struct task_struct, sibling);

        if(task_pid_nr(entry) != task_pid_nr(task)) {
            strcat(msg, entry->comm);

            char str_temp[20];
            sprintf(str_temp, "(%d)\n", task_pid_nr(entry));
            strcat(msg, str_temp);
            // printk(KERN_INFO "%s(%s)\n", entry->comm, str_temp);
        }
    }
    return msg;
}
#endif