#include <sys/queue.h>
#include "proc.h"

proc_t* proc_cria(int id, cpu_estado_t* cpue, mem_t* mem, proc_estado_t estado) {
    proc_t* proc = (proc_t*) malloc(sizeof(proc_t));

    if (proc != NULL){
        proc->cpue = cpue;
        proc->mem = mem;
        proc->estado = estado;
        proc->id = id;
    }

    return proc;
}

void proc_destroi(proc_t* proc) {
    free(proc);
}

proc_list_t* proc_list_cria() {
    proc_list_t* self;
    SLIST_INIT(self);

    return self;
}

void proc_list_insere(proc_list_t* self, proc_t* el) {
    SLIST_INSERT_HEAD(self, el, entries);
}

void proc_list_remove(proc_list_t* self, proc_t* el) {
    SLIST_REMOVE(self, el, proc_t, entries);
}

proc_t* proc_list_encontra(proc_list_t* self, int id) {
    proc_t *it, *el = NULL;
    SLIST_FOREACH(it, self, entries)
        if(it->id == id) {
            el = it;
            break;
        };
    ;
    
    return el;
}