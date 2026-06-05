/**
 * @file main.c
 * @brief Main e loop principal.
 *
 * Ficheiro principal. Tem o loop de interrupções (timer, teclado, rato, video, rtc).
 * 
 * @defgroup MainLoop Main Loop
 * @ingroup Core
 * @brief Loop de eventos do jogo.
 * @{
 */

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "devices/video/video.h"

/*
 * Logging/printf inside the game loop is expensive on MINIX/LCF, especially
 * when it opens and closes a file on every hit/miss. Keep it disabled by
 * default and enable it only while debugging with:
 *   CFLAGS += -DDEBUG_GAME_LOG=1
 */
#ifndef DEBUG_GAME_LOG
#define DEBUG_GAME_LOG 0
#endif

static void write_log(const char *format, ...) {
#if DEBUG_GAME_LOG
  FILE *fp = fopen("/tmp/log.txt", "a");
  if (fp != NULL) {
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
  }
#else
  (void) format;
#endif
}

#if DEBUG_GAME_LOG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...) ((void) 0)
#endif

// Assets Visuais
#include "devices/video/assets/bg1.xpm"
#include "devices/video/assets/bg2.xpm"
#include "devices/video/assets/bg3.xpm"
#include "devices/video/assets/arcade_leaderboard.xpm"
#include "devices/video/assets/nota_verde.xpm"
#include "devices/video/assets/nota_vermelha.xpm"
#include "devices/video/assets/nota_azul.xpm"
#include "devices/video/assets/nota_roxa.xpm"
#include "devices/video/assets/nota_amarela.xpm"
#include "devices/video/assets/menu.xpm"
#include "devices/video/assets/graffiti_song_select.xpm"
#include "devices/video/assets/graffiti_username_entry.xpm"
#include "devices/video/assets/mission_failed_bg.xpm"
#include "devices/video/assets/mission_success_bg.xpm"


// Módulos do Grupo
#include "structures/includes/menu.h"
#include "structures/includes/game.h"
#include "structures/includes/drivers/kbc.h"
#include "structures/includes/drivers/mouse.h"
#include "structures/includes/drivers/i8042.h"
#include "devices/uart/uart.h"
#include "structures/includes/beatmap_loader.h"
#include "structures/includes/drivers/rtc.h" // Relógio integrado!
#include "structures/includes/drivers/timer.h"
#include "structures/includes/leaderboard.h"

extern uint32_t no_interrupts;
extern Note notes[];

static BeatmapEntry beatmap[BEATMAP_MAX_NOTES];
static int beatmap_count = 0;
static int current_note_idx = 0;
static int hit_effect_frames[5] = {0, 0, 0, 0, 0};
static int miss_effect_frames[5] = {0, 0, 0, 0, 0};
static int score = 0;
static int combo_hits = 0;
static int best_combo = 0;
static int session_best_score = 0;
static int session_best_combo = 0;
static char score_text[16] = "0";
static char combo_text[16] = "0X";
#define MAX_HEALTH 20
static int player_health = MAX_HEALTH;
static bool score_hud_dirty = true;
static bool lane_key_down[NUM_LANES] = {false};
static char current_username[LEADERBOARD_USERNAME_MAX] = "PLAYER";
static char username_edit_buffer[LEADERBOARD_USERNAME_MAX] = "";
static int username_edit_length = 0;
static uint32_t pause_start_tick = 0;
static uint32_t pause_elapsed_ticks = 0;
static bool hover_pause_resume = false;
static bool hover_pause_quit = false;
static bool hover_pause_vol_down = false;
static bool hover_pause_vol_up = false;
static int volume_percent = 50;

#define AUDIO_QUEUE_SIZE 32
static uint8_t audio_queue[AUDIO_QUEUE_SIZE];
static int audio_q_head = 0;
static int audio_q_tail = 0;

#define SCORE_BASE_POINTS 10
#define SCORE_TIER_2_COMBO 4
#define SCORE_TIER_3_COMBO 12
#define SCORE_TIER_4_COMBO 24
#define SCORE_TIER_5_COMBO 40

#define A_MAKE_CODE 0x1E
#define S_MAKE_CODE 0x1F
#define D_MAKE_CODE 0x20
#define F_MAKE_CODE 0x21
#define ESC_MAKE_CODE 0x01
#define ENTER_MAKE_CODE 0x1C
#define BACKSPACE_MAKE_CODE 0x0E

#define PAUSE_PANEL_X 230
#define PAUSE_PANEL_Y 140
#define PAUSE_PANEL_W 340
#define PAUSE_PANEL_H 340
#define PAUSE_BTN_W 220
#define PAUSE_BTN_H 60
#define PAUSE_RESUME_X 290
#define PAUSE_RESUME_Y 260
#define PAUSE_QUIT_X 290
#define PAUSE_QUIT_Y 340

#ifndef G_MAKE_CODE
#define G_MAKE_CODE 0x22
#endif

// Estado Global e Coordenadas do Rato
GameState current_state = MENU;
bool music_started = false;
uint32_t play_start_tick = 0;
int song_id = 1;

int mouse_x = 400;
int mouse_y = 500;

static void clamp_mouse_position(void) {
  if (mouse_x < 0) mouse_x = 0;
  if (mouse_x > 799) mouse_x = 799;
  if (mouse_y < 0) mouse_y = 0;
  if (mouse_y > 599) mouse_y = 599;
}

static bool is_menu_state(GameState state) {
  return state == MENU ||
         state == USERNAME_ENTRY ||
         state == SONG_SELECT ||
         state == PAUSE ||
         state == GAME_OVER ||
         state == LEADERBOARD;
}

static bool mouse_packet_has_input(const struct packet *packet) {
  return packet->delta_x != 0 ||
         packet->delta_y != 0 ||
         packet->lb ||
         packet->rb ||
         packet->mb;
}

static int lane_from_make_code(uint8_t make_code) {
  switch (make_code) {
    case A_MAKE_CODE: return 0;
    case S_MAKE_CODE: return 1;
    case D_MAKE_CODE: return 2;
    case F_MAKE_CODE: return 3;
    case G_MAKE_CODE: return 4;
    default: return -1;
  }
}

static bool note_collides_with_hit_zone(const Note *note) {
  int note_top = note->y;
  int note_bottom = note->y + NOTE_HIT_HEIGHT;
  return note_top <= HIT_ZONE_BOTTOM && note_bottom >= HIT_ZONE_TOP;
}

static bool uart_send_audio_event(bool uart_ready, uint8_t event_byte, const char *event_name) {
  if (!uart_ready) return false;

  if (uart_send_byte(event_byte) != 0) {
    write_log("[UART] Falha ao enviar evento %s (0x%02x).\n", event_name, event_byte);
    return false;
  }

  return true;
}

static bool audio_queue_empty(void) {
  return audio_q_head == audio_q_tail;
}

static bool audio_queue_full(void) {
  return ((audio_q_tail + 1) % AUDIO_QUEUE_SIZE) == audio_q_head;
}

static void clear_audio_queue(void) {
  audio_q_head = 0;
  audio_q_tail = 0;
}

static bool queue_audio_event(uint8_t event_byte) {
  if (audio_queue_full()) {
    write_log("[UART] Fila de audio cheia. Evento 0x%02x descartado.\n", event_byte);
    return false;
  }

  audio_queue[audio_q_tail] = event_byte;
  audio_q_tail = (audio_q_tail + 1) % AUDIO_QUEUE_SIZE;
  return true;
}

static void flush_audio_events(bool uart_ready) {
  if (!uart_ready || audio_queue_empty()) return;

  uint8_t event_byte = audio_queue[audio_q_head];
  int result = uart_try_send_byte(event_byte);

  if (result == UART_SEND_OK) {
    audio_q_head = (audio_q_head + 1) % AUDIO_QUEUE_SIZE;
  } else if (result == UART_SEND_ERROR) {
    write_log("[UART] Erro ao enviar evento 0x%02x. Evento descartado.\n", event_byte);
    audio_q_head = (audio_q_head + 1) % AUDIO_QUEUE_SIZE;
  }
  /* UART_SEND_BUSY: nao bloqueia. Tenta outra vez no proximo tick. */
}

static void reset_lane_key_state(void) {
  for (int i = 0; i < NUM_LANES; i++) {
    lane_key_down[i] = false;
  }
}

static int points_for_combo_hit(int combo_after_hit) {
  if (combo_after_hit > SCORE_TIER_5_COMBO) return 50;
  if (combo_after_hit > SCORE_TIER_4_COMBO) return 40;
  if (combo_after_hit > SCORE_TIER_3_COMBO) return 30;
  if (combo_after_hit > SCORE_TIER_2_COMBO) return 20;
  return SCORE_BASE_POINTS;
}

static void reset_score(void) {
  score = 0;
  combo_hits = 0;
  best_combo = 0;
  player_health = MAX_HEALTH;
  score_hud_dirty = true;
}

static int register_hit_score(void) {
  combo_hits++;
  if (combo_hits > best_combo) best_combo = combo_hits;

  int points = points_for_combo_hit(combo_hits);
  score += points;
  score_hud_dirty = true;

  write_log("[SCORE] Hit #%d da combo: +%d pontos (total=%d).\n", combo_hits, points, score);
  return points;
}

static void save_final_score(void);

static void register_miss_score(void) {
  player_health--;
  if (player_health <= 0) {
    player_health = 0;
    save_final_score();
    current_state = GAME_OVER;
    music_started = false;
    queue_audio_event(UART_EVENT_GAME_END);
  }

  if (combo_hits == 0) return;

  write_log("[SCORE] Miss: combo resetada de %d para 0. Pontuacao mantida em %d.\n", combo_hits, score);
  combo_hits = 0;
  score_hud_dirty = true;
}

static bool any_active_notes(void) {
  for (int i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active) return true;
  }
  return false;
}

static const uint8_t *glyph_for_char(char c) {
  static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t glyph_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
  static const uint8_t glyph_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const uint8_t glyph_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
  static const uint8_t glyph_3[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
  static const uint8_t glyph_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
  static const uint8_t glyph_5[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
  static const uint8_t glyph_6[7] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
  static const uint8_t glyph_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
  static const uint8_t glyph_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
  static const uint8_t glyph_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
  static const uint8_t glyph_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
  static const uint8_t glyph_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
  static const uint8_t glyph_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
  static const uint8_t glyph_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
  static const uint8_t glyph_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
  static const uint8_t glyph_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
  static const uint8_t glyph_I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const uint8_t glyph_J[7] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
  static const uint8_t glyph_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
  static const uint8_t glyph_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
  static const uint8_t glyph_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
  static const uint8_t glyph_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
  static const uint8_t glyph_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
  static const uint8_t glyph_Q[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
  static const uint8_t glyph_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
  static const uint8_t glyph_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
  static const uint8_t glyph_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  static const uint8_t glyph_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
  static const uint8_t glyph_W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
  static const uint8_t glyph_X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
  static const uint8_t glyph_Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
  static const uint8_t glyph_Z[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
  static const uint8_t glyph_slash[7] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
  static const uint8_t glyph_dash[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};

  switch (c) {
    case ' ': return glyph_space;
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'A': return glyph_A;
    case 'B': return glyph_B;
    case 'C': return glyph_C;
    case 'D': return glyph_D;
    case 'E': return glyph_E;
    case 'F': return glyph_F;
    case 'G': return glyph_G;
    case 'H': return glyph_H;
    case 'I': return glyph_I;
    case 'J': return glyph_J;
    case 'K': return glyph_K;
    case 'L': return glyph_L;
    case 'M': return glyph_M;
    case 'N': return glyph_N;
    case 'O': return glyph_O;
    case 'P': return glyph_P;
    case 'Q': return glyph_Q;
    case 'R': return glyph_R;
    case 'S': return glyph_S;
    case 'T': return glyph_T;
    case 'U': return glyph_U;
    case 'V': return glyph_V;
    case 'W': return glyph_W;
    case 'X': return glyph_X;
    case 'Y': return glyph_Y;
    case 'Z': return glyph_Z;
    case '/': return glyph_slash;
    case '-': return glyph_dash;
    default: return glyph_space;
  }
}

static int text_width_pixels(const char *text, int scale) {
  int length = 0;
  while (text[length] != '\0') length++;
  if (length == 0) return 0;
  return (length * 6 - 1) * scale;
}

static void draw_text(int x, int y, const char *text, int scale, uint32_t color) {
  for (int i = 0; text[i] != '\0'; i++) {
    const uint8_t *glyph = glyph_for_char(text[i]);
    int char_x = x + i * 6 * scale;

    for (int row = 0; row < 7; row++) {
      for (int col = 0; col < 5; col++) {
        if (glyph[row] & BIT(4 - col)) {
          vg_draw_rectangle(char_x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
  }
}

static void draw_text_centered(int center_x, int y, const char *text, int scale, uint32_t color) {
  draw_text(center_x - text_width_pixels(text, scale) / 2, y, text, scale, color);
}

static void draw_border_main(int x, int y, int w, int h, uint32_t color, int thickness);

static void update_score_hud_cache(void) {
  if (!score_hud_dirty) return;

  snprintf(score_text, sizeof(score_text), "%d", score);
  snprintf(combo_text, sizeof(combo_text), "%dX", combo_hits);
  score_hud_dirty = false;
}

static void draw_score_hud(void) {
  update_score_hud_cache();

  int box_x = 10;
  int box_y = 55;
  int box_w = 170;
  int box_h = 135;

  uint32_t border_color = 0x00FFFF; // Default cyan
  if (combo_hits > SCORE_TIER_5_COMBO) border_color = 0xFF00FF; // Pink
  else if (combo_hits > SCORE_TIER_3_COMBO) border_color = 0xFFFF00; // Yellow

  // Player Name Box (Separate)
  int name_w = text_width_pixels(current_username, 3);
  int name_box_w = name_w + 20 < box_w ? box_w : name_w + 20;
  draw_border_main(10, 10, name_box_w, 36, 0x00FFFF, 2); 
  vg_draw_rectangle(10, 10, name_box_w, 36, 0x222233); 
  draw_text_centered(10 + name_box_w / 2 + 2, 10 + 8, current_username, 3, 0x000000);
  draw_text_centered(10 + name_box_w / 2, 10 + 6, current_username, 3, 0x00FFFF);

  // Background HUD Box with neon styling
  draw_border_main(box_x, box_y, box_w, box_h, border_color, 4); // Neon Border
  vg_draw_rectangle(box_x, box_y, box_w, box_h, 0x0A0A15); // Very Dark background
  
  // Inner subtle highlight
  draw_border_main(box_x + 2, box_y + 2, box_w - 4, box_h - 4, 0x222233, 1);

  // SCORE Label
  draw_text_centered(box_x + box_w / 2 + 2, box_y + 12, "SCORE", 3, 0x000000); // Shadow
  draw_text_centered(box_x + box_w / 2, box_y + 10, "SCORE", 3, 0xFFFFFF); // Main text
  
  // Score Value
  draw_text_centered(box_x + box_w / 2 + 2, box_y + 40, score_text, 4, 0x550000); // Reddish shadow
  draw_text_centered(box_x + box_w / 2, box_y + 38, score_text, 4, 0xFFFF00); // Yellow neon

  // COMBO Label
  draw_text_centered(box_x + box_w / 2 + 2, box_y + 80, "COMBO", 2, 0x000000); // Shadow
  draw_text_centered(box_x + box_w / 2, box_y + 78, "COMBO", 2, 0xAAAAAA);

  // Combo Value (making it larger for impact!)
  uint32_t combo_color = 0x00FFFF; // Cyan default
  int combo_scale = 3;
  if (combo_hits > SCORE_TIER_5_COMBO) {
    // Pulsing/flashing effect for high combo based on score or combo_hits
    combo_color = (combo_hits % 2 == 0) ? 0xFF00FF : 0xFFFFFF; 
    combo_scale = 5;
  } else if (combo_hits > SCORE_TIER_3_COMBO) {
    combo_color = 0xFF00FF;
    combo_scale = 4;
  } else if (combo_hits > SCORE_TIER_2_COMBO) {
    combo_color = 0xFFFF00;
    combo_scale = 4;
  }

  // Adjust Y based on scale
  int combo_y = box_y + 98 - (combo_scale - 3) * 4;

  draw_text_centered(box_x + box_w / 2 + 3, combo_y + 3, combo_text, combo_scale, 0x000000); // Shadow
  draw_text_centered(box_x + box_w / 2, combo_y, combo_text, combo_scale, combo_color); 
}

static void draw_leaderboard_summary(void) {
  char pos[8];
  char score_val[16];
  LeaderboardEntry *scores = leaderboard_get_scores();
  int count = leaderboard_get_count();

  int box_x = 420;
  int box_y = 180;
  int box_w = 300;
  int box_h = 280;

  // Background Box (Purple/Magenta themed)
  draw_border_main(box_x, box_y, box_w, box_h, 0xFF00FF, 4);
  vg_draw_rectangle(box_x, box_y, box_w, box_h, 0x1A001A);
  
  // Subtle inner border
  draw_border_main(box_x + 4, box_y + 4, box_w - 8, box_h - 8, 0x330033, 1);

  // Title
  draw_text_centered(box_x + box_w / 2, box_y + 20, "TOP 5 HEROES", 3, 0xFF00FF);
  vg_draw_rectangle(box_x + 20, box_y + 60, box_w - 40, 2, 0xFF00FF);

  if (count == 0) {
    draw_text_centered(box_x + box_w / 2, box_y + 140, "NO SCORES YET", 3, 0x555555);
    return;
  }

  for (int i = 0; i < count && i < 5; i++) {
    uint32_t color = 0xFFFFFF; // Default white
    if (i == 0) color = 0xFFFF00; // Gold
    else if (i == 1) color = 0xCCCCCC; // Silver
    else if (i == 2) color = 0xCD7F32; // Bronze

    int row_y = box_y + 80 + i * 36;
    
    // Rank
    snprintf(pos, sizeof(pos), "#%d", i + 1);
    draw_text(box_x + 20, row_y, pos, 2, color);
    
    // Name
    draw_text(box_x + 80, row_y, scores[i].username, 2, color);
    
    // Score
    snprintf(score_val, sizeof(score_val), "%d", scores[i].score);
    // Align score to the right
    int sw = text_width_pixels(score_val, 2);
    draw_text(box_x + box_w - 20 - sw, row_y, score_val, 2, color);

    // Barra de progresso horizontal
    int pb_x = box_x + 20;
    int pb_y = row_y + 18;
    int pb_w = box_w - 40;
    int pb_h = 4;
    vg_draw_rectangle(pb_x, pb_y, pb_w, pb_h, 0x333333);
    vg_draw_rectangle(pb_x, pb_y, (pb_w * scores[i].progress) / 100, pb_h, 0x00FF00);
  }
}

static void draw_border_main(int x, int y, int w, int h, uint32_t color, int thickness) {
  vg_draw_rectangle(x - thickness, y - thickness, w + 2*thickness, thickness, color);
  vg_draw_rectangle(x - thickness, y + h, w + 2*thickness, thickness, color);
  vg_draw_rectangle(x - thickness, y, thickness, h, color);
  vg_draw_rectangle(x + w, y, thickness, h, color);
}

static void draw_health_bar(void) {
  int bar_w = 400;
  int bar_h = 24;
  int bar_x = 400 - bar_w / 2;
  int bar_y = 560;

  draw_border_main(bar_x, bar_y, bar_w, bar_h, 0x555555, 3);
  vg_draw_rectangle(bar_x, bar_y, bar_w, bar_h, 0x222222);

  if (player_health > 0) {
    int fill_w = (bar_w * player_health) / MAX_HEALTH;
    uint32_t fill_color = (player_health > 10) ? 0x00FF00 : (player_health > 5) ? 0xFFFF00 : 0xFF0000;
    vg_draw_rectangle(bar_x, bar_y, fill_w, bar_h, fill_color);
  }

  char text[32];
  snprintf(text, sizeof(text), "HP: %d/%d", player_health, MAX_HEALTH);
  draw_text_centered(400, bar_y + 4, text, 2, 0xFFFFFF);
}

static void draw_progress_bar(uint32_t elapsed_ticks) {
  if (beatmap_count == 0) return;

  // Added 120 ticks (approx 2 seconds) to account for final notes reaching the hit zone
  uint32_t total_ticks = beatmap[beatmap_count - 1].spawn_tick + 120;
  if (total_ticks == 0) return;

  // Configuração Vertical ao Lado da Guitarra
  int bar_w = 20;
  int bar_h = 400;
  int bar_x = 680; // Movido mais para a direita
  int bar_y = 100;

  // Borda Amarela
  draw_border_main(bar_x, bar_y, bar_w, bar_h, 0xFFFF00, 2);
  // Fundo da barra escuro com um tom de amarelo para se misturar melhor
  vg_draw_rectangle(bar_x, bar_y, bar_w, bar_h, 0x222200);

  if (elapsed_ticks > total_ticks) elapsed_ticks = total_ticks;

  // Preencher de baixo para cima
  int fill_h = (bar_h * elapsed_ticks) / total_ticks;
  vg_draw_rectangle(bar_x, bar_y + bar_h - fill_h, bar_w, fill_h, 0xFFFF00); // Fundo Amarelo
}

int kbd_gameover_idx = 1; // 1: MENU, 2: RESTART/SONGS

static void draw_game_over_screen(const uint32_t *bg_map, xpm_image_t bg_img) {
  char value[16];

  if (bg_map != NULL) {
    vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
  } else {
    vg_clear_back_buffer(0x100000); // Fundo vermelho muito escuro
  }

  

  // Caixa do Score (Red themed, Green if WON)
  uint32_t score_border = (current_state == GAME_WON) ? 0x00FF00 : 0xFF0000;
  uint32_t score_bg = (current_state == GAME_WON) ? 0x002A00 : 0x2A0000;
  uint32_t score_title_color = (current_state == GAME_WON) ? 0x55FF55 : 0xFF5555;

  draw_border_main(80, 180, 280, 120, score_border, 4);
  vg_draw_rectangle(80, 180, 280, 120, score_bg);
  draw_text_centered(220, 200, "FINAL SCORE", 3, score_title_color);
  snprintf(value, sizeof(value), "%d", score);
  draw_text_centered(220, 240, value, 5, 0xFFFFFF);

  // Caixa do Combo (Orange themed)
  draw_border_main(80, 340, 280, 120, 0xFFA500, 4);
  vg_draw_rectangle(80, 340, 280, 120, 0x221100);
  draw_text_centered(220, 360, "MAX COMBO", 3, 0xFFA500);
  snprintf(value, sizeof(value), "%dX", best_combo);
  draw_text_centered(220, 400, value, 5, 0xFFFFFF);

  // Leaderboard resumo desenha-se a si próprio
  draw_leaderboard_summary();

  // Botões
  bool hover_menu = (menu_mouse_active() && mouse_x >= 120 && mouse_x <= 380 && mouse_y >= 520 && mouse_y <= 570) ||
                    (menu_keyboard_active() && kbd_gameover_idx == 1);
  bool hover_restart = (menu_mouse_active() && mouse_x >= 420 && mouse_x <= 680 && mouse_y >= 520 && mouse_y <= 570) ||
                       (menu_keyboard_active() && kbd_gameover_idx == 2);

  // Main Menu
  draw_border_main(120, 520, 260, 50, hover_menu ? 0xFF5555 : 0xFF0000, 3);
  vg_draw_rectangle(120, 520, 260, 50, hover_menu ? 0x550000 : 0x330000);
  draw_text_centered(250, 535, "MENU", 3, hover_menu ? 0xFFFFFF : 0xFFCCCC);

  // Restart
  draw_border_main(420, 520, 260, 50, hover_restart ? 0xFF5555 : 0xFF0000, 3);
  vg_draw_rectangle(420, 520, 260, 50, hover_restart ? 0x550000 : 0x330000);
  draw_text_centered(550, 535, (current_state == GAME_WON) ? "SONGS" : "RESTART", 3, hover_restart ? 0xFFFFFF : 0xFFCCCC);
}

static void reset_username_entry(void) {
  username_edit_buffer[0] = '\0';
  username_edit_length = 0;
}

static void accept_username_entry(void) {
  if (username_edit_length == 0) {
    strncpy(current_username, "PLAYER", sizeof(current_username));
  } else {
    strncpy(current_username, username_edit_buffer, sizeof(current_username));
  }

  current_username[sizeof(current_username) - 1] = '\0';
  current_state = SONG_SELECT;
}

extern void draw_neo_btn(int x, int y, int w, int h, uint32_t bg_color, bool hovered);

int kbd_username_idx = 2; // 1: BACK, 2: DONE
int kbd_leaderboard_idx = 1; // 1: BACK

#define USERNAME_BACK_X 140
#define USERNAME_BACK_Y 460
#define USERNAME_DONE_X 500
#define USERNAME_DONE_Y 460
#define USERNAME_BTN_W 160
#define USERNAME_BTN_H 60

#define LEADERBOARD_BACK_X 300
#define LEADERBOARD_BACK_Y 530
#define LEADERBOARD_BACK_W 200
#define LEADERBOARD_BACK_H 50

static char username_char_from_make_code(uint8_t make_code) {
  switch (make_code) {
    case 0x02: return '1';
    case 0x03: return '2';
    case 0x04: return '3';
    case 0x05: return '4';
    case 0x06: return '5';
    case 0x07: return '6';
    case 0x08: return '7';
    case 0x09: return '8';
    case 0x0A: return '9';
    case 0x0B: return '0';
    case 0x10: return 'Q';
    case 0x11: return 'W';
    case 0x12: return 'E';
    case 0x13: return 'R';
    case 0x14: return 'T';
    case 0x15: return 'Y';
    case 0x16: return 'U';
    case 0x17: return 'I';
    case 0x18: return 'O';
    case 0x19: return 'P';
    case A_MAKE_CODE: return 'A';
    case S_MAKE_CODE: return 'S';
    case D_MAKE_CODE: return 'D';
    case F_MAKE_CODE: return 'F';
    case G_MAKE_CODE: return 'G';
    case 0x23: return 'H';
    case 0x24: return 'J';
    case 0x25: return 'K';
    case 0x26: return 'L';
    case 0x2C: return 'Z';
    case 0x2D: return 'X';
    case 0x2E: return 'C';
    case 0x2F: return 'V';
    case 0x30: return 'B';
    case 0x31: return 'N';
    case 0x32: return 'M';
    case 0x0C: return '-';
    default: return '\0';
  }
}

static void handle_username_key(uint8_t scancode) {
  if (scancode & BIT(7)) return;

  if (scancode == ENTER_MAKE_CODE) {
    if (kbd_username_idx == 1) { // BACK
      current_state = MENU;
      music_started = false;
    } else { // DONE
      accept_username_entry();
    }
    return;
  }

  // Handle arrow keys for navigation
  if (scancode == 0x4B) { // Left arrow
    kbd_username_idx = 1; // BACK
    return;
  }
  if (scancode == 0x4D) { // Right arrow
    kbd_username_idx = 2; // DONE
    return;
  }

  if (scancode == BACKSPACE_MAKE_CODE) {
    if (username_edit_length > 0) {
      username_edit_length--;
      username_edit_buffer[username_edit_length] = '\0';
    }
    return;
  }

  char next_char = username_char_from_make_code(scancode);
  if (next_char == '\0') return;

  if (username_edit_length < LEADERBOARD_USERNAME_MAX - 1) {
    username_edit_buffer[username_edit_length++] = next_char;
    username_edit_buffer[username_edit_length] = '\0';
  }
}

static void draw_username_entry_screen(const uint32_t *bg_map, xpm_image_t bg_img) {
  const char *visible_name = (username_edit_length == 0) ? "PLAYER" : username_edit_buffer;

  if (bg_map != NULL) {
    vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
  } else {
    vg_clear_back_buffer(0xCCCCCC); // Neo-brutalism light background fallback
  }
  
  // Cabeçalho Background Box
  draw_border_main(180, 20, 440, 120, 0x00FFFF, 3);
  vg_draw_rectangle(180, 20, 440, 120, 0x111122);

  // Cabeçalho (move up to fit the background better)
  draw_text_centered(400 + 4, 40 + 4, "NEW SINGER", 5, 0x000000); // 3D Black Shadow
  draw_text_centered(400, 40, "NEW SINGER", 5, 0x00FFFF); // Cyan Main
  
  draw_text_centered(400 + 2, 100 + 2, "ENTER NAME", 3, 0x000000); // 3D Black Shadow
  draw_text_centered(400, 100, "ENTER NAME", 3, 0xFFFFFF); // White Main

  // Caixa de Input Neo-brutalism (Positioned to fit inside the "Hello My Name Is" sticker)
  // The sticker in the image is roughly centered. Let's place it at 200, 270
  draw_border_main(200, 270, 400, 80, 0xFFFFFF, 4); // White border
  vg_draw_rectangle(200, 270, 400, 80, 0x000000); // Black fill
  draw_text_centered(400, 295, visible_name, 4, 0x00FFFF); // Cyan text

  // Botões
  bool hover_bk = (menu_mouse_active() &&
                   mouse_x >= USERNAME_BACK_X && mouse_x <= USERNAME_BACK_X + USERNAME_BTN_W &&
                   mouse_y >= USERNAME_BACK_Y && mouse_y <= USERNAME_BACK_Y + USERNAME_BTN_H) ||
                  (menu_keyboard_active() && kbd_username_idx == 1);
  draw_neo_btn(USERNAME_BACK_X, USERNAME_BACK_Y, USERNAME_BTN_W, USERNAME_BTN_H, 0xFF5555, hover_bk);
  draw_text_centered(220, 480, "BACK", 3, 0x000000);

  bool hover_dn = (menu_mouse_active() &&
                   mouse_x >= USERNAME_DONE_X && mouse_x <= USERNAME_DONE_X + USERNAME_BTN_W &&
                   mouse_y >= USERNAME_DONE_Y && mouse_y <= USERNAME_DONE_Y + USERNAME_BTN_H) ||
                  (menu_keyboard_active() && kbd_username_idx == 2);
  draw_neo_btn(USERNAME_DONE_X, USERNAME_DONE_Y, USERNAME_BTN_W, USERNAME_BTN_H, 0x55FF55, hover_dn);
  draw_text_centered(580, 480, "DONE", 3, 0x000000);
}

static void draw_leaderboard_screen(const uint32_t *bg_map, xpm_image_t bg_img) {
  LeaderboardEntry *scores = leaderboard_get_scores();
  int count = leaderboard_get_count();
  char row[64];

  if (bg_map != NULL) {
    vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
  } else {
    vg_clear_back_buffer(0xDDDDDD);
  }
  
  // Título e sublinhado (Drop shadow para destacar do Placard)
  draw_text_centered(400 + 4, 40 + 4, "HALL OF FAME", 5, 0x000000);
  draw_text_centered(400, 40, "HALL OF FAME", 5, 0xFFFF00); // Yellow
  vg_draw_rectangle(100, 90, 600, 4, 0x000000);

  // Fundos para os cabeçalhos (estilo caixa/rótulo) para ficarem mais visuais
  uint32_t header_bg = 0x333366; // Fundo azulão
  vg_draw_rectangle(25, 115, 65,  26, header_bg); // RANK
  vg_draw_rectangle(95, 115, 255, 26, header_bg); // NAME
  vg_draw_rectangle(355, 115, 205, 26, header_bg); // SONG
  vg_draw_rectangle(575, 115, 85,  26, header_bg); // SCORE
  vg_draw_rectangle(665, 115, 120, 26, header_bg); // DATE

  // Cabeçalhos
  draw_text(30, 120, "RANK", 2, 0xFFFF00); // Amarelo
  draw_text(100, 120, "NAME", 2, 0xFFFF00);
  draw_text(360, 120, "SONG", 2, 0xFFFF00);
  draw_text(580, 120, "SCORE", 2, 0xFFFF00);
  draw_text(670, 120, "DATE", 2, 0xFFFF00);
  vg_draw_rectangle(20, 150, 760, 2, 0x00FFFF); // Neon Cyan separador

  // Fundo Tabela escuro para contraste
  draw_border_main(20, 160, 760, 340, 0x00FFFF, 2); // Borda fina Cyan
  vg_draw_rectangle(20, 160, 760, 340, 0x111122); // Fundo escuro azulado

  if (count == 0) {
    draw_text_centered(400, 300, "NO SCORES FOUND", 3, 0xFFFFFF);
  } else {
    int rows = (count < MAX_SCORES) ? count : MAX_SCORES;
    for (int i = 0; i < rows; i++) {
      uint32_t color = 0xFFFFFF; // Branco para contrastar com o fundo
      
      snprintf(row, sizeof(row), "#%d", i + 1);
      draw_text(35, 180 + i * 32, row, 2, color);
      
      draw_text(100, 180 + i * 32, scores[i].username, 2, color);
      
      const char* song_name = "EVERY TIME";
      if (scores[i].song_id == 1) song_name = "EVERY TIME";
      else if (scores[i].song_id == 4) song_name = "SUMMER";
      else if (scores[i].song_id == 2) song_name = "DIAMOND M.";
      else if (scores[i].song_id == 3) song_name = "HIGHWAY";
      
      draw_text(360, 180 + i * 32, song_name, 2, color);
      
      snprintf(row, sizeof(row), "%d", scores[i].score);
      draw_text(580, 180 + i * 32, row, 2, color);
      
      snprintf(row, sizeof(row), "%02d/%02d/%02d",
               scores[i].date.day,
               scores[i].date.month,
               scores[i].date.year);
      draw_text(670, 180 + i * 32, row, 2, color);
      
      // Barra de progresso horizontal
      int pb_x = 35;
      int pb_y = 180 + i * 32 + 18;
      int pb_w = 730;
      int pb_h = 4;
      vg_draw_rectangle(pb_x, pb_y, pb_w, pb_h, 0x222222);
      vg_draw_rectangle(pb_x, pb_y, (pb_w * scores[i].progress) / 100, pb_h, 0x00FF00);
      
      // Linha separadora subtil por baixo de cada entrada
      vg_draw_rectangle(25, 180 + i * 32 + 25, 750, 1, 0x333333);
    }
  }

  // Botão "BACK" em baixo
  bool hover_lb_bk = (menu_mouse_active() &&
                      mouse_x >= LEADERBOARD_BACK_X && mouse_x <= LEADERBOARD_BACK_X + LEADERBOARD_BACK_W &&
                      mouse_y >= LEADERBOARD_BACK_Y && mouse_y <= LEADERBOARD_BACK_Y + LEADERBOARD_BACK_H) ||
                     (menu_keyboard_active() && kbd_leaderboard_idx == 1);
  draw_neo_btn(LEADERBOARD_BACK_X, LEADERBOARD_BACK_Y, LEADERBOARD_BACK_W, LEADERBOARD_BACK_H, 0xFF00FF, hover_lb_bk);
  draw_text_centered(400, 545, "BACK", 3, 0xFFFFFF);
}

static void draw_play_frame(const uint32_t *bg_map,
                            xpm_image_t bg_img,
                            xpm_image_t img_notas[NUM_LANES],
                            uint32_t *mapas_notas[NUM_LANES],
                            const uint32_t cores_pistas[NUM_LANES],
                            uint32_t elapsed_ticks,
                            bool animate_effects) {
  // --- 1. CAMADA DE FUNDO OTIMIZADA ---
  if (bg_map != NULL) {
    vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
  } else {
    vg_clear_back_buffer(0x000000);
  }

  // --- 1.5. BRAÇO DA GUITARRA NEON (GLOW EFFECT) ---
  int HORIZON_Y = 0; // O braço começa no topo para dar tempo de reação ao jogador!
  int TRACK_H = 600 - HORIZON_Y;

  // Fundo Negro da Pista (Mascara a perspetiva 3D da imagem e cria a nossa pista Arcade reta)
  vg_draw_rectangle(196, HORIZON_Y, 408, TRACK_H, 0x030303); // Fundo quase preto absoluto

  // Bordas Esquerda (Glow + Core)
  vg_draw_rectangle(188, HORIZON_Y, 14, TRACK_H, 0x550055); // Glow exterior rosa escuro
  vg_draw_rectangle(192, HORIZON_Y, 8, TRACK_H, 0xFF00FF);  // Cor principal Rosa Choque
  vg_draw_rectangle(195, HORIZON_Y, 2, TRACK_H, 0xFFFFFF);  // Núcleo branco brilhante

  // Bordas Direita (Glow + Core)
  vg_draw_rectangle(598, HORIZON_Y, 14, TRACK_H, 0x550055);
  vg_draw_rectangle(600, HORIZON_Y, 8, TRACK_H, 0xFF00FF);
  vg_draw_rectangle(603, HORIZON_Y, 2, TRACK_H, 0xFFFFFF);

  // --- 2. AS CORDAS DA GUITARRA (Neon Cyan Layers) ---
  for (int i = 0; i <= 5; i++) {
    int linha_x = 200 + (i * 80);
    vg_draw_rectangle(linha_x - 2, HORIZON_Y, 5, TRACK_H, 0x005555); // Glow exterior
    vg_draw_rectangle(linha_x - 1, HORIZON_Y, 3, TRACK_H, 0x00FFFF); // Cor principal Cyan
    vg_draw_rectangle(linha_x, HORIZON_Y, 1, TRACK_H, 0xFFFFFF);     // Núcleo branco
  }

  // --- 2.5. OS TRASTES (Lasers Rosa Que Descem com Glow) ---
  int distancia_trastes = 150;
  int velocidade_scroll = 4;
  int offset = (elapsed_ticks * velocidade_scroll) % distancia_trastes;

  for (int i = -2; i <= 4; i++) {
    int traste_y = (i * distancia_trastes) + offset;
    if (traste_y >= HORIZON_Y && traste_y < 596) {
      // Efeito Néon com 3 camadas
      vg_draw_rectangle(200, traste_y - 3, 400, 9, 0x550055); // Glow rosa escuro espesso
      vg_draw_rectangle(200, traste_y - 1, 400, 5, 0xFF00FF); // Cor principal
      vg_draw_rectangle(200, traste_y + 1, 400, 1, 0xFFFFFF); // Fio laser branco no meio
    }
  }

  // --- 3. A ZONA DE ACERTO (High-Tech Laser Grid) ---
  // Fundo estilo "vidro fumado"
  vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_TOP, NUM_LANES * LANE_WIDTH, HIT_ZONE_BOTTOM - HIT_ZONE_TOP, 0x111122); 
  
  // Linha superior a pulsar (Amarelo Neon)
  uint32_t pulse_color = ((elapsed_ticks / 15) % 2 == 0) ? 0xFFFF00 : 0xAAAA00;
  vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_TOP - 3, NUM_LANES * LANE_WIDTH, 6, 0x555500); // Glow
  vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_TOP - 1, NUM_LANES * LANE_WIDTH, 2, pulse_color); // Core
  
  // Linha inferior
  vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_BOTTOM - 3, NUM_LANES * LANE_WIDTH, 6, 0x555500);
  vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_BOTTOM - 1, NUM_LANES * LANE_WIDTH, 2, pulse_color);

  for (int i = 0; i < NUM_LANES; i++) {
    int lane_x_center = LANE_BASE_X + i * LANE_WIDTH + LANE_WIDTH / 2;
    uint32_t lane_colors[NUM_LANES] = {0x00FF00, 0xFF0000, 0x0000FF, 0x800080, 0xFFFF00};

    // --- 3.5 DESENHO DOS BOTÕES (RECEPTORES) ---
    int btn_w = 60;
    int btn_h = 24;
    int btn_y = (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - btn_h / 2;
    
    if (lane_key_down[i]) {
      // Botão Pressionado (brilhante e descido)
      vg_draw_rectangle(lane_x_center - btn_w / 2, btn_y + 4, btn_w, btn_h - 4, lane_colors[i]);
      vg_draw_rectangle(lane_x_center - btn_w / 2 + 15, btn_y + 8, btn_w - 30, btn_h - 12, 0xFFFFFF);
    } else {
      // Botão Solto (Borda e miolo)
      vg_draw_rectangle(lane_x_center - btn_w / 2, btn_y, btn_w, btn_h, 0x222233);
      draw_border_main(lane_x_center - btn_w / 2, btn_y, btn_w, btn_h, lane_colors[i], 3);
      vg_draw_rectangle(lane_x_center - btn_w / 2 + 15, btn_y + 8, btn_w - 30, btn_h - 16, lane_colors[i]);
    }

    if (miss_effect_frames[i] > 0) {
      vg_draw_rectangle(LANE_BASE_X + i * LANE_WIDTH + 10, HIT_ZONE_TOP,
                        LANE_WIDTH - 20, HIT_ZONE_BOTTOM - HIT_ZONE_TOP, 0xFF0000);
      if (animate_effects) miss_effect_frames[i]--;
    }

    if (hit_effect_frames[i] > 0) {
      int frame_diff = 12 - hit_effect_frames[i];
      int w = 20 + frame_diff * 4;
      int h = 8 + frame_diff * 2;

      // 1. Público a explodir (Muitas partículas gigantes e brilhantes)
      int cx_l1 = 30 + (i * 10) + (frame_diff % 3) * 15;
      int cy_l1 = 350 - (frame_diff * 6);
      int cx_l2 = 100 - (frame_diff % 4) * 12;
      int cy_l2 = 400 - (frame_diff * 8);
      int cx_l3 = 70 + (frame_diff % 2) * 20;
      int cy_l3 = 450 - (frame_diff * 4);
      
      int cx_r1 = 670 - (i * 10) + (frame_diff % 3) * 12;
      int cy_r1 = 350 - (frame_diff * 6);
      int cx_r2 = 750 - (frame_diff % 2) * 14;
      int cy_r2 = 400 - (frame_diff * 8);
      int cx_r3 = 700 + (frame_diff % 4) * 15;
      int cy_r3 = 460 - (frame_diff * 5);

      // Plateia Esquerda - Tamanhos Grandes e Brilhantes
      vg_draw_rectangle(cx_l1, cy_l1, 10, 10, 0xFFFF00);
      vg_draw_rectangle(cx_l2, cy_l2, 12, 12, 0xFFCC00);
      vg_draw_rectangle(cx_l3, cy_l3, 8, 8, 0xFFFFFF);
      vg_draw_rectangle(cx_l1 + 50, cy_l1 + 40, 10, 10, 0xFFEE00);
      vg_draw_rectangle(cx_l2 - 30, cy_l2 - 30, 14, 14, 0xFFFF55);
      vg_draw_rectangle(cx_l3 + 40, cy_l3 - 50, 12, 12, 0xFFFFAA);

      // Plateia Direita - Tamanhos Grandes e Brilhantes
      vg_draw_rectangle(cx_r1, cy_r1, 10, 10, 0xFFFF00);
      vg_draw_rectangle(cx_r2, cy_r2, 12, 12, 0xFFCC00);
      vg_draw_rectangle(cx_r3, cy_r3, 8, 8, 0xFFFFFF);
      vg_draw_rectangle(cx_r1 - 40, cy_r1 + 50, 10, 10, 0xFFEE00);
      vg_draw_rectangle(cx_r2 + 30, cy_r2 - 20, 14, 14, 0xFFFF55);
      vg_draw_rectangle(cx_r3 - 50, cy_r3 - 40, 12, 12, 0xFFFFAA);
      
      // Mais intensidade extrema nas laterais distantes!
      vg_draw_rectangle(cx_l1 - 60, cy_l1 + 10, 16, 16, 0xFFFF00);
      vg_draw_rectangle(cx_r1 + 70, cy_r1 + 20, 16, 16, 0xFFFF00);

      // 2. Ondas de Choque (Shockwave) expandindo
      draw_border_main(lane_x_center - w, (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - h,
                       w * 2, h * 2, lane_colors[i], 2);

      // 3. Faíscas (Sparks) a voar para cima
      int spark_y = (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - (frame_diff * 8);
      int spark_x1 = lane_x_center - (frame_diff * 6);
      int spark_x2 = lane_x_center + (frame_diff * 6);
      vg_draw_rectangle(spark_x1, spark_y, 4, 4, 0xFFFFFF);
      vg_draw_rectangle(spark_x2, spark_y, 4, 4, 0xFFFFFF);
      vg_draw_rectangle(spark_x1 - 8, spark_y + 10, 6, 6, lane_colors[i]);
      vg_draw_rectangle(spark_x2 + 8, spark_y + 10, 6, 6, lane_colors[i]);

      // 4. Hit Box Central
      vg_draw_rectangle(lane_x_center - w / 2,
                        (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - h / 2,
                        w, h, lane_colors[i]);

      int iw = w / 2;
      int ih = h / 2;
      vg_draw_rectangle(lane_x_center - iw / 2,
                        (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - ih / 2,
                        iw, ih, 0xFFFFFF);

      if (animate_effects) hit_effect_frames[i]--;
    }
  }

  for (int i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active && notes[i].y >= HORIZON_Y - 10) { // Ocultar notas acima do horizonte
      int pista = (notes[i].x - LANE_BASE_X) / LANE_WIDTH;
      if (pista < 0) pista = 0;
      if (pista > 4) pista = 4;
      uint32_t *mapa_atual = mapas_notas[pista];

      if (mapa_atual != NULL) {
        vg_draw_xpm_image_tinted(mapa_atual, img_notas[pista].width, img_notas[pista].height,
                                 notes[i].x + 18, notes[i].y + 8, 0xFF00FF, 0x1A1A1A);
        vg_draw_xpm_image(mapa_atual, img_notas[pista].width, img_notas[pista].height,
                          notes[i].x + 10, notes[i].y, 0xFF00FF, true);
      } else {
        vg_draw_rectangle(notes[i].x + 15, notes[i].y, 50, 20, cores_pistas[pista]);
      }
    }
  }

  draw_score_hud();
  draw_health_bar();

  // Instrução para mudar de fundo (Canto superior direito)
  vg_draw_rectangle(440, 20, 340, 32, 0x111122);
  draw_border_main(440, 20, 340, 32, 0x00FFFF, 2);
  draw_text(450, 28, "PRESS M TO CHANGE BG", 2, 0xFFFFFF);

  // Barra de progresso da música
  draw_progress_bar(elapsed_ticks);
}

static void change_volume(bool increase, bool uart_ready) {
  if (increase) {
    if (volume_percent < 100) {
      volume_percent += 10;
      if (uart_send_audio_event(uart_ready, UART_EVENT_VOLUME_UP, "VOLUME_UP")) {
        printf("[UART] Evento VOLUME_UP (0x%02x) enviado.\n", UART_EVENT_VOLUME_UP);
      }
    }
  } else {
    if (volume_percent > 0) {
      volume_percent -= 10;
      if (uart_send_audio_event(uart_ready, UART_EVENT_VOLUME_DOWN, "VOLUME_DOWN")) {
        printf("[UART] Evento VOLUME_DOWN (0x%02x) enviado.\n", UART_EVENT_VOLUME_DOWN);
      }
    }
  }
}

static void draw_pause_menu(int mouse_x, int mouse_y) {
  hover_pause_resume = (menu_mouse_active() &&
                        mouse_x >= PAUSE_RESUME_X && mouse_x <= PAUSE_RESUME_X + PAUSE_BTN_W &&
                        mouse_y >= PAUSE_RESUME_Y && mouse_y <= PAUSE_RESUME_Y + PAUSE_BTN_H) ||
                       (menu_keyboard_active() && kbd_pause_idx == 1);
  hover_pause_quit = (menu_mouse_active() &&
                      mouse_x >= PAUSE_QUIT_X && mouse_x <= PAUSE_QUIT_X + PAUSE_BTN_W &&
                      mouse_y >= PAUSE_QUIT_Y && mouse_y <= PAUSE_QUIT_Y + PAUSE_BTN_H) ||
                     (menu_keyboard_active() && kbd_pause_idx == 2);
  
  hover_pause_vol_down = (menu_mouse_active() &&
                          mouse_x >= 300 && mouse_x <= 335 &&
                          mouse_y >= 415 && mouse_y <= 450);
  hover_pause_vol_up = (menu_mouse_active() &&
                        mouse_x >= 465 && mouse_x <= 500 &&
                        mouse_y >= 415 && mouse_y <= 450);

  // Caixa central com design Sci-Fi
  vg_draw_rectangle(PAUSE_PANEL_X, PAUSE_PANEL_Y, PAUSE_PANEL_W, PAUSE_PANEL_H, 0x111122);
  draw_border_main(PAUSE_PANEL_X, PAUSE_PANEL_Y, PAUSE_PANEL_W, PAUSE_PANEL_H, 0x00FFFF, 2);

  draw_text_centered(400, 175, "PAUSED", 6, 0x00FFFF);
  draw_text_centered(400, 225, "ESC TO RESUME", 2, 0x008888);

  uint32_t resume_color = hover_pause_resume ? 0x00FF88 : 0x008844;
  uint32_t quit_color = hover_pause_quit ? 0xFF4444 : 0x882222;

  if (hover_pause_resume) draw_border_main(PAUSE_RESUME_X, PAUSE_RESUME_Y, PAUSE_BTN_W, PAUSE_BTN_H, 0x00FF00, 4);
  vg_draw_rectangle(PAUSE_RESUME_X, PAUSE_RESUME_Y, PAUSE_BTN_W, PAUSE_BTN_H, resume_color);
  draw_text_centered(400, PAUSE_RESUME_Y + 20, "RESUME", 3, 0xFFFFFF);

  if (hover_pause_quit) draw_border_main(PAUSE_QUIT_X, PAUSE_QUIT_Y, PAUSE_BTN_W, PAUSE_BTN_H, 0xFF0000, 4);
  vg_draw_rectangle(PAUSE_QUIT_X, PAUSE_QUIT_Y, PAUSE_BTN_W, PAUSE_BTN_H, quit_color);
  draw_text_centered(400, PAUSE_QUIT_Y + 20, "QUIT SONG", 3, 0xFFFFFF);

  // Controle de volume
  char vol_str[16];
  snprintf(vol_str, sizeof(vol_str), "VOL: %d%%", volume_percent);
  draw_text_centered(400, 422, vol_str, 2, 0x00FFFF);

  uint32_t vol_down_color = hover_pause_vol_down ? 0x00FFFF : 0x008888;
  vg_draw_rectangle(300, 415, 35, 35, vol_down_color);
  draw_text_centered(317, 422, "-", 2, 0xFFFFFF);

  uint32_t vol_up_color = hover_pause_vol_up ? 0x00FFFF : 0x008888;
  vg_draw_rectangle(465, 415, 35, 35, vol_up_color);
  draw_text_centered(482, 422, "+", 2, 0xFFFFFF);
}

static bool pause_resume_clicked(int mouse_x, int mouse_y) {
  return mouse_x >= PAUSE_RESUME_X && mouse_x <= PAUSE_RESUME_X + PAUSE_BTN_W &&
         mouse_y >= PAUSE_RESUME_Y && mouse_y <= PAUSE_RESUME_Y + PAUSE_BTN_H;
}

static bool pause_quit_clicked(int mouse_x, int mouse_y) {
  return mouse_x >= PAUSE_QUIT_X && mouse_x <= PAUSE_QUIT_X + PAUSE_BTN_W &&
         mouse_y >= PAUSE_QUIT_Y && mouse_y <= PAUSE_QUIT_Y + PAUSE_BTN_H;
}

static void save_final_score(void) {
  rtc_timestamp timestamp;

  if (rtc_read_time(&timestamp) != 0) {
    write_log("[LEADERBOARD] Nao foi possivel ler o RTC. Score nao guardado.\n");
    return;
  }

  uint32_t total_ticks = (beatmap_count > 0) ? beatmap[beatmap_count - 1].spawn_tick + 120 : 1;
  uint32_t elapsed_ticks = no_interrupts - play_start_tick;
  if (elapsed_ticks > total_ticks) elapsed_ticks = total_ticks;
  int progress = (elapsed_ticks * 100) / total_ticks;

  leaderboard_add_score(current_username, score, progress, song_id, timestamp);
  write_log("[LEADERBOARD] Score %d de %s (Progresso: %d%%) guardado em %02d/%02d/20%02d %02d:%02d:%02d.\n",
            score,
            current_username,
            progress,
            timestamp.day,
            timestamp.month,
            timestamp.year,
            timestamp.hours,
            timestamp.minutes,
            timestamp.seconds);
}

static void finish_current_run(bool uart_ready) {
  if (uart_ready && music_started) {
    uart_send_audio_event(uart_ready, UART_EVENT_GAME_END, "FIM_JOGO");
    printf("[UART] Evento FIM_JOGO (0x%02x) enviado.\n", UART_EVENT_GAME_END);
  }

  clear_audio_queue();
  save_final_score();

  music_started = false;
  current_state = GAME_WON;
  printf("[GAME] Fim da run. Score final=%d, melhor combo=%d.\n", score, best_combo);
  write_log("[GAME] Fim da run. Score final=%d, melhor combo=%d.\n", score, best_combo);
}

static void enter_pause(bool uart_ready) {
  if (current_state != PLAY || !music_started) return;

  pause_start_tick = no_interrupts;
  pause_elapsed_ticks = no_interrupts - play_start_tick;
  reset_lane_key_state();
  clear_audio_queue();

  if (uart_send_audio_event(uart_ready, UART_EVENT_MUSIC_PAUSE, "PAUSA_MUSICA")) {
    printf("[UART] Evento PAUSA_MUSICA (0x%02x) enviado.\n", UART_EVENT_MUSIC_PAUSE);
  }

  current_state = PAUSE;
}

static void resume_paused_run(bool uart_ready) {
  if (current_state != PAUSE) return;

  play_start_tick += no_interrupts - pause_start_tick;
  reset_lane_key_state();

  if (uart_send_audio_event(uart_ready, UART_EVENT_MUSIC_RESUME, "RETOMAR_MUSICA")) {
    printf("[UART] Evento RETOMAR_MUSICA (0x%02x) enviado.\n", UART_EVENT_MUSIC_RESUME);
  }

  current_state = PLAY;
}

static void quit_paused_run(bool uart_ready) {
  if (uart_ready && music_started) {
    uart_send_audio_event(uart_ready, UART_EVENT_GAME_END, "SAIR_MUSICA");
    printf("[UART] Evento SAIR_MUSICA (0x%02x) enviado.\n", UART_EVENT_GAME_END);
  }

  clear_audio_queue();
  init_notes();
  reset_score();
  reset_lane_key_state();

  for (int i = 0; i < NUM_LANES; i++) {
    hit_effect_frames[i] = 0;
    miss_effect_frames[i] = 0;
  }

  beatmap_count = 0;
  current_note_idx = 0;
  pause_start_tick = 0;
  pause_elapsed_ticks = 0;
  music_started = false;
  current_state = MENU;
}

/**
 * Tenta acertar numa nota na pista correspondente ao make_code premido.
 * @return A pista acertada (0-4) em caso de hit; -1 se a pista for invalida
 *         ou se nao houver nota na hit zone (miss ativo).
 */


static int try_hit_note(uint8_t make_code) {
  int lane = lane_from_make_code(make_code);
  if (lane < 0) return -1;  

  for (int i = 0; i < MAX_NOTES; i++) {
    if (!notes[i].active) continue;

    int note_lane = (notes[i].x - LANE_BASE_X) / LANE_WIDTH;
    if (note_lane == lane && note_collides_with_hit_zone(&notes[i])) {
      notes[i].active = false;
      DEBUG_PRINTF("ACERTOU! (pista %d)\n", lane);
      return lane;
    }
  }

  DEBUG_PRINTF("MISS! (pista %d)\n", lane);
  return -2;
}

static void handle_play_key(uint8_t scancode) {
  bool is_break = (scancode & BIT(7)) != 0;
  uint8_t make_code = scancode & ~BIT(7);
  int lane = lane_from_make_code(make_code);

  if (lane < 0) return;

  if (is_break) {
    lane_key_down[lane] = false;
    return;
  }

  /* Evita auto-repeat: uma tecla segurada nao deve gerar hits/misses infinitos. */
  if (lane_key_down[lane]) return;
  lane_key_down[lane] = true;

  int hit_result = try_hit_note(make_code);
  if (hit_result >= 0) {
    int points = register_hit_score();
    (void) points;
    DEBUG_PRINTF("SCORE: +%d (total=%d, combo=%d)\n", points, score, combo_hits);
    queue_audio_event(UART_EVENT_HIT);
    hit_effect_frames[hit_result] = 12;
  } else if (hit_result == -2) {
    register_miss_score();
    queue_audio_event(UART_EVENT_MISS);
    if (lane >= 0 && lane < NUM_LANES) {
      miss_effect_frames[lane] = 8;
    }
  }
}

int main(int argc, char *argv[]) {
  lcf_set_language("EN-US");
  if (lcf_start(argc, argv)) return 1;
  lcf_cleanup();
  return 0;
}
static void draw_loading_progress(int current, int total) {
  vg_clear_back_buffer(0x111122); // Fundo escuro azulado
  draw_text_centered(400, 100, "FEUP HERO", 6, 0x00FFFF); // Título adicionado

  draw_border_main(200, 220, 400, 180, 0x00FFFF, 4); // Borda Ciano Néon
  vg_draw_rectangle(200, 220, 400, 180, 0x000000); // Fundo preto
  draw_text_centered(400, 240, "LOADING ASSETS...", 3, 0x00FFFF);

  // Mensagens divertidas dependendo do progresso
  const char* funny_msgs[] = {
      "WARMING UP THE BAND...",
      "TUNING THE GUITARS...",
      "TIME TO STRETCH FINGERS...",
      "TESTING THE MICS...",
      "GET READY TO ROCK..."
  };
  int msg_idx = (current * 5) / (total > 0 ? total : 1);
  if (msg_idx >= 5) msg_idx = 4;
  draw_text_centered(400, 280, funny_msgs[msg_idx], 2, 0xFFFF00);

  // Loading bar outline
  int bar_x = 240;
  int bar_y = 320;
  int bar_w = 320;
  int bar_h = 24;
  draw_border_main(bar_x, bar_y, bar_w, bar_h, 0x555555, 2);
  vg_draw_rectangle(bar_x, bar_y, bar_w, bar_h, 0x222222);

  if (total > 0) {
    int fill_w = (bar_w * current) / total;
    vg_draw_rectangle(bar_x, bar_y, fill_w, bar_h, 0x00FF00); // Green fill
  }

  char text[32];
  snprintf(text, sizeof(text), "%d / %d", current, total);
  draw_text_centered(400, 360, text, 2, 0xFFFFFF);

  vg_swap_buffers();
}

int (proj_main_loop)(int argc, char *argv[]) {

  bool uart_ready = (uart_init() == 0);
  if (!uart_ready) {
    printf("[UART] AVISO: falha ao inicializar COM1. Audio desativado.\n");
  } else {
    printf("[UART] COM1 inicializada a 115200 bps (8N1). Pronta.\n");
  }

  if (vg_init(0x115) == NULL) {
    printf("Falha ao iniciar o modo grafico!\n");
    return 1;
  }

  uint8_t timer = 0;
  uint32_t freq = 60;
  if (timer_set_frequency(timer, freq) != 0) {
    vg_exit();
    return 1;
  }

  int total_assets = 14;
  int current_asset = 0;
  draw_loading_progress(current_asset, total_assets);

  for (int i = 0; i < 5; i++) {
    hit_effect_frames[i] = 0;
    miss_effect_frames[i] = 0;
  }
  // --- PRÉ-CARREGAMENTO DOS FUNDOS DE GAMEPLAY ---
  xpm_image_t bg_img;
  uint32_t *bg_maps[3];
  bg_maps[0] = (uint32_t *)xpm_load((xpm_map_t)bg1_xpm, XPM_8_8_8_8, &bg_img);
  draw_loading_progress(++current_asset, total_assets);
  
  bg_maps[1] = (uint32_t *)xpm_load((xpm_map_t)bg2_xpm, XPM_8_8_8_8, &bg_img);
  draw_loading_progress(++current_asset, total_assets);
  
  bg_maps[2] = (uint32_t *)xpm_load((xpm_map_t)bg3_xpm, XPM_8_8_8_8, &bg_img);
  draw_loading_progress(++current_asset, total_assets);
  int current_bg_idx = 0;

  xpm_image_t arcade_leaderboard_img;
  uint32_t *arcade_leaderboard_map = (uint32_t *)xpm_load((xpm_map_t)arcade_leaderboard_xpm, XPM_8_8_8_8, &arcade_leaderboard_img);
  draw_loading_progress(++current_asset, total_assets);

  xpm_image_t menu_img;
  uint32_t *menu_map_bytes = (uint32_t *)xpm_load((xpm_map_t)menu_xpm, XPM_8_8_8_8, &menu_img);
  draw_loading_progress(++current_asset, total_assets);

  xpm_image_t graffiti_song_img;
  uint32_t *graffiti_song_map = (uint32_t *)xpm_load((xpm_map_t)graffiti_song_select_xpm, XPM_8_8_8_8, &graffiti_song_img);
  draw_loading_progress(++current_asset, total_assets);

  xpm_image_t graffiti_user_img;
  uint32_t *graffiti_user_map = (uint32_t *)xpm_load((xpm_map_t)graffiti_username_entry_xpm, XPM_8_8_8_8, &graffiti_user_img);
  uint32_t *menu_map = (uint32_t *) menu_map_bytes;
  draw_loading_progress(++current_asset, total_assets);

  xpm_image_t mission_failed_img;
  uint32_t *mission_failed_map = (uint32_t *)xpm_load((xpm_map_t)mission_failed_xpm, XPM_8_8_8_8, &mission_failed_img);
  draw_loading_progress(++current_asset, total_assets);

  xpm_image_t mission_success_img;
  uint32_t *mission_success_map = (uint32_t *)xpm_load((xpm_map_t)mission_success_xpm, XPM_8_8_8_8, &mission_success_img);
  draw_loading_progress(++current_asset, total_assets);

  if (bg_maps[0] == NULL || bg_maps[1] == NULL || bg_maps[2] == NULL) {
    printf("Aviso: Falha ao pré-carregar os XPMs de fundo!\n");
  }

  xpm_image_t img_notas[5];
  uint32_t *mapas_notas[5];

  mapas_notas[0] = (uint32_t *)xpm_load((xpm_map_t)nota_verde_xpm, XPM_8_8_8_8, &img_notas[0]);
  draw_loading_progress(++current_asset, total_assets);
  
  mapas_notas[1] = (uint32_t *)xpm_load((xpm_map_t)nota_vermelha_xpm, XPM_8_8_8_8, &img_notas[1]);
  draw_loading_progress(++current_asset, total_assets);
  
  mapas_notas[2] = (uint32_t *)xpm_load((xpm_map_t)nota_azul_xpm, XPM_8_8_8_8, &img_notas[2]);
  draw_loading_progress(++current_asset, total_assets);
  
  mapas_notas[3] = (uint32_t *)xpm_load((xpm_map_t)nota_roxa_xpm, XPM_8_8_8_8, &img_notas[3]);
  draw_loading_progress(++current_asset, total_assets);
  
  mapas_notas[4] = (uint32_t *)xpm_load((xpm_map_t)nota_amarela_xpm, XPM_8_8_8_8, &img_notas[4]);
  draw_loading_progress(++current_asset, total_assets);

  uint32_t cores_pistas[5] = {0x00FF00, 0xFF0000, 0x0000FF, 0x800080, 0xFFFF00};
  {
    FILE *log_init = fopen("/tmp/log.txt", "w");
    if (log_init != NULL) {
      fprintf(log_init, "--- DETAILED GAME LOG ---\n");
      fclose(log_init);
    }
  }
  write_log("[SYSTEM] Motor de fisica e video iniciados a 60 Hz.\n");
  leaderboard_init();
  write_log("[LEADERBOARD] %d scores carregados.\n", leaderboard_get_count());

  /* --- INSCRIÇÃO NAS INTERRUPÇÕES DE HARDWARE --- */
  uint8_t timer_bit_no;
  if (timer_subscribe_int(&timer_bit_no) != 0) {
    vg_exit();
    return 1;
  }
  uint32_t timer_irq_set = BIT(timer_bit_no);

  uint8_t mouse_bit_no;
  if (mouse_subscribe_int(&mouse_bit_no) != 0) {
    timer_unsubscribe_int();
    vg_exit();
    return 1;
  }
  uint32_t mouse_irq_set = BIT(mouse_bit_no);

  if (mouse_write_command(0xF4) != 0) {
    mouse_unsubscribe_int();
    timer_unsubscribe_int();
    vg_exit();
    return 1;
  }

  // Esvaziar Out Buffer do KBC e Subscrever Teclado
  {
    uint32_t stat, trash;
    int limit = 20; 
    while (limit-- > 0) {
      if (sys_inb(KBC_STAT_REG, &stat) != 0) break;
      if (stat & KBC_OBF) {
        sys_inb(KBC_OUT_BUF, &trash);
        tickdelay(micros_to_ticks(DELAY_US));
      } else {
        break;
      }
    }
  }
  scancode_byte = 0;

  uint8_t kbd_bit_no;
  if (kbd_subscribe_int(&kbd_bit_no) != 0) {
    mouse_unsubscribe_int();
    timer_unsubscribe_int();
    vg_exit();
    return 1;
  }
  uint32_t kbd_irq_set = BIT(kbd_bit_no);

  int ipc_status;
  message msg;
  int r;
  bool game_running = true;

  uint8_t mouse_bytes[3];
  uint8_t mouse_byte_index = 0;
  struct packet mouse_packet;


  while (game_running) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          if (msg.m_notify.interrupts & kbd_irq_set) {
            kbc_ih();
            if (!ih_error) {
              if (scancode_byte == ESC_MAKE_CODE) {
                if (current_state == PLAY && music_started) {
                  menu_set_keyboard_input();
                  enter_pause(uart_ready);
                } else if (current_state == PAUSE) {
                  resume_paused_run(uart_ready);
                }
              }
              else if (scancode_byte == ESC_BREAKCODE) {
                if (current_state == USERNAME_ENTRY ||
                    current_state == SONG_SELECT ||
                    current_state == GAME_OVER || current_state == GAME_WON ||
                    current_state == LEADERBOARD) {
                  menu_set_keyboard_input();
                  current_state = MENU;
                  music_started = false;
                } else if (current_state == MENU) {
                  game_running = false;
                }
              }
              else if (current_state == MENU) {
                if (scancode_byte == 0x48) { // UP
                  menu_set_keyboard_input();
                  kbd_menu_idx--;
                  if (kbd_menu_idx < 1) kbd_menu_idx = 3;
                } else if (scancode_byte == 0x50) { // DOWN
                  menu_set_keyboard_input();
                  kbd_menu_idx++;
                  if (kbd_menu_idx > 3) kbd_menu_idx = 1;
                } else if (scancode_byte == 0x1C) { // ENTER
                  menu_set_keyboard_input();
                  if (kbd_menu_idx == 1) { current_state = USERNAME_ENTRY; reset_username_entry(); }
                  else if (kbd_menu_idx == 2) current_state = LEADERBOARD;
                  else if (kbd_menu_idx == 3) game_running = false;
                }
              }
              else if (current_state == SONG_SELECT) {
                if (scancode_byte == 0x48) { // UP
                  menu_set_keyboard_input();
                  kbd_song_idx--;
                  if (kbd_song_idx < 1) kbd_song_idx = 5;
                } else if (scancode_byte == 0x50) { // DOWN
                  menu_set_keyboard_input();
                  kbd_song_idx++;
                  if (kbd_song_idx > 5) kbd_song_idx = 1;
                } else if (scancode_byte == 0x1C) { // ENTER
                  menu_set_keyboard_input();
                  if (kbd_song_idx == 1) { song_id = 1; current_state = PLAY; music_started = false; }
                  else if (kbd_song_idx == 2) { song_id = 4; current_state = PLAY; music_started = false; }
                  else if (kbd_song_idx == 3) { song_id = 2; current_state = PLAY; music_started = false; }
                  else if (kbd_song_idx == 4) { song_id = 3; current_state = PLAY; music_started = false; }
                  else if (kbd_song_idx == 5) { current_state = MENU; music_started = false; }
                }
              }
              else if (current_state == PAUSE) {
                if (scancode_byte == 0x48) { // UP
                  menu_set_keyboard_input();
                  kbd_pause_idx--;
                  if (kbd_pause_idx < 1) kbd_pause_idx = 2;
                } else if (scancode_byte == 0x50) { // DOWN
                  menu_set_keyboard_input();
                  kbd_pause_idx++;
                  if (kbd_pause_idx > 2) kbd_pause_idx = 1;
                } else if (scancode_byte == 0x4B) { // LEFT (Decrease Volume)
                  menu_set_keyboard_input();
                  change_volume(false, uart_ready);
                } else if (scancode_byte == 0x4D) { // RIGHT (Increase Volume)
                  menu_set_keyboard_input();
                  change_volume(true, uart_ready);
                } else if (scancode_byte == 0x1C) { // ENTER
                  menu_set_keyboard_input();
                  if (kbd_pause_idx == 1) resume_paused_run(uart_ready);
                  else if (kbd_pause_idx == 2) quit_paused_run(uart_ready);
                }
              }
              else if (current_state == GAME_OVER || current_state == GAME_WON || current_state == LEADERBOARD) {
                if (scancode_byte == 0x4B && current_state != LEADERBOARD) { // LEFT
                  menu_set_keyboard_input();
                  kbd_gameover_idx = 1;
                } else if (scancode_byte == 0x4D && current_state != LEADERBOARD) { // RIGHT
                  menu_set_keyboard_input();
                  kbd_gameover_idx = 2;
                } else if (scancode_byte == 0x1C) { // ENTER
                  menu_set_keyboard_input();
                  if (current_state == LEADERBOARD) {
                    current_state = MENU;
                    music_started = false;
                  } else {
                    if (kbd_gameover_idx == 1) { // MENU
                      current_state = MENU;
                      music_started = false;
                    } else if (kbd_gameover_idx == 2) { // RESTART/SONGS
                      current_state = (current_state == GAME_WON) ? SONG_SELECT : PLAY;
                      music_started = false;
                    }
                  }
                }
              }
              else if (current_state == USERNAME_ENTRY) {
                if (!(scancode_byte & BIT(7))) menu_set_keyboard_input();
                handle_username_key(scancode_byte);
              }
              else if (current_state == PLAY) {
                if (scancode_byte == 0x32) { // 'M' key Make Code
                  current_bg_idx = (current_bg_idx + 1) % 3;
                } else if (scancode_byte == 0x11) { // 'W' key Make Code for instant win
                  finish_current_run(uart_ready);
                } else {
                  handle_play_key(scancode_byte);
                }
              }
            }
          }

          if (msg.m_notify.interrupts & mouse_irq_set) {
            mouse_ih();
            if (!mouse_error) {
              mouse_bytes[mouse_byte_index] = mouse_byte;
              if (mouse_byte_index == 0 && !(mouse_bytes[0] & BIT(3))) {
                mouse_byte_index = 0;
              } else {
                mouse_byte_index++;
              }

              if (mouse_byte_index == 3) {
                mouse_parse_packet(mouse_bytes, &mouse_packet);
                mouse_x += mouse_packet.delta_x;
                mouse_y -= mouse_packet.delta_y;
                clamp_mouse_position();

                if (is_menu_state(current_state) && mouse_packet_has_input(&mouse_packet)) {
                  menu_set_mouse_input();
                }

                if (current_state == MENU) {
                  GameState previous_state = current_state;
                  check_menu_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state, &game_running);
                  if (previous_state == MENU && current_state == USERNAME_ENTRY) {
                    reset_username_entry();
                  }
                } else if (current_state == USERNAME_ENTRY && mouse_packet.lb) {
                  // BACK
                  if (mouse_x >= USERNAME_BACK_X && mouse_x <= USERNAME_BACK_X + USERNAME_BTN_W &&
                      mouse_y >= USERNAME_BACK_Y && mouse_y <= USERNAME_BACK_Y + USERNAME_BTN_H) {
                    current_state = MENU;
                    music_started = false;
                  }
                  // DONE
                  else if (mouse_x >= USERNAME_DONE_X && mouse_x <= USERNAME_DONE_X + USERNAME_BTN_W &&
                           mouse_y >= USERNAME_DONE_Y && mouse_y <= USERNAME_DONE_Y + USERNAME_BTN_H) {
                    accept_username_entry();
                  }
                } else if (current_state == SONG_SELECT) {
                  check_song_select_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state);
                  /* Ao entrar em PLAY a partir de SONG_SELECT, reset da musica */
                  if (current_state == PLAY) music_started = false;
                } else if (current_state == PAUSE && mouse_packet.lb) {
                  menu_set_mouse_input();
                  if (pause_resume_clicked(mouse_x, mouse_y)) {
                    resume_paused_run(uart_ready);
                  } else if (pause_quit_clicked(mouse_x, mouse_y)) {
                    quit_paused_run(uart_ready);
                  } else if (mouse_x >= 300 && mouse_x <= 335 && mouse_y >= 415 && mouse_y <= 450) {
                    change_volume(false, uart_ready);
                  } else if (mouse_x >= 465 && mouse_x <= 500 && mouse_y >= 415 && mouse_y <= 450) {
                    change_volume(true, uart_ready);
                  }
                } else if ((current_state == GAME_OVER || current_state == GAME_WON || current_state == LEADERBOARD) && mouse_packet.lb) {
                  if (current_state == LEADERBOARD &&
                      mouse_x >= LEADERBOARD_BACK_X && mouse_x <= LEADERBOARD_BACK_X + LEADERBOARD_BACK_W &&
                      mouse_y >= LEADERBOARD_BACK_Y && mouse_y <= LEADERBOARD_BACK_Y + LEADERBOARD_BACK_H) {
                    current_state = MENU;
                    music_started = false;
                  } else if (current_state == GAME_OVER || current_state == GAME_WON) {
                    if (mouse_y >= 520 && mouse_y <= 570) {
                      if (mouse_x >= 120 && mouse_x <= 380) { // Main Menu button
                        current_state = MENU;
                        music_started = false;
                      } else if (mouse_x >= 420 && mouse_x <= 680) { // Restart/Songs button
                        current_state = (current_state == GAME_WON) ? SONG_SELECT : PLAY;
                        music_started = false;
                      }
                    }
                  }
                }
                mouse_byte_index = 0;
              }
            }
          }

          if (msg.m_notify.interrupts & timer_irq_set) {
            timer_int_handler();
            flush_audio_events(uart_ready);

            /*
             * Each state's draw function handles clearing the screen or drawing a full background.
             * This prevents double-clearing and massive memory bandwidth bottlenecks.
             */

            if (current_state == MENU) {
              draw_main_menu(mouse_x, mouse_y, menu_map, menu_img);
              draw_text_centered(400, 260, "PLAY", 3, 0x000000);
              draw_text_centered(400, 350, "HALL OF FAME", 3, 0x000000);
              draw_text_centered(400, 440, "EXIT", 3, 0x000000);
            } else if (current_state == USERNAME_ENTRY) {
              draw_username_entry_screen(graffiti_user_map, graffiti_user_img);
            } else if (current_state == SONG_SELECT) {
              draw_song_select(mouse_x, mouse_y, graffiti_song_map, graffiti_song_img);
              
              // Título e User com Efeito 3D (Sombra)
              draw_text_centered(400 + 4, 40 + 4, "SELECT TRACK", 5, 0x000000);
              draw_text_centered(400, 40, "SELECT TRACK", 5, 0x00FFFF);
              
              // Desenha o nome do jogador com fundo à esquerda
              int name_w = text_width_pixels(current_username, 2);
              draw_border_main(10, 10, name_w + 20, 36, 0x00FFFF, 2); // borda ciano fina
              vg_draw_rectangle(10, 10, name_w + 20, 36, 0x222233); // fundo escuro
              draw_text(20 + 2, 20 + 2, current_username, 2, 0x000000);
              draw_text(20, 20, current_username, 2, 0xFFFF00);
              
              // Textos das Músicas (Sombra nas músicas também para destacarem)
              draw_text_centered(400 + 2, 145 + 2, "EVERY TIME WE TOUCH", 2, 0x000000);
              draw_text_centered(400, 145, "EVERY TIME WE TOUCH", 2, 0xFFFFFF);
              
              draw_text_centered(400 + 2, 235 + 2, "SUMMER", 2, 0x000000);
              draw_text_centered(400, 235, "SUMMER", 2, 0xFFFFFF);
              
              draw_text_centered(400 + 2, 325 + 2, "DIAMOND MORNING", 2, 0x000000);
              draw_text_centered(400, 325, "DIAMOND MORNING", 2, 0xFFFFFF);
              
              draw_text_centered(400 + 2, 415 + 2, "HIGHWAY TO HELL", 2, 0x000000);
              draw_text_centered(400, 415, "HIGHWAY TO HELL", 2, 0xFFFFFF);
              
              // Texto Back
              draw_text_centered(400, 515, "BACK", 3, 0x000000);
            } 
            else if (current_state == PLAY) {
              if (!music_started) {
                play_start_tick = no_interrupts;

                // --- CARREGAMENTO DO BEATMAP DINÂMICO AO INICIAR ---
                char rel_path[64];
                char tmp_path1[128];
                char tmp_path2[128];
                char abs_path1[128];
                char abs_path2[128];
                char abs_path3[128];
                char abs_path4[128];
                char abs_path5[128];
                char abs_path6[128];

                snprintf(rel_path, sizeof(rel_path), "beatmaps/song%d.txt", song_id);
                snprintf(tmp_path1, sizeof(tmp_path1), "/tmp/beatmaps/song%d.txt", song_id);
                snprintf(tmp_path2, sizeof(tmp_path2), "/tmp/song%d.txt", song_id);
                snprintf(abs_path1, sizeof(abs_path1), "/shares/lcom/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);
                snprintf(abs_path2, sizeof(abs_path2), "/home/lcom/labs/shared/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);
                snprintf(abs_path3, sizeof(abs_path3), "/home/lcom/labs/Project/beatmaps/song%d.txt", song_id);
                snprintf(abs_path4, sizeof(abs_path4), "/home/lcom/labs/g2/Project/beatmaps/song%d.txt", song_id);
                snprintf(abs_path5, sizeof(abs_path5), "/home/lcom/labs/shared/Project/beatmaps/song%d.txt", song_id);
                snprintf(abs_path6, sizeof(abs_path6), "/home/lcom/shared/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);

                const char *candidate_paths[] = {
                  tmp_path1,
                  tmp_path2,
                  rel_path,
                  abs_path1,
                  abs_path2,
                  abs_path3,
                  abs_path4,
                  abs_path5,
                  abs_path6
                };
                int num_candidates = sizeof(candidate_paths) / sizeof(candidate_paths[0]);

                beatmap_count = 0;
                current_note_idx = 0;

                write_log("[BEATMAP] A tentar carregar beatmap para musica %d...\n", song_id);
                for (int i = 0; i < num_candidates; i++) {
                  if (beatmap_load(candidate_paths[i], beatmap, &beatmap_count) == 0 && beatmap_count > 0) {
                    write_log("[BEATMAP] SUCESSO! Carregado a partir de: %s\n", candidate_paths[i]);
                    break;
                  }
                }

                if (beatmap_count == 0) {
                  write_log("[BEATMAP] ERRO CRITICO: Nao foi possivel carregar o beatmap de nenhum caminho candidato!\n");
                } else {
                  write_log("[BEATMAP] %d notas carregadas com sucesso. Jogo pronto.\n", beatmap_count);
                }

                init_notes();
                reset_score();
                reset_lane_key_state();
                clear_audio_queue();

                uint8_t start_event = UART_EVENT_GAME_START_SONG1;
                if (song_id == 2) start_event = UART_EVENT_GAME_START_SONG2;
                else if (song_id == 3) start_event = UART_EVENT_GAME_START_SONG3;
                else if (song_id == 4) start_event = UART_EVENT_GAME_START_SONG4;

                if (uart_send_audio_event(uart_ready, start_event, "INICIO_JOGO")) {
                  printf("[UART] Evento INICIO_JOGO (0x%02x) enviado pelo Menu para musica %d.\n", start_event, song_id);
                }
                music_started = true;
              }

              while (current_note_idx < beatmap_count) {
                if ((no_interrupts - play_start_tick) < beatmap[current_note_idx].spawn_tick) break;
                beatmap[current_note_idx].spawned = true;
                for (int j = 0; j < MAX_NOTES; j++) {
                  if (!notes[j].active) {
                    notes[j].x = LANE_BASE_X + (beatmap[current_note_idx].lane * LANE_WIDTH);
                    notes[j].y = 0;
                    notes[j].speed = 8;
                    notes[j].active = true;
                    notes[j].missed = false;
                    break;
                  }
                }
                current_note_idx++;
              }

              // --- TRIGGER PASSIVE MISS VISUAL FEEDBACK ---
              for (int i = 0; i < MAX_NOTES; i++) {
                if (notes[i].active && notes[i].y + notes[i].speed > HIT_ZONE_BOTTOM) {
                  int lane = (notes[i].x - LANE_BASE_X) / LANE_WIDTH;
                  if (lane >= 0 && lane < 5) {
                    miss_effect_frames[lane] = 8; // Trigger red flash on passive miss!
                  }
                }
              }

              int passive_misses = update_notes();
              if (passive_misses > 0) {
                register_miss_score();
                for (int m = 0; m < passive_misses; m++) {
                  queue_audio_event(UART_EVENT_MISS);
                }
              }

              draw_play_frame(bg_maps[current_bg_idx], bg_img, img_notas, mapas_notas, cores_pistas,
                              no_interrupts - play_start_tick, true);

              if (beatmap_count > 0 && current_note_idx >= beatmap_count && !any_active_notes()) {
                finish_current_run(uart_ready);
              }
            } // fecho do else if (current_state == PLAY)
            else if (current_state == PAUSE) {
              draw_play_frame(bg_maps[current_bg_idx], bg_img, img_notas, mapas_notas, cores_pistas,
                              pause_elapsed_ticks, false);
              draw_pause_menu(mouse_x, mouse_y);
            }
            else if (current_state == GAME_OVER) {
              draw_game_over_screen(mission_failed_map, mission_failed_img);
            }
            else if (current_state == GAME_WON) {
              draw_game_over_screen(mission_success_map, mission_success_img);
            }
            else if (current_state == LEADERBOARD) {
              draw_leaderboard_screen(arcade_leaderboard_map, arcade_leaderboard_img);
            }

            // SCI-FI MOUSE CURSOR (12x15)
            // T = Transparent (0xFF00FF), C = Cyan (0x00FFFF), D = Dark Cyan (0x008888), W = White (0xFFFFFF)
            static const uint32_t mouse_cursor_map[12*15] = {
              0xFF00FF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0x008888, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0x008888, 0x008888, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0x008888, 0x008888, 0x008888, 0x008888, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0x008888, 0x008888, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x008888, 0xFFFFFF, 0x008888, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x008888, 0x00FFFF, 0xFF00FF, 0xFFFFFF, 0x008888, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0x00FFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF, 0x008888, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF, 0x008888, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF,
              0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0x00FFFF, 0xFF00FF, 0xFF00FF, 0xFF00FF, 0xFF00FF
            };

            vg_draw_xpm_image(mouse_cursor_map, 12, 15, mouse_x, mouse_y, 0xFF00FF, 1);
            
            // Maintain session maximums
            if (score > session_best_score) session_best_score = score;
            if (best_combo > session_best_combo) session_best_combo = best_combo;
            
            vg_swap_buffers();
          }
          break;
        default:
          break;
      }
    }
  }

  /* Envia o evento de fecho/paragem de música para o script Python */
  if (uart_ready && music_started) {
    uart_send_audio_event(uart_ready, UART_EVENT_GAME_END, "FIM_JOGO");
    printf("[UART] Evento FIM_JOGO (0x%02x) enviado.\n", UART_EVENT_GAME_END);
  }

  mouse_write_command(0xF5); // Disable mouse data reporting

  mouse_unsubscribe_int();
  kbd_unsubscribe_int();
  timer_unsubscribe_int();

  // Esvaziar Out Buffer do KBC para o MINIX não bloquear o teclado
  {
    uint32_t stat, trash;
    int limit = 20; 
    while (limit-- > 0) {
      if (sys_inb(KBC_STAT_REG, &stat) != 0) break;
      if (stat & KBC_OBF) {
        sys_inb(KBC_OUT_BUF, &trash);
        tickdelay(micros_to_ticks(DELAY_US));
      } else {
        break;
      }
    }
  }
  vg_exit();

  // --- LOGICA DE GAME OVER ---
  printf("\n");
  printf("  _____  ______  _    _  _____    _    _  ______  _____    ____  \n");
  printf(" |  ___||  ____|| |  | ||  __ \\  | |  | ||  ____||  __ \\  / __ \\ \n");
  printf(" | |__  | |__   | |  | || |__) | | |__| || |__   | |__) || |  | |\n");
  printf(" |  __| |  __|  | |  | ||  ___/  |  __  ||  __|  |  _  / | |  | |\n");
  printf(" | |    | |____ | |__| || |      | |  | || |____ | | \\ \\ | |__| |\n");
  printf(" |_|    |______| \\____/ |_|      |_|  |_||______||_|  \\_\\ \\____/ \n");
  printf("\n");
  printf("   ___           _,.---.,_                                      ___\n");
  printf("  [ _ ]        ,'   _     `.                                   [ _ ]\n");
  printf("  [(_)]       /    (o)      \\                                  [(_)]\n");
  printf("  [___]      |               |                                 [___]\n");
  printf("  [   ]      |  _  ========|====================[|||]--.       [   ]\n");
  printf("  [(O)] )))  | (_) ========|====================|||||   |  ((( [(O)]\n");
  printf("  [___]       \\    ========|====================[|||]--'       [___]\n");
  printf("               `.          ,'                                      \n");
  printf("                 `--.___,-'                                        \n");
  printf("\n");
  printf("=========================================================\n");
  printf("      M A X  S C O R E : %-5d |  M A X  C O M B O : %-3d\n", session_best_score, session_best_combo);
  printf("=========================================================\n");

  rtc_timestamp tempo_atual;
  if (rtc_read_time(&tempo_atual) == 0) {
      printf("      Sessao terminada a: %02d/%02d/20%02d as %02d:%02d:%02d\n", 
             tempo_atual.day, tempo_atual.month, tempo_atual.year,
             tempo_atual.hours, tempo_atual.minutes, tempo_atual.seconds);
      printf("=========================================================\n");
  }
  printf("\n");

  return 0;

}
