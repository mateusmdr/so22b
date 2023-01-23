#include "so.h"
#include "tela.h"
#include "proc.h"
#include <stdlib.h>
#include <sys/queue.h>

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

struct so_t {
  contr_t *contr;            // o controlador do hardware
  bool paniquei;             // apareceu alguma situação intratável
  cpu_estado_t *cpue;        // cópia do estado da CPU
  tab_proc_t processos;      // tabela de processos do SO
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

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->processos.bloqueados = proc_list_cria();
  self->processos.prontos = proc_list_cria();
  self->processos.atual = NULL;
  self->processos.max_pid = 0;
  self->processos.escalonador = ROUND_ROBIN;

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
  if(STAILQ_EMPTY(self->processos.prontos) && STAILQ_EMPTY(self->processos.bloqueados)) {
    t_printf("SO: Nenhum processo disponível para o escalonador");
    panico(self);
    return;
  }

  if(self->processos.atual == NULL) { // Nenhum processo em execução
    if(STAILQ_EMPTY(self->processos.prontos)) { // Nenhum processo pronto, coloca a CPU em modo zumbi
      cpue_muda_modo(self->cpue, zumbi);
      return;
    }

    proc_t* proc;
    proc = self->processos.escalonador == ROUND_ROBIN ?
      so_encontra_first(self) :
      so_encontra_shortest(self)
    ;

    so_carrega_processo(self, proc);
  }
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
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
    t_printf("Processo %d inicializado", proc->id);
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
  
static void panico(so_t *self) 
{
  self->paniquei = true;
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
  cpue_muda_modo(self->cpue, usuario);
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
}

// Desbloqueia um processo, alterando a tabela de processos
static void so_desbloqueia_processo(so_t *self, proc_t* proc) {
  proc_list_pop(self->processos.bloqueados, proc);

  proc_list_push_front(self->processos.prontos, proc);
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