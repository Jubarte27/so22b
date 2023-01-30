#ifndef SO22B_ESCALONADOR_H
#define SO22B_ESCALONADOR_H

#include <stdbool.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

#define QUANTUM 20

typedef struct escalonador_t escalonador_t;


escalonador_t *esc_cria(processo_t *processo_em_execucao);
processo_t *esc_escalonar(escalonador_t *escalonador, tabela_processos_t *tabela_processos, int clock, int delta_clock);
void esc_remover_processo_execucao(escalonador_t *escalonador);

#endif //SO22B_ESCALONADOR_H
