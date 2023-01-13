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
  int max_proc_id;      // último id de processo gerado
  proc_t* proc;         // processo atual em execução (NULL caso nenhum)
};

// funções auxiliares
static void panico(so_t *self);
static proc_t* so_cria_processo(so_t *self);
static void so_finaliza_processo(so_t *self);
static proc_t* so_encontra_processo(so_t *self);
static void so_troca_processo(so_t *self);

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->proc_list = proc_list_cria();
  self->max_proc_id = 0;

  self->proc = so_cria_processo(self);
  if(!proc_inicializa(self->proc, 0)) {
    panico(self);
  }
  

  // coloca a CPU em modo usuário
  /*
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_modo(self->cpue, usuario);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  */

  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  proc_list_destroi(self->proc_list);
  free(self);
}

// trata chamadas de sistema

// chamada de sistema para leitura de E/S
// recebe em A a identificação do dispositivo
// retorna em X o valor lido
//            A o código de erro
static void so_trata_sisop_le(so_t *self)
{
  int disp = cpue_A(self->cpue);
  if(es_pronto(contr_es(self->contr), disp, leitura)) {
    int val;
    err_t err = es_le(contr_es(self->contr), disp, &val);
    cpue_muda_A(self->cpue, err);
    if (err == ERR_OK) {
      cpue_muda_X(self->cpue, val);
    }
    // incrementa o PC
    cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  }else {
    self->proc->estado = BLOQUEADO;
    self->proc->disp = disp;
    self->proc->acesso = leitura;
    so_troca_processo(self);
  }

  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
}

// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);

  if(es_pronto(contr_es(self->contr), disp, escrita)) {
    err_t err = es_escreve(contr_es(self->contr), disp, val);
    cpue_muda_A(self->cpue, err);
    // incrementa o PC
    cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  }else {
    self->proc->estado = BLOQUEADO;
    self->proc->disp = disp;
    self->proc->acesso = escrita;
    so_troca_processo(self);
  }
  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  so_finaliza_processo(self);
  so_troca_processo(self);
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  self->max_proc_id++;
  int prog_id = cpue_A(self->cpue);

  proc_t* proc = so_cria_processo(self);
  if(!proc_inicializa(proc, prog_id)) {
    panico(self);
  }

  proc_list_insere(self->proc_list, proc);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  // o tipo de chamada está no "complemento" do cpue
  exec_copia_estado(contr_exec(self->contr), self->cpue);
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
      t_printf("so: chamada de sistema não reconhecida %d\n", chamada);
      panico(self);
  }
}
// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  proc_t* el;
  SLIST_FOREACH(el, self->proc_list, entries){
    if(el->estado == BLOQUEADO) {
      if(es_pronto(contr_es(self->contr), el->disp, el->acesso)) {
        el->estado = PRONTO;
      }
    }
  }
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      self->paniquei = true;
  }
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

static proc_t* so_cria_processo(so_t *self)
{
  proc_t* proc = proc_cria(self->max_proc_id, mem_tam(contr_mem(self->contr)));
  proc_list_insere(self->proc_list, proc);
  self->max_proc_id++;

  return proc;
}

static void so_finaliza_processo(so_t *self)
{
  proc_list_remove(self->proc_list, self->proc);
  proc_destroi(self->proc);
  self->proc = NULL;
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}

static void so_muda_modo(so_t* self, cpu_modo_t modo) {
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_modo(self->cpue, modo);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

static void so_troca_processo(so_t *self) {
  proc_t* proc = so_encontra_processo(self);
  // TODO salva o processo atual (caso exista)
  if(proc == NULL) {
    so_muda_modo(self, zumbi);
    return;
  }

  // recupera o estado do processo
  // cpue_copia(proc->cpue, contr_cpue(self->cpue));
  // carrega a memória do processo
  // mem_copia(proc->mem, contr_mem(self->contr));

  // atualiza o estado dos processos
  self->proc->estado = BLOQUEADO;
  proc->estado = EXECUTANDO;

  // altera o processo atual em execução
  self->proc = proc;
}

// Encontra e retorna um processo pronto para ser executado
// NULL caso não haja nenhum
static proc_t* so_encontra_processo(so_t *self) {
  return proc_list_encontra_estado(self->proc_list, PRONTO);
}