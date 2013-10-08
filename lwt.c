#include "lwt.h"
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

enum lwt_tag_states {
    lwt_READY = 0, lwt_RUNNING, thrd_WAIT, sem_WAIT, lwt_SLEEP, lwt_EXIT
};

struct lwt_tag_thread{
    jmp_buf env;
    char *stack;
    lwt_states state;
    char prior;     // priority, 0,1,2,3,4  default 2
    useconds_t time;       // totally cpu time
    struct lwt_tag_thread *next; // next thread
    int tid;
    lwt_f func;
    void *argv;
    void *result;
    int status;
    struct lwt_tag_thread *waited;    // waited by some thread.
    clock_t resume_time;    // when a thread is in lwt_SLEEP, it will be resumed when
                            // the process time reach resume_time
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
    int ntid;
    useconds_t interval;
};


struct lwt_tag_S{
    int s;
    int size;
    lwt_thread **queue;
};


void lwt_sighandler(int signo);
void lwt_exam_wait(lwt_thread *t);

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
    ualarm(lwt_QUANTUM, 0);
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
        /* 
         * More note at http://blog.chengh.com/2013/09/modify-stack-frame-of-c/
         */
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
        //printf("switch stack, running");
        #ifdef _X86
        __asm__("mov %0, %%esp;": :"r"(g_lwt_env.current->stack + lwt_STACK_SIZE) );
        #else
        __asm__("mov %0, %%rsp;": :"r"(g_lwt_env.current->stack + lwt_STACK_SIZE) );
        #endif
        //__asm__("mov $0x100054000, %rsp");
        // rsp is for x64, esp is for x86
        g_lwt_env.current->func(g_lwt_env.current->argv);
        g_lwt_env.current->state = lwt_EXIT;
        ualarm(0,0);        // reset and raise to the handler
        raise(SIGALRM);
        
    }
}

void lwt_sighandler(int signo){
    if(setjmp(g_lwt_env.current->env) == 0){
        g_lwt_env.current->time += g_lwt_env.interval;
        do{
            g_lwt_env.current = g_lwt_env.current->next;
            switch(g_lwt_env.current->state){
                case lwt_RUNNING:
                    break;
                case thrd_WAIT:         // the sem_WAIT will be changed on V operation.
                    // exam whether the thread waited by current is terminated.
                    // If yes, switch the curren to lwt_RUNNING
                    lwt_exam_wait(g_lwt_env.current);
                    break;
                case lwt_SLEEP:
                    if(clock() > g_lwt_env.current->resume_time)
                        g_lwt_env.current->state = lwt_RUNNING;
                    break;
            }
        }while(g_lwt_env.current->state != lwt_RUNNING);

        g_lwt_env.interval = lwt_QUANTUM;
        switch(g_lwt_env.current->prior){   // adjust the slice based on prior
            case 1:
                g_lwt_env.interval >> 1;
                break;
            case 2:
                break;
            case 3:
                g_lwt_env.interval << 2;
                break;
        }
        ualarm(g_lwt_env.interval, 0);
        // long jmp, 1 indicate that it's jumped, not return directly.
        longjmp(g_lwt_env.current->env, 1);
    }
    return;
}

void lwt_sleep(int s){
    g_lwt_env.interval -=ualarm(0,0);       // make sure the running time is correct
    g_lwt_env.current->state = lwt_SLEEP;
    g_lwt_env.current->resume_time = clock() + s*CLOCKS_PER_SEC; // when to resume
    raise(SIGALRM);
    return;
}
int lwt_wait(lwt_thread *t){
    g_lwt_env.interval -= ualarm(0,0);
    g_lwt_env.current->state = thrd_WAIT;
    t->waited = g_lwt_env.current;
    raise(SIGALRM);
    return t->status;;
}
void lwt_exit(int status){
    g_lwt_env.current->status = status;
    if(g_lwt_env.current == g_lwt_env.current->next)
        exit(status);
    g_lwt_env.current->state = lwt_EXIT;
}

int lwt_gettid(){
    return g_lwt_env.current->tid;
}

void lwt_exam_wait(lwt_thread *t){
    // find out which thred that t is waitting for.
    // check those thread
    lwt_thread *p;
    p = t; 
    do{
        p = p->next;
        if(p->waited == t){
            if(p->state == lwt_EXIT){ // if the thread, which t is waiting, finished, set t to lwt_RUNNING
                t->state = lwt_RUNNING;
                break;
            }
        }
    }while(p != t);
    return;
}
lwt_S *lwt_createS(int ss){
    lwt_S *s;
    s = malloc(sizeof(lwt_S));
    s->s=ss;
    s->size = 5;
    s->queue = malloc(sizeof(lwt_thread *) * s->size);
    return s;
}

void lwt_P(lwt_S *s){
    int left;
    left = ualarm(0,0);
    if(s->s >0){
        s->s -=1;
        ualarm(left,0);
    }
    else{
        s->s -=1;
        if ( s->size < abs(s->s)){
            s->size +=5;
            s->queue = realloc(s->queue, sizeof(lwt_thread *) * s->size);
        }
        g_lwt_env.current->state = sem_WAIT;
        s->queue[abs(s->s)-1] = g_lwt_env.current;
        raise(SIGALRM);
    }
    return;
}

void lwt_V(lwt_S *s){
    int left;
    left = ualarm(0,0);
    if(s->s >= 0){
        s->s +=1;
    }
    else{
        s->s +=1;
        s->queue[abs(s->s)]->state = lwt_RUNNING;
    }
    ualarm(left, 0);
    return;
}
int lwt_getS(lwt_S *s){
    return s->s;
}
void lwt_del(lwt_thread *t){
    lwt_thread *pre;
    pre = t;
    if(t == t->next)
        return;
    while(pre->next !=t){
        pre = pre->next;
    }
    pre->next = t->next;
    free(t);
    return;
}
void lwt_set_prior(lwt_thread *t, int prior){
    if(prior >=1 && prior <=3)
        t->prior = prior;
    else
        t->prior = 2;
}
