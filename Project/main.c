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
#include "devices/video/assets/fundo_plateia.xpm"
#include "devices/video/assets/nota_verde.xpm"
#include "devices/video/assets/nota_vermelha.xpm"
#include "devices/video/assets/nota_azul.xpm"
#include "devices/video/assets/nota_roxa.xpm"
#include "devices/video/assets/nota_amarela.xpm"

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
static char score_text[16] = "0";
static char combo_text[16] = "0X";
static bool score_hud_dirty = true;
static bool lane_key_down[NUM_LANES] = {false};
static char current_username[LEADERBOARD_USERNAME_MAX] = "PLAYER";
static char username_edit_buffer[LEADERBOARD_USERNAME_MAX] = "";
static int username_edit_length = 0;

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
#define ENTER_MAKE_CODE 0x1C
#define BACKSPACE_MAKE_CODE 0x0E
#ifndef G_MAKE_CODE
#define G_MAKE_CODE 0x22
#endif

// Estado Global e Coordenadas do Rato
GameState current_state = MENU;
bool music_started = false;
uint32_t play_start_tick = 0;
int song_id = 1;

int mouse_x = 400;
int mouse_y = 300;

static void clamp_mouse_position(void) {
  if (mouse_x < 0) mouse_x = 0;
  if (mouse_x > 799) mouse_x = 799;
  if (mouse_y < 0) mouse_y = 0;
  if (mouse_y > 599) mouse_y = 599;
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

static void register_miss_score(void) {
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

static void update_score_hud_cache(void) {
  if (!score_hud_dirty) return;

  snprintf(score_text, sizeof(score_text), "%d", score);
  snprintf(combo_text, sizeof(combo_text), "%dX", combo_hits);
  score_hud_dirty = false;
}

static void draw_score_hud(void) {
  update_score_hud_cache();

  draw_text(18, 18, "SCORE", 3, 0xFFFFFF);
  draw_text(18, 44, score_text, 4, 0xFFFF00);

  draw_text(18, 88, "COMBO", 2, 0xFFFFFF);
  draw_text(18, 108, combo_text, 3, 0x00FFFF);
}

static void draw_leaderboard_summary(void) {
  char row[48];
  LeaderboardEntry *scores = leaderboard_get_scores();
  int count = leaderboard_get_count();

  draw_text(470, 185, "TOP 5", 3, 0xFFFFFF);

  if (count == 0) {
    draw_text(470, 230, "NO SCORES", 2, 0xAAAAAA);
    return;
  }

  for (int i = 0; i < count && i < 5; i++) {
    snprintf(row, sizeof(row), "%d %s %d",
             i + 1,
             scores[i].username,
             scores[i].score);
    draw_text(470, 230 + i * 34, row, 2, 0xDDDDDD);
  }
}

static void draw_game_over_screen(void) {
  char value[16];

  vg_draw_rectangle(0, 0, 800, 600, 0x000000);
  draw_text_centered(400, 75, "GAME OVER", 6, 0xFFFFFF);

  draw_text(90, 205, "FINAL SCORE", 3, 0xFFFF00);
  snprintf(value, sizeof(value), "%d", score);
  draw_text(90, 245, value, 6, 0x00FF00);

  draw_text(90, 360, "BEST COMBO", 3, 0xFFFFFF);
  snprintf(value, sizeof(value), "%dX", best_combo);
  draw_text(90, 400, value, 4, 0x00FFFF);

  draw_leaderboard_summary();

  draw_text_centered(400, 530, "CLICK TO MENU", 2, 0xAAAAAA);
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
    accept_username_entry();
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

static void draw_username_entry_screen(void) {
  const char *visible_name = (username_edit_length == 0) ? "PLAYER" : username_edit_buffer;

  vg_draw_rectangle(0, 0, 800, 600, 0x000000);
  draw_text_centered(400, 90, "USERNAME", 6, 0xFFFFFF);
  draw_text_centered(400, 175, "TYPE YOUR NAME", 3, 0xAAAAAA);

  vg_draw_rectangle(170, 245, 460, 80, 0x222222);
  vg_draw_rectangle(170, 245, 460, 4, 0x00FFFF);
  vg_draw_rectangle(170, 321, 460, 4, 0x00FFFF);
  vg_draw_rectangle(170, 245, 4, 80, 0x00FFFF);
  vg_draw_rectangle(626, 245, 4, 80, 0x00FFFF);
  draw_text_centered(400, 270, visible_name, 4, username_edit_length == 0 ? 0x777777 : 0xFFFF00);

  draw_text_centered(400, 390, "ENTER TO CONTINUE", 2, 0xFFFFFF);
  draw_text_centered(400, 425, "BACKSPACE TO DELETE", 2, 0xAAAAAA);
  draw_text_centered(400, 500, "ESC TO MENU", 2, 0x777777);
}

static void draw_leaderboard_screen(void) {
  LeaderboardEntry *scores = leaderboard_get_scores();
  int count = leaderboard_get_count();
  char row[64];

  vg_draw_rectangle(0, 0, 800, 600, 0x000000);
  draw_text_centered(400, 55, "LEADERBOARD", 5, 0xFFFFFF);

  draw_text(125, 130, "RANK", 2, 0x00FFFF);
  draw_text(235, 130, "NAME", 2, 0x00FFFF);
  draw_text(430, 130, "SCORE", 2, 0x00FFFF);
  draw_text(560, 130, "DATE", 2, 0x00FFFF);
  vg_draw_rectangle(110, 160, 580, 3, 0x333333);

  if (count == 0) {
    draw_text_centered(400, 280, "NO SCORES YET", 3, 0xAAAAAA);
  } else {
    int rows = (count < MAX_SCORES) ? count : MAX_SCORES;
    for (int i = 0; i < rows; i++) {
      snprintf(row, sizeof(row), "%d", i + 1);
      draw_text(140, 190 + i * 34, row, 2, 0xFFFFFF);

      draw_text(235, 190 + i * 34, scores[i].username, 2, 0xFFFF00);

      snprintf(row, sizeof(row), "%d", scores[i].score);
      draw_text(430, 190 + i * 34, row, 2, 0x00FF00);

      snprintf(row, sizeof(row), "%02d/%02d/%02d",
               scores[i].date.day,
               scores[i].date.month,
               scores[i].date.year);
      draw_text(560, 190 + i * 34, row, 2, 0xAAAAAA);
    }
  }

  draw_text_centered(400, 545, "CLICK OR ESC TO MENU", 2, 0xAAAAAA);
}

static void save_final_score(void) {
  rtc_timestamp timestamp;

  if (rtc_read_time(&timestamp) != 0) {
    write_log("[LEADERBOARD] Nao foi possivel ler o RTC. Score nao guardado.\n");
    return;
  }

  leaderboard_add_score(current_username, score, timestamp);
  write_log("[LEADERBOARD] Score %d de %s guardado em %02d/%02d/20%02d %02d:%02d:%02d.\n",
            score,
            current_username,
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
  current_state = GAME_OVER;
  printf("[GAME] Fim da run. Score final=%d, melhor combo=%d.\n", score, best_combo);
  write_log("[GAME] Fim da run. Score final=%d, melhor combo=%d.\n", score, best_combo);
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

  for (int i = 0; i < 5; i++) {
    hit_effect_frames[i] = 0;
    miss_effect_frames[i] = 0;
  }
  // --- PRÉ-CARREGAMENTO DO XPM NA RAM ---
  xpm_image_t bg_img;
  uint8_t *bg_map_bytes = xpm_load((xpm_map_t)fundo_plateia_xpm, XPM_8_8_8_8, &bg_img);
  uint32_t *bg_map = (uint32_t *) bg_map_bytes; 

  if (bg_map == NULL) {
    printf("Aviso: Falha ao pré-carregar o XPM de fundo!\n");
  }

  xpm_image_t img_notas[5];
  uint32_t *mapas_notas[5];

  mapas_notas[0] = (uint32_t *)xpm_load((xpm_map_t)nota_verde_xpm, XPM_8_8_8_8, &img_notas[0]);
  mapas_notas[1] = (uint32_t *)xpm_load((xpm_map_t)nota_vermelha_xpm, XPM_8_8_8_8, &img_notas[1]);
  mapas_notas[2] = (uint32_t *)xpm_load((xpm_map_t)nota_azul_xpm, XPM_8_8_8_8, &img_notas[2]);
  mapas_notas[3] = (uint32_t *)xpm_load((xpm_map_t)nota_roxa_xpm, XPM_8_8_8_8, &img_notas[3]);
  mapas_notas[4] = (uint32_t *)xpm_load((xpm_map_t)nota_amarela_xpm, XPM_8_8_8_8, &img_notas[4]);

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
              if (scancode_byte == ESC_BREAKCODE) {
                if (current_state == PLAY && music_started) {
                  finish_current_run(uart_ready);
                } else if (current_state == USERNAME_ENTRY ||
                           current_state == SONG_SELECT ||
                           current_state == GAME_OVER ||
                           current_state == LEADERBOARD) {
                  current_state = MENU;
                  music_started = false;
                } else {
                  game_running = false;
                }
              }
              else if (current_state == USERNAME_ENTRY) {
                handle_username_key(scancode_byte);
              }
              else if (current_state == PLAY) {
                handle_play_key(scancode_byte);
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

                if (current_state == MENU) {
                  GameState previous_state = current_state;
                  check_menu_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state, &game_running);
                  if (previous_state == MENU && current_state == USERNAME_ENTRY) {
                    reset_username_entry();
                  }
                } else if (current_state == SONG_SELECT) {
                  check_song_select_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state);
                  /* Ao entrar em PLAY a partir de SONG_SELECT, reset da musica */
                  if (current_state == PLAY) music_started = false;
                } else if ((current_state == GAME_OVER || current_state == LEADERBOARD) && mouse_packet.lb) {
                  current_state = MENU;
                  music_started = false;
                }
                mouse_byte_index = 0;
              }
            }
          }

          if (msg.m_notify.interrupts & timer_irq_set) {
            timer_int_handler();
            flush_audio_events(uart_ready);

            /*
             * Only clear when the current screen does not draw a full background.
             * In PLAY, the preloaded background covers the whole framebuffer, so
             * clearing first just burns CPU for no visible benefit. Classic.
             */
            if (current_state != PLAY || bg_map == NULL) {
              vg_clear_back_buffer(0x000000);
            }

            if (current_state == MENU) {
              draw_main_menu(mouse_x, mouse_y);
              draw_text_centered(400, 282, "LEADERBOARD", 2, 0xFFFFFF);
            } else if (current_state == USERNAME_ENTRY) {
              draw_username_entry_screen();
            } else if (current_state == SONG_SELECT) {
              draw_song_select(mouse_x, mouse_y);
              draw_text_centered(400, 135, current_username, 3, 0xFFFF00);
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

                uint8_t start_event = (song_id == 2) ? UART_EVENT_GAME_START_SONG2 : UART_EVENT_GAME_START_SONG1;
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
                    notes[j].speed = 4;
                    notes[j].active = true;
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

              // --- 1. CAMADA DE FUNDO OTIMIZADA ---
              if (bg_map != NULL) {
                vg_draw_xpm_image(bg_map, bg_img.width, bg_img.height, 0, 0, 0, false);
              } else {
                vg_clear_back_buffer(0x000000);
              }

              // --- 1.5. O BRAÇO DE MADEIRA REALISTA ---
              vg_draw_rectangle(200, 0, 400, 600, 0x30190E); 

              // Veios da madeira dinâmicos
              for (int v = 205; v < 595; v += 15) {
                  vg_draw_rectangle(v, 0, 3, 600, 0x241109);
              }

              // Bordas 3D do braço
              vg_draw_rectangle(200, 0, 10, 600, 0x110804); 
              vg_draw_rectangle(590, 0, 10, 600, 0x110804); 

              // --- 2. AS CORDAS DA GUITARRA (Efeito Metálico 3D) ---
              for (int i = 0; i <= 5; i++) {
                  int linha_x = 200 + (i * 80);
                  vg_draw_rectangle(linha_x - 1, 0, 1, 600, 0x111111); // Sombra esquerda
                  vg_draw_rectangle(linha_x, 0, 2, 600, 0xEEEEEE);     // Brilho central
                  vg_draw_rectangle(linha_x + 2, 0, 1, 600, 0x444444); // Sombra direita
              }

              // --- 2.5. OS TRASTES (Scrolling Infinito com Correção de Limites) ---
              int distancia_trastes = 150; 
              int velocidade_scroll = 4; 
              int offset = ((no_interrupts - play_start_tick) * velocidade_scroll) % distancia_trastes;

              for (int i = -1; i <= 4; i++) {
                  int traste_y = (i * distancia_trastes) + offset;
                  if (traste_y >= 0 && traste_y < 596) {
                      vg_draw_rectangle(200, traste_y + 2, 400, 3, 0x111111); // Sombra 3D projetada
                      vg_draw_rectangle(200, traste_y, 400, 2, 0x999999);     // Metal do traste
                  }
              }

              // --- 3. A ZONA DE ACERTO ---
              vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_TOP - 2, NUM_LANES * LANE_WIDTH, 2, 0x555555); // Brilho superior
              vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_TOP, NUM_LANES * LANE_WIDTH, HIT_ZONE_BOTTOM - HIT_ZONE_TOP, 0x222222); // Base
              vg_draw_rectangle(LANE_BASE_X, HIT_ZONE_BOTTOM, NUM_LANES * LANE_WIDTH, 4, 0x000000); // Sombra inferior

              for (int i = 0; i < 5; i++) {
                int lane_x_center = LANE_BASE_X + i * LANE_WIDTH + LANE_WIDTH / 2;
                uint32_t lane_colors[5] = {0x00FF00, 0xFF0000, 0x0000FF, 0x800080, 0xFFFF00}; // Verde, Vermelho, Azul, Roxo, Amarelo

                // Desenhar MISS FLASH (Vermelho)
                if (miss_effect_frames[i] > 0) {
                  // Flash vermelho que cobre a zona de toque da pista
                  vg_draw_rectangle(LANE_BASE_X + i * LANE_WIDTH + 10, HIT_ZONE_TOP, LANE_WIDTH - 20, HIT_ZONE_BOTTOM - HIT_ZONE_TOP, 0xFF0000);
                  miss_effect_frames[i]--;
                }

                // Desenhar HIT GLOW (Brilho Expansivo)
                if (hit_effect_frames[i] > 0) {
                  int frame_diff = 12 - hit_effect_frames[i];
                  int w = 20 + frame_diff * 4;   // Expande horizontalmente
                  int h = 8 + frame_diff * 2;    // Expande verticalmente
                  
                  // Desenhar halo de cor da pista
                  vg_draw_rectangle(lane_x_center - w / 2, (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - h / 2, w, h, lane_colors[i]);
                  
                  int iw = w / 2;
                  int ih = h / 2;
                  vg_draw_rectangle(lane_x_center - iw / 2, (HIT_ZONE_TOP + HIT_ZONE_BOTTOM) / 2 - ih / 2, iw, ih, 0xFFFFFF);
                  
                  hit_effect_frames[i]--;
                }
              }

              for (int i = 0; i < MAX_NOTES; i++) {
                if (notes[i].active) {
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

              if (beatmap_count > 0 && current_note_idx >= beatmap_count && !any_active_notes()) {
                finish_current_run(uart_ready);
              }
            } // fecho do else if (current_state == PLAY)
            else if (current_state == GAME_OVER) {
              draw_game_over_screen();
            }
            else if (current_state == LEADERBOARD) {
              draw_leaderboard_screen();
            }
            vg_draw_rectangle(mouse_x, mouse_y, 10, 10, 0xFFFFFF);
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

  if (mouse_unsubscribe_int() != 0) return 1;
  if (kbd_unsubscribe_int() != 0) return 1;
  if (timer_unsubscribe_int() != 0) return 1;

  kbd_enable_interrupts();
  vg_exit(); 

  // --- LOGICA DE GAME OVER COM RTC ---
  rtc_timestamp tempo_atual;
  if (rtc_read_time(&tempo_atual) == 0) {
      printf("\n=========================================\n");
      printf("              GAME OVER                  \n");
      printf("=========================================\n");
      printf("Score final: %d | Melhor combo: %d\n", score, best_combo);
      printf("=========================================\n");
      printf("Sessao terminada a: %02d/%02d/20%02d as %02d:%02d:%02d\n", 
             tempo_atual.day, tempo_atual.month, tempo_atual.year,
             tempo_atual.hours, tempo_atual.minutes, tempo_atual.seconds);
      printf("=========================================\n\n");
  }

  return 0;

}
