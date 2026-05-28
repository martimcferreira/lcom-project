#include "structures/includes/menu.h"
#include "devices/video/video.h"

// Inicialização das variáveis globais de hover
bool hover_play = false;
bool hover_options = false; 
bool hover_exit = false;

// Dimensões e posições dos botões
#define BTN_WIDTH  200
#define BTN_HEIGHT 60
#define BTN_PLAY_X 300
#define BTN_PLAY_Y 200
#define BTN_EXIT_X 300
#define BTN_EXIT_Y 300

void draw_main_menu(int mouse_x, int mouse_y) {
    // 1. Atualizar o estado do hover
    hover_play = (mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
                  mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT);
                  
    hover_exit = (mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
                  mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT);

    // --- DESENHO DO BOTÃO PLAY ---
    uint32_t cor_play = hover_play ? 0x00FF00 : 0x008800; 
    vg_draw_rectangle(BTN_PLAY_X, BTN_PLAY_Y, BTN_WIDTH, BTN_HEIGHT, cor_play);
    
    // Ícone PLAY (Triângulo branco desenhado à mão)
    // Desenhamos fatias verticais que vão diminuindo de altura
    for(int i = 0; i < 20; i++) {
        vg_draw_rectangle(390 + i, 210 + i, 2, 40 - 2*i, 0xFFFFFF);
    }

    // --- DESENHO DO BOTÃO EXIT ---
    uint32_t cor_exit = hover_exit ? 0xFF0000 : 0x880000;
    vg_draw_rectangle(BTN_EXIT_X, BTN_EXIT_Y, BTN_WIDTH, BTN_HEIGHT, cor_exit);
    
    // Ícone EXIT (Cruz branca desenhada à mão)
    // Desenhamos duas diagonais que se cruzam
    for(int i = 0; i < 20; i++) {
        vg_draw_rectangle(390 + i, 320 + i, 4, 4, 0xFFFFFF); // Diagonal principal
        vg_draw_rectangle(410 - i, 320 + i, 4, 4, 0xFFFFFF); // Diagonal invertida
    }
}

void check_menu_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state, bool *game_running) {
    if (!left_click) return;

    if (mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
        mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT) {
        *current_state = PLAY; 
    }
    else if (mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
             mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT) {
        *game_running = false; 
    }
}
