#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "devices/video/video.h"

/*static void write_log(const char *format, ...) {
  FILE *fp = fopen("/tmp/log.txt", "a");
  if (fp != NULL) {
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
  }
}
  */

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

#define HIT_ZONE_TOP 490
#define HIT_ZONE_BOTTOM 530
#define NOTE_HIT_HEIGHT 60

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

  if (uart_init() != 0) {
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

  init_notes();
  extern Note notes[]; 

  xpm_image_t bg_img;
  uint8_t *bg_map_bytes = xpm_load((xpm_map_t)fundo_plateia_xpm, XPM_8_8_8_8, &bg_img);
  uint32_t *bg_map = (uint32_t *) bg_map_bytes; 

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

  int song_id = 2;
  char rel_path[64], tmp_path1[128], tmp_path2[128], abs_path1[128], abs_path2[128], abs_path3[128], abs_path4[128], abs_path5[128], abs_path6[128];

  snprintf(rel_path, sizeof(rel_path), "beatmaps/song%d.txt", song_id);
  snprintf(tmp_path1, sizeof(tmp_path1), "/tmp/beatmaps/song%d.txt", song_id);
  snprintf(tmp_path2, sizeof(tmp_path2), "/tmp/song%d.txt", song_id);
  snprintf(abs_path1, sizeof(abs_path1), "/shares/lcom/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);
  snprintf(abs_path2, sizeof(abs_path2), "/home/lcom/labs/shared/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);
  snprintf(abs_path3, sizeof(abs_path3), "/home/lcom/labs/Project/beatmaps/song%d.txt", song_id);
  snprintf(abs_path4, sizeof(abs_path4), "/home/lcom/labs/g2/Project/beatmaps/song%d.txt", song_id);
  snprintf(abs_path5, sizeof(abs_path5), "/home/lcom/labs/shared/Project/beatmaps/song%d.txt", song_id);
  snprintf(abs_path6, sizeof(abs_path6), "/home/lcom/shared/grupo_2leic02_2/Project/beatmaps/song%d.txt", song_id);

  const char *candidate_paths[] = {tmp_path1, tmp_path2, rel_path, abs_path1, abs_path2, abs_path3, abs_path4, abs_path5, abs_path6};
  int num_candidates = sizeof(candidate_paths) / sizeof(candidate_paths[0]);

  for (int i = 0; i < num_candidates; i++) {
    if (beatmap_load(candidate_paths[i], beatmap, &beatmap_count) == 0 && beatmap_count > 0) break;
  }

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
                int hit_result = try_hit_note(scancode_byte);
                if (hit_result >= 0) {
                  uart_send_packet(0x20, (uint8_t)(hit_result + 1));
                } else if (hit_result == -2) {
                  uart_send_packet(0x21, 0x00); 
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
                }
                mouse_byte_index = 0;
              }
            }
          }

          if (msg.m_notify.interrupts & timer_irq_set) {
            timer_int_handler(); 
            vg_draw_rectangle(0, 0, 800, 600, 0x000000); 

            if (current_state == MENU) {
              draw_main_menu(mouse_x, mouse_y);
            } 
            else if (current_state == PLAY) {
              if (!music_started) {
                uart_send_packet(0x10, (uint8_t)song_id);
                printf("[UART] Pacote START MUSIC (0x10, 0x%02x) enviado pelo Menu.\n", song_id);
                music_started = true;
              }

              while (current_note_idx < beatmap_count) {
                if (no_interrupts < beatmap[current_note_idx].spawn_tick) break;
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

              int passive_misses = update_notes();
              if (passive_misses > 0) {
                for (int m = 0; m < passive_misses; m++) {
                  uart_send_packet(0x21, 0x00);
                }
              }

              if (bg_map != NULL) {
                for (int y = 0; y < bg_img.height; y++) {
                  for (int x = 0; x < bg_img.width; x++) {
                    vg_draw_pixel(x, y, bg_map[y * bg_img.width + x]);
                  }
                }
              }

              vg_draw_rectangle(200, 0, 400, 600, 0x30190E); 
              for (int v = 205; v < 595; v += 15) vg_draw_rectangle(v, 0, 3, 600, 0x241109);
              vg_draw_rectangle(200, 0, 10, 600, 0x110804); 
              vg_draw_rectangle(590, 0, 10, 600, 0x110804); 

              for (int i = 0; i <= 5; i++) {
                  int linha_x = 200 + (i * 80);
                  vg_draw_rectangle(linha_x - 1, 0, 1, 600, 0x111111); 
                  vg_draw_rectangle(linha_x, 0, 2, 600, 0xEEEEEE);     
                  vg_draw_rectangle(linha_x + 2, 0, 1, 600, 0x444444); 
              }

              int offset = (no_interrupts * 4) % 150;
              for (int i = -1; i <= 4; i++) {
                  int traste_y = (i * 150) + offset;
                  if (traste_y >= 0 && traste_y < 596) {
                      vg_draw_rectangle(200, traste_y + 2, 400, 3, 0x111111); 
                      vg_draw_rectangle(200, traste_y, 400, 2, 0x999999);     
                  }
              }

              vg_draw_rectangle(200, 498, 400, 2, 0x555555); 
              vg_draw_rectangle(200, 500, 400, 20, 0x222222); 
              vg_draw_rectangle(200, 520, 400, 4, 0x000000); 

              for (int i = 0; i < MAX_NOTES; i++) {
                if (notes[i].active) {
                  int pista = (notes[i].x - 200) / 80;
                  if (pista < 0) pista = 0;
                  if (pista > 4) pista = 4;
                  uint32_t *mapa_atual = mapas_notas[pista];

                  if (mapa_atual != NULL) {
                    for (int y = 0; y < img_notas[pista].height; y++) {
                      for (int x = 0; x < img_notas[pista].width; x++) {
                          uint32_t cor_pixel = mapa_atual[y * img_notas[pista].width + x];
                          if (cor_pixel != 0xFF00FF) vg_draw_pixel(notes[i].x + 10 + x + 8, notes[i].y + y + 8, 0x1A1A1A);
                      }
                    }
                    for (int y = 0; y < img_notas[pista].height; y++) {
                      for (int x = 0; x < img_notas[pista].width; x++) {
                          uint32_t cor_pixel = mapa_atual[y * img_notas[pista].width + x];
                          if (cor_pixel != 0xFF00FF) vg_draw_pixel(notes[i].x + 10 + x, notes[i].y + y, cor_pixel);
                      }
                    }
                  } else {
                    vg_draw_rectangle(notes[i].x + 15, notes[i].y, 50, 20, cores_pistas[pista]);
                  }
                }
              }
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

  /* --- SHUTDOWN: envia paragem de musica e desliga periféricos --- */
  uart_send_packet(0x11, 0x00);
  uart_send_packet(0x42, 0x00);
  
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
      printf("Sessao terminada a: %02d/%02d/20%02d as %02d:%02d:%02d\n", 
             tempo_atual.day, tempo_atual.month, tempo_atual.year,
             tempo_atual.hours, tempo_atual.minutes, tempo_atual.seconds);
      printf("=========================================\n\n");
  }

  return 0;
}
