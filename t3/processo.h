#ifndef SO22B_PROCESSO_H
#define SO22B_PROCESSO_H

#include <stdlib.h>
#include <stdbool.h>
#include "err.h"
#include "cpu_estado.h"
#include "mem.h"
#include "so.h"

typedef enum tipo_bloqueio_processo {
    ES
} tipo_bloqueio_processo;

enum proc_estado_t {
    BLOQUEADO, PRONTO, EM_EXECUCAO
};

typedef enum proc_estado_t proc_estado_t;

typedef struct tabela_processos_t tabela_processos_t;
typedef struct processo_t processo_t;
typedef struct metricas_t metricas_t;
typedef struct info_escalonador info_escalonador;

struct tabela_processos_t {
    processo_t **processos;
    size_t tam;
};

struct metricas_t {
    int clk_ultima_troca_estado;
    int clk_inicio;
    int clk_fim;
    int clk_total_E;
    int clk_total_B;
    int clk_total_P;
    int qtd_bloqueios;
    int qtd_preempcoes;
};

err_t proc_cria(processo_t **proc, programa_t *programa, int tam_mem, int clock);
err_t proc_destroi(processo_t *processo);
cpu_estado_t *proc_cpue(processo_t *self);
mem_t *proc_mem(processo_t *self);
bool proc_pronto(processo_t *proc);
bool proc_bloqueado(processo_t *proc);
bool proc_em_execucao(processo_t *proc);
tipo_bloqueio_processo proc_tipo_bloqueio(processo_t *proc);
void *proc_info_bloqueio(processo_t *proc);
void proc_bloqueia(processo_t *proc, tipo_bloqueio_processo tipo_bloqueio, void *info_bloqueio, int clock, int delta_clock);
void proc_altera_estado(processo_t *proc, proc_estado_t estado, int clock, int delta_clock);
metricas_t *proc_metricas(processo_t *proc);
void proc_fim(processo_t *processo, int clock);


tabela_processos_t *tabela_cria(size_t tam);
size_t tabela_adiciona_processo(tabela_processos_t *tabela, processo_t *processo);
bool tabela_algum_proc_nao_nulo(tabela_processos_t *tabela);
size_t proc_num(tabela_processos_t *self, processo_t *proc);



#endif //SO22B_PROCESSO_H
