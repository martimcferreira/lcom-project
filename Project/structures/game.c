#include "game.h"

Note notes[MAX_NOTES];

void init_notes() {
    // Coloca todas as notas no estado inativo no arranque do jogo
    for (int i = 0; i < MAX_NOTES; i++) {
        notes[i].active = false;
        notes[i].missed = false;
    }
}

int update_notes() {
    int misses = 0;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (notes[i].active) {
            // Atualiza a posição vertical
            notes[i].y += notes[i].speed;

            // Se a nota ultrapassar a zona de acerto sem ter sido acertada -> miss passivo
            if (notes[i].y > HIT_ZONE_BOTTOM && !notes[i].missed) {
                notes[i].missed = true;
                misses++;
            }

            // Apenas desativar visualmente quando sai completamente do ecrã
            if (notes[i].y > 600) {
                notes[i].active = false;
            }
        }
    }
    return misses;
}
