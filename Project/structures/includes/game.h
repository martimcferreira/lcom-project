#pragma once
#include <stdbool.h>

typedef struct {
    int x;         // Posição horizontal
    int y;         // Posição vertical atual
    int speed;     // Velocidade de descida
    bool active;   // Indica se a nota está a cair
} Note;

#define MAX_NOTES 100 

void init_notes();

/**
 * Atualiza a posição de todas as notas ativas.
 * @return Número de notas que passaram o limite inferior sem serem acertadas (misses).
 */
int update_notes();
