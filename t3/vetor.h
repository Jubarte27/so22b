//
// Created by jubarte on 29/01/23.
//

#ifndef SO22B_VETOR_H
#define SO22B_VETOR_H
#include <stdlib.h>
#include <stdbool.h>

typedef struct vetor {
    void *elementos;
    size_t tam;
    size_t tam_bloco;
} vetor_t;

/**
 * Realoca o vetor e seta as novas posições (caso existam) para 0
 * */
void vetor_realocar_elementos(vetor_t *self, size_t novo_tam);

/**
 * Retorna a posição da primeira aparição do alvo no vetor, ou o tamanho do vetor, caso não exista o alvo no vetor
 * */
size_t vetor_buscar(vetor_t *self, void *alvo);

/**
 * Retorna a primeira posição nula, ou o tamanho do vetor, caso não exista elemento nulo no vetor
 * */
size_t vetor_primeiro_nulo(vetor_t *self);
/**
 * Retorna true se o vetor possui apena elementos nulos, ou possui tamanho 0
 * */
bool vetor_vazio(vetor_t *self);

/**
 * Adiciona o elemento na primeira posição nula. Expande o vetor e modifica self, se necessário
 * Retorna a posicao na qual o valor foi inserido.
 * */
size_t vetor_adiciona_primeira_posicao(vetor_t *self, void *valor);

#endif //SO22B_VETOR_H
