#ifndef CADMUXE_LWT_H
#define CADMUXE_LWT_H

#define lwt_STACK_SIZE (16*1024*8)
#define lwt_QUANTUM 100

typedef void (*lwt_f)(void *);

enum lwt_tag_states;
typedef enum lwt_tag_states lwt_states;

struct lwt_tag_thread;
typedef struct lwt_tag_thread lwt_thread;

struct lwt_tag_env;

struct lwt_tag_S;
typedef struct lwt_tag_S lwt_S;

void lwt_init();
lwt_thread *lwt_create(lwt_f func, void *argv);
void lwt_del(lwt_thread *t);
void lwt_run(lwt_thread *thread);
void lwt_sleep(int s);
int lwt_wait(lwt_thread *t);
void lwt_exit(int status);
lwt_S *lwt_createS(int s);      // create a semaphore
void lwt_P(lwt_S *s);
void lwt_V(lwt_S *s);
int lwt_getS(lwt_S *s);         // get the value of a semaphore, for test
void lwt_set_prior(lwt_thread *t, int prior);  // set the prior of a thread
int lwt_gettid();


#endif
