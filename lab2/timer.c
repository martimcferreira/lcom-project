#include <lcom/lcf.h>
#include <lcom/timer.h>
#include <stdint.h>

#include "i8254.h"

// hook_id usado pelo sistema de interrupções.
// O valor inicial costuma ser 0 para o timer 0.
static int hook_id_timer = 0;

// Contador global incrementado sempre que ocorre uma interrupção do timer.
// "volatile" porque pode ser alterado fora do fluxo normal do programa.
volatile int timer_counter = 0;

//Devolve os bits de seleção do timer para o control word.
static uint8_t timer_select_bits(uint8_t timer) {
  switch (timer) {
    case 0: return TIMER_SEL0;
    case 1: return TIMER_SEL1;
    case 2: return TIMER_SEL2;
    default: return 0;
  }
}

//Devolve a porta correspondente ao timer.
static uint8_t timer_port(uint8_t timer) {
  return TIMER_0 + timer;
}

//Lê o status byte de um timer.
int(timer_get_conf)(uint8_t timer, uint8_t *st) {
  // Validar argumentos
  if (st == NULL) return 1;
  if (timer > 2) return 1;
  uint8_t rb_cmd = TIMER_RB_CMD | TIMER_RB_COUNT_ | BIT(timer + 1);

  // Enviar comando para o registo de controlo
  if (sys_outb(TIMER_CTRL, rb_cmd) != OK) return 1;

  // Ler o status byte da porta do timer escolhido
  if (util_sys_inb(timer_port(timer), st) != 0) return 1;

  return 0;
}

//Interpreta o status byte e envia a informação para timer_print_config.
int(timer_display_conf)(uint8_t timer, uint8_t conf, enum timer_status_field field) {
  union timer_status_field_val val;

  switch (field) {
    case tsf_all:
      // Entrega diretamente o byte completo
      val.byte = conf;
      break;

    case tsf_initial: {
      //Bits 5 e 4 do status byte indicam o modo de inicialização
      uint8_t init = (conf >> 4) & 0x03;

      switch (init) {
        case 1:
          val.in_mode = LSB_only;
          break;
        case 2:
          val.in_mode = MSB_only;
          break;
        case 3:
          val.in_mode = MSB_after_LSB;
          break;
        default:
          val.in_mode = INVAL_val;
          break;
      }
      break;
    }
     case tsf_mode: {
      //Bits 3,2,1 indicam o modo.
      uint8_t mode = (conf >> 1) & 0x07;

      if (mode == 6) mode = 2;
      if (mode == 7) mode = 3;

      val.count_mode = mode;
      break;
    }
      case tsf_base:
      val.bcd = (conf & BIT(0)) != 0;
      break;

    default:
      return 1;
  }
  // Função dada pelo LCF para mostrar a configuração interpretada
  return timer_print_config(timer, field, val);
}

//Programa uma nova frequência para um timer.
int(timer_set_frequency)(uint8_t timer, uint32_t freq) {
  if (timer > 2) return 1;

  // freq não pode ser 0 e não deve exceder a frequência base
  if (freq == 0 || freq > TIMER_FREQ) return 1;

  // Divisor a programar no timer
  uint32_t div = TIMER_FREQ / freq;

  // O divisor tem de caber em 16 bits
  if (div == 0 || div > 0xFFFF) return 1;

  uint8_t status;
  if (timer_get_conf(timer, &status) != 0) return 1;
  uint8_t control = timer_select_bits(timer) | TIMER_LSB_MSB | (status & 0x0F);

  // Envia o novo control word
  if (sys_outb(TIMER_CTRL, control) != OK) return 1;

  uint8_t lsb, msb;

  // Divide o divisor em LSB e MSB
  if (util_get_LSB((uint16_t)div, &lsb) != 0) return 1;
  if (util_get_MSB((uint16_t)div, &msb) != 0) return 1;

  // O timer espera primeiro o LSB e depois o MSB
  if (sys_outb(timer_port(timer), lsb) != OK) return 1;
  if (sys_outb(timer_port(timer), msb) != OK) return 1;

  return 0;
}

//Handler de interrupção do timer.
void(timer_int_handler)() {
  timer_counter++;
}

//Subscreve interrupções do timer 0.
int(timer_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) return 1;

  // Guarda o valor atual do hook para o chamador poder fazer BIT(bit_no)
  *bit_no = hook_id_timer;

  // Regista a policy da interrupção
  if (sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &hook_id_timer) != OK)
    return 1;

  return 0;
}

//Remove a subscrição de interrupções do timer.
int(timer_unsubscribe_int)() {
  if (sys_irqrmpolicy(&hook_id_timer) != OK)
    return 1;

  return 0;
}
