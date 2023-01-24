#include "so.h"
#include "tela.h"
#include "proc.h"
#include "rel.h"
#include <stdlib.h>
#include <sys/queue.h>
#include <stdio.h>

typedef enum {
  ROUND_ROBIN,
  SHORTEST
} escalonador_t;

/**
 * Tabela de processos do SO
*/

typedef struct {
  proc_list_t* bloqueados;
  proc_list_t* prontos;
  proc_t* atual;          // processo atual em execução (NULL caso nenhum)
  int max_pid;            // último id de processo gerado
  escalonador_t escalonador; // tipo de escalonador a ser utilizado
} tab_proc_t;

typedef struct {
  /**Auxiliares*/
  int hora_inicio;
  int hora_bloqueio;
  int hora_desbloqueio;

  /**Métricas computadas*/ 
  int tempo_total;
  int tempo_cpu;
  int tempo_parado;
  int interrupcoes;
  int sisops;
} metricas_so_t;

struct so_t {
  contr_t *contr;            // o controlador do hardware
  bool paniquei;             // apareceu alguma situação intratável
  cpu_estado_t *cpue;        // cópia do estado da CPU
  rel_t *rel;                // referência do relógio do controlador
  tab_proc_t processos;      // tabela de processos do SO
  metricas_so_t metricas;    // métricas do SO
};

#define MAX_QUANTUM 50

// funções auxiliares
static void panico(so_t *self);
static proc_t* so_cria_processo(so_t *self, int prog);
static void so_finaliza_processo(so_t *self, proc_t* proc);
static void so_bloqueia_processo(so_t *self, proc_t* proc);
static void so_desbloqueia_processo(so_t *self, proc_t* proc);
static proc_t* so_encontra_first(so_t *self);
static proc_t* so_encontra_shortest(so_t *self);
static void so_carrega_processo(so_t *self, proc_t* proc);
static void so_salva_processo(so_t *self, proc_t* proc);
static void so_resolve_es(so_t* self);
static void so_escalona(so_t* self);
static void so_ativa_cpu(so_t* self);
static void so_desativa_cpu(so_t* self);
static void so_imprime_metricas(so_t* self);
// static void so_imprime_metricas_processo(so_t* self, proc_t* proc);

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->rel = contr_rel(self->contr);
  /**Cria tabela de processos*/
  self->processos.bloqueados = proc_list_cria();
  self->processos.prontos = proc_list_cria();
  self->processos.atual = NULL;
  self->processos.max_pid = 0;
  self->processos.escalonador = ROUND_ROBIN;
  /**Inicializa métricas do sistema*/
  self->metricas.hora_inicio = rel_agora(self->rel);
  self->metricas.interrupcoes = 0;
  self->metricas.sisops = 0;
  self->metricas.tempo_total = 0;
  self->metricas.tempo_cpu = 0;
  self->metricas.tempo_parado = 0;

  proc_t* proc = so_cria_processo(self, 0);
  so_carrega_processo(self, proc);

  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  proc_list_destroi(self->processos.bloqueados);
  proc_list_destroi(self->processos.prontos);
  free(self);
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

// chamada de sistema para leitura de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_le(so_t *self)
{
  proc_t* atual = self->processos.atual;
  atual->disp = cpue_A(self->cpue);
  atual->acesso = leitura;

  so_bloqueia_processo(self, atual);
}

// chamada de sistema para escrita de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_escr(so_t *self)
{
  proc_t* atual = self->processos.atual;
  atual->disp = cpue_A(self->cpue);
  atual->acesso = escrita;
  
  so_bloqueia_processo(self, atual);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  so_finaliza_processo(self, self->processos.atual);
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  int prog = cpue_A(self->cpue);
  so_cria_processo(self, prog);
  // incrementa o PC
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  self->metricas.sisops++;
  so_chamada_t chamada = cpue_complemento(self->cpue);
  switch (chamada) {
    case SO_LE:
      so_trata_sisop_le(self);
      break;
    case SO_ESCR:
      so_trata_sisop_escr(self);
      break;
    case SO_FIM:
      so_trata_sisop_fim(self);
      break;
    case SO_CRIA:
      so_trata_sisop_cria(self);
      break;
    default:
      t_printf("SO: chamada de sistema não reconhecida %d\n", chamada);
      panico(self);
  }
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  if(self->processos.atual == NULL) return;

  if(self->processos.escalonador == ROUND_ROBIN) {
    self->processos.atual->quantum++;
  }
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  self->metricas.interrupcoes++;
  exec_copia_estado(contr_exec(self->contr), self->cpue);

  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      panico(self);
  }

  so_resolve_es(self);
  so_escalona(self);
  
  cpue_muda_erro(self->cpue, ERR_OK, 0); // interrupção da cpu foi atendida
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

/**Percorre todos os processos bloqueados
 * e verifica os dispositivos de E/S estão
 * prontos, resolvendo a solicitação
*/
static void so_resolve_es(so_t* self)
{
  proc_t* el;

  STAILQ_FOREACH(el, self->processos.bloqueados, entries){
    int val = cpue_X(el->cpue);
    err_t err;

    if(!es_pronto(contr_es(self->contr), el->disp, el->acesso)) continue;

    if(el->acesso == leitura) {
      err = es_le(contr_es(self->contr), el->disp, &val);
    }else {
      err = es_escreve(contr_es(self->contr), el->disp, val);
    }

    cpue_muda_A(el->cpue, err);
    if (err == ERR_OK) {
      cpue_muda_X(el->cpue, val); // muda registrador X, no caso da leitura
      cpue_muda_PC(el->cpue, cpue_PC(el->cpue)+2); // incrementa o PC
    }

    so_desbloqueia_processo(self, el);
  }
}

/**
 * Decide qual será o processo a ser executado
 * e termina o SO caso não haja nenhum
*/
static void so_escalona(so_t* self)
{
  bool nenhumPronto = STAILQ_EMPTY(self->processos.prontos);
  bool nenhumBloqueado = STAILQ_EMPTY(self->processos.bloqueados);
  proc_t* atual = self->processos.atual;

  if(nenhumPronto && nenhumBloqueado && atual == NULL) {
    t_printf("SO: Nenhum processo disponível para o escalonador");
    panico(self);
    return;
  }

  // Todos os processos estão bloqueados, coloca em modo zumbi
  if(atual == NULL && nenhumPronto) {
    so_desativa_cpu(self);
    return;
  }

  proc_t* proc;
  if(atual == NULL) { // Nenhum processo em execução
    proc = self->processos.escalonador == ROUND_ROBIN ? so_encontra_first(self) : so_encontra_shortest(self);
    proc->quantum = 0;
  }else if(self->processos.escalonador == ROUND_ROBIN) {
    if(atual->quantum <= MAX_QUANTUM) return; // Continua executando o processo atual
    // Salva o processo atual
    so_salva_processo(self, self->processos.atual);
    proc_list_push_back(self->processos.prontos, self->processos.atual);
    // Substitui por um novo processo
    proc = so_encontra_first(self);
    proc_list_pop(self->processos.prontos, proc);
    atual->quantum = 0;
  }else {
    // TODO
  }

  so_carrega_processo(self, proc);
}

static void so_ativa_cpu(so_t* self) {
  cpue_muda_modo(self->cpue, usuario);
  self->metricas.tempo_parado += self->metricas.hora_bloqueio - rel_agora(self->rel);
}
static void so_desativa_cpu(so_t* self) {
  cpue_muda_modo(self->cpue, zumbi);
  self->metricas.hora_bloqueio = rel_agora(self->rel);
}

/** Cria um processo e o inicializa com o programa desejado */
static proc_t* so_cria_processo(so_t *self, int prog)
{
  proc_t* proc = proc_cria(self->processos.max_pid, mem_tam(contr_mem(self->contr)));
  if(proc == NULL) return proc;

  proc_list_push_front(self->processos.prontos, proc);
  self->processos.max_pid++;

  t_printf("Processo %d criado", proc->id);

  if(!proc_inicializa(proc, prog)) {
    so_finaliza_processo(self, proc);
    t_printf("Falha na inicialização do processo %d", proc->id);
  }else {
    t_printf("Processo %d inicializado com o programa %d", proc->id, prog);
  }

  return proc;
}

// Destroi um processo
static void so_finaliza_processo(so_t *self, proc_t* proc)
{
  if(self->processos.atual == proc) {
    self->processos.atual = NULL;
    proc_destroi(proc);
    return;
  }

  proc_list_pop(self->processos.bloqueados, proc);
  proc_list_pop(self->processos.prontos, proc);
  proc_destroi(proc);
}

// Desliga o sistema
static void panico(so_t *self) 
{
  self->paniquei = true;

  self->metricas.tempo_total = self->metricas.hora_inicio - rel_agora(self->rel);
  self->metricas.tempo_cpu = self->metricas.tempo_total - self->metricas.tempo_parado;
  so_imprime_metricas(self);
}

// Carrega um processo e o coloca em execução
static void so_carrega_processo(so_t *self, proc_t* proc){
  // atualiza o estado do processo
  proc_list_pop(self->processos.prontos, proc);
  self->processos.atual = proc;
  // altera o estado da CPU para o armazenado no processo
  cpue_copia(proc->cpue, self->cpue);
  // carrega a memória do processo
  mem_copia(proc->mem, contr_mem(self->contr));
  so_ativa_cpu(self);
}

// Salva o estado de um processo
static void so_salva_processo(so_t *self, proc_t* proc){
  cpue_copia(self->cpue, proc->cpue);
  mem_copia(contr_mem(self->contr), proc->mem);
}

// Bloqueia um processo, alterando a tabela de processos
static void so_bloqueia_processo(so_t *self, proc_t* proc) {
  if(proc == self->processos.atual) {
    self->processos.atual = NULL;
    so_salva_processo(self, proc);
  }else {
    proc_list_pop(self->processos.prontos, proc);
  }

  proc_list_push_front(self->processos.bloqueados, proc);
  if(self->processos.escalonador == SHORTEST) {
    proc->quantum = (proc->quantum + proc->quantum /**TODO*/)/2;
  }
}

// Desbloqueia um processo, alterando a tabela de processos
static void so_desbloqueia_processo(so_t *self, proc_t* proc) {
  proc_list_pop(self->processos.bloqueados, proc);

  // coloca o processo no final da lista
  proc_list_push_back(self->processos.prontos, proc);
}

// Encontra e retorna o primeiro processo pronto para ser executado
// NULL caso não haja nenhum
static proc_t* so_encontra_first(so_t *self) {
  return STAILQ_FIRST(self->processos.prontos);
}

// Encontra e retorna o processo "mais curto"
// NULL caso não haja nenhum
static proc_t* so_encontra_shortest(so_t *self) {
  // TODO
  return NULL;
}

// static void so_imprime_metricas_processo(so_t* self, proc_t* proc) {
//   // TODO
// }

static void so_imprime_metricas(so_t* self) {
  FILE* file = fopen("./metricas/so.txt", "w");
  if(file == NULL) return;

  metricas_so_t metricas = self->metricas;
  fprintf(file, "\nMétricas do Sistema Operacional\n");
  fprintf(file, "Tempo total do sistema (unidades de tempo): %d\n", metricas.tempo_total);
  fprintf(file, "Tempo da CPU ativa (unidades de tempo): %d\n", metricas.tempo_cpu);
  fprintf(file, "Tempo da CPU parada (unidades de tempo): %d\n", metricas.tempo_cpu);
  fprintf(file, "Número de interrupções recebidas: %d\n", metricas.interrupcoes);
  fprintf(file, "Número de sisops recebidas: %d\n", metricas.sisops);

  fclose(file);
}