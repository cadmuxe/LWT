#include <stdio.h>
#include "lwt.h"

void fun(){
    printf("In thread: %d\n", lwt_gettid());
    printf("Sleep for 2 seconds.\n");
    lwt_sleep(2);
    lwt_exit(10);
}

int main(int argc, char *argv[]){
        int s;
        lwt_thread *t;
        lwt_init();
        t = lwt_create(fun, NULL);
        lwt_run(t);
        s = lwt_wait(t);
        printf("status for fun: %d\n", s);
        lwt_del(t);
        lwt_exit(0);
}
