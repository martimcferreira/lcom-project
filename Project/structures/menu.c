#include "structures/includes/menu.h"
#include "devices/video/video.h"

// ============================================================
// VARIÁVEIS DE HOVER
// ============================================================
bool hover_play  = false;
bool hover_exit  = false;
bool hover_song1 = false;
bool hover_song2 = false;
bool hover_back  = false;

// ============================================================
// MENU PRINCIPAL  (PLAY + EXIT)
// ============================================================
#define BTN_WIDTH   200
#define BTN_HEIGHT   60
#define BTN_PLAY_X  300
#define BTN_PLAY_Y  220
#define BTN_EXIT_X  300
#define BTN_EXIT_Y  310

void draw_main_menu(int mouse_x, int mouse_y) {
    hover_play = (mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
                  mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT);

    hover_exit = (mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
                  mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT);

    // --- BOTÃO PLAY (verde) ---
    uint32_t cor_play = hover_play ? 0x00FF00 : 0x007700;
    vg_draw_rectangle(BTN_PLAY_X, BTN_PLAY_Y, BTN_WIDTH, BTN_HEIGHT, cor_play);
    // Triângulo branco (ícone play)
    for (int i = 0; i < 20; i++) {
        vg_draw_rectangle(390 + i, BTN_PLAY_Y + 10 + i, 2, 40 - 2*i, 0xFFFFFF);
    }

    // --- BOTÃO EXIT (vermelho) ---
    uint32_t cor_exit = hover_exit ? 0xFF3333 : 0x880000;
    vg_draw_rectangle(BTN_EXIT_X, BTN_EXIT_Y, BTN_WIDTH, BTN_HEIGHT, cor_exit);
    // Cruz branca (ícone exit)
    for (int i = 0; i < 20; i++) {
        vg_draw_rectangle(390 + i, BTN_EXIT_Y + 10 + i, 4, 4, 0xFFFFFF);
        vg_draw_rectangle(410 - i, BTN_EXIT_Y + 10 + i, 4, 4, 0xFFFFFF);
    }
}

void check_menu_clicks(int mouse_x, int mouse_y, bool left_click,
                       GameState *current_state, bool *game_running) {
    if (!left_click) return;

    if (mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
        mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT) {
        *current_state = SONG_SELECT;   /* vai para o ecrã de escolha */
    } else if (mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
               mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT) {
        *game_running = false;
    }
}

// ============================================================
// ECRÃ DE SELEÇÃO DE MÚSICA
// ============================================================
#define SONG_BTN_W  320
#define SONG_BTN_H   80
#define SONG1_X      240
#define SONG1_Y      180
#define SONG2_X      240
#define SONG2_Y      290
#define BACK_BTN_W  120
#define BACK_BTN_H   40
#define BACK_BTN_X  340
#define BACK_BTN_Y  400

void draw_song_select(int mouse_x, int mouse_y) {
    hover_song1 = (mouse_x >= SONG1_X && mouse_x <= SONG1_X + SONG_BTN_W &&
                   mouse_y >= SONG1_Y && mouse_y <= SONG1_Y + SONG_BTN_H);

    hover_song2 = (mouse_x >= SONG2_X && mouse_x <= SONG2_X + SONG_BTN_W &&
                   mouse_y >= SONG2_Y && mouse_y <= SONG2_Y + SONG_BTN_H);

    hover_back  = (mouse_x >= BACK_BTN_X && mouse_x <= BACK_BTN_X + BACK_BTN_W &&
                   mouse_y >= BACK_BTN_Y && mouse_y <= BACK_BTN_Y + BACK_BTN_H);

    // --- BOTÃO SONG 1 ---
    uint32_t s1_base  = (song_id == 1) ? 0x009999 : 0x333333;
    uint32_t s1_hover = (song_id == 1) ? 0x00FFFF : 0x555555;
    uint32_t cor_s1   = hover_song1 ? s1_hover : s1_base;
    vg_draw_rectangle(SONG1_X, SONG1_Y, SONG_BTN_W, SONG_BTN_H, cor_s1);

    /* Borda brilhante se selecionado */
    if (song_id == 1) {
        vg_draw_rectangle(SONG1_X,   SONG1_Y,   SONG_BTN_W, 3,          0x00FFFF);
        vg_draw_rectangle(SONG1_X,   SONG1_Y+SONG_BTN_H-3, SONG_BTN_W, 3, 0x00FFFF);
        vg_draw_rectangle(SONG1_X,   SONG1_Y,   3, SONG_BTN_H,          0x00FFFF);
        vg_draw_rectangle(SONG1_X+SONG_BTN_W-3, SONG1_Y, 3, SONG_BTN_H, 0x00FFFF);
    }

    /* Dígito "1" */
    vg_draw_rectangle(SONG1_X + 16, SONG1_Y + 12, 5, 56, 0xFFFFFF); /* barra vertical */
    vg_draw_rectangle(SONG1_X +  8, SONG1_Y + 12, 8, 5,  0xFFFFFF); /* serifa superior */

    // --- BOTÃO SONG 2
    uint32_t s2_base  = (song_id == 2) ? 0x880088 : 0x333333;
    uint32_t s2_hover = (song_id == 2) ? 0xFF00FF : 0x555555;
    uint32_t cor_s2   = hover_song2 ? s2_hover : s2_base;
    vg_draw_rectangle(SONG2_X, SONG2_Y, SONG_BTN_W, SONG_BTN_H, cor_s2);

    if (song_id == 2) {
        vg_draw_rectangle(SONG2_X,   SONG2_Y,   SONG_BTN_W, 3,           0xFF00FF);
        vg_draw_rectangle(SONG2_X,   SONG2_Y+SONG_BTN_H-3, SONG_BTN_W, 3, 0xFF00FF);
        vg_draw_rectangle(SONG2_X,   SONG2_Y,   3, SONG_BTN_H,           0xFF00FF);
        vg_draw_rectangle(SONG2_X+SONG_BTN_W-3, SONG2_Y, 3, SONG_BTN_H,  0xFF00FF);
    }

    /* Dígito "2"*/
    vg_draw_rectangle(SONG2_X +  8, SONG2_Y + 12, 25, 5, 0xFFFFFF); /* topo */
    vg_draw_rectangle(SONG2_X + 28, SONG2_Y + 12, 5, 19, 0xFFFFFF); /* vertical direita superior */
    vg_draw_rectangle(SONG2_X +  8, SONG2_Y + 27, 25, 5, 0xFFFFFF); /* meio */
    vg_draw_rectangle(SONG2_X +  8, SONG2_Y + 27, 5, 18, 0xFFFFFF); /* vertical esquerda inferior */
    vg_draw_rectangle(SONG2_X +  8, SONG2_Y + 41, 25, 5, 0xFFFFFF); /* fundo */

    // --- BOTÃO BACK ---
    uint32_t cor_back = hover_back ? 0x888888 : 0x444444;
    vg_draw_rectangle(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, cor_back);
    /* Seta para voltar ao menu principal*/
    for (int i = 0; i < 10; i++) {
        vg_draw_rectangle(BACK_BTN_X + 10 + i, BACK_BTN_Y + 20 - i, 3, 3, 0xFFFFFF); /* braço superior */
        vg_draw_rectangle(BACK_BTN_X + 10 + i, BACK_BTN_Y + 20 + i, 3, 3, 0xFFFFFF); /* braço inferior */
    }
    vg_draw_rectangle(BACK_BTN_X + 20, BACK_BTN_Y + 18, 60, 4, 0xFFFFFF); /* linha horizontal */
}

void check_song_select_clicks(int mouse_x, int mouse_y, bool left_click,
                               GameState *current_state) {
    if (!left_click) return;

    if (mouse_x >= SONG1_X && mouse_x <= SONG1_X + SONG_BTN_W &&
        mouse_y >= SONG1_Y && mouse_y <= SONG1_Y + SONG_BTN_H) {
        song_id = 1;
        *current_state = PLAY;
    } else if (mouse_x >= SONG2_X && mouse_x <= SONG2_X + SONG_BTN_W &&
               mouse_y >= SONG2_Y && mouse_y <= SONG2_Y + SONG_BTN_H) {
        song_id = 2;
        *current_state = PLAY;
    } else if (mouse_x >= BACK_BTN_X && mouse_x <= BACK_BTN_X + BACK_BTN_W &&
               mouse_y >= BACK_BTN_Y && mouse_y <= BACK_BTN_Y + BACK_BTN_H) {
        *current_state = MENU;
    }
}
