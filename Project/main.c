#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "structures/includes/game.h"

extern uint32_t no_interrupts; 

int main(int argc, char *argv[]) {
  lcf_set_language("EN-US");
  if (lcf_start(argc, argv)) return 1;
  lcf_cleanup();
  return 0;
}

// A framework LCOM exige os parênteses à volta do nome da função!
int (proj_main_loop)(int argc, char *argv[]) {
  
  uint8_t timer = 0;
  uint32_t freq = 60;
  if (timer_set_frequency(timer, freq) != 0) {
    return 1;
  }

  uint8_t timer_bit_no;
  if (timer_subscribe_int(&timer_bit_no) != 0) {
    return 1;
  }
  uint32_t timer_irq_set = BIT(timer_bit_no);

  init_notes();

  extern Note notes[]; 
  notes[0].x = 200;    
  notes[0].y = 0;      
  notes[0].speed = 4;  
  notes[0].active = true;

  int ipc_status;
  message msg;
  int r;
  bool game_running = true;

  printf("Motor de física rítmica iniciado a 60 Hz. A simular queda...\n");

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

            // Print a cada segundo (60 frames)
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
    return 1;
  }

  printf("Ciclo terminado com sucesso.\n");
  return 0;
}
