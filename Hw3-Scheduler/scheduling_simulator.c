#include "scheduling_simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ucontext.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

#ifdef _LP64
 	#define STACK_SIZE 2097152+16384 /* large enough value for AMODE 64 */
#else
 	#define STACK_SIZE 16384  /* AMODE 31 addressing*/
#endif


void find_next();
static void alr_handler(int signo);
void shell_function();
static void ctrlz_signalhdr(int);
void return_function();
typedef struct _task
{
	ucontext_t task_context;
	int task_id;
	int task_status;
	char task_priority;
	char task_name[8];
	int task_suspend_time;
	int task_time_quantum;
	int task_queueing_time;
}task;

ucontext_t return_context;
ucontext_t shell;
ucontext_t ctrlz_context;
task child_list[10000];
static volatile int child_count = 0;
//current running thread number
static volatile int running = 0;
struct itimerval it;
struct itimerval it_zero;
//input info
char input_priority = 'L';
char input_time_quantum = 'S';
//calculate queueing time
static volatile int time_quantum_count = 0;

//**********************************************************************
//Queue section*********************************************************
struct node{
    int data;
    struct node* next;
    struct node* prev;
};

struct node* HQ_HEAD = NULL;
struct node* HQ_TAIL = NULL;
struct node* LQ_HEAD = NULL;
struct node* LQ_TAIL = NULL;
struct node* WQ_HEAD = NULL;
struct node* WQ_TAIL = NULL;

int HQ_NUM = 0;
int LQ_NUM = 0;
int WQ_NUM = 0;

void Push(int data, char priority);
int Pop(int id, char priority);
int isEmpty(char priority);
//END of queue section**************************************************
//**********************************************************************


//**********************************************************************
//Four API**************************************************************
void hw_suspend(int msec_10)
{
	int self = running;
	child_list[self].task_suspend_time = msec_10;
	child_list[self].task_status = TASK_WAITING;

	Pop(self, child_list[self].task_priority);
	//set running  to prev
	find_next();
	Push(self, 'W');

	swapcontext(&child_list[self].task_context, &child_list[running].task_context);
	return;
}

void hw_wakeup_pid(int pid)
{
	if(child_list[pid].task_status != TASK_WAITING)
		printf("Wake wrong id\n");
	else
	{
		Pop(pid, 'W');
		Push(pid, child_list[pid].task_priority);
		child_list[pid].task_status = TASK_READY;	
		child_list[pid].task_suspend_time = 0;
	}
	return;
}

int hw_wakeup_taskname(char *task_name)
{
	int count = 0;
	//remember to set suspend time to 0
	for (int i = 1; i <= child_count; ++i)
	{
		if(child_list[i].task_status == TASK_WAITING
		   && !strcmp(child_list[i].task_name, task_name)
		   && child_list[i].task_suspend_time > 0)
		{
			++count;
			Pop(i, 'W');
			Push(i, child_list[i].task_priority);
			child_list[i].task_status = TASK_READY;
			child_list[i].task_suspend_time = 0;
		}
	}
    return count;
}

int hw_task_create(char *task_name)
{
    ++child_count;

    printf("Create:%s %d\n", task_name, child_count);
    //We can not add other priority
    //But if we did, it will be in waiting queue
    //defualt Wait
    if(input_priority != 'H' && input_priority != 'L')
    	input_priority = 'L';

    //We can not add other priority
    //But if we did, it will be small time quantum
    if (input_time_quantum != 'L' && input_time_quantum != 'S')
	    input_time_quantum = 'S';

    //SET child list
    //priority, id,  name
    child_list[child_count].task_priority = input_priority;
    child_list[child_count].task_id = child_count;
    child_list[child_count].task_time_quantum = (input_time_quantum == 'L'? 2 : 1);
    child_list[child_count].task_queueing_time = 0;
    strcpy(child_list[child_count].task_name, task_name);

    //SET status
    //default no waiting status 
    child_list[child_count].task_status = TASK_READY;
    child_list[child_count].task_suspend_time = 0;
    
    int id = child_count;

	//allocate task context
	getcontext(&child_list[id].task_context);
	if ((child_list[id].task_context.uc_stack.ss_sp = (char *) malloc(STACK_SIZE)) != NULL) 
	{
  		child_list[id].task_context.uc_stack.ss_size = STACK_SIZE;
	    child_list[id].task_context.uc_stack.ss_flags = 0;
	    child_list[id].task_context.uc_link = &return_context;
	    if(sigprocmask(SIG_UNBLOCK, &(child_list[id].task_context.uc_sigmask), NULL) != 0)
	    	printf("reset uc_sigmask error\n");
		errno = 0;
	}
  	else 
  	{
    	perror("not enough storage for stack");
    	abort();
  	}

  	if (!strcmp(task_name, "task1"))
  		makecontext(&child_list[id].task_context, task1,0);
  	else if(!strcmp(task_name, "task2"))
  		makecontext(&child_list[id].task_context, task2,0);
	else if(!strcmp(task_name, "task3"))
  		makecontext(&child_list[id].task_context, task3,0);
  	else if(!strcmp(task_name, "task4"))
  		makecontext(&child_list[id].task_context, task4,0);
  	else if(!strcmp(task_name, "task5"))
  		makecontext(&child_list[id].task_context, task5,0);
  	else if(!strcmp(task_name, "task6"))
  		makecontext(&child_list[id].task_context, task6,0);
  	else
  	{
  		printf("\"%s\" not found\n", task_name);
  		child_list[id].task_status = TASK_TERMINATED;
  		return -1;
  	}
    Push(id, child_list[child_count].task_priority);

    input_priority = 'L';
    input_time_quantum = 'S';

    return id; // the id of created task name
}
//End of four API*******************************************************
//**********************************************************************
//----------------------------------------------------------------------

int main(void) 
{
	// signal(SIGALRM, alr_handler);    //timer
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = alr_handler;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	sigaddset(&act.sa_mask, SIGTSTP);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, NULL) < 0)
		return 0;

	// signal(SIGTSTP, ctrlz_signalhdr); //ctrlz	
	struct sigaction act2;
	memset(&act, 0, sizeof(act));
	act2.sa_handler = ctrlz_signalhdr;
	sigemptyset(&act2.sa_mask);
	sigaddset(&act2.sa_mask, SIGTSTP);
	sigaddset(&act2.sa_mask, SIGALRM);
	act2.sa_flags = 0;
	if (sigaction(SIGTSTP, &act2, NULL) < 0)
		return 0;

	//10 ms timer
	memset(&it, 0, sizeof(it));
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 10000;
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 10000;
	if (setitimer(ITIMER_REAL, &it, 0)) {
		perror("setitimer");
		exit(1);
	}

	//zero timer
	memset(&it_zero, 0, sizeof(it_zero));
	it_zero.it_interval.tv_usec = 0;
    it_zero.it_interval.tv_sec = 0;
    it_zero.it_value.tv_usec = 0;
    it_zero.it_value.tv_sec = 0;
	printf("Set setitimer completed\n");

  	//set terminate function
 	getcontext(&return_context);
 	if ((return_context.uc_stack.ss_sp = (char *) malloc(STACK_SIZE)) != NULL) 
 	{
   		return_context.uc_stack.ss_size = STACK_SIZE;
 	    return_context.uc_stack.ss_flags = 0;
 	    return_context.uc_link = &child_list[0].task_context;
 		errno = 0;
 		makecontext(&return_context, return_function,0);
 	}
   	else 
   	{
     	perror("not enough storage for stack");
     	abort();
   	}

   	//shell context
	getcontext(&shell);
	if ((shell.uc_stack.ss_sp = (char *) malloc(STACK_SIZE)) != NULL) 
	{
  		shell.uc_stack.ss_size = STACK_SIZE;
	    shell.uc_stack.ss_flags = 0;
	    shell.uc_link = &return_context;
		errno = 0;
		makecontext(&shell, shell_function,0);
	}
  	else 
  	{
    	perror("not enough storage for stack");
    	abort();
  	}

 	getcontext(&child_list[0].task_context);
 	printf("Set context completed\n");

 	//infinite loop
 	while(1);
 
  	printf("Main return\n");
  	return 0;
}

void find_next()
{
	//High priority first
    if(!isEmpty('H'))
    {
    	running = HQ_HEAD->data;
    }
    else if(!isEmpty('L'))
    {
		running = LQ_HEAD->data;
    }
    else
    {
    	// printf("No current available task\n");
    	running = 0;
    }
    //reset time quantum
    time_quantum_count = 0;
}

/*currently not used*/
// void find_prev(int current)
// {
// 	printf("find prev\n");
// 	//assing running to number in prev of running
// 	if(child_list[current].task_priority == 'H')
// 	{
// 		if(HQ_NUM >= 2)
// 		{
// 			if (current == HQ_HEAD->data)
// 				running = HQ_TAIL->data;
// 			else
// 			{
// 				struct node* ptr = HQ_HEAD;
// 				while(ptr != NULL && ptr->data != current)
// 					ptr = ptr->next;
// 				if(ptr != NULL)
// 					running = ptr->prev->data;
// 				else
// 					running = 0;
// 			}
// 		}
// 		else
// 		{
// 			if(!isEmpty('L'))
// 				running = LQ_HEAD->data;
// 			else
// 				running = 0;
// 		}
// 	}
// 	else if(child_list[current].task_priority == 'L')
// 	{
// 		if(LQ_NUM >= 2)
// 		{
// 			if (current == LQ_HEAD->data)
// 				running = LQ_TAIL->data;
// 			else
// 			{
// 				struct node *ptr = LQ_HEAD;
// 				while(ptr != NULL && ptr->data != current)
// 					ptr = ptr->next;
// 				if(ptr != NULL)
// 					running = ptr->prev->data;
// 				else
// 					running = 0;
// 			}
// 		}
// 		else
// 		{
// 			if(!isEmpty('H'))
// 				running = HQ_HEAD->data;
// 			else
// 				running = 0;
// 		}
// 	}
// 	else
// 		printf("WTF priority\n");
// 	time_quantum_count = 0;
// }

static void alr_handler(int signo)
{
	//increase queueing time
	for (int i = 1; i <= child_count; ++i)
		if(child_list[i].task_status == TASK_READY && child_list[i].task_id != running)
			++child_list[i].task_queueing_time;
	
	// printf("----------(Alarm triggered)-----------\n");

	//decrease task suspend time
	struct node* ptr = WQ_HEAD;
    while(ptr != NULL)
    {
    	int index = ptr->data;
    	if(child_list[index].task_status != TASK_WAITING)
    		printf("Error checking:%d not waiting\n", index);
    	--child_list[index].task_suspend_time;
    	//if it's time to wake up, then wake up
    	if(child_list[index].task_suspend_time <= 0)
    	{
    		child_list[index].task_status = TASK_READY;
    		child_list[index].task_suspend_time = 0;
    		Pop(child_list[index].task_id, 'W');
    		Push(child_list[index].task_id, child_list[index].task_priority);
    		printf("wake %d\n", index);
    	}
    	ptr = ptr->next;
    }

    int prev_run;
	// find next only when time quantum expire
	++time_quantum_count;
	if(time_quantum_count >= child_list[running].task_time_quantum)
	{
		prev_run = running;
		//push to the latest one
		if(running != 0)
		{
			Pop(running, child_list[running].task_priority);
			Push(running, child_list[running].task_priority);
		}
		find_next();
		//we should not swap context if they are the same
		if(prev_run != running)
			swapcontext(&child_list[prev_run].task_context, &child_list[running].task_context);
	}
}

void shell_function()
{
	//Stop timer
    if(setitimer(ITIMER_REAL, &it_zero, NULL) < 0)
	   	printf("Stop timer error.\n");  

	printf("Shell mode:\n");

	//Read command
	char command[10];
	while(1)
	{
		printf("$ ");
    	scanf("%s", command);
	    if(!strcmp(command, "add"))
	    {
	    	//default
	    	input_priority = 'L';
	    	input_time_quantum = 'S';

	    	char str[50];
	    	memset(str, 0, sizeof(str));
	    	char tt[5], pp[5];
	    	char name[10];
	    	fgets(str, 50, stdin);
	    	if(strstr(str, "-t") == NULL && strstr(str, "-p") == NULL)
	    		sscanf(str, "%s", name);
    		else if(strstr(str, "-t") == NULL && strstr(str, "-p") != NULL)
    			sscanf(str, "%s %s %c", name, pp, &input_priority);
    		else if(strstr(str, "-t") != NULL && strstr(str, "-p") == NULL)
    			sscanf(str, "%s %s %c", name, tt, &input_time_quantum);
    		else
    			sscanf(str, "%s %s %c %s %c", name, tt, &input_time_quantum, pp, &input_priority);

	    	hw_task_create(name);
	    }
	    else if(!strcmp(command, "remove"))
	    {
	    	//Input target id
	    	int id;
	    	scanf("%d", &id);
	    	

	    	char priority_to_remove;
	    	priority_to_remove = 
	    			(child_list[id].task_status == TASK_WAITING? 'W' : child_list[id].task_priority);

	    	if(Pop(id, priority_to_remove) == -1)
	    		printf("Pop nothing\n");

	    	//Change status
	    	child_list[id].task_status = TASK_TERMINATED;

	    	find_next();
	    }
	    else if(!strcmp(command, "start"))
	    {
		    //resume timer
			if (setitimer(ITIMER_REAL, &it, 0)) 
			{
				perror("Resume timer fail");
				exit(1);
			}
	    	
	    	break;
	    }
	    else if(!strcmp(command, "ps"))
	    {
	    	for (int i = 1; i <= child_count; ++i)
	    	{
	    		printf("%d %s ", i, child_list[i].task_name);
	    		switch(child_list[i].task_status)
	    		{
	    			case TASK_RUNNING:
	    				printf("TASK_RUNNING ");
	    				break;
	    			case TASK_READY:
	    				printf("TASK_READY ");
	    				break;
	    			case TASK_WAITING:
	    				printf("TASK_WAITING ");
	    				break;
	    			case TASK_TERMINATED:
	    				printf("TASK_TERMINATED ");
	    				break;
	    			default:
	    				printf("Wrong\n");
	    				break;
	    		}

	    		printf("%d ", 10 * (child_list[i].task_queueing_time));
	    		printf("%c ", child_list[i].task_priority);
	    		printf("%c\n", (child_list[i].task_time_quantum == 2 ? 'L' : 'S'));
	    		// printf("%d\n", child_list[i].task_suspend_time);
	    	}

	    }
	    else
	    	printf("wrong command\n");
	}
	setcontext(&ctrlz_context);

}

static void ctrlz_signalhdr(int a)
{
	swapcontext(&ctrlz_context, &shell);
	printf("simulating....\n");
}


void return_function()
{

  	printf("Child:%d terminate~~~~\n", running);
  	int prev_run;
	prev_run = running;

  	if(Pop(prev_run, child_list[prev_run].task_priority) == -1)
  		printf("(Error)Child%d terminate but pop nothing\n", running);
	//set the running to proper value
	//coz I turn running to the prev, I should resume it
	find_next();

  	child_list[prev_run].task_status = TASK_TERMINATED;

   	swapcontext(&child_list[prev_run].task_context, &child_list[running].task_context);
}

void Push(int data, char priority)
{
	struct node* Q_HEAD = NULL;
	struct node* Q_TAIL = NULL;
	int *Q_NUMptr;
	//set Head and Tail
	if(priority == 'H')
	{
		Q_HEAD = HQ_HEAD;
		Q_TAIL = HQ_TAIL;
		Q_NUMptr = &HQ_NUM;
	}
	else if(priority == 'L')
	{
		Q_HEAD = LQ_HEAD;
		Q_TAIL = LQ_TAIL;
		Q_NUMptr = &LQ_NUM;
	}
	else if(priority == 'W')
  	{
  		Q_HEAD = WQ_HEAD;
		Q_TAIL = WQ_TAIL;
		Q_NUMptr = &WQ_NUM;	
  	}
  	else
  		printf("Push: wrong priority\n");
  
  	//Empty queue
    if(Q_HEAD == NULL)
    {
        Q_HEAD = (struct node*)malloc(sizeof(struct node));
        Q_HEAD->data = data;
        Q_HEAD->prev = NULL;
        Q_HEAD->next = NULL;
        Q_TAIL = Q_HEAD;
    }
    else
    {
        struct node* ptr = (struct node*)malloc(sizeof(struct node));
        ptr->data = data;
        ptr->prev = Q_TAIL;
        ptr->next = NULL;
        Q_TAIL->next = ptr;
        Q_TAIL = ptr;
    }
    (*Q_NUMptr)++;
    // //test
    // printf("%c after Push:", priority);
    // struct node* ptr = Q_HEAD;
    // while(ptr != NULL)
    // {
    // 	printf("%d ", ptr->data);
    // 	ptr = ptr->next;
    // }
    // printf("\n");
    // //end test

    //reset Head and Tail
    if(priority == 'H')
	{
		HQ_HEAD = Q_HEAD;
		HQ_TAIL = Q_TAIL;
	}
	else if(priority == 'L')
	{
		LQ_HEAD = Q_HEAD;
		LQ_TAIL = Q_TAIL;
	}
	else if(priority == 'W')
  	{
  		WQ_HEAD = Q_HEAD;
		WQ_TAIL = Q_TAIL;
  	}
}

int Pop(int id, char priority)
{
	//check Empty
	if(isEmpty(priority))
		return -1;

	struct node* Q_HEAD = NULL;
	struct node* Q_TAIL = NULL;
	int *Q_NUMptr;
	//set Head and Tail
	if(priority == 'H')
	{
		Q_HEAD = HQ_HEAD;
		Q_TAIL = HQ_TAIL;
		Q_NUMptr = &HQ_NUM;
	}
	else if(priority == 'L')
	{
		Q_HEAD = LQ_HEAD;
		Q_TAIL = LQ_TAIL;
		Q_NUMptr = &LQ_NUM;
	}
	else if(priority == 'W')
  	{
  		Q_HEAD = WQ_HEAD;
		Q_TAIL = WQ_TAIL;
		Q_NUMptr = &WQ_NUM;	
  	}
  	else
  		printf("Pop: wrong priority\n");
 
    struct node* ptr;
    for (ptr = Q_HEAD; ptr != NULL; ptr = ptr->next)
 	{
 		if(ptr->data == id)
 			break;
 	}
 	int result;
 	if(ptr != NULL)
 	{
	    result = ptr->data;

	    if(ptr != Q_HEAD)
	    	ptr->prev->next = ptr->next;
	    else
	    	Q_HEAD = ptr->next;
	    if(ptr != Q_TAIL)
	    	ptr->next->prev = ptr->prev;
	    else
	    	Q_TAIL = ptr->prev;
	    free(ptr);
	    (*Q_NUMptr)--;
	}
	else
	{
		printf("Nothing poped\n");
		result = -1;
	}

	//reset Head and Tail
	if(priority == 'H')
	{
		HQ_HEAD = Q_HEAD;
		HQ_TAIL = Q_TAIL;
	}
	else if(priority == 'L')
	{
		LQ_HEAD = Q_HEAD;
		LQ_TAIL = Q_TAIL;
	}
	else if(priority == 'W')
  	{
  		WQ_HEAD = Q_HEAD;
		WQ_TAIL = Q_TAIL;
  	}

  	// //test
  	// printf("%c after Pop:", priority);
   //  ptr = Q_HEAD;
   //  while(ptr != NULL)
   //  {
   //  	printf("%d ", ptr->data);
   //  	ptr = ptr->next;
   //  }
   //  printf("\n");
    // //end test

    return result;
}

int isEmpty(char priority)
{
	int Q_NUM = 0;
	if(priority == 'H')
		Q_NUM = HQ_NUM;
	else if(priority == 'L')
		Q_NUM = LQ_NUM;
	else if(priority == 'W')
		Q_NUM = WQ_NUM;
  	else
  		printf("isEmpty: wrong priority\n");
 
    if(Q_NUM == 0) return 1;
    else return 0;
}