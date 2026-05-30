#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "devices/video/video.h"

static void write_log(const char *format, ...) {
  FILE *fp = fopen("/tmp/log.txt", "a");
  if (fp != NULL) {
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
  }
}

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

#define HIT_ZONE_TOP 490
#define HIT_ZONE_BOTTOM 530
#define NOTE_HIT_HEIGHT 60

#define SCORE_BASE_POINTS 10
#define SCORE_TIER_2_COMBO 4
#define SCORE_TIER_3_COMBO 12
#define SCORE_TIER_4_COMBO 24
#define SCORE_TIER_5_COMBO 40

#define A_MAKE_CODE 0x1E
#define S_MAKE_CODE 0x1F
#define D_MAKE_CODE 0x20
#define F_MAKE_CODE 0x21
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
    printf("[UART] Falha ao enviar evento %s (0x%02x).\n", event_name, event_byte);
    return false;
  }

  return true;
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
}

static int register_hit_score(void) {
  combo_hits++;
  if (combo_hits > best_combo) best_combo = combo_hits;

  int points = points_for_combo_hit(combo_hits);
  score += points;
  write_log("[SCORE] Hit #%d da combo: +%d pontos (total=%d).\n", combo_hits, points, score);
  return points;
}

static void register_miss_score(void) {
  if (combo_hits > 0) {
    write_log("[SCORE] Miss: combo resetada de %d para 0. Pontuacao mantida em %d.\n", combo_hits, score);
  }
  combo_hits = 0;
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
  static const uint8_t glyph_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
  static const uint8_t glyph_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
  static const uint8_t glyph_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
  static const uint8_t glyph_I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const uint8_t glyph_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
  static const uint8_t glyph_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
  static const uint8_t glyph_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
  static const uint8_t glyph_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
  static const uint8_t glyph_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
  static const uint8_t glyph_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
  static const uint8_t glyph_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  static const uint8_t glyph_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  static const uint8_t glyph_V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
  static const uint8_t glyph_X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};

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
    case 'E': return glyph_E;
    case 'F': return glyph_F;
    case 'G': return glyph_G;
    case 'I': return glyph_I;
    case 'K': return glyph_K;
    case 'L': return glyph_L;
    case 'M': return glyph_M;
    case 'N': return glyph_N;
    case 'O': return glyph_O;
    case 'R': return glyph_R;
    case 'S': return glyph_S;
    case 'T': return glyph_T;
    case 'U': return glyph_U;
    case 'V': return glyph_V;
    case 'X': return glyph_X;
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

static void draw_score_hud(void) {
  char value[16];

  draw_text(18, 18, "SCORE", 3, 0xFFFFFF);
  snprintf(value, sizeof(value), "%d", score);
  draw_text(18, 44, value, 4, 0xFFFF00);

  draw_text(18, 88, "COMBO", 2, 0xFFFFFF);
  snprintf(value, sizeof(value), "%dX", combo_hits);
  draw_text(18, 108, value, 3, 0x00FFFF);
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

  draw_text_centered(400, 530, "CLICK TO MENU", 2, 0xAAAAAA);
}

static void finish_current_run(bool uart_ready) {
  if (uart_ready && music_started) {
    uart_send_audio_event(uart_ready, UART_EVENT_GAME_END, "FIM_JOGO");
    printf("[UART] Evento FIM_JOGO (0x%02x) enviado.\n", UART_EVENT_GAME_END);
  }

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

    int note_lane = (notes[i].x - 200) / 80;
    if (note_lane == lane && note_collides_with_hit_zone(&notes[i])) {
      notes[i].active = false;
      printf("ACERTOU! (pista %d)\n", lane);
      return lane;
    }
  }

  printf("MISS! (pista %d)\n", lane);
  return -2;
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
  extern Note notes[]; 

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
              if (scancode_byte == ESC_BREAKCODE) game_running = false;
              else if (current_state == PLAY && (scancode_byte & BIT(7)) == 0) {
                 int lane = lane_from_make_code(scancode_byte);
                 int hit_result = try_hit_note(scancode_byte);
                 if (hit_result >= 0) {
                   int points = register_hit_score();
                   printf("SCORE: +%d (total=%d, combo=%d)\n", points, score, combo_hits);
                   uart_send_audio_event(uart_ready, UART_EVENT_HIT, "ACERTOU");
                   hit_effect_frames[hit_result] = 12; // Trigger expanding color halo!
                 } else if (hit_result == -2) {
                   register_miss_score();
                   uart_send_audio_event(uart_ready, UART_EVENT_MISS, "ERRO");  /* Miss ativo */
                   if (lane >= 0 && lane < 5) {
                     miss_effect_frames[lane] = 8; // Trigger red flash in this lane!
                   }
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

                if (current_state == MENU) {
                  check_menu_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state, &game_running);
                } else if (current_state == SONG_SELECT) {
                  check_song_select_clicks(mouse_x, mouse_y, mouse_packet.lb, &current_state);
                  /* Ao entrar em PLAY a partir de SONG_SELECT, reset da musica */
                  if (current_state == PLAY) music_started = false;
                } else if (current_state == GAME_OVER && mouse_packet.lb) {
                  current_state = MENU;
                  music_started = false;
                }
                mouse_byte_index = 0;
              }
            }
          }

          if (msg.m_notify.interrupts & timer_irq_set) {
            timer_int_handler(); 

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
            } else if (current_state == SONG_SELECT) {
              draw_song_select(mouse_x, mouse_y);
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
                    notes[j].x = 200 + (beatmap[current_note_idx].lane * 80);
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
                if (notes[i].active && notes[i].y + notes[i].speed > 500) {
                  int lane = (notes[i].x - 200) / 80;
                  if (lane >= 0 && lane < 5) {
                    miss_effect_frames[lane] = 8; // Trigger red flash on passive miss!
                  }
                }
              }

              int passive_misses = update_notes();
              if (passive_misses > 0) {
                register_miss_score();
                for (int m = 0; m < passive_misses; m++) {
                  uart_send_audio_event(uart_ready, UART_EVENT_MISS, "ERRO");
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
              vg_draw_rectangle(200, 498, 400, 2, 0x555555); // Brilho superior
              vg_draw_rectangle(200, 500, 400, 20, 0x222222); // Base
              vg_draw_rectangle(200, 520, 400, 4, 0x000000); // Sombra inferior

              for (int i = 0; i < 5; i++) {
                int lane_x_center = 200 + i * 80 + 40;
                uint32_t lane_colors[5] = {0x00FF00, 0xFF0000, 0x0000FF, 0x800080, 0xFFFF00}; // Verde, Vermelho, Azul, Roxo, Amarelo

                // Desenhar MISS FLASH (Vermelho)
                if (miss_effect_frames[i] > 0) {
                  // Flash vermelho que cobre a zona de toque da pista
                  vg_draw_rectangle(200 + i * 80 + 10, 500, 60, 20, 0xFF0000);
                  miss_effect_frames[i]--;
                }

                // Desenhar HIT GLOW (Brilho Expansivo)
                if (hit_effect_frames[i] > 0) {
                  int frame_diff = 12 - hit_effect_frames[i];
                  int w = 20 + frame_diff * 4;   // Expande horizontalmente
                  int h = 8 + frame_diff * 2;    // Expande verticalmente
                  
                  // Desenhar halo de cor da pista
                  vg_draw_rectangle(lane_x_center - w / 2, 510 - h / 2, w, h, lane_colors[i]);
                  
                  int iw = w / 2;
                  int ih = h / 2;
                  vg_draw_rectangle(lane_x_center - iw / 2, 510 - ih / 2, iw, ih, 0xFFFFFF);
                  
                  hit_effect_frames[i]--;
                }
              }

              for (int i = 0; i < MAX_NOTES; i++) {
                if (notes[i].active) {
                  int pista = (notes[i].x - 200) / 80;
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
