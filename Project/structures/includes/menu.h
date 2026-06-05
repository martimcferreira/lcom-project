#ifndef _MENU_H_
#define _MENU_H_

#include <lcom/lcf.h>
#include <stdbool.h> 
#include "devices/video/video.h" 

// Enumeração global dos estados do jogo
typedef enum {
  MENU,
  USERNAME_ENTRY,
  SONG_SELECT,
  PLAY,
  PAUSE,
  GAME_OVER,
  LEADERBOARD
} GameState;

// Declaração das variáveis de hover para estarem acessíveis
extern bool hover_play;
extern bool hover_exit;
extern bool hover_leaderboard;
extern bool hover_song1;
extern bool hover_song2;
extern bool hover_song3;
extern bool hover_song4;
extern bool hover_back;

extern int song_id;

// Variáveis para navegação por teclado (0 = nada/rato, 1..N = índice selecionado)
extern int kbd_menu_idx;
extern int kbd_song_idx;
extern int kbd_pause_idx;
// Função responsável por desenhar o menu principal
void draw_main_menu(int mouse_x, int mouse_y, uint32_t *bg_map, xpm_image_t bg_img);

// Função que verifica cliques no menu principal
void check_menu_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state, bool *game_running);

// Função responsável por desenhar o ecrã de seleção de música
void draw_song_select(int mouse_x, int mouse_y, const uint32_t *bg_map, xpm_image_t bg_img);

// Função que verifica cliques no ecrã de seleção de música
void check_song_select_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state);

#endif /* _MENU_H_ */
