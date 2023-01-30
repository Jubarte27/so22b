#include "escalonador.h"
#include "tela.h"

struct escalonador_t {
    processo_t *em_execucao;
};

escalonador_t *esc_cria(processo_t *processo_em_execucao) {
    escalonador_t *escalonador = malloc(sizeof(escalonador_t *));
    if (escalonador == NULL) return NULL;
    escalonador->em_execucao = processo_em_execucao;
    return escalonador;
}

int tempo_esperado(metricas_t *metricas) {
    int qtd_paradas = metricas->qtd_bloqueios + metricas->qtd_preempcoes;
    return qtd_paradas <= 0 ? QUANTUM : metricas->clk_total_E / qtd_paradas;
}

processo_t *esc_escalonar(escalonador_t *escalonador, tabela_processos_t *tabela_processos, int clock, int delta_clock) {
    if (escalonador->em_execucao != NULL && proc_em_execucao(escalonador->em_execucao)) {
        metricas_t *metricas = proc_metricas(escalonador->em_execucao);
        if (clock - metricas->clk_ultima_troca_estado > QUANTUM) {
            proc_altera_estado(escalonador->em_execucao, PRONTO, clock, delta_clock);
        } else {
            return escalonador->em_execucao;
        }
    }
    processo_t **processos = tabela_processos->processos;
    size_t tam = tabela_processos->tam;

    processo_t *mais_curto_pronto = NULL;
    int clk_mais_curto;
    for (size_t i = 0; i < tam; i++) {
        processo_t *proc = processos[i];
        if (proc == NULL || !proc_pronto(proc)) continue;

        metricas_t *metricas = proc_metricas(proc);
        int clk_atual = tempo_esperado(metricas);
        if (mais_curto_pronto == NULL || clk_atual < clk_mais_curto) {
            clk_mais_curto = clk_atual;
            mais_curto_pronto = proc;
        }
    }

    escalonador->em_execucao = mais_curto_pronto;
    return escalonador->em_execucao;
}

void esc_remover_processo_execucao(escalonador_t *escalonador) {
    escalonador->em_execucao = NULL;
}
