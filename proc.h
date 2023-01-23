#ifndef PROC_H
#define PROC_H

#include <stdlib.h>
#include <sys/queue.h>
#include <stdbool.h>
#include "cpu_estado.h"
#include "mem.h"
#include "es.h"

// Processo
// Define a estrutura de um processo gerenciado pelo SO
// E as funções de manipulação desta estrutura

typedef struct proc_t {
    int id;
    int prog;
    cpu_estado_t* cpue;
    mem_t* mem; // Memória Principal
    int disp; // Número do dispositivo (caso bloqueado por e/s)
    acesso_t acesso; // Tipo de acesso (caso bloqueado por e/s)
    int quantum; // Valor utilizado pelo escalonador

    STAILQ_ENTRY(proc_t) entries;
} proc_t;

typedef STAILQ_HEAD(proc_list_t, proc_t) proc_list_t;

// aloca um processo
proc_t* proc_cria(int id, int mem_tam);

// desaloca um processo
void proc_destroi(proc_t* proc);

// inicializa a memória de um processo com um programa (falso caso falhe)
bool proc_inicializa(proc_t* proc, int prog_id);

// inicializa uma lista de processos
proc_list_t* proc_list_cria();

// destroi uma lista de processos
void proc_list_destroi(proc_list_t* self);

// insere um processo no início da lista
void proc_list_push_front(proc_list_t* self, proc_t* el);

// insere um processo no final da lista
void proc_list_push_back(proc_list_t* self, proc_t* el);

// remove um processo da lista
void proc_list_pop(proc_list_t* self, proc_t* el);

#endif