#include "so_mem.h"
#include "proc.h"
#include <assert.h>
#include <stdlib.h>

struct so_mem {
  quadro_t quadros[N_QUADROS];    // Contém a informação acerca dos quadros da memória principal
  int ultima_posicao;
};

so_mem_t* so_mem_cria() {
    assert(MEM_TAM % QUADRO_TAM == 0);

    so_mem_t* self = malloc(sizeof(so_mem_t));
    self->ultima_posicao = 0;
    for(int c=0; c<N_QUADROS; c++) {
        self->quadros[c].livre = true;
        self->quadros[c].proc = NULL;
        self->quadros[c].pagina = -1;
        self->quadros[c].posicao = -1;
    }

    return self;
}

int so_mem_encontra_livre(so_mem_t* self) {
    int index = -1;
    for(int c=0; c<N_QUADROS; c++) {
        if(self->quadros[c].livre) {
            index = c;
            break;
        }
    }
    
    return index;
}

quadro_t so_mem_quadro(so_mem_t* self, int n_quadro) {
    return self->quadros[n_quadro];
}

void so_mem_libera(so_mem_t* self, int n_quadro) {
    self->quadros[n_quadro].livre = true;
}

void so_mem_ocupa(so_mem_t* self, int n_quadro, proc_t* proc, int pagina) {
    self->quadros[n_quadro].livre = false;
    self->quadros[n_quadro].proc = proc;
    self->quadros[n_quadro].pagina = pagina;
    self->ultima_posicao++;
}

void so_mem_destroi(so_mem_t* self) {
    free(self);
}