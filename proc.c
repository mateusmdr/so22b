#include <sys/queue.h>
#include <string.h>
#include "mem.h"
#include "proc.h"
#include "cpu_estado.h"

proc_t* proc_cria(int id) {
    proc_t* proc = (proc_t*) malloc(sizeof(proc_t));

    if (proc == NULL) return NULL;

    proc->cpue = cpue_cria();
    proc->id = id;
    proc->prog = -1;
    cpue_muda_modo(proc->cpue, usuario);
    

    return proc;
}

void proc_destroi(proc_t* proc) {
    cpue_destroi(proc->cpue);
    free(proc);
}

proc_list_t* proc_list_cria() {
    proc_list_t* self = malloc(sizeof(proc_list_t));
    STAILQ_INIT(self);

    return self;
}

void proc_list_destroi(proc_list_t* self) {
    while(!STAILQ_EMPTY(self)) {
        proc_t* el = STAILQ_FIRST(self);
        STAILQ_REMOVE_HEAD(self, entries);
        proc_destroi(el);
    }
    free(self);
}

void proc_list_push_front(proc_list_t* self, proc_t* el) {
    STAILQ_INSERT_HEAD(self, el, entries);
}

void proc_list_push_back(proc_list_t* self, proc_t* el) {
    STAILQ_INSERT_TAIL(self, el, entries);
}

void proc_list_pop(proc_list_t* self, proc_t* el) {
    STAILQ_REMOVE(self, el, proc_t, entries);
}

bool proc_list_empty(proc_list_t* self) {
    return STAILQ_EMPTY(self);
}