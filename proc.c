#include <sys/queue.h>
#include <string.h>
#include "proc.h"

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

void proc_destroi(proc_t* self) {
    tab_pag_destroi(self->tab_pag);
    mem_destroi(self->mem);
    cpue_destroi(self->cpue);
    free(self);
}