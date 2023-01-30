#include "escalonador.h"


bool precisa_escalonar(tabela_processos_t *tabela_processos, size_t num_proc_execucao) {
    return num_proc_execucao >= tabela_processos->tam || proc_em_execucao(tabela_processos->processos[num_proc_execucao]);
}


processo_t *escalonar(tabela_processos_t *tabela_processos) {
    processo_t **processos = tabela_processos->processos;
    size_t tam = tabela_processos->tam;

    processo_t *proc_escolhido = NULL;

    size_t i;
    for (i = 0; i < tam; i++) {
        processo_t *proc = processos[i];
        if (proc != NULL) {
            if (proc_pronto(proc)) {
                proc_escolhido = proc;
            }
        }
    }

    return proc_escolhido;
}