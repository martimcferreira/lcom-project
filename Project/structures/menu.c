#include "structures/includes/menu.h"
#include "devices/video/video.h"

// ============================================================
// VARIÁVEIS DE HOVER
// ============================================================
bool hover_play  = false;
bool hover_exit  = false;
bool hover_leaderboard = false;
bool hover_song1 = false;
bool hover_song2 = false;
bool hover_back  = false;

int kbd_menu_idx = 1; // 1: Play, 2: Leaderboard, 3: Exit
int kbd_song_idx = 1; // 1: Song1, 2: Song2, 3: Back
int kbd_pause_idx = 1; // 1: Resume, 2: Quit

MenuInputMode menu_input_mode = MENU_INPUT_KEYBOARD;

void menu_set_keyboard_input(void) {
    menu_input_mode = MENU_INPUT_KEYBOARD;
}

void menu_set_mouse_input(void) {
    menu_input_mode = MENU_INPUT_MOUSE;
}

bool menu_keyboard_active(void) {
    return menu_input_mode == MENU_INPUT_KEYBOARD;
}

bool menu_mouse_active(void) {
    return menu_input_mode == MENU_INPUT_MOUSE;
}

// ============================================================
// MENU PRINCIPAL  (PLAY + LEADERBOARD + EXIT)
// ============================================================
#define BTN_WIDTH   300
#define BTN_HEIGHT   70
#define BTN_PLAY_X  250
#define BTN_PLAY_Y  240
#define BTN_LEADERBOARD_X 250
#define BTN_LEADERBOARD_Y 330
#define BTN_EXIT_X  250
#define BTN_EXIT_Y  420

static void draw_border(int x, int y, int w, int h, uint32_t color, int thickness) {
    vg_draw_rectangle(x - thickness, y - thickness, w + 2*thickness, thickness, color);
    vg_draw_rectangle(x - thickness, y + h, w + 2*thickness, thickness, color);
    vg_draw_rectangle(x - thickness, y, thickness, h, color);
    vg_draw_rectangle(x + w, y, thickness, h, color);
}

void draw_neo_btn(int x, int y, int w, int h, uint32_t bg_color, bool hovered) {
    int offset = hovered ? 8 : 4;
    
    // Sombra Neobrutalism
    vg_draw_rectangle(x + offset, y + offset, w, h, 0x000000);
    
    // Fundo do botão todo
    uint32_t fill_color = bg_color;
    if (hovered) {
        uint8_t r = (bg_color >> 16) & 0xFF;
        uint8_t g = (bg_color >> 8) & 0xFF;
        uint8_t b = bg_color & 0xFF;
        r = r + (255 - r) / 2;
        g = g + (255 - g) / 2;
        b = b + (255 - b) / 2;
        fill_color = (r << 16) | (g << 8) | b;
    }
    vg_draw_rectangle(x, y, w, h, fill_color);
    
    // Borda exterior
    uint32_t border_color = hovered ? 0xFFFF00 : 0x000000; // Amarelo vivo quando selecionado
    int border_thickness = hovered ? 6 : 3;
    draw_border(x, y, w, h, border_color, border_thickness);
}
void draw_main_menu(int mouse_x, int mouse_y, uint32_t *bg_map, xpm_image_t bg_img) {
    
    hover_play = (menu_mouse_active() &&
                  mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
                  mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT) ||
                 (menu_keyboard_active() && kbd_menu_idx == 1);

    hover_leaderboard = (menu_mouse_active() &&
                         mouse_x >= BTN_LEADERBOARD_X && mouse_x <= BTN_LEADERBOARD_X + BTN_WIDTH &&
                         mouse_y >= BTN_LEADERBOARD_Y && mouse_y <= BTN_LEADERBOARD_Y + BTN_HEIGHT) ||
                        (menu_keyboard_active() && kbd_menu_idx == 2);

    hover_exit = (menu_mouse_active() &&
                  mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
                  mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT) ||
                 (menu_keyboard_active() && kbd_menu_idx == 3);

    vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, 0);

    draw_neo_btn(BTN_PLAY_X, BTN_PLAY_Y, BTN_WIDTH, BTN_HEIGHT, 0x66FF99, hover_play);
    draw_neo_btn(BTN_LEADERBOARD_X, BTN_LEADERBOARD_Y, BTN_WIDTH, BTN_HEIGHT, 0x66DDFF, hover_leaderboard);
    draw_neo_btn(BTN_EXIT_X, BTN_EXIT_Y, BTN_WIDTH, BTN_HEIGHT, 0xFF7799, hover_exit);
}

void check_menu_clicks(int mouse_x, int mouse_y, bool left_click,
                       GameState *current_state, bool *game_running) {
    if (!left_click) return;

    if (mouse_x >= BTN_PLAY_X && mouse_x <= BTN_PLAY_X + BTN_WIDTH &&
        mouse_y >= BTN_PLAY_Y && mouse_y <= BTN_PLAY_Y + BTN_HEIGHT) {
        *current_state = USERNAME_ENTRY;   /* primeiro pede o nome do jogador */
    } else if (mouse_x >= BTN_LEADERBOARD_X && mouse_x <= BTN_LEADERBOARD_X + BTN_WIDTH &&
               mouse_y >= BTN_LEADERBOARD_Y && mouse_y <= BTN_LEADERBOARD_Y + BTN_HEIGHT) {
        *current_state = LEADERBOARD;
    } else if (mouse_x >= BTN_EXIT_X && mouse_x <= BTN_EXIT_X + BTN_WIDTH &&
               mouse_y >= BTN_EXIT_Y && mouse_y <= BTN_EXIT_Y + BTN_HEIGHT) {
        *game_running = false;
    }
}

// ============================================================
// ECRÃ DE SELEÇÃO DE MÚSICA
// ============================================================
#define SONG_BTN_W  320
#define SONG_BTN_H   70
#define SONG1_X      240
#define SONG1_Y      120
#define SONG2_X      240
#define SONG2_Y      210
#define SONG3_X      240
#define SONG3_Y      300
#define SONG4_X      240
#define SONG4_Y      390

#define BACK_BTN_W  250
#define BACK_BTN_H   70
#define BACK_BTN_X  275
#define BACK_BTN_Y  500

void draw_song_select(int mouse_x, int mouse_y, const uint32_t *bg_map, xpm_image_t bg_img) {
    if (bg_map != NULL) {
        vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
    } else {
        vg_clear_back_buffer(0x000000);
    }

    hover_song1 = (menu_mouse_active() &&
                   mouse_x >= SONG1_X && mouse_x <= SONG1_X + SONG_BTN_W &&
                   mouse_y >= SONG1_Y && mouse_y <= SONG1_Y + SONG_BTN_H) ||
                  (menu_keyboard_active() && kbd_song_idx == 1);

    hover_song2 = (menu_mouse_active() &&
                   mouse_x >= SONG2_X && mouse_x <= SONG2_X + SONG_BTN_W &&
                   mouse_y >= SONG2_Y && mouse_y <= SONG2_Y + SONG_BTN_H) ||
                  (menu_keyboard_active() && kbd_song_idx == 2);

    hover_back  = (menu_mouse_active() &&
                   mouse_x >= BACK_BTN_X && mouse_x <= BACK_BTN_X + BACK_BTN_W &&
                   mouse_y >= BACK_BTN_Y && mouse_y <= BACK_BTN_Y + BACK_BTN_H) ||
                  (menu_keyboard_active() && kbd_song_idx == 3);

    // --- BOTÃO SONG 1 ---
    uint32_t s1_base  = (song_id == 1) ? 0x00CC66 : 0x008844;
    draw_neo_btn(SONG1_X, SONG1_Y, SONG_BTN_W, SONG_BTN_H, s1_base, hover_song1);

    // --- BOTÃO SONG 2 ---
    uint32_t s2_base  = (song_id == 2) ? 0xCC3333 : 0x882222;
    draw_neo_btn(SONG2_X, SONG2_Y, SONG_BTN_W, SONG_BTN_H, s2_base, hover_song2);

    // --- BOTÃO SONG 3 (LOCKED) ---
    draw_neo_btn(SONG3_X, SONG3_Y, SONG_BTN_W, SONG_BTN_H, 0x444444, false);

    // --- BOTÃO SONG 4 (LOCKED) ---
    draw_neo_btn(SONG4_X, SONG4_Y, SONG_BTN_W, SONG_BTN_H, 0x444444, false);

    // --- BOTÃO BACK ---
    draw_neo_btn(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 0xFFCC00, hover_back);
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
