#include <sys/queue.h>
#include <string.h>
#include "mem.h"
#include "proc.h"
#include "cpu_estado.h"

// programas disponíveis
int progr1[] = {
  #include "p1.maq"
};

int progr2[] = {
  #include "p2.maq"
};

int *progrs[] = {
  progr1,
  progr2
};
int progrs_size[] = {
    sizeof(progr1),
    sizeof(progr2)
};

proc_t* proc_cria(int id, int mem_tam) {
    proc_t* proc = (proc_t*) malloc(sizeof(proc_t));

    if (proc != NULL){
        proc->mem = mem_cria(mem_tam);
        if(proc->mem == NULL) {
            free(proc);
            return NULL;
        }
        proc->cpue = cpue_cria();
        proc->estado = PRONTO;
        proc->id = id;
    }

    return proc;
}

void proc_destroi(proc_t* proc) {
    mem_destroi(proc->mem);
    cpue_destroi(proc->cpue);
    free(proc);
}

bool proc_inicializa(proc_t* proc, int prog_id) {
    if(prog_id > sizeof(progrs)-1 || prog_id < 0) {
        return false;
    }

    int* progr = progrs[prog_id];
    int tam_progr = progrs_size[prog_id]/sizeof(progr[0]);

    for (int i = 0; i < tam_progr; i++) {
        if (mem_escreve(proc->mem, i, progr[i]) != ERR_OK) {
            return false;
        }
    }

    return true;
}

proc_list_t* proc_list_cria() {
    proc_list_t* self = malloc(sizeof(proc_list_t));
    SLIST_INIT(self);

    return self;
}

void proc_list_destroi(proc_list_t* self) {
    while(!SLIST_EMPTY(self)) {
        proc_t* el = SLIST_FIRST(self);
        SLIST_REMOVE_HEAD(self, entries);
        proc_destroi(el);
    }
}

void proc_list_insere(proc_list_t* self, proc_t* el) {
    SLIST_INSERT_HEAD(self, el, entries);
}

void proc_list_remove(proc_list_t* self, proc_t* el) {
    SLIST_REMOVE(self, el, proc_t, entries);
}

proc_t* proc_list_encontra_id(proc_list_t* self, int id) {
    proc_t *it, *el = NULL;
    SLIST_FOREACH(it, self, entries){
        if(it->id == id) {
            el = it;
            break;
        }
    }
    
    return el;
}

proc_t* proc_list_encontra_estado(proc_list_t* self, proc_estado_t estado) {
    proc_t *it, *el = NULL;
    SLIST_FOREACH(it, self, entries){
        if(it->estado == estado) {
            el = it;
            break;
        }
    }
    
    return el;
}

void proc_list_percorre(proc_list_t* self, void(*fn)(proc_t* proc, ...)) {
    proc_t* el;
    SLIST_FOREACH(el, self, entries){
        fn(el);
    }
}