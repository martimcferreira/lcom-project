#ifndef _MENU_H_
#define _MENU_H_

#include <lcom/lcf.h>
#include <stdbool.h> 
#include "devices/video/video.h" 

// Enumeração global dos estados do jogo
typedef enum {
  MENU,
  PLAY,
  GAME_OVER
} GameState;

// Declaração das variáveis de hover para estarem acessíveis
extern bool hover_play;
extern bool hover_options;
extern bool hover_exit;

// Função responsável apenas por desenhar o menu principal no back buffer
void draw_main_menu(int mouse_x, int mouse_y);

// Função que verifica se houve um clique num botão e muda o estado
void check_menu_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state, bool *game_running);

#endif /* _MENU_H_ */
