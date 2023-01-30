#include "processo.h"
#include "vetor.h"
#include "tela.h"

typedef struct programa_t programa_t;
struct processo_t {
    cpu_estado_t *cpue;   // cópia do estado da CPU
    mem_t *mem;           // cópia da memória
    proc_estado_t estado;   // estado do processo
    tipo_bloqueio_processo tipo_bloqueio;
    metricas_t *metricas;
    void *info_bloqueio;
};

err_t proc_cria(processo_t **proc, programa_t *programa, int tam_mem) {
    mem_t *mem = mem_cria(tam_mem);
    *proc = malloc(sizeof(processo_t));
    for (int i = 0; i < programa->tam; i++) {
        err_t erro = mem_escreve(mem, i, programa->codigo[i]);
        if (erro != ERR_OK) {
            t_printf("so.cria_proc: erro de memória, endereco %d\n", i);
            mem_destroi(mem);
            free(proc);
            return erro;
        }
    }

    (*proc)->mem = mem;
    (*proc)->cpue = cpue_cria();
    (*proc)->estado = PRONTO;
    (*proc)->info_bloqueio = NULL;
    (*proc)->metricas = malloc(sizeof(metricas_t));
    (*proc)->metricas->clk_tempo_em_execucao = 0;
    (*proc)->metricas->clk_tempo_total = 0;
    (*proc)->metricas->clk_ultima_troca_estado = 0;
    (*proc)->metricas->qtd_trocas_estado = 0;

    return ERR_OK;
}

err_t proc_destroi(processo_t *processo) {
    mem_destroi(processo->mem);
    cpue_destroi(processo->cpue);
    free(processo);
    return ERR_OK;
}

bool proc_pronto(processo_t *proc) {
    return proc->estado == PRONTO;
}


bool proc_bloqueado(processo_t *proc) {
    return proc->estado == BLOQUEADO;
}

bool proc_em_execucao(processo_t *proc) {
    return proc->estado == EM_EXECUCAO;
}

tipo_bloqueio_processo proc_tipo_bloqueio(processo_t *proc) {
    return proc->tipo_bloqueio;
}

void *proc_info_bloqueio(processo_t *proc) {
    return proc->info_bloqueio;
}

cpu_estado_t *proc_cpue(processo_t *self) {
    return self->cpue;
}

mem_t *proc_mem(processo_t *self) {
    return self->mem;
}

metricas_t *proc_metricas(processo_t *proc) {
    return proc->metricas;
}

void proc_bloqueia(processo_t *proc, tipo_bloqueio_processo tipo_bloqueio, void *info_bloqueio, int clock, int delta_clock) {
    proc_altera_estado(proc, BLOQUEADO, clock, delta_clock);
    proc->tipo_bloqueio = tipo_bloqueio;
    proc->info_bloqueio = info_bloqueio;
}

void proc_altera_estado(processo_t *proc, proc_estado_t estado, int clock, int delta_clock) {
    proc->estado = estado;
    proc->metricas->clk_ultima_troca_estado = clock;
    proc->metricas->qtd_trocas_estado++;
}

tabela_processos_t *tabela_cria(size_t tam) {
    tabela_processos_t *tabela = malloc(sizeof(tabela_processos_t));
    tabela->tam = tam;
    tabela->processos = calloc(tam, sizeof(processo_t *));
    return tabela;
}

vetor_t vetor_processos(tabela_processos_t *tabela) {
    vetor_t vetor = {
            tabela->processos,
            tabela->tam,
            sizeof(processo_t *)
    };
    return vetor;
}

size_t proc_num(tabela_processos_t *self, processo_t *proc) {
    vetor_t vetor = vetor_processos(self);
    return vetor_buscar(&vetor, proc);
}

bool tabela_algum_proc_nao_nulo(tabela_processos_t *self) {
    vetor_t vetor = vetor_processos(self);
    return !vetor_vazio(&vetor);
}

void vetor_atualiza_processos(tabela_processos_t *tabela, vetor_t *vetor) {
    tabela->processos = vetor->elementos;
    tabela->tam = vetor->tam;
}

size_t tabela_adiciona_processo(tabela_processos_t *tabela, processo_t *processo) {
    vetor_t vetor = vetor_processos(tabela);
    size_t pos_processo = vetor_adiciona_primeira_posicao(&vetor, processo);
    vetor_atualiza_processos(tabela, &vetor);
    return pos_processo;
}
