#ifndef SO_MEM_H
#define SO_MEM_H

/** Gerenciador de memória do SO */
#include "contr.h"
#include "proc.h"

typedef struct {
  bool livre;                     // Informa se o quadro está livre
  proc_t* proc;                   // Qual o último processo que usou o quadro
  int pagina;                     // A qual página do processo o quadro corresponde
  int posicao;                    // Qual a posição do quadro em relação aos outros (usado no FIFO)
} quadro_t;

#define MEM_TAM 16000 // tamanho da memória principal
#define N_QUADROS 10
#define QUADRO_TAM (MEM_TAM/N_QUADROS)

typedef struct so_mem so_mem_t;

// aloca um descritor
so_mem_t* so_mem_cria();

// retorna o índice de um quadro livre (-1 caso nenhum)
int so_mem_encontra_livre(so_mem_t* self);

// marca um quadro como livre
void so_mem_libera(so_mem_t* self, int n_quadro);

// marca um quadro como ocupado
void so_mem_ocupa(so_mem_t* so_mem, int n_quadro, proc_t* proc, int pagina);

// retorna informações sobre o quadro
quadro_t so_mem_quadro(so_mem_t* self, int n_quadro);

// desaloca um descritor
void so_mem_destroi(so_mem_t* self);

#endif