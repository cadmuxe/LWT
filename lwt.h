#ifndef CADMUXE_LWT_H
#define CADMUXE_LWT_H

#define lwt_STACK_SIZE (16*1024*8)
#define lwt_QUANTUM 100

typedef void (*lwt_f)(void *);
typedef int lwt_tid;

enum lwt_tag_states;
typedef enum lwt_tag_states lwt_states;

struct lwt_tag_thread;
typedef struct lwt_tag_thread lwt_thread;

struct lwt_tag_env;

struct lwt_tag_S;
typedef struct lwt_tag_S lwt_S;

void lwt_init();
lwt_thread *lwt_create(lwt_f func, void *argv);
void lwt_run(lwt_thread *thread);
void lwt_exit(lwt_thread *t);
void lwt_sleep(int ms);
void lwt_wait(lwt_thread *t);
void lwt_P(lwt_S *s);
void lwt_V(lwt_S *s);

void lwt_sighandler(int signo);
void lwt_context_save(lwt_thread *current);
void lwt_context_load(lwt_thread *target);


#endif
