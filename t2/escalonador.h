#ifndef SO22B_ESCALONADOR_H
#define SO22B_ESCALONADOR_H

#include <stdbool.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

bool precisa_escalonar(tabela_processos_t *tabela_processos, size_t num_proc_execucao);
processo_t *escalonar(tabela_processos_t *tabela_processos);

#endif //SO22B_ESCALONADOR_H
