#pragma once
#include <stdbool.h>

typedef struct {
    int x;         // Posição horizontal
    int y;         // Posição vertical atual
    int speed;     // Velocidade de descida
    bool active;   // Indica se a nota está a cair
    bool missed;   // Indica se o miss lógico já foi contabilizado
} Note;

#define MAX_NOTES 100

#define LANE_BASE_X 200
#define LANE_WIDTH 80
#define NUM_LANES 5

#define HIT_ZONE_TOP 490
#define HIT_ZONE_BOTTOM 530
#define NOTE_HIT_HEIGHT 60

void init_notes();

/**
 * Atualiza a posição de todas as notas ativas.
 * @return Número de notas que passaram o limite inferior sem serem acertadas (misses).
 */
int update_notes();
