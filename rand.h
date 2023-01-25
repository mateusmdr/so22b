#ifndef RAND_H
#define RAND_H

#include <stdbool.h>
#include "es.h"
#include "err.h"
#include "rel.h"

// Dispositivo de geração de valores aleatórios
// Ocupado durante 10 instruções (para simular e/s)
typedef struct rand_t rand_t;

// cria e inicializa o dispositivo
// retorna NULL em caso de erro
rand_t *rand_cria(rel_t* rel;);

// destrói o dispositivo
// nenhuma outra operação pode ser realizada no dispositivo após esta chamada
void rand_destroi(rand_t *self);

// caso operação (id) = 0
// lê um número inteiro do dispositivo e o coloca em pvalor
err_t rand_le(void *disp, int id, int *pvalor);

// Ocupado durante 10 instruções
bool rand_pronto(void *disp, int id, acesso_t acesso);

#endif // RAND_H
