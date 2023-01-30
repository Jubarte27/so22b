#include "so.h"
#include "tela.h"
#include "processo.h"
#include "escalonador.h"
#include <stdlib.h>
#include <string.h>

#define REL_CICLOS 99
#define REL_TEMPO_TOTAL 98

typedef struct info_es info_es;

struct so_t {
    contr_t *contr;         // o controlador do hardware
    bool paniquei;          // apareceu alguma situação intratável
    tabela_processos_t *tabela_processos;
    size_t num_proc_em_execucao;
    programa_t **programas;    //vetor com os programas
    size_t qtd_programas;   // quantidade de programas no vetor
    escalonador_t *escalonador;
    int clock;
    int delta_clock;
};

struct info_es {
    int disp;
    acesso_t tipo_acesso;
};

// funções auxiliares
info_es *dados_es(processo_t *self);
void so_carrega_programas(so_t *self);
static void panico(so_t *self);
void so_primeiro_processo(so_t *self);
void so_troca_processo(so_t *self, processo_t *proc_novo);
processo_t *processo_em_execucao(so_t *self);
void so_alterar_estados_processos_bloqueados(so_t *self);
void incrementa_pc(cpu_estado_t *cpue);
void muda_modo(so_t *self, cpu_modo_t modo);
void so_carrega_contexto(so_t *self, processo_t *proc);
err_t so_cria_proc(so_t *self, processo_t **proc, size_t num_programa);
void so_proc_altera_estado(so_t *self, processo_t *proc, proc_estado_t estado);
void so_proc_bloqueia(so_t *self, processo_t *processo, tipo_bloqueio_processo tipo_bloqueio, void *info_bloqueio);
void so_atualiza_metricas_processos(so_t *self);
void salva_metrica(processo_t *processo);

so_t *so_cria(contr_t *contr) {
    so_t *self = malloc(sizeof(*self));
    if (self == NULL) return NULL;
    self->contr = contr;
    self->paniquei = false;
    so_carrega_programas(self);
    self->tabela_processos = tabela_cria(self->qtd_programas);
    // inicializa a memória com o programa 0
    so_primeiro_processo(self);
    self->escalonador = esc_cria(processo_em_execucao(self));
    so_carrega_contexto(self, processo_em_execucao(self));
    // coloca a CPU em modo usuário
    muda_modo(self, usuario);
    return self;
}

void so_destroi(so_t *self) {
    /*Matar os processos*/
    free(self);
}

// trata chamadas de sistema

// chamada de sistema para leitura de E/S
// recebe em A a identificação do dispositivo
// retorna em X o valor lido
//            A o código de erro
static void so_trata_sisop_le(so_t *self) {
    // faz leitura assíncrona.
    // deveria ser síncrono, verificar es_pronto() e bloquear o processo
    processo_t *proc = processo_em_execucao(self);
    es_t *es = contr_es(self->contr);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc_cpue(proc);
    int disp = cpue_A(cpue);
    if (es_pronto(es, disp, leitura)) {
        int val;
        err_t err = es_le(es, disp, &val);
        cpue_muda_A(cpue, err);
        if (err == ERR_OK) {
            cpue_muda_X(cpue, val);
        }
        // incrementa o PC
        incrementa_pc(cpue);
        // altera o estado da CPU (deveria alterar o estado do processo)
        exec_altera_estado(exec, cpue);
    } else {
        so_proc_bloqueia(self, proc, ES, dados_es(proc));
    }
}

// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self) {
    processo_t *proc = processo_em_execucao(self);
    es_t *es = contr_es(self->contr);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc_cpue(proc);
    int disp = cpue_A(cpue);
    if (es_pronto(es, disp, escrita)) {
        // deveria ser síncrono, verificar es_pronto() e bloquear o processo
        // faz escrita assíncrona.
        int val = cpue_X(cpue);
        err_t err = es_escreve(es, disp, val);
        cpue_muda_A(cpue, err);
        // incrementa o PC
        incrementa_pc(cpue);
        // altera o estado da CPU (deveria alterar o estado do processo)
        exec_altera_estado(exec, cpue);
    } else {
        so_proc_bloqueia(self, proc, ES, dados_es(proc));
    }
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self) {
    processo_t *proc = processo_em_execucao(self);
    salva_metrica(proc);
    if (proc_info_bloqueio(proc))
        free(proc_info_bloqueio(proc));
    proc_destroi(proc);
    esc_remover_processo_execucao(self->escalonador);
    self->tabela_processos->processos[self->num_proc_em_execucao] = NULL;
    self->num_proc_em_execucao = self->tabela_processos->tam;
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self) {
    processo_t *proc_em_execucao = processo_em_execucao(self);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc_cpue(proc_em_execucao);

    int num_programa = cpue_A(cpue);
    processo_t *proc;
    err_t err = so_cria_proc(self, &proc, num_programa);
    cpue_muda_A(cpue, err);
    tabela_adiciona_processo(self->tabela_processos, proc);
    // incrementa o PC
    incrementa_pc(cpue);
    // altera o estado da CPU (deveria alterar o estado do processo)
    exec_altera_estado(exec, cpue);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self) {
    // o tipo de chamada está no "complemento" do cpue
    processo_t *proc = processo_em_execucao(self);
    exec_copia_estado(contr_exec(self->contr), proc_cpue(proc));
    so_chamada_t chamada = cpue_complemento(proc_cpue(proc));
    switch (chamada) {
        case SO_LE:
            so_trata_sisop_le(self);
            break;
        case SO_ESCR:
            so_trata_sisop_escr(self);
            break;
        case SO_FIM:
            so_trata_sisop_fim(self);
            break;
        case SO_CRIA:
            so_trata_sisop_cria(self);
            break;
        default:
            t_printf("so: chamada de sistema não reconhecida %d\n", chamada);
            panico(self);
    }
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self) {
    // TODO: tratar a interrupção do relógio
}

void so_muda_cpu_erro(so_t *self) {
    processo_t *proc = processo_em_execucao(self);
    cpu_estado_t *cpue = proc == NULL ? cpue_cria() : proc_cpue(proc);
    if (cpue_erro(cpue) == ERR_SISOP && !self->paniquei) {
        // interrupção da cpu foi atendida
        exec_copia_estado(contr_exec(self->contr), cpue);
        cpue_muda_erro(cpue, ERR_OK, 0);
        exec_altera_estado(contr_exec(self->contr), cpue);
    }
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err) {
    muda_modo(self, supervisor);
    so_atualiza_metricas_processos(self);
    if (processo_em_execucao(self) != NULL) {
        switch (err) {
            case ERR_SISOP:
                so_trata_sisop(self);
                break;
            case ERR_TIC:
                so_trata_tic(self);
                break;
            default:
                t_printf("SO: interrupção não tratada [%s]", err_nome(err));
                self->paniquei = true;
        }
    }
    so_alterar_estados_processos_bloqueados(self);
    processo_t *novo_processo = esc_escalonar(self->escalonador, self->tabela_processos, self->clock, self->delta_clock);
    so_troca_processo(self, novo_processo);
    so_muda_cpu_erro(self);
    muda_modo(self, usuario);
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self) {
    return !self->paniquei;
}

void finalizar(so_t *self) {
    self->paniquei = true;
}

static void panico(so_t *self) {
    t_printf("Problema irrecuperável no SO");
    finalizar(self);
}

void muda_modo(so_t *self, cpu_modo_t modo) {
    processo_t *proc = processo_em_execucao(self);
    cpu_estado_t *cpue = proc == NULL ? cpue_cria() : proc_cpue(proc);
    exec_copia_estado(contr_exec(self->contr), cpue);
    cpue_muda_modo(cpue, modo);
    exec_altera_estado(contr_exec(self->contr), cpue);
}

// Programas

programa_t *programa_cria(int *codigo, size_t tam) {
    programa_t *programa = malloc(sizeof(programa_t));

    int *codigo_heap = malloc(sizeof(int) * tam);
    memcpy(codigo_heap, codigo, sizeof(int) * tam);

    programa->tam = tam;
    programa->codigo = codigo_heap;
    return programa;
}

void so_carrega_programas(so_t *self) {
    self->qtd_programas = 5;
    self->programas = malloc(sizeof(programa_t) * self->qtd_programas);
    size_t i = 0;

    /*É uma péssima maneira de se fazer isso, mas prefiro assim do que repetir o código todo*/
    #define PROGRAMAS self->programas
    #define I i

    #define VAR init
    #include "carrega_programa.h"

    #define VAR grande_cpu
    #include "carrega_programa.h"

    #define VAR grande_es
    #include "carrega_programa.h"

    #define VAR peq_cpu
    #include "carrega_programa.h"

    #define VAR peq_es
    #include "carrega_programa.h"

    #undef PROGRAMAS
    #undef I
}

/** Retorna uma cópia da memória original.
 * Se o destino for nulo, cria uma memória nova, senão, utitliza o próprio destino
 **/
void copia_memoria(mem_t *destino, mem_t *original) {
    int tam_original = mem_tam(original);

    for (int i = 0; i < tam_original; i++) {
        int valor;
        err_t erro_leitura = mem_le(original, i, &valor);
        if (erro_leitura != ERR_OK) {
            t_printf("copia_memoria: erro de leitura na memória, endereco %d\n", i);
        }
        err_t erro_escrita = mem_escreve(destino, i, valor);
        if (erro_escrita != ERR_OK) {
            t_printf("copia_memoria: erro de escrita na memória, endereco %d\n", i);
        }
    }
}

void so_primeiro_processo(so_t *self) {
    processo_t *proc;
    so_cria_proc(self, &proc, 0);
    self->num_proc_em_execucao = tabela_adiciona_processo(self->tabela_processos, proc);
    so_proc_altera_estado(self, proc, EM_EXECUCAO);
}

processo_t *processo_em_execucao(so_t *self) {
    if (self->num_proc_em_execucao >= self->tabela_processos->tam) {
        return NULL;
    } else {
        return self->tabela_processos->processos[self->num_proc_em_execucao];
    }
}


void incrementa_pc(cpu_estado_t *cpue) {
    cpue_muda_PC(cpue, cpue_PC(cpue) + 2);
}

void so_salvar_contexto(so_t *self, processo_t *proc) {
    copia_memoria(proc_mem(proc), contr_mem(self->contr));
    exec_copia_estado(contr_exec(self->contr), proc_cpue(proc));
}

void so_carrega_contexto(so_t *self, processo_t *proc) {
    copia_memoria(contr_mem(self->contr), proc_mem(proc));
    exec_altera_estado(contr_exec(self->contr), proc_cpue(proc));
}

void so_troca_processo(so_t *self, processo_t *proc_novo) {
    if (tabela_algum_proc_nao_nulo(self->tabela_processos)) {
        processo_t *proc_atual = processo_em_execucao(self);
        if (proc_novo == proc_atual) {
            return;
        }
        if (proc_atual != NULL) {
            so_salvar_contexto(self, proc_atual);
        }
        if (proc_novo == NULL) {
            /*Coloca no modo zumbi*/
            muda_modo(self, zumbi);
        } else {
            so_carrega_contexto(self, proc_novo);
            so_proc_altera_estado(self, proc_novo, EM_EXECUCAO);
            self->num_proc_em_execucao = proc_num(self->tabela_processos, proc_novo);
        }
    } else {
        /*Não há processos na tabela de processos, não há mais o que fazer*/
        finalizar(self);
    }
}

info_es *dados_es(processo_t *self) {
    info_es *dados = malloc(sizeof(info_es));
    dados->tipo_acesso = cpue_complemento(proc_cpue(self)) == SO_LE ? leitura : escrita;
    dados->disp = cpue_A(proc_cpue(self));

    return dados;
}

bool precisa_desbloquar(so_t *self, processo_t *proc) {
    if (proc == NULL || !proc_bloqueado(proc)) {
        return false;
    }
    switch (proc_tipo_bloqueio(proc)) {
        case ES:; // <- Só para o compilador não reclamar
            info_es *dados = proc_info_bloqueio(proc);
            return es_pronto(contr_es(self->contr), dados->disp, dados->tipo_acesso);
        default:
            return false;
    }
}

void so_alterar_estados_processos_bloqueados(so_t *self) {
    for (size_t i = 0; i < self->tabela_processos->tam; i++) {
        processo_t *proc = self->tabela_processos->processos[i];
        if (precisa_desbloquar(self, proc)) {
            so_proc_altera_estado(self, proc, PRONTO);
        }
    }
}

err_t so_cria_proc(so_t *self, processo_t **proc, size_t num_programa) {
    return proc_cria(proc, self->programas[num_programa], mem_tam(contr_mem(self->contr)));
}

void so_proc_bloqueia(so_t *self, processo_t *processo, tipo_bloqueio_processo tipo_bloqueio, void *info_bloqueio) {
    proc_bloqueia(processo, tipo_bloqueio, info_bloqueio, self->clock, self->delta_clock);
}

void so_proc_altera_estado(so_t *self, processo_t *proc, proc_estado_t estado) {
    proc_altera_estado(proc, estado, self->clock, self->delta_clock);
}

void so_atualiza_metricas_processos(so_t *self) {
    int clock;
    es_le(contr_es(self->contr), REL_CICLOS, &clock);
    self->delta_clock = clock - self->clock;
    self->clock = clock;

    tabela_processos_t *tabela_processos = self->tabela_processos;

    for (size_t i = 0; i < tabela_processos->tam; i++) {
        processo_t *processo = tabela_processos->processos[i];
        if (processo == NULL) {
            continue;
        }
        metricas_t *metricas = proc_metricas(processo);
        metricas->clk_tempo_total += self->delta_clock;
        if (proc_em_execucao(processo)) {
            metricas->clk_tempo_em_execucao += self->delta_clock;
        }
    }
}

void salva_metrica(processo_t *processo) {
    metricas_t *metricas = proc_metricas(processo);
    t_printf("(%u) total: %d, exec: %d, trocas: %d, ultima_troca: %d", processo, metricas->clk_tempo_total,
             metricas->clk_tempo_em_execucao, metricas->qtd_trocas_estado, metricas->clk_ultima_troca_estado);
}