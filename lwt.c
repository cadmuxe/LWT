#include "lwt.h"
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

enum lwt_tag_states {
    lwt_READY = 0, lwt_RUNNING, thrd_WAIT, sem_WAIT, lwt_SLEEP, lwt_EXIT, lwt_DEAD 
};

struct lwt_tag_thread{
    jmp_buf env;
    char *stack;
    lwt_states state;
    char prior;     // priority, 0,1,2,3,4  default 2
    int time;       // totally cpu time
    struct lwt_tag_thread *next; // next thread
    lwt_tid tid;
    lwt_f func;
    void *argv;
    lwt_tid waitfor;    // waitfor a thread terminate
};
struct lwt_tag_env{
    /* This struct contain all the necessary data that need by the lwt.
     * All the threads in a link-list, schedule function will use this
     * link-list.
     */

    //current threading, new thread will insert after it.
    lwt_thread *current;
        /* main threading, no different to others spawned therading, but it's the orginal "process".
         * it will handled by the lwt library too.
         * Since main is the first thread that inserted into the thread list, so it's also the head of list
         */
    lwt_thread *main;
    lwt_tid ntid;
};


struct lwt_tag_S{
    int s;
    void* queue[];
};

struct lwt_tag_env g_lwt_env;
void lwt_init(){
    lwt_thread *ptrt;
    if (signal(SIGALRM, lwt_sighandler) == SIG_ERR){
        printf("Can not handle SIGALRM\n");
        exit(EXIT_FAILURE);
    }
    ptrt = (lwt_thread *)malloc( sizeof(lwt_thread));
    if (ptrt == NULL){
        printf("Memory Error\n");
        exit(EXIT_FAILURE);
    }
    ptrt->stack = NULL;         // "main thread" will use the process's default stack
    ptrt->state = lwt_RUNNING;
    ptrt->prior = 2;            // default prior
    ptrt->time = clock() * 1000.0 / CLOCKS_PER_SEC;
    ptrt->next = ptrt;          // when initialized, it's the only thread
    ptrt->tid = 0;
    ptrt->func = NULL;
    ptrt->argv = NULL;
 
    g_lwt_env.current = ptrt;
    g_lwt_env.main = ptrt;
    g_lwt_env.ntid = 1;
    //ualarm(lwt_QUANTUM, lwt_QUANTUM);
    alarm(2);
    return ;
}
lwt_thread *lwt_create(lwt_f func, void *argv){
    lwt_thread *ptrt;
    ptrt = (lwt_thread *)malloc( sizeof(lwt_thread));
    if (ptrt == NULL){
        printf("Memory Error\n");
        exit(EXIT_FAILURE);
    }
    ptrt->stack = (char *)malloc( lwt_STACK_SIZE);
    if (ptrt->stack == NULL){
        printf("Memory Error\n");
        exit(EXIT_FAILURE);
    }
    ptrt->state = lwt_READY;
    ptrt->prior = 2;
    ptrt->time = 0;
    ptrt->tid = g_lwt_env.ntid;
    ptrt->func = func;
    ptrt->argv = argv;
    g_lwt_env.ntid += 1;
    
    ptrt->next = g_lwt_env.main->next;
    g_lwt_env.main->next = ptrt;
    return ptrt;
}
void lwt_run(lwt_thread *thread){
    void *p;
    if(setjmp(thread->env) == 0){
        /* create an environment for new thread(need to change stack)
         * in here, we sill in the current thread, so just return;
         * the new thread will be execuated when it have time slice.
         */
        thread->state = lwt_RUNNING;
        if(setjmp(g_lwt_env.current->env) ==0){
            g_lwt_env.current = thread;
            longjmp(thread->env, 1);
        }
        else
            return;
    }
    else {
        /* the return value from setjmp is not equal to 0, (should be 1,
         * because we will use longjmp(*.env, 1))
         * So we are in the new thread. 
         */
        // change the stack, stack increcre from high to low address,
        // but the malloc memory begin at low address.
        printf("switch stack, running");
        p = g_lwt_env.current->stack + lwt_STACK_SIZE; 
        __asm__("mov %0, %%rsp;": :"r"(g_lwt_env.current->stack + lwt_STACK_SIZE) );
        //__asm__("mov $0x100054000, %rsp");
        // rsp is for x64, esp is for x86
        g_lwt_env.current->func(g_lwt_env.current->argv);
    
        // some other work need to do.
    }
}

void lwt_sighandler(int signo){
    alarm(2);
    printf("Enter %d\n", g_lwt_env.current->tid);
    if(setjmp(g_lwt_env.current->env) == 0){
        g_lwt_env.current->time += lwt_QUANTUM;
        do{
            g_lwt_env.current = g_lwt_env.current->next;
        }
        while(g_lwt_env.current->state != lwt_RUNNING);
        // long jmp, 1 indicate that it's jumped, not return directly.
        longjmp(g_lwt_env.current->env, 1);
    }
    printf("Out %d\n", g_lwt_env.current->tid);
    return;
}




