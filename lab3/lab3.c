#include <lcom/lcf.h>
#include "i8042.h"
#include <lcom/lab3.h>
#include "kbc.h"
#include <stdbool.h>
#include <stdint.h>


extern uint8_t scancode_byte; 
extern bool ih_error;
extern uint32_t no_interrupts;

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/lab3/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab3/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int(kbd_test_scan)() {
  uint8_t bit_no;
  if (kbd_subscribe_int (&bit_no)!=0) return 1;
  uint32_t irq_set = BIT(bit_no);

  int r, ipc_status;
  message msg;
  uint8_t bytes[2];
  uint8_t size=0;
  while (scancode_byte != ESC_BREAKCODE){
    if ((r=driver_receive(ANY, &msg, &ipc_status)) !=0) continue;
    if (is_ipc_notify(ipc_status)){
      switch (_ENDPOINT_P(msg.m_source)){
        case HARDWARE:
        if (msg.m_notify.interrupts & irq_set){
          kbc_ih();
          if(ih_error)continue;

          bytes[size]=scancode_byte;
          size++;

          if (scancode_byte==TWO_BYTE_CODE)continue;
          bool make = !(scancode_byte & BIT(7));
          kbd_print_scancode(make, size, bytes);

          size=0;
        }
        break;
      }
    }
  }
  return kbd_unsubscribe_int();
}

int(kbd_test_poll)() {
  uint8_t bytes[2];
  uint8_t size = 0;
  uint8_t status;
  
  // 1. RESET obrigatório das globais para o loop começar limpo
  scancode_byte = 0; 
  ih_error = false;

  // 2. Loop de Polling
  while (scancode_byte != ESC_BREAKCODE) {
    if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;
    
    // Verifica se há dados para ler (OBF)
    if (status & KBC_OBF) {
      kbc_ih(); // O IH lê o porto 0x60
      if (ih_error) continue;

      bytes[size] = scancode_byte;
      size++;

      if (scancode_byte == TWO_BYTE_CODE) continue;

      bool make = !(scancode_byte & BIT(7));
      kbd_print_scancode(make, size, bytes);
      size = 0;
    }
    tickdelay(micros_to_ticks(DELAY_US)); // Essencial para o Minix
  }

  // 3. REATIVAR INTERRUPÇÕES (Obrigatório pela LCF)
  if (kbd_enable_interrupts() != 0) return 1;

  // REMOVIDO: kbd_unsubscribe_int() e timer_unsubscribe_int() 
  // No polling não subscrevemos nada, logo não podemos cancelar.
  
  return 0;
}

int (kbd_test_timed_scan)(uint8_t n) {
  scancode_byte = 0;
  no_interrupts = 0;
  ih_error = false;

  uint8_t bit_no_timer, bit_no_kbd;
  if (timer_subscribe_int(&bit_no_timer) != 0) return 1;
  if (kbd_subscribe_int(&bit_no_kbd) != 0) {
    timer_unsubscribe_int();
    return 1;
  }

  uint32_t irq_set_timer = BIT(bit_no_timer);
  uint32_t irq_set_kbd = BIT(bit_no_kbd);
  uint32_t time_limit = (uint32_t)n * 60;

  int r, ipc_status;
  message msg;
  uint8_t bytes[2];
  uint8_t size = 0;
  bool done = false;

  while (!done) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          if (msg.m_notify.interrupts & irq_set_timer) {
            timer_int_handler();
            if (no_interrupts >= time_limit) {
              done = true;
            }
          }

          if (msg.m_notify.interrupts & irq_set_kbd) {
            kbc_ih();
            if (ih_error) break;

            no_interrupts = 0; // reset timer ao pressionar tecla

            bytes[size] = scancode_byte;
            size++;

            if (scancode_byte == TWO_BYTE_CODE) break;

            bool make = !(scancode_byte & BIT(7));
            kbd_print_scancode(make, size, bytes);
            size = 0;

            if (scancode_byte == ESC_BREAKCODE) {
              done = true;
            }
          }
          break;
      }
    }
  }

  kbd_unsubscribe_int();
  timer_unsubscribe_int();
  return 0;
}
