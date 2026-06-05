/**
 * @file menu.h
 * @brief Menus e state machine.
 *
 * Máquina de estados do jogo, menus e UI.
 * 
 * @defgroup Menu Menu & States
 * @ingroup Core
 * @brief Gestão dos menus.
 * @{
 */

#ifndef _MENU_H_
#define _MENU_H_

#include <lcom/lcf.h>
#include <stdbool.h> 
#include "devices/video/video.h" 

/**
 * @brief States do jogo.
 */
typedef enum {
  MENU,             /**< @brief Menu principal. */
  USERNAME_ENTRY,   /**< @brief Inserir o nome. */
  SONG_SELECT,      /**< @brief Escolher a música. */
  PLAY,             /**< @brief A jogar. */
  PAUSE,            /**< @brief Pausa. */
  GAME_OVER,        /**< @brief Perdeu (0 HP). */
  GAME_WON,         /**< @brief Ganhou. */
  LEADERBOARD       /**< @brief Menu da leaderboard. */
} GameState;

/**
 * @brief Modo de controlo do menu.
 */
typedef enum {
  MENU_INPUT_KEYBOARD, /**< @brief A usar as setas. */
  MENU_INPUT_MOUSE     /**< @brief A usar o rato. */
} MenuInputMode;

/** @name Variáveis Externas de Hover do Rato nos Menus */
/**@{*/
extern bool hover_play;
extern bool hover_exit;
extern bool hover_leaderboard;
extern bool hover_song1;
extern bool hover_song2;
extern bool hover_song3;
extern bool hover_song4;
extern bool hover_back;
/**@}*/

/** @brief Variável que armazena a música selecionada. */
extern int song_id;

/** @name Variáveis para Navegação por Teclado nos Menus */
/**@{*/
extern int kbd_menu_idx;   /**< @brief Índice do botão focado no Menu. */
extern int kbd_song_idx;   /**< @brief Índice da música focada no Song Select. */
extern int kbd_pause_idx;  /**< @brief Índice da opção focada no ecrã de Pausa. */
/**@}*/

/** @brief Estado atual do modo de input nos menus (Teclado ou Rato). */
extern MenuInputMode menu_input_mode;

/**
 * @brief Altera o modo de input do menu para Teclado.
 */
void menu_set_keyboard_input(void);

/**
 * @brief Altera o modo de input do menu para Rato.
 */
void menu_set_mouse_input(void);

/**
 * @brief Verifica se o teclado está a ser o periférico dominante nos menus.
 * @return true Se for modo teclado.
 */
bool menu_keyboard_active(void);

/**
 * @brief Verifica se o rato está a ser o periférico dominante nos menus.
 * @return true Se for modo rato.
 */
bool menu_mouse_active(void);

/**
 * @brief Renderiza o menu principal no ecrã.
 * 
 * Processa as posições do rato/teclado para destacar (hover) as opções.
 * 
 * @param mouse_x Coordenada X atual do cursor.
 * @param mouse_y Coordenada Y atual do cursor.
 * @param bg_map Pixmap de fundo pré-carregado.
 * @param bg_img Imagem do XPM de fundo (usado para altura/largura).
 */
void draw_main_menu(int mouse_x, int mouse_y, uint32_t *bg_map, xpm_image_t bg_img);

/**
 * @brief Verifica interações e cliques no Menu Principal.
 * 
 * @param mouse_x Coordenada X onde ocorreu a interação (ou a posição do teclado focada).
 * @param mouse_y Coordenada Y onde ocorreu a interação.
 * @param left_click true se o utilizador premiu um botão de aceitar (clique esquerdo ou Enter).
 * @param current_state Apontador para a máquina de estados global (é alterada consoante a seleção).
 * @param game_running Apontador para a variável de loop do LCOM (colocada a false se selecionado 'Sair').
 */
void check_menu_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state, bool *game_running);

/**
 * @brief Renderiza o ecrã de seleção de músicas.
 * 
 * @param mouse_x Coordenada X atual do cursor.
 * @param mouse_y Coordenada Y atual do cursor.
 * @param bg_map Pixmap de fundo pré-carregado.
 * @param bg_img Imagem do XPM de fundo.
 */
void draw_song_select(int mouse_x, int mouse_y, const uint32_t *bg_map, xpm_image_t bg_img);

/**
 * @brief Verifica interações e cliques no ecrã de Seleção de Música.
 * 
 * @param mouse_x Coordenada X.
 * @param mouse_y Coordenada Y.
 * @param left_click true se houve confirmação (clique ou Enter).
 * @param current_state Apontador para o estado do jogo (pode mudar para PLAY se uma música for escolhida).
 */
void check_song_select_clicks(int mouse_x, int mouse_y, bool left_click, GameState *current_state);

#endif /* _MENU_H_ */
/** @} */
