#include "structures/includes/game.h"

Note notes[MAX_NOTES];

void init_notes() {
    // Coloca todas as notas no estado inativo no arranque do jogo
    for (int i = 0; i < MAX_NOTES; i++) {
        notes[i].active = false;
    }
}

void update_notes() {
    for (int i = 0; i < MAX_NOTES; i++) {
        if (notes[i].active) {
            // Atualiza a posição vertical
            notes[i].y += notes[i].speed; 

            // Se a nota ultrapassar o limite inferior do ecrã (ex: 600 píxeis)
            if (notes[i].y > 600) {
                notes[i].active = false; // Desativa a nota
            }
        }
    }
}
