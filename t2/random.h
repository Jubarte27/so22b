#ifndef SO22B_RANDOM_H
#define SO22B_RANDOM_H

#include "err.h"

#define RANDOM_MIN 0
#define RANDOM_MAX 10

typedef struct random_t random_t;

random_t *rand_cria();
void rand_destroi(random_t *self);

// gera um inteiro aleatório entre RANDOM_MIN e RANDOM_MAX
err_t rand_le(void *disp, int id, int *pvalor);

#endif //SO22B_RANDOM_H
