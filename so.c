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
} so_metricas_t;

struct so_t {
  contr_t *contr;            // o controlador do hardware
  bool paniquei;             // apareceu alguma situação intratávele
  cpu_estado_t *cpue;        // cópia do estado da CPU
  rel_t *rel;                // referência do relógio do controlador
  tab_proc_t processos;      // tabela de processos do SO
  so_metricas_t metricas;    // métricas do SO
};

#define MAX_QUANTUM 2
#define PROGRAMA_INICIAL 1
#define ESCALONADOR SHORTEST

// funções auxiliares
static void panico(so_t *self);
static proc_t* so_cria_processo(so_t *self, int prog);
static void so_finaliza_processo(so_t *self, proc_t* proc);
static void so_bloqueia_processo(so_t *self);
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
static void so_imprime_metricas_processo(so_t* self, proc_t* proc);

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
  self->processos.escalonador = ESCALONADOR;
  /**Inicializa métricas do sistema*/
  self->metricas.hora_inicio = rel_agora(self->rel);
  self->metricas.interrupcoes = 0;
  self->metricas.sisops = 0;
  self->metricas.tempo_total = 0;
  self->metricas.tempo_cpu = 0;
  self->metricas.tempo_parado = 0;

  proc_t* proc = so_cria_processo(self, PROGRAMA_INICIAL);
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

bool so_ok(so_t *self)
{
  return !self->paniquei;
}

int so_pid(so_t* self) {
  if(self->processos.atual == NULL) return -1;

  return self->processos.atual->id;
}

// chamada de sistema para leitura de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_le(so_t *self)
{
  proc_t* atual = self->processos.atual;
  atual->disp = cpue_A(self->cpue);
  atual->acesso = leitura;
  so_bloqueia_processo(self);
}

// chamada de sistema para escrita de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_escr(so_t *self)
{
  proc_t* atual = self->processos.atual;
  atual->disp = cpue_A(self->cpue);
  atual->acesso = escrita;
  
  so_bloqueia_processo(self);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  int pid = self->processos.atual->id;
  so_finaliza_processo(self, self->processos.atual);
  t_printf("Processo %d finalizado", pid);
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
      t_printf("SO: chamada de sistema não reconhecida %d feita pelo processo %d\n", chamada, self->processos.atual->id);
      so_finaliza_processo(self, self->processos.atual);
  }
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  if(self->processos.atual == NULL) return;

  self->processos.atual->quantum--;
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
      t_printf("SO: interrupção não tratada [%s] feita pelo processo %d", err_nome(err), self->processos.atual->id);
      so_finaliza_processo(self, self->processos.atual);
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
    t_printf("Dispositivo %d liberado para o processo %d", el->disp, el->id);
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

  if(atual != NULL && (nenhumPronto || atual->quantum >= 0)) return; // Continua executando o processo atual

  // Preempção
  proc_t* proc = (self->processos.escalonador == ROUND_ROBIN) ?
    so_encontra_first(self) :
    so_encontra_shortest(self);
  int agora = rel_agora(self->rel);

  proc->quantum = MAX_QUANTUM;
  
  if(atual != NULL) {
    atual->metricas.preempcoes++;
    atual->metricas.hora_desbloqueio_preempcao = agora;
    atual->metricas.duracao_ultimo_bloqueio = 0;

    // Salva o processo atual
    so_salva_processo(self, self->processos.atual);
    proc_list_push_back(self->processos.prontos, self->processos.atual);

    if(self->processos.escalonador == SHORTEST) {
      // Calcula o tempo esperado do processo que foi colocado em preempção
      int ultimo_tempo = agora - atual->metricas.hora_execucao;
      atual->tempo_esperado = (atual->tempo_esperado + ultimo_tempo)/2;
    }
  }

  so_carrega_processo(self, proc);
}

static void so_ativa_cpu(so_t* self) {
  if(cpue_modo(self->cpue) != usuario) {
    cpue_muda_modo(self->cpue, usuario);
    self->metricas.tempo_parado += rel_agora(self->rel) - self->metricas.hora_bloqueio;
  }
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

  proc->quantum = MAX_QUANTUM;
  proc->tempo_esperado = MAX_QUANTUM;

  proc->metricas.hora_criacao = rel_agora(self->rel);
  proc->metricas.duracao_ultimo_bloqueio = 0;

  proc->metricas.tempo_total = 0;
  proc->metricas.tempo_bloqueado = 0;
  proc->metricas.tempo_CPU = 0;
  proc->metricas.tempo_espera = 0;
  proc->metricas.tempo_medio_retorno = 0;
  proc->metricas.bloqueios = 0;
  proc->metricas.preempcoes = 0;

  return proc;
}

// Destroi um processo
static void so_finaliza_processo(so_t *self, proc_t* proc)
{
  if(self->processos.atual == proc) {
    self->processos.atual = NULL;
  }else{
    proc_list_pop(self->processos.bloqueados, proc);
    proc_list_pop(self->processos.prontos, proc);
  }
  int agora = rel_agora(self->rel);
  proc->metricas.tempo_total = agora - proc->metricas.hora_criacao;
  proc->metricas.tempo_CPU += agora - proc->metricas.hora_execucao;

  so_imprime_metricas_processo(self, proc);

  proc_destroi(proc);
}

// Desliga o sistema
static void panico(so_t *self) 
{
  self->paniquei = true;

  self->metricas.tempo_total = rel_agora(self->rel) - self->metricas.hora_inicio;
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

  int agora = rel_agora(self->rel);
  proc->metricas.hora_execucao = agora;
  proc->metricas.tempo_espera += agora - proc->metricas.hora_desbloqueio_preempcao;
  if(proc->metricas.duracao_ultimo_bloqueio == 0) return;

  int tempo_retorno = agora - proc->metricas.duracao_ultimo_bloqueio;
  proc->metricas.duracao_ultimo_bloqueio = 0;
  if(proc->metricas.tempo_medio_retorno == 0) {
    proc->metricas.tempo_medio_retorno = tempo_retorno;
  }else {
    proc->metricas.tempo_medio_retorno = (tempo_retorno + proc->metricas.tempo_medio_retorno)/2;
  }
}

// Salva o estado de um processo
static void so_salva_processo(so_t *self, proc_t* proc){
  cpue_copia(self->cpue, proc->cpue);
  mem_copia(contr_mem(self->contr), proc->mem);
}

// Bloqueia o processo atual, alterando a tabela de processos
static void so_bloqueia_processo(so_t *self) {
  proc_t* proc = self->processos.atual;
  int agora = rel_agora(self->rel);
  self->processos.atual = NULL;

  so_salva_processo(self, proc);
  proc_list_push_front(self->processos.bloqueados, proc);

  if(self->processos.escalonador == SHORTEST) {
    // Calcula o tempo esperado do processo que foi colocado em preempção
    int ultimo_tempo = agora - proc->metricas.hora_execucao;
    proc->tempo_esperado = (proc->tempo_esperado + ultimo_tempo)/2;
  }
  proc->metricas.bloqueios++;
  proc->metricas.hora_bloqueio = agora;
  proc->metricas.tempo_CPU += agora - proc->metricas.hora_execucao;
}

// Desbloqueia um processo, alterando a tabela de processos
static void so_desbloqueia_processo(so_t *self, proc_t* proc) {
  proc_list_pop(self->processos.bloqueados, proc);

  // coloca o processo no final da lista
  proc_list_push_back(self->processos.prontos, proc);

  int agora = rel_agora(self->rel);
  int tempo_bloqueio = agora - proc->metricas.hora_bloqueio;

  proc->metricas.tempo_bloqueado += tempo_bloqueio;
  proc->metricas.hora_desbloqueio_preempcao = agora;
  proc->metricas.duracao_ultimo_bloqueio = tempo_bloqueio;
}

// Encontra e retorna o primeiro processo pronto para ser executado
static proc_t* so_encontra_first(so_t *self) {
  return STAILQ_FIRST(self->processos.prontos);
}

// Encontra e retorna o processo "mais curto"
static proc_t* so_encontra_shortest(so_t *self) {
  proc_t *el, *shortest = STAILQ_FIRST(self->processos.prontos);
  STAILQ_FOREACH(el, self->processos.prontos, entries) {
    if(el->tempo_esperado < shortest->tempo_esperado) {
      shortest = el;
    }
  }

  return shortest;
}

static void so_imprime_metricas_processo(so_t* self, proc_t* proc) {
  char filename[64];
  snprintf(filename, sizeof(filename), "%s%d%s", "./metricas/proc-", proc->id, ".txt");
  FILE* file = fopen(filename, "w");
  if(file == NULL) return;

  proc_metricas_t metricas = proc->metricas;

  fprintf(file, "Métricas do Processo PID=%d\n\n", proc->id);
  fprintf(file, "Tempo entre criação e término (unidades de tempo): ..... %d\n", metricas.tempo_total);
  fprintf(file, "Tempo total bloqueado (unidades de tempo): ............. %d\n", metricas.tempo_bloqueado);
  fprintf(file, "Tempo total executando (unidades de tempo): ............ %d\n", metricas.tempo_CPU);
  fprintf(file, "Tempo total em espera  (unidades de tempo): ............ %d\n", metricas.tempo_espera);
  fprintf(file, "Tempo médio de retorno (unidades de tempo): ............ %f\n", metricas.tempo_medio_retorno);
  fprintf(file, "Número de bloqueios: ................................... %d\n", metricas.bloqueios);
  fprintf(file, "Número de preempções: .................................. %d\n", metricas.preempcoes);

  fclose(file);
}

static void so_imprime_metricas(so_t* self) {
  FILE* file = fopen("./metricas/so.txt", "w");
  if(file == NULL) return;

  so_metricas_t metricas = self->metricas;
  fprintf(file, "Métricas do Sistema Operacional\n\n");
  fprintf(file, "Tempo total do sistema (unidades de tempo):... %d\n", metricas.tempo_total);
  fprintf(file, "Tempo da CPU ativa (unidades de tempo): ...... %d\n", metricas.tempo_cpu);
  fprintf(file, "Tempo da CPU parada (unidades de tempo): ..... %d\n", metricas.tempo_parado);
  fprintf(file, "Número de interrupções recebidas: ............ %d\n", metricas.interrupcoes);
  fprintf(file, "Número de sisops recebidas: .................. %d\n", metricas.sisops);

  fclose(file);
}