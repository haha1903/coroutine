#include "coroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;

struct schedule {
    int nco;
    int cap;
    struct coroutine **co;
};

struct coroutine {
    coroutine_func func;
    void *ud;
    ucontext_t main;
    ucontext_t ctx;
    struct schedule * sch;
    ptrdiff_t cap;
    ptrdiff_t size;
    int status;
    char stack[STACK_SIZE];
};

struct coroutine * 
_co_new(struct schedule *S , coroutine_func func, void *ud) {
    struct coroutine * co = malloc(sizeof(struct coroutine));
    co->func = func;
    co->ud = ud;
    co->sch = S;
    co->cap = 0;
    co->size = 0;
    co->status = COROUTINE_READY;
    return co;
}

void
_co_delete(struct coroutine *co) {
    if(co->status == COROUTINE_END)
	free(co);
}

struct schedule * 
coroutine_open(void) {
    struct schedule *S = malloc(sizeof(struct schedule));
    S->nco = 0;
    S->cap = DEFAULT_COROUTINE;
    S->co = malloc(sizeof(struct coroutine *) * S->cap);
    memset(S->co, 0, sizeof(struct coroutine *) * S->cap);
    return S;
}

void 
coroutine_close(struct schedule *S) {
    int i;
    for (i=0;i<S->cap;i++) {
	struct coroutine * co = S->co[i];
	if (co) {
	    free(co);
	}
    }
    free(S->co);
    S->co = NULL;
    free(S);
}

int 
coroutine_new(struct schedule *S, coroutine_func func, void *ud) {
    struct coroutine *co = _co_new(S, func , ud);
    if (S->nco >= S->cap) {
	int id = S->cap;
	S->co = realloc(S->co, S->cap * 2 * sizeof(struct coroutine *));
	memset(S->co + S->cap , 0 , sizeof(struct coroutine *) * S->cap);
	S->co[S->cap] = co;
	S->cap *= 2;
	++S->nco;
	return id;
    } else {
	int i;
	for (i=0;i<S->cap;i++) {
	    int id = (i+S->nco) % S->cap;
	    if (S->co[id] == NULL) {
		S->co[id] = co;
		++S->nco;
		return id;
	    }
	}
    }
    assert(0);
    return -1;
}

static void
mainfunc(uint32_t low32, uint32_t hi32, int id) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    struct schedule *S = (struct schedule *)ptr;
    struct coroutine *C = S->co[id];
    C->func(S,id,C->ud);
    C->status = COROUTINE_END;
    S->co[id] = NULL;
    --S->nco;
}

void 
coroutine_resume(struct schedule * S, int id) {
    assert(id >=0 && id < S->cap);
    struct coroutine *C = S->co[id];
    if (C == NULL)
	return;
    int status = C->status;
    switch(status) {
    case COROUTINE_READY:
	getcontext(&C->ctx);
	C->ctx.uc_stack.ss_sp = C->stack;
	C->ctx.uc_stack.ss_size = STACK_SIZE;
	C->ctx.uc_link = &C->main;
	C->status = COROUTINE_RUNNING;
	uintptr_t ptr = (uintptr_t)S;
	makecontext(&C->ctx, (void (*)(void)) mainfunc, 3, (uint32_t)ptr, (uint32_t)(ptr>>32), id);
	swapcontext(&C->main, &C->ctx);
	_co_delete(C);
	break;
    case COROUTINE_SUSPEND:
	C->status = COROUTINE_RUNNING;
	swapcontext(&C->main, &C->ctx);
	_co_delete(C);
	break;
    default:
	assert(0);
    }
}

void
coroutine_yield(struct schedule * S, int id) {
    assert(id >= 0);
    struct coroutine * C = S->co[id];
    C->status = COROUTINE_SUSPEND;
    swapcontext(&C->ctx , &C->main);
}

int 
coroutine_status(struct schedule * S, int id) {
    assert(id>=0 && id < S->cap);
    if (S->co[id] == NULL) {
	return COROUTINE_DEAD;
    }
    return S->co[id]->status;
}
