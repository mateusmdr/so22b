#include <stdlib.h>
#include <time.h>
#include "rand.h"

struct rand_t {
    int min;
    int max;
};

rand_t *rand_cria(void)
{
    rand_t* self = malloc(sizeof(rand_t));
    srand(time(NULL));
    self->min = 0;
    self->max = 10000;

    return self;
}

void rand_destroi(rand_t *self)
{
    free(self);
}

// gera um número pseudo-aleatório entre min e max
// usando a seed gerada quando o dispositivo foi iniciado
int rand_inteiro(rand_t* self) {
    return self->min + rand() % self->max;
}

err_t rand_le(void *disp, int id, int *pvalor)
{
    if(id != 0) return ERR_OP_INV;

    rand_t* self = disp;
    
    // coloca a saída no endereço de memória recebido
    *pvalor = rand_inteiro(self);

    return ERR_OK;
}

// Tem 50% de chance de estar pronto
bool rand_pronto(void *disp, int id, acesso_t acesso) {
    return rand() % 2 == 0;
}
