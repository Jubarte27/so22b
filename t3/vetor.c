#include "vetor.h"
#include <string.h>
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