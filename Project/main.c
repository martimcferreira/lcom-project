#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/video/assets/fundo_plateia.xpm"
#include "structures/includes/game.h"
#include "devices/video/video.h" 

extern uint32_t no_interrupts; 

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

  init_notes();

  extern Note notes[]; 

  // Inicializar a primeira nota na Pista 1 (Vermelho)
  // As pistas começam nos seguintes X: 200, 280, 360, 440, 520
  notes[0].x = 280;    
  notes[0].y = 0;      
  notes[0].speed = 4;  
  notes[0].active = true;

  int ipc_status;
  message msg;
  int r;
  bool game_running = true;

  // --- OTIMIZAÇÃO: CARREGAR O XPM APENAS UMA VEZ NA RAM ---
  xpm_image_t bg_img;
  // Carrega em modo True Color (XPM_8_8_8_8) condizente com o modo 0x115
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
            update_notes(); 

            // --- 1. CAMADA DE FUNDO OTIMIZADA ---
            // Despeja os píxeis pré-carregados diretamente para o back_buffer
            if (bg_map != NULL) {
              for (int y = 0; y < bg_img.height; y++) {
                for (int x = 0; x < bg_img.width; x++) {
                  vg_draw_pixel(x, y, bg_map[y * bg_img.width + x]);
                }
              }
            } else {
              // Fallback de segurança (fundo preto) caso a imagem falhe
              vg_draw_rectangle(0, 0, 800, 600, 0x000000); 
            }

            for (int i = 0; i <= 5; i++) {
                int linha_x = 200 + (i * 80);
                vg_draw_rectangle(linha_x, 0, 2, 600, 0x555555); // Linhas cinzentas
            }

            vg_draw_rectangle(200, 500, 400, 20, 0x333333);

            // --- 4. AS NOTAS A CAIR ---
            for (int i = 0; i < MAX_NOTES; i++) {
              if (notes[i].active) {
                // Calcular matematicamente em que pista a nota está baseada no seu X
                int pista = (notes[i].x - 200) / 80;
                
                // Proteção para não aceder fora do array de cores caso o X seja inválido
                if (pista < 0) pista = 0;
                if (pista > 4) pista = 4;

                // Desenha a nota com a cor correspondente e centrada na pista (+15px de margem)
                vg_draw_rectangle(notes[i].x + 15, notes[i].y, 50, 20, cores_pistas[pista]);
              }
            }

            // --- 5. SWAP BUFFERS ---
            vg_swap_buffers();
            
            if (no_interrupts % 60 == 0) {
              if (notes[0].active) {
                printf("[DEBUG] Segundo %d -> Posicao Y da nota: %d\n", (no_interrupts / 60), notes[0].y);
              } else {
                printf("[DEBUG] A nota chegou ao fundo!\n");
                game_running = false; 
              }
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
