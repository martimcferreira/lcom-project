#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "devices/video/assets/fundo_plateia.xpm"
#include "structures/includes/game.h"
#include "devices/video/video.h" 

extern uint32_t no_interrupts; 

// --- ESTRUTURA DO BEATMAP ---
typedef struct {
  uint32_t spawn_tick; // O tique exato do temporizador (60Hz) para a nota surgir
  uint8_t lane;        // A pista da nota (0 a 4)
  bool spawned;        // Flag para garantir que a nota só nasce uma vez
} BeatmapNote;

// Pauta da música (Exemplo com 4 notas a surgir em tempos e pistas diferentes)
BeatmapNote current_song[] = {
  {120, 0, false}, // Aos 2 segundos, nasce na Pista 0 (Verde)
  {180, 2, false}, // Aos 3 segundos, nasce na Pista 2 (Azul)
  {240, 1, false}, // Aos 4 segundos, nasce na Pista 1 (Vermelho)
  {300, 4, false}  // Aos 5 segundos, nasce na Pista 4 (Amarelo)
};

int main(int argc, char *argv[]) {
  lcf_set_language("EN-US");
  if (lcf_start(argc, argv)) return 1;
  lcf_cleanup();
  return 0;
}

int (proj_main_loop)(int argc, char *argv[]) {
  
  if (vg_init(0x115) == NULL) {
    printf("Falha ao iniciar o modo gráfico!\n");
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

  // Inicializar o array global de notas como limpo/inativo
  init_notes();
  extern Note notes[]; 

  int ipc_status;
  message msg;
  int r;
  bool game_running = true;

  // --- PRÉ-CARREGAMENTO DO XPM NA RAM ---
  xpm_image_t bg_img;
  uint8_t *bg_map_bytes = xpm_load((xpm_map_t)fundo_plateia_xpm, XPM_8_8_8_8, &bg_img);
  uint32_t *bg_map = (uint32_t *) bg_map_bytes; 

  if (bg_map == NULL) {
    printf("Aviso: Falha ao pré-carregar o XPM de fundo!\n");
  }

  // Array com as cores das 5 pistas (Verde, Vermelho, Azul, Roxo, Amarelo)
  uint32_t cores_pistas[5] = {0x00FF00, 0xFF0000, 0x0000FF, 0x800080, 0xFFFF00};

  printf("Motor de física e vídeo iniciados a 60 Hz...\n");

  while (game_running) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          if (msg.m_notify.interrupts & timer_irq_set) {
            
            timer_int_handler(); 

            // --- LÓGICA DE SPAWN DO BEATMAP ---
            for (size_t i = 0; i < (sizeof(current_song) / sizeof(current_song[0])); i++) {
              // Se o tique atual for igual ao spawn_tick da nota e ela ainda não nasceu
              if (!current_song[i].spawned && no_interrupts == current_song[i].spawn_tick) {
                current_song[i].spawned = true;

                // Procurar um slot livre no array global de notas do jogo para a ativar
                for (int j = 0; j < MAX_NOTES; j++) {
                  if (!notes[j].active) {
                    // Define o X com base na pista (largura de 80px por pista, a começar no X=200)
                    notes[j].x = 200 + (current_song[i].lane * 80);
                    notes[j].y = 0;
                    notes[j].speed = 4;
                    notes[j].active = true;
                    
                    printf("[DEBUG] Spawning nota na pista %u no tique %u\n", current_song[i].lane, current_song[i].spawn_tick);
                    break; 
                  }
                }
              }
            }

            // Atualizar a física de todas as notas que se encontram ativas
            update_notes(); 

            // --- 1. DESENHAR O FUNDO (XPM) ---
            if (bg_map != NULL) {
              for (int y = 0; y < bg_img.height; y++) {
                for (int x = 0; x < bg_img.width; x++) {
                  vg_draw_pixel(x, y, bg_map[y * bg_img.width + x]);
                }
              }
            } else {
              vg_draw_rectangle(0, 0, 800, 600, 0x000000); 
            }

            // --- 2. DESENHAR AS LINHAS DAS PISTAS ---
            for (int i = 0; i <= 5; i++) {
                int linha_x = 200 + (i * 80);
                vg_draw_rectangle(linha_x, 0, 2, 600, 0x555555); 
            }

            // --- 3. DESENHAR A ZONA DE HIT ---
            vg_draw_rectangle(200, 500, 400, 20, 0x333333);

            // --- 4. DESENHAR AS NOTAS ATIVAS NO ECO ---
            for (int i = 0; i < MAX_NOTES; i++) {
              if (notes[i].active) {
                int pista = (notes[i].x - 200) / 80;
                
                // Salvaguarda para evitar acessos fora do array de cores
                if (pista < 0) pista = 0;
                if (pista > 4) pista = 4;

                // Desenha o retângulo colorido centrado na respetiva pista (+15px de desvio estrutural)
                vg_draw_rectangle(notes[i].x + 15, notes[i].y, 50, 20, cores_pistas[pista]);
              }
            }

            // --- 5. DOUBLE BUFFERING SWAP ---
            vg_swap_buffers();
            
            // Debug regular no terminal a cada segundo
            if (no_interrupts % 60 == 0) {
              printf("[DEBUG] Segundo %d de jogo decorrido.\n", (no_interrupts / 60));
            }
          }
          break;
        default:
          break; 
      }
    }
  }

  if (timer_unsubscribe_int() != 0) {
    vg_exit(); 
    return 1;
  }
  
  vg_exit(); 

  printf("Ciclo terminado com sucesso.\n");
  return 0;
}
