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

#include "devices/video/assets/fundo_plateia.xpm"
#include "devices/video/assets/nota_verde.xpm"
#include "devices/video/assets/nota_vermelha.xpm"
#include "devices/video/assets/nota_azul.xpm"
#include "devices/video/assets/nota_roxa.xpm"
#include "devices/video/assets/nota_amarela.xpm"

#include "structures/includes/game.h"
#include "structures/includes/drivers/kbc.h"
#include "structures/includes/drivers/i8042.h"
#include "devices/uart/uart.h"
#include "structures/includes/beatmap_loader.h"

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

/**
 * Tenta acertar numa nota na pista correspondente ao make_code premido.
 * @return A pista acertada (0-4) em caso de hit; -1 se a pista for invalida
 *         ou se nao houver nota na hit zone (miss ativo).
 */
static int try_hit_note(uint8_t make_code) {
  int lane = lane_from_make_code(make_code);
  if (lane < 0) return -1;  /* Tecla que nao e de jogo -- ignora */

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

  if (lcf_start(argc, argv)) {
    return 1;
  }

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

  init_notes();
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

  /* --- SELECÇÃO DA MÚSICA & CARREGAMENTO DE BEATMAP DINÂMICO --- */
  int song_id = 2;

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

  uint8_t timer_bit_no;
  if (timer_subscribe_int(&timer_bit_no) != 0) {
    vg_exit();
    return 1;
  }
  uint32_t timer_irq_set = BIT(timer_bit_no);

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
    timer_unsubscribe_int();
    vg_exit();
    return 1;
  }
  uint32_t kbd_irq_set = BIT(kbd_bit_no);

  int ipc_status;
  message msg;
  int r;
  bool game_running = true;

  if (uart_send_audio_event(uart_ready, UART_EVENT_GAME_START, "INICIO_JOGO")) {
    printf("[UART] Evento INICIO_JOGO (0x%02x) enviado.\n", UART_EVENT_GAME_START);
  }

  while (game_running) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          if (msg.m_notify.interrupts & kbd_irq_set) {
            kbc_ih();

            if (!ih_error) {
              if (scancode_byte == ESC_BREAKCODE) {
                game_running = false;
              }
              else if ((scancode_byte & BIT(7)) == 0) {
                /*
                 * try_hit_note devolve:
                 *   >= 0  : pista acertada (hit)  -> envia byte 0x0A
                 *   == -2 : tecla valida mas miss  -> envia byte 0x0E
                 *   == -1 : tecla nao e de jogo   -> ignora
                 */
                int hit_result = try_hit_note(scancode_byte);
                if (hit_result >= 0) {
                  uart_send_audio_event(uart_ready, UART_EVENT_HIT, "ACERTOU");
                } else if (hit_result == -2) {
                  uart_send_audio_event(uart_ready, UART_EVENT_MISS, "ERRO");  /* Miss ativo */
                }
              }
            }
          }

          if (msg.m_notify.interrupts & timer_irq_set) {
            
            timer_int_handler(); 

            
             while (current_note_idx < beatmap_count) {
               if (no_interrupts < beatmap[current_note_idx].spawn_tick) {
                 break;
               }

               beatmap[current_note_idx].spawned = true;
               write_log("[DEBUG] NOTA SPAWNADA! idx=%d, tick=%u, lane=%d, no_interrupts=%u\n",
                      current_note_idx, beatmap[current_note_idx].spawn_tick, 
                      (int)beatmap[current_note_idx].lane, no_interrupts);

               for (int j = 0; j < MAX_NOTES; j++) {
                 if (!notes[j].active) {
                   notes[j].x      = 200 + (beatmap[current_note_idx].lane * 80);
                   notes[j].y      = 0;
                   notes[j].speed  = 4;
                   notes[j].active = true;
                   break;
                 }
               }
               current_note_idx++;
             }

            {
              int passive_misses = update_notes();
              if (passive_misses > 0) {
                for (int m = 0; m < passive_misses; m++) {
                  uart_send_audio_event(uart_ready, UART_EVENT_MISS, "ERRO");
                }
              }
            } 

            // --- 1. CAMADA DE FUNDO OTIMIZADA ---
            if (bg_map != NULL) {
              for (int y = 0; y < bg_img.height; y++) {
                for (int x = 0; x < bg_img.width; x++) {
                  vg_draw_pixel(x, y, bg_map[y * bg_img.width + x]);
                }
              }
            } else {
              vg_draw_rectangle(0, 0, 800, 600, 0x000000); 
            }

            // --- 1.5. O BRAÇO DE MADEIRA REALISTA ---
            // Base da madeira
            vg_draw_rectangle(200, 0, 400, 600, 0x30190E); 

            // Gerar veios da madeira dinâmicos (linhas verticais mais escuras)
            for (int v = 205; v < 595; v += 15) {
                vg_draw_rectangle(v, 0, 3, 600, 0x241109);
            }

            // Bordas 3D do braço (sombras laterais grossas para dar relevo)
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
            int offset = (no_interrupts * velocidade_scroll) % distancia_trastes;

            for (int i = -1; i <= 4; i++) {
                int traste_y = (i * distancia_trastes) + offset;
                
                // CORREÇÃO: Limite alterado de 600 para 596 para não abortar o retângulo!
                if (traste_y >= 0 && traste_y < 596) {
                    vg_draw_rectangle(200, traste_y + 2, 400, 3, 0x111111); // Sombra 3D projetada
                    vg_draw_rectangle(200, traste_y, 400, 2, 0x999999);     // Metal do traste
                }
            }

            // --- 3. A ZONA DE ACERTO COM RELEVO 3D ---
            vg_draw_rectangle(200, 498, 400, 2, 0x555555); // Brilho superior
            vg_draw_rectangle(200, 500, 400, 20, 0x222222); // Base
            vg_draw_rectangle(200, 520, 400, 4, 0x000000); // Sombra inferior profunda

            // --- 4. AS NOTAS A CAIR (COM DROP SHADOWS PARA PROFUNDIDADE) ---
            for (int i = 0; i < MAX_NOTES; i++) {
              if (notes[i].active) {
                
                int pista = (notes[i].x - 200) / 80;
                if (pista < 0) pista = 0;
                if (pista > 4) pista = 4;

                uint32_t *mapa_atual = mapas_notas[pista];

                if (mapa_atual != NULL) {
                  int margem_x = 10; 

                  // PASSO A: Desenhar as SOMBRAS da nota primeiro (+8px para baixo e direita)
                  for (int y = 0; y < img_notas[pista].height; y++) {
                    for (int x = 0; x < img_notas[pista].width; x++) {
                        uint32_t cor_pixel = mapa_atual[y * img_notas[pista].width + x];
                        if (cor_pixel != 0xFF00FF) {
                            // Pinta um píxel semitransparente escuro ligeiramente deslocado
                            vg_draw_pixel(notes[i].x + margem_x + x + 8, notes[i].y + y + 8, 0x1A1A1A);
                        }
                    }
                  }

                  // PASSO B: Desenhar a NOTA REAL por cima da sombra
                  for (int y = 0; y < img_notas[pista].height; y++) {
                    for (int x = 0; x < img_notas[pista].width; x++) {
                        uint32_t cor_pixel = mapa_atual[y * img_notas[pista].width + x];
                        if (cor_pixel != 0xFF00FF) {
                            vg_draw_pixel(notes[i].x + margem_x + x, notes[i].y + y, cor_pixel);
                        }
                    }
                  }
                } else {
                  vg_draw_rectangle(notes[i].x + 15, notes[i].y, 50, 20, cores_pistas[pista]);
                }
              }
            }

            // --- 5. DOUBLE BUFFERING SWAP ---
            vg_swap_buffers();
            
            // Debug regular no terminal a cada segundo
            if (no_interrupts % 60 == 0) {
              write_log("[DEBUG] Segundo %d: no_interrupts=%u, current_note_idx=%d, beatmap_count=%d\n", 
                        (no_interrupts / 60), no_interrupts, current_note_idx, beatmap_count);
            }
          }
          break;
        default:
          break;
      }
    }
  }

  /* O protocolo simples pedido para o Membro 3 só define início, acerto e erro. */
  printf("[UART] Jogo terminado. Sem evento UART extra no shutdown.\n");

  if (kbd_unsubscribe_int() != 0) {
    vg_exit();
    return 1;
  }

  if (timer_unsubscribe_int() != 0) {
    vg_exit();
    return 1;
  }

  kbd_enable_interrupts();
  
  vg_exit(); 

  printf("Ciclo terminado com sucesso.\n");
  return 0;
}
