#ifndef RAND_H
#define RAND_H

#include "err.h"

// dispositivo de geração de valores aleatórios

typedef struct rand_t rand_t;

// cria e inicializa o dispositivo
// retorna NULL em caso de erro
rand_t *rand_cria(void);

// destrói o dispositivo
// nenhuma outra operação pode ser realizada no dispositivo após esta chamada
void rand_destroi(rand_t *self);

// caso operação (id) = 0
// lê um número inteiro do dispositivo e o coloca em pvalor
err_t rand_le(void *disp, int id, int *pvalor);

#endif // RAND_H
