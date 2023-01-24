#include <sys/queue.h>
#include <string.h>
#include "mem.h"
#include "proc.h"
#include "cpu_estado.h"

// programas disponíveis
int progr0[] = {
#include "init.maq"
};

int progr1[] = {
#include "peq_es.maq"
};

int progr2[] = {
#include "grande_es.maq"
};

int progr3[] = {
#include "peq_cpu.maq"
};

int progr4[] = {
#include "grande_cpu.maq"
};

int *progrs[] = {
    progr0,
    progr1,
    progr2,
    progr3,
    progr4
};
int progrs_size[] = {
    sizeof(progr0),
    sizeof(progr1),
    sizeof(progr2),
    sizeof(progr3),
    sizeof(progr4)
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
        proc->id = id;
        proc->quantum = 0;
    }

    return proc;
}

void proc_destroi(proc_t* proc) {
    mem_destroi(proc->mem);
    cpue_destroi(proc->cpue);
    free(proc);
}

bool proc_inicializa(proc_t* proc, int prog_id) {
    if(prog_id > sizeof(progrs)/4-1 || prog_id < 0) {
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