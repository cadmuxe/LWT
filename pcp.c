#include <stdio.h>
#include <time.h>
#include "lwt.h"
#define N 50

int size = N;
int current =0;
int buffer[N];

lwt_S *empty;
lwt_S *full;
lwt_S *lock;    // lock the buffer
void producter(){
    int item;
    while(1){
        item = clock();  // just generate somethin as the production.
        lwt_P(empty);
        lwt_P(lock);
        buffer[current] = item;
        current += 1;
        printf("Size:%d\n", current);
        lwt_V(lock);
        lwt_V(full);
        printf("Thread %d, product:\t%d\n", lwt_gettid(), item);
    }
}

void consumer(){
    int item;
    while(1){
        lwt_P(full);
        lwt_P(lock);
        current -=1;
        item = buffer[current];
        lwt_V(lock);
        lwt_V(empty);
        printf("Thread %d, consume:\t%d\n", lwt_gettid(), item);
    }

}

int main(int argc, char *argv[]){
    int i,j;
    lwt_thread *t;
    lwt_init();
    empty = lwt_createS(size);
    full = lwt_createS(0);
    lock = lwt_createS(1);
    for(i=0;i<4;i++){
        t = lwt_create(producter, NULL);
        lwt_run(t);
    }
    for(i=0; i<2;i++){
        t = lwt_create(consumer, NULL);
        lwt_run(t);
    }
    
    lwt_wait(t);
}



