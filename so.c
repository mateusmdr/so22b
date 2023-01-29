#include "so.h"
#include "tela.h"
#include "proc.h"
#include "rel.h"
#include "so_mem.h"
#include "progr.h"
#include <stdlib.h>
#include <sys/queue.h>
#include <stdio.h>
#include <time.h>

#define PROGRAMA_INICIAL 0
#define ESCALONADOR ROUND_ROBIN
#define MAX_QUANTUM 16
#define ALG_PAG FIFO

typedef enum {
  ROUND_ROBIN,
  SHORTEST
} escalonador_t;

typedef enum {
  ALEATORIO,       // Escolhe uma página qualquer (péssimo)
  FIFO             // Escolhe a página mais antiga
} alg_pag_t;

/**
 * Tabela de processos do SO
*/

typedef struct {
  proc_list_t* bloqueados;
  proc_list_t* prontos;
  proc_t* atual;          // processo atual em execução (NULL caso nenhum)
  int max_pid;            // último id de processo gerado
} tab_proc_t;

typedef struct {
  /**Auxiliares*/
  time_t hora_inicio_real;
  int hora_inicio;
  int hora_bloqueio;
  int hora_desbloqueio;

  /**Métricas computadas*/ 
  double tempo_total_real;
  int tempo_total;
  int tempo_cpu;
  int tempo_parado;
  int interrupcoes;
  int sisops;
  int falhas_pagina;
} so_metricas_t;

struct so_t {
  contr_t *contr;            // o controlador do hardware
  bool paniquei;             // apareceu alguma situação intratável
  rel_t *rel;                // referência do relógio do controlador
  tab_proc_t processos;      // tabela de processos do SO
  so_metricas_t metricas;    // métricas do SO
  so_mem_t* so_mem;          // gerenciador de memória do SO
  alg_pag_t alg_pag;         // algoritmo de substituição de páginas do SO
  escalonador_t escalonador; // tipo de escalonador a ser utilizado
};

// funções auxiliares
static void panico(so_t *self);
static proc_t* so_cria_processo(so_t *self, int prog);
static void so_finaliza_processo(so_t *self, proc_t* proc);
static void so_bloqueia_processo(so_t *self);
static void so_desbloqueia_processo(so_t *self, proc_t* proc);
static proc_t* so_encontra_first(so_t *self);
static proc_t* so_encontra_shortest(so_t *self);
static void so_despacha(so_t *self, proc_t* proc);
static bool so_resolve_es(so_t* self, proc_t* proc);
static proc_t* so_escalona(so_t* self);
static void so_imprime_metricas(so_t* self);
static void so_imprime_metricas_processo(so_t* self, proc_t* proc);
static void so_verifica_bloqueados(so_t* self);

static void so_cria_tab_proc(so_t* self) {
  self->processos.bloqueados = proc_list_cria();
  self->processos.prontos = proc_list_cria();
  self->processos.atual = NULL;
  self->processos.max_pid = 0;
  self->escalonador = ESCALONADOR;
}

static void so_inicializa_metricas(so_t* self) {
  self->metricas.hora_inicio = rel_agora(self->rel);
  self->metricas.interrupcoes = 0;
  self->metricas.sisops = 0;
  self->metricas.tempo_total = 0;
  self->metricas.tempo_cpu = 0;
  self->metricas.tempo_parado = 0;
  self->metricas.hora_inicio_real = time(NULL);
  self->metricas.falhas_pagina = 0;
}

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->rel = contr_rel(self->contr);
  self->so_mem = so_mem_cria();
  
  so_cria_tab_proc(self);
  so_inicializa_metricas(self);

  so_despacha(self, so_cria_processo(self, PROGRAMA_INICIAL));

  return self;
}

void so_destroi(so_t *self)
{
  proc_list_destroi(self->processos.bloqueados);
  proc_list_destroi(self->processos.prontos);
  so_mem_destroi(self->so_mem);
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
  proc_t* proc = self->processos.atual;
  proc->disp = cpue_A(proc->cpue);
  proc->acesso = leitura;
  if(!so_resolve_es(self, proc)) so_bloqueia_processo(self);
}

// chamada de sistema para escrita de E/S, marca o processo
// atual como bloqueado com informações sobre a solicitação
static void so_trata_sisop_escr(so_t *self)
{
  proc_t* proc = self->processos.atual;
  proc->disp = cpue_A(proc->cpue);
  proc->acesso = escrita;
  
  if(!so_resolve_es(self, proc)) so_bloqueia_processo(self);
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
  proc_t* proc = self->processos.atual;
  int prog = cpue_A(proc->cpue);
  so_cria_processo(self, prog);
  // incrementa o PC
  cpue_muda_PC(proc->cpue, cpue_PC(proc->cpue)+2);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  self->metricas.sisops++;
  so_chamada_t chamada = cpue_complemento(self->processos.atual->cpue);
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

// decide qual quadro vai ser liberado
static int so_escolhe_quadro(so_t* self) {
  if(self->alg_pag == ALEATORIO) {
    return rand() % N_QUADROS;
  }

  int indice_ultimo = 0;
  quadro_t primeiro_quadro = so_mem_quadro(self->so_mem, 0);
  for(int c=1; c<N_QUADROS; c++) {
    if(so_mem_quadro(self->so_mem, c).posicao < primeiro_quadro.posicao) {
      indice_ultimo = c;
    }
  }

  return indice_ultimo;
}

// trata uma falha de página
static void so_trata_falpag(so_t* self)
{
  proc_t* proc = self->processos.atual;
  tab_pag_t* tab_pag = proc->tab_pag;
  int end = mmu_ultimo_endereco(contr_mmu(self->contr));
  int pagina = end / QUADRO_TAM;
  int quadro = so_mem_encontra_livre(self->so_mem);

  if(quadro == -1) { // Nenhum quadro disponível, troca
    quadro = so_escolhe_quadro(self);
    quadro_t antigo = so_mem_quadro(self->so_mem, quadro);
    tab_pag_muda_valida(antigo.proc->tab_pag, antigo.pagina, false);
    for(int c=0; c<QUADRO_TAM; c++) { // copia a memória do quadro para o processo
      int val;
      mem_le(contr_mem(self->contr), quadro * QUADRO_TAM + c, &val);
      mem_escreve(antigo.proc->mem, antigo.pagina * QUADRO_TAM + c, val);
    }
  }

  for(int c=0; c<QUADRO_TAM; c++) { // copia a memória do processo para o quadro
    int val;
    mem_le(proc->mem, pagina * QUADRO_TAM + c, &val);
    mem_escreve(contr_mem(self->contr), quadro * QUADRO_TAM + c, val);
  }

  tab_pag_muda_quadro(tab_pag, pagina, quadro);
  tab_pag_muda_valida(tab_pag, pagina, true);
  so_mem_ocupa(self->so_mem, quadro, proc, pagina);

  proc->metricas.falhas_pagina++;
  self->metricas.falhas_pagina++;
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  self->metricas.interrupcoes++;
  proc_t* proc = self->processos.atual;

  if(proc != NULL) { // Salva o estado do processo atual
    exec_copia_estado(contr_exec(self->contr), proc->cpue);
  }

  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    case ERR_FALPAG:
      so_trata_falpag(self);
      break;
    case ERR_PAGINV:
      t_printf("Página inválida: %d", mmu_ultimo_endereco(contr_mmu(self->contr)));
    default:
      t_printf("SO: interrupção não tratada [%s] feita pelo processo %d", err_nome(err), proc->id);
      so_finaliza_processo(self, proc);
  }

  so_verifica_bloqueados(self);
  proc = so_escalona(self);

  so_despacha(self, proc);
}

/**
 * Resolve a E/S de um processo, retornando false caso o disp não esteja pronto
*/
static bool so_resolve_es(so_t* self, proc_t* proc) {
  if(!es_pronto(contr_es(self->contr), proc->disp, proc->acesso)) return false;

  int val = cpue_X(proc->cpue);
  err_t err;
  if(proc->acesso == leitura) {
    err = es_le(contr_es(self->contr), proc->disp, &val);
  }else {
    err = es_escreve(contr_es(self->contr), proc->disp, val);
  }

  cpue_muda_A(proc->cpue, err);
  cpue_muda_X(proc->cpue, val); // muda registrador X, no caso da leitura
  cpue_muda_PC(proc->cpue, cpue_PC(proc->cpue)+2); // incrementa o PC

  return true;
}

/**
 * Percorre todos os processos bloqueados
 * e verifica o que deve ser feito
*/
static void so_verifica_bloqueados(so_t* self)
{
  proc_t* el;

  STAILQ_FOREACH(el, self->processos.bloqueados, entries){
    if(so_resolve_es(self, el)) so_desbloqueia_processo(self, el);
  }
}

/**
 * Decide qual será o processo a ser executado
 * e termina o SO caso não haja nenhum
*/
static proc_t* so_escalona(so_t* self)
{
  proc_t* atual = self->processos.atual;
  bool nenhumPronto = proc_list_empty(self->processos.prontos);
  bool nenhumBloqueado = proc_list_empty(self->processos.bloqueados);

  if(nenhumPronto && nenhumBloqueado && atual == NULL) {
    t_printf("SO: Nenhum processo disponível para o escalonador");
    panico(self);
    return NULL;
  }

  // Continua executando o processo atual
  if(nenhumPronto || (atual != NULL && atual->quantum >= 0)) {
    return atual;
  }

  // Preempção
  proc_t* proc = (self->escalonador == ROUND_ROBIN) ?
    so_encontra_first(self) :
    so_encontra_shortest(self);
  
  int agora = rel_agora(self->rel);
  proc->quantum = MAX_QUANTUM;
  
  if(atual != NULL) {
    atual->metricas.preempcoes++;
    atual->metricas.hora_desbloqueio_preempcao = agora;
    atual->metricas.foi_bloqueado = false;
    atual->metricas.tempo_cpu += agora - atual->metricas.hora_execucao;

    if(self->escalonador == SHORTEST) {
      // Calcula o tempo esperado do processo que foi colocado em preempção
      int ultimo_tempo = agora - atual->metricas.hora_execucao;
      atual->tempo_esperado = (atual->tempo_esperado + ultimo_tempo)/2;
    }
  }

  return proc;
}

static void so_inicializa_metricas_proc(so_t* self, proc_t* proc) {
  int agora = rel_agora(self->rel);

  proc->metricas.hora_criacao = agora;
  proc->metricas.hora_desbloqueio_preempcao = agora;
  proc->metricas.tempo_total = 0;
  proc->metricas.tempo_bloqueado = 0;
  proc->metricas.tempo_cpu = 0;
  proc->metricas.tempo_espera = 0;
  proc->metricas.tempo_medio_retorno = 0;
  proc->metricas.bloqueios = 0;
  proc->metricas.preempcoes = 0;
  proc->metricas.foi_bloqueado = false;
  proc->metricas.falhas_pagina = 0;
}

/** Cria um processo e o inicializa com o programa desejado */
static proc_t* so_cria_processo(so_t *self, int prog)
{
  if(prog > sizeof(PROGRS)/4-1 || prog < 0) {
    t_printf("Programa inválido");
    return NULL;
  }

  proc_t* proc = (proc_t*) malloc(sizeof(proc_t));

  int* progr = PROGRS[prog];
  int tam_progr = PROGRS_SIZE[prog]/sizeof(progr[0]);

  proc->quantum = MAX_QUANTUM;
  proc->tempo_esperado = MAX_QUANTUM;
  proc->cpue = cpue_cria();
  proc->mem = mem_cria(tam_progr);
  proc->tab_pag = tab_pag_cria(tam_progr/QUADRO_TAM + 1, QUADRO_TAM);
  proc->id = self->processos.max_pid;
  proc->prog = prog;
  cpue_muda_modo(proc->cpue, usuario);
  so_inicializa_metricas_proc(self, proc);

  proc_list_push_front(self->processos.prontos, proc);
  self->processos.max_pid++;

  t_printf("Processo %d criado", proc->id);

  for (int i = 0; i < tam_progr; i++) {
    if (mem_escreve(proc->mem, i, progr[i]) != ERR_OK) {
      t_printf("Falha na inicialização do processo %d", proc->id);
      panico(self);
      return proc;
    }
  }

  return proc;
}

// Destroi um processo
static void so_finaliza_processo(so_t *self, proc_t* proc)
{
  if(self->processos.atual == proc) {
    self->processos.atual = NULL;
  }
  int agora = rel_agora(self->rel);
  proc->metricas.tempo_total = agora - proc->metricas.hora_criacao;
  proc->metricas.tempo_cpu += agora - proc->metricas.hora_execucao;

  so_imprime_metricas_processo(self, proc);
  for(int c=0; c<N_QUADROS; c++) {
    if(proc == so_mem_quadro(self->so_mem, c).proc) {
      so_mem_libera(self->so_mem, c);
    }
  }

  proc_destroi(proc);
}

// Desliga o sistema
static void panico(so_t *self) 
{
  self->paniquei = true;

  self->metricas.tempo_total_real = difftime(time(NULL), self->metricas.hora_inicio_real);
  self->metricas.tempo_total = rel_agora(self->rel) - self->metricas.hora_inicio;
  self->metricas.tempo_cpu = self->metricas.tempo_total - self->metricas.tempo_parado;
  so_imprime_metricas(self);
}

static void so_despacha(so_t *self, proc_t* proc){
  proc_t* atual = self->processos.atual;
  self->processos.atual = proc;

  if(proc != NULL){
    cpue_muda_erro(proc->cpue, ERR_OK, 0); // interrupção da cpu foi atendida
  }

  cpu_estado_t* cpue = cpue_cria();
  exec_t* exec = contr_exec(self->contr);
  exec_copia_estado(exec, cpue);
  
  if(proc == NULL) { // Coloca a CPU em modo zumbi
    if(cpue_modo(cpue) != zumbi){
      self->metricas.hora_bloqueio = rel_agora(self->rel);
      cpue_muda_modo(cpue, zumbi);
      exec_altera_estado(exec,cpue);
    }
  }else{
    if(proc != atual) { // troca o processo atual por outro
      // muda o estado do processo atual
      if(atual != NULL) {
        proc_list_push_back(self->processos.prontos, atual);
      }
      proc_list_pop(self->processos.prontos, proc);

      if(cpue_modo(cpue) != usuario){
        self->metricas.tempo_parado += rel_agora(self->rel) - self->metricas.hora_bloqueio;
      }

      int agora = rel_agora(self->rel);
      int duracao_ultima_espera = agora - proc->metricas.hora_desbloqueio_preempcao; // tempo no estado P
      proc->metricas.hora_execucao = agora;
      proc->metricas.tempo_espera += duracao_ultima_espera;

      if(proc->metricas.foi_bloqueado) { // Foi para o estado P pois estava bloqueado
        int duracao_retorno = duracao_ultima_espera + proc->metricas.duracao_ultimo_bloqueio;
        if(proc->metricas.tempo_medio_retorno == 0) {
          proc->metricas.tempo_medio_retorno = duracao_retorno;
        }else {
          proc->metricas.tempo_medio_retorno = (proc->metricas.tempo_medio_retorno + duracao_retorno)/2;
        }
      }
    }
    // altera o estado da CPU para o armazenado no processo
    exec_altera_estado(contr_exec(self->contr), proc->cpue);
    // altera a tabela de páginas da MMU
    mmu_usa_tab_pag(contr_mmu(self->contr), proc->tab_pag);
  }

  cpue_destroi(cpue);
}

// Bloqueia o processo atual, alterando a tabela de processos
static void so_bloqueia_processo(so_t *self) {
  proc_t* proc = self->processos.atual;
  int agora = rel_agora(self->rel);
  self->processos.atual = NULL;

  proc_list_push_front(self->processos.bloqueados, proc);

  if(self->escalonador == SHORTEST) {
    // Calcula o tempo esperado do processo que foi colocado em preempção
    int ultimo_tempo = agora - proc->metricas.hora_execucao;
    proc->tempo_esperado = (proc->tempo_esperado + ultimo_tempo)/2;
  }
  proc->metricas.bloqueios++;
  proc->metricas.hora_bloqueio = agora;
  proc->metricas.tempo_cpu += agora - proc->metricas.hora_execucao;
  proc->metricas.foi_bloqueado = true;
}

// Desbloqueia um processo, alterando a tabela de processos
static void so_desbloqueia_processo(so_t *self, proc_t* proc) {
  proc_list_pop(self->processos.bloqueados, proc);

  // coloca o processo no final da lista
  proc_list_push_back(self->processos.prontos, proc);

  int agora = rel_agora(self->rel);
  int duracao_bloqueio = agora - proc->metricas.hora_bloqueio;

  proc->metricas.tempo_bloqueado += duracao_bloqueio;
  proc->metricas.hora_desbloqueio_preempcao = agora;
  proc->metricas.duracao_ultimo_bloqueio = duracao_bloqueio;
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
  fprintf(file, "Tempo total executando (unidades de tempo): ............ %d\n", metricas.tempo_cpu);
  fprintf(file, "Tempo total em espera  (unidades de tempo): ............ %d\n", metricas.tempo_espera);
  fprintf(file, "Tempo médio de retorno (unidades de tempo): ............ %f\n", metricas.tempo_medio_retorno);
  fprintf(file, "Número de bloqueios: ................................... %d\n", metricas.bloqueios);
  fprintf(file, "Número de preempções: .................................. %d\n", metricas.preempcoes);
  fprintf(file, "Número de falhas de página: ............................ %d\n", metricas.falhas_pagina);

  fclose(file);
}

static void so_imprime_metricas(so_t* self) {
  FILE* file = fopen("./metricas/so.txt", "w");
  if(file == NULL) return;

  so_metricas_t metricas = self->metricas;
  fprintf(file, "Métricas do Sistema Operacional\n\n");
  fprintf(file, "Tempo total do sistema (segundos): ........... %lf\n", metricas.tempo_total_real);
  fprintf(file, "Tempo total do sistema (unidades de tempo):... %d\n", metricas.tempo_total);
  fprintf(file, "Tempo da CPU ativa (unidades de tempo): ...... %d\n", metricas.tempo_cpu);
  fprintf(file, "Tempo da CPU parada (unidades de tempo): ..... %d\n", metricas.tempo_parado);
  fprintf(file, "Número de interrupções recebidas: ............ %d\n", metricas.interrupcoes);
  fprintf(file, "Número de sisops recebidas: .................. %d\n", metricas.sisops);
  fprintf(file, "Número de falhas de página: .................. %d\n", metricas.falhas_pagina);

  fclose(file);
}