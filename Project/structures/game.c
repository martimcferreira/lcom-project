#include "structures/includes/game.h"

Note notes[MAX_NOTES];

void init_notes() {
    // Coloca todas as notas no estado inativo no arranque do jogo
    for (int i = 0; i < MAX_NOTES; i++) {
        notes[i].active = false;
    }
}

int update_notes() {
    int misses = 0;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (notes[i].active) {
            // Atualiza a posição vertical
            notes[i].y += notes[i].speed;

            // Se a nota ultrapassar o limite inferior do ecrã (530px)
            // sem ter sido acertada pelo jogador -> miss passivo
            if (notes[i].y > 530) {
                notes[i].active = false;
                misses++;
            }
        }
    }
    return misses;
}
