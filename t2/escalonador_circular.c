#include "escalonador.h"

struct escalonador_t {
    processo_t *em_execucao;
};

escalonador_t *esc_cria(processo_t *processo_em_execucao) {
    escalonador_t *escalonador = malloc(sizeof(escalonador_t *));
    if (escalonador == NULL) return NULL;
    escalonador->em_execucao = processo_em_execucao;
    return escalonador;
}



processo_t *esc_escalonar(escalonador_t *escalonador, tabela_processos_t *tabela_processos, int clock, int delta_clock) {
    if (escalonador->em_execucao != NULL && proc_em_execucao(escalonador->em_execucao)) {
        return escalonador->em_execucao;
    }
    processo_t **processos = tabela_processos->processos;
    size_t tam = tabela_processos->tam;

    escalonador->em_execucao = NULL;

    for (size_t i = 0; i < tam; i++) {
        processo_t *proc = processos[i];
        if (proc != NULL && proc_pronto(proc)) {
            escalonador->em_execucao = proc;
        }
    }

    return escalonador->em_execucao;
}

void esc_remover_processo_execucao(escalonador_t *escalonador) {
    escalonador->em_execucao = NULL;
}
