#ifndef SO_MEM_H
#define SO_MEM_H

/** Gerenciador de memória do SO */
#include "contr.h"

#define QUADRO_TAM 2000 // Suficiente para conter qualquer processo
#define N_QUADROS MEM_TAM/QUADRO_TAM

typedef struct so_mem so_mem_t;

// aloca um descritor
so_mem_t* so_mem_cria();

// retorna o índice de um quadro livre
int so_mem_encontra_livre(so_mem_t* so_mem);

// marca um quadro como livre
void so_mem_libera(so_mem_t* so_mem, int n_quadro);

// marca um quadro como ocupado
void so_mem_ocupa(so_mem_t* so_mem, int n_quadro);

#endif