#include "so.h"
#include "tela.h"
#include "proc.h"
#include <stdlib.h>
#include <sys/queue.h>

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  proc_list_t* proc_list;   // lista contendo os processos do SO
  int max_pid;      // último id de processo gerado
  proc_t* proc;         // processo atual em execução (NULL caso nenhum)
};

// funções auxiliares
static void panico(so_t *self);
static proc_t* so_cria_processo(so_t *self, int prog);
static void so_finaliza_processo(so_t *self, proc_t* proc);
static proc_t* so_encontra_processo(so_t *self);
static void so_troca_processo(so_t *self);
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
  self->proc_list = proc_list_cria();
  self->max_pid = 0;

  proc_t* proc = so_cria_processo(self, 0);
  so_carrega_processo(self, proc);

  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  proc_list_destroi(self->proc_list);
  free(self);
}

// chamada de sistema para leitura de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_le(so_t *self)
{
  int disp = cpue_A(self->cpue);
  self->proc->estado = BLOQUEADO;
  self->proc->disp = disp;
  self->proc->acesso = leitura;
  
}

// chamada de sistema para escrita de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_escr(so_t *self)
{
  int disp = cpue_A(self->cpue);
  self->proc->estado = BLOQUEADO;
  self->proc->disp = disp;
  self->proc->acesso = escrita;
  
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  so_finaliza_processo(self, self->proc);
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
  // Vazio, por enquanto
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
  // Salva o processo atual caso tenha sido bloqueado
  if(self->proc != NULL && self->proc->estado == BLOQUEADO) {
    so_salva_processo(self, self->proc);
  }

  proc_t* el;

  SLIST_FOREACH(el, self->proc_list, entries){
    int val = cpue_X(el->cpue);
    err_t err;

    if(el->estado != BLOQUEADO) continue;

    if(!es_pronto(contr_es(self->contr), el->disp, el->acesso)) continue;

    el->estado = PRONTO;
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

    if(self->proc == el) {
      so_carrega_processo(self, el);
    }
  }
}

/**
 * Decide qual será o processo a ser executado
 * e termina o SO caso não haja nenhum
*/
static void so_escalona(so_t* self)
{
  if(SLIST_EMPTY(self->proc_list)) {
    panico(self);
    return;
  }
  
  if(self->proc == NULL || self->proc->estado == BLOQUEADO) {
    so_troca_processo(self);
  }  
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

static proc_t* so_cria_processo(so_t *self, int prog)
{
  proc_t* proc = proc_cria(self->max_pid, mem_tam(contr_mem(self->contr)));
  proc_list_insere(self->proc_list, proc);

  int pid = self->max_pid;
  self->max_pid++;

  t_printf("Processo %d criado", pid);

  if(!proc_inicializa(proc, prog)) {
    so_finaliza_processo(self, proc);
    t_printf("Falha na inicialização do processo %d", pid);
  }else {
    t_printf("Processo %d inicializado", pid);
  }

  return proc;
}

// Destroi o processo atual
static void so_finaliza_processo(so_t *self, proc_t* proc)
{
  if(self->proc == proc) {
    self->proc = NULL;
  }
  proc_list_remove(self->proc_list, proc);
  proc_destroi(proc);
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}

// Carrega um processo e o coloca em execução
static void so_carrega_processo(so_t *self, proc_t* proc){
  // atualiza o estado do processo
  proc->estado = EXECUTANDO;
  // regista como processo em execução no SO
  self->proc = proc;
  // altera o estado da CPU para o armazenado no processo
  cpue_copia(self->proc->cpue, self->cpue);
  // carrega a memória do processo
  mem_copia(self->proc->mem, contr_mem(self->contr));
  cpue_muda_modo(self->cpue, usuario);
}

// Salva o estado de um processo
static void so_salva_processo(so_t *self, proc_t* proc){
  cpue_copia(self->cpue, self->proc->cpue);
  mem_copia(contr_mem(self->contr), self->proc->mem);
}

static void so_troca_processo(so_t *self) {
  // salva processo atual (caso exista)
  if(self->proc != NULL) {
    so_salva_processo(self, self->proc);
    if(self->proc->estado == EXECUTANDO){
      self->proc->estado = PRONTO;
    } 
  }

  // Altera o processo atual
  proc_t* proc = so_encontra_processo(self);
  if(proc == NULL) {
    self->proc = NULL;
    cpue_muda_modo(self->cpue, zumbi);
    return;
  }

  so_carrega_processo(self, proc);
}

// Encontra e retorna um processo pronto para ser executado
// NULL caso não haja nenhum
static proc_t* so_encontra_processo(so_t *self) {
  proc_t *el;
  SLIST_FOREACH(el, self->proc_list, entries){
    if(el->estado == PRONTO) {
      return el;
    }
  }

  return NULL;
}