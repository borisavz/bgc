#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct mutator_info_t {
    bool stw_call;
    pthread_mutex_t stw_call_m;
} mutator_info;

void mutator_thread(mutator_info *info);
void safepoint(mutator_info *info);
void stw();

bool stw_end = false;
pthread_mutex_t stw_end_m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stw_end_c = PTHREAD_COND_INITIALIZER;

int safepoint_ack_count = 0;
pthread_mutex_t safepoint_m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t safepoint_c = PTHREAD_COND_INITIALIZER;

mutator_info mutators[5];

int main() {
    pthread_t threads[5];
    
    for(int i = 0; i < 5; i++) {
        mutators[i].stw_call = false;
        pthread_mutex_init(&mutators[i].stw_call_m, NULL);

        pthread_create(&threads[i], NULL, &mutator_thread, &mutators[i]);
    }

    sleep(1);

    stw();

    for(int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

void mutator_thread(mutator_info *info) {
    pthread_t self_thread = pthread_self();

    printf("enter mutator %ld\n", self_thread);
    printf("start mutator logic %ld\n", self_thread);
    sleep(10);
    printf("end mutator logic %ld\n", self_thread);
    safepoint(info);
    printf("exit mutator %ld\n", self_thread);
}

void safepoint(mutator_info *info) {
    pthread_mutex_lock(&info->stw_call_m);
    bool stw_call_copy = info->stw_call;
    pthread_mutex_unlock(&info->stw_call_m);

    pthread_mutex_lock(&safepoint_m);
    safepoint_ack_count++;
    pthread_cond_signal(&safepoint_c);
    pthread_mutex_unlock(&safepoint_m);

    if(stw_call_copy) {
        printf("start wait stw end");

        pthread_mutex_lock(&stw_end_m);
        while (stw_end == false) {
            pthread_cond_wait(&stw_end_c, &stw_end_m);
        }
        pthread_mutex_unlock(&stw_end_m);

        printf("end wait stw end");
    }
}

void stw() {
    printf("enter stw\n");

    pthread_mutex_lock(&stw_end_m);
    stw_end = false;
    pthread_mutex_unlock(&stw_end_m);

    printf("begin stw call\n");

    for(int i = 0; i < 5; i++) {
        pthread_mutex_lock(&mutators[i].stw_call_m);
        mutators[i].stw_call = true;
        pthread_mutex_unlock(&mutators[i].stw_call_m);
    }

    printf("end stw call\n");
    printf("begin wait safepoint\n");
    
    pthread_mutex_lock(&safepoint_m);
    while (safepoint_ack_count != 5) {
        pthread_cond_wait(&safepoint_c, &safepoint_m);
    }
    pthread_mutex_unlock(&safepoint_m);

    for(int i = 0; i < 5; i++) {
        pthread_mutex_lock(&mutators[i].stw_call_m);
        mutators[i].stw_call = false;
        pthread_mutex_unlock(&mutators[i].stw_call_m);
    }

    printf("end wait safepoint\n");
    printf("start stw logic\n");

    sleep(5);

    printf("end stw logic\n");

    pthread_mutex_lock(&stw_end_m);
    stw_end = true;
    pthread_cond_broadcast(&stw_end_c);
    pthread_mutex_unlock(&stw_end_m);

    printf("exit stw\n");
}