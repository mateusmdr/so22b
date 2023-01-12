#ifndef PROC_H
#define PROC_H

#include <stdlib.h>
#include <sys/queue.h>
#include <stdbool.h>
#include "cpu_estado.h"
#include "mem.h"

// Processo
// Define a estrutura de um processo gerenciado pelo SO
// E as funções de manipulação desta estrutura

// Estado atual do processo
typedef enum {
    EXECUTANDO,
    PRONTO,
    BLOQUEADO
} proc_estado_t;

typedef struct proc_t {
    int id;
    int prog;
    cpu_estado_t* cpue;
    mem_t* mem; // Memória Principal
    proc_estado_t estado;
    int disp; // Número do dispositivo (caso bloqueado por e/s)

    SLIST_ENTRY(proc_t) entries;
} proc_t;

typedef SLIST_HEAD(proc_list_t, proc_t) proc_list_t;

// aloca um processo
proc_t* proc_cria(int id, int mem_tam, proc_estado_t estado);

// desaloca um processo
void proc_destroi(proc_t* proc);

// inicializa a memória de um processo com um programa (falso caso falhe)
bool proc_inicializa(proc_t* proc, int prog_id);

// inicializa uma lista de processos
proc_list_t* proc_list_cria();

// destroi uma lista de processos
void proc_list_destroi();

// insere um processo na lista
void proc_list_insere(proc_list_t* self, proc_t* el);

// remove um processo da lista
void proc_list_remove(proc_list_t* self, proc_t* el);

// retorna um processo da lista com o id fornecido, NULL caso não exista
proc_t* proc_list_encontra(proc_list_t* self, int id);

#endif