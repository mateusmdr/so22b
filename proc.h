#ifndef PROC_H
#define PROC_H

#include <stdlib.h>
#include <sys/queue.h>
#include <stdbool.h>
#include "cpu_estado.h"
#include "mem.h"
#include "es.h"
#include "tab_pag.h"

// Processo
// Define a estrutura de um processo gerenciado pelo SO
// E as funções de manipulação desta estrutura

typedef struct {
    /** Variáveis auxiliares*/
    int hora_criacao;  // processo foi criado
    int hora_bloqueio; // processo foi bloqueado (E -> B)
    int hora_execucao; // processo começou a executar (P -> E)
    int hora_desbloqueio_preempcao; // processo foi desbloqueado ou perdeu a cpu (E/B -> P)
    int duracao_ultimo_bloqueio; // ultimo tempo (E -> B)
    bool foi_bloqueado; // caso o processo tenha ido para o estado P a partir do estado B, na última vez que perdeu a CPU

    /** Métricas do processo */
    int tempo_total;
    int tempo_bloqueado;
    int tempo_cpu;
    int tempo_espera;
    float tempo_medio_retorno;
    int bloqueios;
    int preempcoes;
} proc_metricas_t;

typedef struct proc_t {
    int id;
    int prog;
    cpu_estado_t* cpue;       // Estado da CPU do processo
    int disp;                 // Número do dispositivo (caso bloqueado por e/s)
    acesso_t acesso;          // Tipo de acesso (caso bloqueado por e/s)
    
    /** Valores utilizados pelos escalonadores */
    int quantum;
    float tempo_esperado;

    tab_pag_t* tab_pag;       // Tabela de páginas do processo

    proc_metricas_t metricas; // Métricas do processo

    STAILQ_ENTRY(proc_t) entries;
} proc_t;

typedef STAILQ_HEAD(proc_list_t, proc_t) proc_list_t;

// aloca um processo
proc_t* proc_cria(int id);

// desaloca um processo
void proc_destroi(proc_t* proc);

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

// verdadeiro se a lista está vazia
bool proc_list_empty(proc_list_t* self);

#endif