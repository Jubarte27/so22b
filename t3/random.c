#include "random.h"
#include <stdlib.h>
#include <time.h>

typedef struct random_t random_t;

random_t *rand_cria() {
    srand(time(NULL));
    return malloc(sizeof(random_t*));
}

void rand_destroi(random_t *self) {
    free(self);
}

// gera um inteiro aleatório entre RAND_MIN e RAND_MAX
err_t rand_le(void *disp, int id, int *pvalor) {
    *pvalor = (rand() % (RANDOM_MAX - RANDOM_MIN)) + RANDOM_MIN;
    return ERR_OK;
}

