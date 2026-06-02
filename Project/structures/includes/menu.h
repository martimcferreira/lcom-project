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
  GAME_OVER,
  LEADERBOARD
} GameState;

// Declaração das variáveis de hover para estarem acessíveis
extern bool hover_play;
extern bool hover_exit;
extern bool hover_leaderboard;
extern bool hover_song1;
extern bool hover_song2;
extern bool hover_back;

extern int song_id;

// Função responsável por desenhar o menu principal
void draw_main_menu(int mouse_x, int mouse_y);

// Função que verifica cliques no menu principal
void check_menu_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state, bool *game_running);

// Função responsável por desenhar o ecrã de seleção de música
void draw_song_select(int mouse_x, int mouse_y);

// Função que verifica cliques no ecrã de seleção de música
void check_song_select_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state);

#endif /* _MENU_H_ */
