#include "so.h"
#include "tela.h"
#include <stdlib.h>
#include <string.h>

typedef enum proc_estado_t proc_estado_t;
typedef struct so_proc_t so_proc_t;
typedef struct programa_t programa_t;
typedef struct tabela_processos_t tabela_processos_t;
typedef struct info_es info_es;

enum proc_estado_t {
    BLOQUEADO, PRONTO, EM_EXECUCAO
};

typedef enum tipo_bloqueio_processo {
    ES
} tipo_bloqueio_processo;

struct tabela_processos_t {
    so_proc_t **processos;
    size_t tam;
};

struct programa_t {
    int *codigo;
    size_t tam;
};

struct so_proc_t {
    cpu_estado_t *cpue;   // cópia do estado da CPU
    mem_t *mem;           // cópia da memória
    proc_estado_t estado;   // estado do processo
    tipo_bloqueio_processo tipo_bloqueio;
    void *info_bloqueio;
};

struct so_t {
    contr_t *contr;         // o controlador do hardware
    bool paniquei;          // apareceu alguma situação intratável
    tabela_processos_t *tabela_processos;
    size_t num_proc_em_execucao;
    programa_t **programas;    //vetor com os programas
    size_t qtd_programas;   // quantidade de programas no vetor
};

struct info_es {
    int disp;
    acesso_t tipo_acesso;
};

// funções auxiliares
void so_carrega_programas(so_t *self);
err_t so_cria_proc(so_t *self, so_proc_t **proc, size_t num_programa);
err_t so_destroi_proc(so_t *self, size_t num_processo);
static void panico(so_t *self);
void so_primeiro_processo(so_t *self);
tabela_processos_t *so_cria_tabela(size_t tam);
size_t tabela_adiciona_processo(tabela_processos_t *tabela, so_proc_t *processo);
void so_troca_processo(so_t *self, so_proc_t *proc_novo);
void so_bloqueia_processo(so_proc_t *proc, tipo_bloqueio_processo tipo_bloqueio);
so_proc_t *so_escalonar(so_t *self);
bool so_precisa_escalonar(so_t *self);
so_proc_t *processo_em_execucao(so_t *self);
size_t num_processo(tabela_processos_t *self, so_proc_t *proc);
void so_alterar_estados_processos_bloqueados(so_t *self);
void incrementa_pc(cpu_estado_t *cpue);
void muda_modo(so_t *self, cpu_modo_t modo);
void so_carrega_contexto(so_t *self, so_proc_t *proc);

so_t *so_cria(contr_t *contr) {
    so_t *self = malloc(sizeof(*self));
    if (self == NULL) return NULL;
    self->contr = contr;
    self->paniquei = false;
    so_carrega_programas(self);
    self->tabela_processos = so_cria_tabela(self->qtd_programas);
    // inicializa a memória com o programa 0
    so_primeiro_processo(self);
    t_printf("primeiro processo: %u", processo_em_execucao(self)->estado);
    so_carrega_contexto(self, processo_em_execucao(self));
    // coloca a CPU em modo usuário
    muda_modo(self, usuario);
    return self;
}

void so_destroi(so_t *self)
{
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
    so_proc_t *proc = processo_em_execucao(self);
    es_t *es = contr_es(self->contr);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc->cpue;
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
        so_bloqueia_processo(proc, ES);
    }
}

// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self) {
    so_proc_t *proc = processo_em_execucao(self);
    es_t *es = contr_es(self->contr);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc->cpue;
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
        so_bloqueia_processo(proc, ES);
    }
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self) {
    so_destroi_proc(self, self->num_proc_em_execucao);
    self->num_proc_em_execucao = self->tabela_processos->tam;
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self) {
    so_proc_t *proc_em_execucao = processo_em_execucao(self);
    exec_t *exec = contr_exec(self->contr);
    cpu_estado_t *cpue = proc_em_execucao->cpue;

    int num_programa = cpue_A(cpue);
    so_proc_t *proc;
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
    so_proc_t *proc = processo_em_execucao(self);
    exec_copia_estado(contr_exec(self->contr), proc->cpue);
    so_chamada_t chamada = cpue_complemento(proc->cpue);
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
static void so_trata_tic(so_t *self)
{
  // TODO: tratar a interrupção do relógio
}

void so_muda_cpu_erro(so_t *self) {
    so_proc_t *proc = processo_em_execucao(self);
    cpu_estado_t *cpue = proc == NULL ? cpue_cria() : proc->cpue;
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
    if (so_precisa_escalonar(self) && !self->paniquei) {
        so_troca_processo(self, so_escalonar(self));
    }
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
    so_proc_t *proc = processo_em_execucao(self);
    cpu_estado_t *cpue = proc == NULL ? cpue_cria() : proc->cpue;
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
    self->qtd_programas = 4;
    self->programas = malloc(sizeof(programa_t) * self->qtd_programas);
    size_t i = 0;

    /*É uma péssima maneira de se fazer isso, mas prefiro assim do que repetir o código todo*/
    #define PROGRAMAS self->programas
    #define I i

    #define MAQ "init.maq"
    #define VAR init
    #include "carrega_programa.h"

    #define MAQ "p1.maq"
    #define VAR p1
    #include "carrega_programa.h"

    #define MAQ "p2.maq"
    #define VAR p2
    #include "carrega_programa.h"

    #define MAQ "ex1.maq"
    #define VAR ex1
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

tabela_processos_t *so_cria_tabela(size_t tam) {
    tabela_processos_t *tabela = malloc(sizeof(tabela_processos_t));
    tabela->tam = tam;
    tabela->processos = calloc(tam, sizeof(so_proc_t*));
    return tabela;
}

typedef struct vetor {
    void *elementos;
    size_t tam;
    size_t tam_bloco;
} vetor_t;

/**
 * Realoca o vetor e seta as novas posições (caso existam) para 0
 * */
void vetor_realocar_elementos(vetor_t *self, size_t novo_tam) {
    size_t diferenca = novo_tam - self->tam;
    self->elementos = realloc(self->elementos, novo_tam * self->tam_bloco);
    if (diferenca > 0) {
        memset(self->elementos + self->tam, 0, diferenca);
    }
    self->tam = novo_tam;
}

/**
 * Retorna a posição da primeira aparição do alvo no vetor, ou o tamanho do vetor, caso não exista o alvo no vetor
 * */
size_t vetor_buscar(vetor_t *self, void *alvo) {
    void **elementos = self->elementos;
    size_t i;
    for (i = 0; i < self->tam; i++) {
        if (elementos[i] == alvo) {
            break;
        }
    }
    return i;
}

/**
 * Retorna a primeira posição nula, ou o tamanho do vetor, caso não exista elemento nulo no vetor
 * */
size_t vetor_primeiro_nulo(vetor_t *self) {
    return vetor_buscar(self, NULL);
}

/**
 * Retorna true se o vetor possui apena elementos nulos, ou possui tamanho 0
 * */
bool vetor_vazio(vetor_t *self) {
    void **elementos = self->elementos;
    size_t i;
    for (i = 0; i < self->tam; i++) {
        if (elementos[i] != NULL) {
            return false;
        }
    }
    return true;
}

/**
 * Adiciona o elemento na primeira posição nula. Expande o vetor e modifica self, se necessário
 * Retorna a posicao na qual o valor foi inserido.
 * */
size_t vetor_adiciona_primeira_posicao(vetor_t *self, void *valor) {
    size_t i = vetor_primeiro_nulo(self);
    if (i >= self->tam) {
        vetor_realocar_elementos(self, self->tam + 5);
    }
    void **elementos = self->elementos;
    elementos[i] = valor;
    return i;
}

vetor_t vetor_processos(tabela_processos_t *tabela) {
    vetor_t vetor = {
            tabela->processos,
            tabela->tam,
            sizeof(so_proc_t*)
    };
    return vetor;
}

void vetor_atualiza_processos(tabela_processos_t *tabela, vetor_t *vetor) {
    tabela->processos = vetor->elementos;
    tabela->tam = vetor->tam;
}

size_t tabela_adiciona_processo(tabela_processos_t *tabela, so_proc_t *processo) {
    vetor_t vetor = vetor_processos(tabela);
    size_t pos_processo = vetor_adiciona_primeira_posicao(&vetor, processo);
    vetor_atualiza_processos(tabela, &vetor);
    return pos_processo;
}

err_t so_cria_proc(so_t *self, so_proc_t **proc, size_t num_programa) {
    if (num_programa >= self->qtd_programas) {
        return ERR_OP_INV;
    }
    programa_t *programa = self->programas[num_programa];
    mem_t *mem = mem_cria(mem_tam(contr_mem(self->contr)));
    *proc = malloc(sizeof(so_proc_t));
    for (int i = 0; i < programa->tam; i++) {
        err_t erro = mem_escreve(mem, i, programa->codigo[i]);
        if (erro != ERR_OK) {
            t_printf("so.cria_proc: erro de memória, endereco %d\n", i);
            panico(self);
            mem_destroi(mem);
            free(proc);
            return erro;
        }
    }

    (*proc)->mem = mem;
    (*proc)->cpue = cpue_cria();
    (*proc)->estado = PRONTO;
    (*proc)->info_bloqueio = NULL;

    return ERR_OK;
}

err_t so_destroi_proc(so_t *self, size_t num_processo) {
    mem_destroi(self->tabela_processos->processos[num_processo]->mem);
    cpue_destroi(self->tabela_processos->processos[num_processo]->cpue);
    if (self->tabela_processos->processos[num_processo]->info_bloqueio) free(self->tabela_processos->processos[num_processo]->info_bloqueio);
    free(self->tabela_processos->processos[num_processo]);
    self->tabela_processos->processos[num_processo] = NULL;
    return ERR_OK;
}

void so_primeiro_processo(so_t *self) {
    so_proc_t *proc;
    so_cria_proc(self, &proc, 0);
    self->num_proc_em_execucao = tabela_adiciona_processo(self->tabela_processos, proc);
    proc->estado = EM_EXECUCAO;
}

so_proc_t *processo_em_execucao(so_t *self) {
    if (self->num_proc_em_execucao >= self->tabela_processos->tam) {
        return NULL;
    } else {
        return self->tabela_processos->processos[self->num_proc_em_execucao];
    }
}


void incrementa_pc(cpu_estado_t *cpue) {
    cpue_muda_PC(cpue, cpue_PC(cpue) + 2);
}

bool proc_esta_pronto(so_proc_t *proc) {
    return proc->estado == PRONTO;
}

void so_salvar_contexto(so_t *self, so_proc_t *proc) {
    copia_memoria(proc->mem, contr_mem(self->contr));
    exec_copia_estado(contr_exec(self->contr), proc->cpue);
}

void so_carrega_contexto(so_t *self, so_proc_t *proc) {
    copia_memoria(contr_mem(self->contr), proc->mem);
    exec_altera_estado(contr_exec(self->contr), proc->cpue);
}

bool so_precisa_escalonar(so_t *self) {
    so_proc_t *proc = processo_em_execucao(self);
    return proc == NULL || proc->estado != EM_EXECUCAO;
}

so_proc_t *so_escalonar(so_t *self) {
    so_proc_t **processos = self->tabela_processos->processos;
    size_t tam = self->tabela_processos->tam;

    so_proc_t *proc_escolhido = NULL;

    size_t i;
    for (i = 0; i < tam; i++) {
        so_proc_t *proc = processos[i];
        if (proc != NULL) {
            if (proc_esta_pronto(proc)) {
                proc_escolhido = proc;
            }
        }
    }

    return proc_escolhido;
}

bool algum_proc_nao_nulo(tabela_processos_t *self) {
    vetor_t vetor = vetor_processos(self);
    return !vetor_vazio(&vetor);
}

size_t num_processo(tabela_processos_t *self, so_proc_t *proc) {
    vetor_t vetor = vetor_processos(self);
    return vetor_buscar(&vetor, proc);
}

void so_troca_processo(so_t *self, so_proc_t *proc_novo) {
    if (algum_proc_nao_nulo(self->tabela_processos)) {
        so_proc_t *proc_atual = processo_em_execucao(self);
        if (proc_novo != proc_atual && proc_atual != NULL) {
            so_salvar_contexto(self, proc_atual);
        }
        if (proc_novo == NULL) {
            /*Coloca no modo zumbi*/
            muda_modo(self, zumbi);
        } else {
            so_carrega_contexto(self, proc_novo);
            proc_novo->estado = EM_EXECUCAO;
            self->num_proc_em_execucao = num_processo(self->tabela_processos, proc_novo);
        }
    } else {
        /*Não há processos na tabela de processos, não há mais o que fazer*/
        finalizar(self);
    }
}

info_es *dados_es(so_proc_t *self) {
    info_es *dados = malloc(sizeof(info_es));
    dados->tipo_acesso = cpue_complemento(self->cpue) == SO_LE ? leitura : escrita;
    dados->disp = cpue_A(self->cpue);

    return dados;
}

void so_bloqueia_processo(so_proc_t *proc, tipo_bloqueio_processo tipo_bloqueio) {
    proc->estado = BLOQUEADO;

    switch (tipo_bloqueio) { // NOLINT(hicpp-multiway-paths-covered)
        case ES:
            proc->tipo_bloqueio = ES;
            proc->info_bloqueio = dados_es(proc);
            break;
        default:
            break;
    }
}

bool precisa_desbloquar(so_t *self, so_proc_t *proc) {
    if (proc == NULL || proc->estado != BLOQUEADO) {
        return false;
    }
    switch (proc->tipo_bloqueio) {
        case ES:; // <- Só para o compilador não reclamar
            info_es *dados = proc->info_bloqueio;
            return es_pronto(contr_es(self->contr), dados->disp, dados->tipo_acesso);
        default:
            return false;
    }
}

void so_alterar_estados_processos_bloqueados(so_t *self) {
    for (size_t i = 0; i < self->tabela_processos->tam; i++) {
        so_proc_t *proc = self->tabela_processos->processos[i];
        if (precisa_desbloquar(self, proc)) {
            proc->estado = PRONTO;
        }
    }
}
