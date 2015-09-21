#include "coroutine.h"
#include <stdio.h>
#include <pthread.h>

struct args {
    int n;
};

struct schedule * S;

static void* sp(int i) {
    sleep(i);
    int j;
    for(j = 0;j < i; j++)
	printf("\n");
}

static void*
resume_callback(void *i) {
    int id = *((int *)i);
    sp(5);
    coroutine_resume(S, id);
    free(i);
}

static void
start_io(int co) {
    pthread_t tid;
    int *s = malloc(sizeof(*s));
    *s = co;
    pthread_create(&tid, NULL, resume_callback, s);
}

static void
eat(struct schedule *S, int id) {
    start_io(id);
    coroutine_yield(S, id);
    printf("eat cake = %d\n", id);
}

static void
test(struct schedule *S) {
    while(1) {
	sp(3);
	int id = coroutine_new(S, eat, NULL);
	printf("product cake id = %d\n", id);
	coroutine_resume(S, id);
	printf("product next\n");
    }
}

int 
main() {
    printf("start\n");
    S = coroutine_open();
    test(S);
    coroutine_close(S);
    printf("end\n");	
    return 0;
}
