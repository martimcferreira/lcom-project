#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/video/video.h"

#include "devices/video/assets/fundo_plateia.xpm"
#include "devices/video/assets/nota_verde.xpm"
#include "devices/video/assets/nota_vermelha.xpm"
#include "devices/video/assets/nota_azul.xpm"
#include "devices/video/assets/nota_roxa.xpm"
#include "devices/video/assets/nota_amarela.xpm"

#include "structures/includes/game.h"

extern uint32_t no_interrupts;
extern Note notes[];

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

  if (lcf_start(argc, argv)) {
    return 1;
  }

  lcf_cleanup();
  return 0;
}

int (proj_main_loop)(int argc, char *argv[]) {
  
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

  // Inicializar o array global de notas como limpo/inativo
  init_notes();
  extern Note notes[]; 

  // --- INICIALIZAR 5 NOTAS (UMA EM CADA PISTA) ---
  
  // Pista 0: Verde
  notes[0].x = 200;    
  notes[0].y = 0;      
  notes[0].speed = 4;  
  notes[0].active = true;

  // Pista 1: Vermelha
  notes[1].x = 280;    
  notes[1].y = -40;     
  notes[1].speed = 4;  
  notes[1].active = true;

  // Pista 2: Azul
  notes[2].x = 360;    
  notes[2].y = -80;      
  notes[2].speed = 4;  
  notes[2].active = true;

  // Pista 3: Roxa
  notes[3].x = 440;    
  notes[3].y = -40;      
  notes[3].speed = 4;  
  notes[3].active = true;

  // Pista 4: Amarela
  notes[4].x = 520;    
  notes[4].y = 0;      
  notes[4].speed = 4;  
  notes[4].active = true;

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

  
  xpm_image_t img_notas[5];
  uint32_t *mapas_notas[5];

  mapas_notas[0] = (uint32_t *)xpm_load((xpm_map_t)nota_verde_xpm, XPM_8_8_8_8, &img_notas[0]);
  mapas_notas[1] = (uint32_t *)xpm_load((xpm_map_t)nota_vermelha_xpm, XPM_8_8_8_8, &img_notas[1]);
  mapas_notas[2] = (uint32_t *)xpm_load((xpm_map_t)nota_azul_xpm, XPM_8_8_8_8, &img_notas[2]);
  mapas_notas[3] = (uint32_t *)xpm_load((xpm_map_t)nota_roxa_xpm, XPM_8_8_8_8, &img_notas[3]);
  mapas_notas[4] = (uint32_t *)xpm_load((xpm_map_t)nota_amarela_xpm, XPM_8_8_8_8, &img_notas[4]);

  // Array provisório caso alguma imagem falhe a carregar
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
