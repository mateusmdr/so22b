#include <stdlib.h>
#include <time.h>
#include "rand.h"

struct rand_t {
    int min;
    int max;
    rel_t* rel;
    int n_inst_ultima_leitura;
};

rand_t *rand_cria(rel_t *rel)
{
    rand_t* self = malloc(sizeof(rand_t));
    srand(time(NULL));
    self->min = 0;
    self->max = 1000;
    self->rel = rel;
    self->n_inst_ultima_leitura = -30; // Inicia desbloqueado

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

    rand_t* self = (rand_t*)disp;
    
    // coloca a saída no endereço de memória recebido
    *pvalor = rand_inteiro(self);

    // grava o horário de leitura
    rel_le(self->rel, 0, &self->n_inst_ultima_leitura);

    return ERR_OK;
}

// Ocupado durante 30 instrucoes
bool rand_pronto(void *disp, int id, acesso_t acesso) {
    rand_t* self = (rand_t*)disp;
    int instrucoes;
    rel_le(self->rel, 0, &instrucoes);

    return instrucoes - self->n_inst_ultima_leitura >= 30;
}
