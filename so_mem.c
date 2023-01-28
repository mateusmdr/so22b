#include "so_mem.h"
#include <assert.h>
#include <stdlib.h>

typedef struct {
  bool livre;                     // Informa se o quadro está livre
} quadro_t;

struct so_mem {
  quadro_t quadros[N_QUADROS];    // Contém a informação acerca dos quadros da memória principal
};

so_mem_t* so_mem_cria() {
    assert(MEM_TAM % QUADRO_TAM == 0);

    so_mem_t* so_mem =  malloc(sizeof(so_mem_t));
    for(int c=0; c<N_QUADROS; c++) {
        so_mem->quadros[c].livre = true;
    }

    return so_mem;
}

int so_mem_encontra_livre(so_mem_t* so_mem) {
    int index = -1;
    for(int c=0; c<N_QUADROS; c++) {
        if(so_mem->quadros[c].livre) {
            index = c;
            break;
        }
    }
    
    return index;
}

void so_mem_libera(so_mem_t* so_mem, int n_quadro) {
    so_mem->quadros[n_quadro].livre = true;
}
void so_mem_ocupa(so_mem_t* so_mem, int n_quadro) {
    so_mem->quadros[n_quadro].livre = false;
}