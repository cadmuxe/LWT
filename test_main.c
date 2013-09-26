#include "lwt.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void run(void *);
void run1(void *);

int main(int argc, char *argv[]){
    lwt_thread *t;
    lwt_init();
    t = lwt_create(run, NULL);
    lwt_run(t);
    while(1){
        printf("Main!\n");
    }
}
void run(void *argv){
    while(1){
        printf("Run\n");
    }
}
void run1(void * f){
    run(NULL);
}
