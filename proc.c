#include <sys/queue.h>
#include <string.h>
#include "mem.h"
#include "proc.h"
#include "cpu_estado.h"

int progr0[] = {
#include "programas/benchmark_full.maq"
};

int progr1[] = {
#include "programas/benchmark_es.maq"
};

int progr2[] = {
#include "programas/benchmark_cpu.maq"
};

int progr3[] = {
#include "programas/grande_es_t0.maq"
};

int progr4[] = {
#include "programas/grande_es_t1.maq"
};

int progr5[] = {
#include "programas/peq_es_t2.maq"
};

int progr6[] = {
#include "programas/peq_es_t3.maq"
};

int progr7[] = {
#include "programas/grande_cpu_t4.maq"
};

int progr8[] = {
#include "programas/grande_cpu_t5.maq"
};

int progr9[] = {
#include "programas/peq_cpu_t6.maq"
};

int progr10[] = {
#include "programas/peq_cpu_t7.maq"
};

int progr11[] = {
#include "programas/p1.maq"
};

int progr12[] = {
#include "programas/p2.maq"
};

// programas disponíveis
int *progrs[] = {
    progr0,
    progr1,
    progr2,
    progr3,
    progr4,
    progr5,
    progr6,
    progr7,
    progr8,
    progr9,
    progr10,
    progr11,
    progr12
};
int progrs_size[] = {
    sizeof(progr0),
    sizeof(progr1),
    sizeof(progr2),
    sizeof(progr3),
    sizeof(progr4),
    sizeof(progr5),
    sizeof(progr6),
    sizeof(progr7),
    sizeof(progr8),
    sizeof(progr9),
    sizeof(progr10),
    sizeof(progr11),
    sizeof(progr12)
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