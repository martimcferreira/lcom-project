#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdint.h>

#include "i8254.h"

int timer_set_frequency(uint8_t timer, uint32_t freq) {
  if (timer > 2 || freq < 19 || freq > TIMER_FREQ) {
    return 1;
  }

  uint8_t st;
  if (timer_get_conf(timer, &st) != 0) {
    return 1;
  }

  uint8_t ctrl_word = (uint8_t) ((timer << 6) | TIMER_LSB_MSB | (st & 0x0F));
  uint16_t div = (uint16_t) (TIMER_FREQ / freq);
  uint8_t lsb;
  uint8_t msb;

  if (util_get_LSB(div, &lsb) != 0 || util_get_MSB(div, &msb) != 0) {
    return 1;
  }

  if (sys_outb(TIMER_CTRL, ctrl_word) != 0) {
    return 1;
  }

  int port = TIMER_0 + timer;
  if (sys_outb(port, lsb) != 0) {
    return 1;
  }

  if (sys_outb(port, msb) != 0) {
    return 1;
  }

  return 0;
}

static int hook_id = 0;

int timer_subscribe_int(uint8_t *bit_no) {
  if (bit_no == NULL) {
    return 1;
  }

  *bit_no = (uint8_t) hook_id;
  if (sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &hook_id) != 0) {
    return 1;
  }

  return 0;
}

int timer_unsubscribe_int(void) {
  if (sys_irqrmpolicy(&hook_id) != 0) {
    return 1;
  }

  return 0;
}

int timer_get_conf(uint8_t timer, uint8_t *st) {
  if (timer > 2 || st == NULL) {
    return 1;
  }

  uint8_t rb_cmd = TIMER_RB_CMD | TIMER_RB_COUNT_ | TIMER_RB_SEL(timer);

  if (sys_outb(TIMER_CTRL, rb_cmd) != 0) {
    return 1;
  }

  return util_sys_inb(TIMER_0 + timer, st);
}

int timer_display_conf(uint8_t timer, uint8_t st, enum timer_status_field field) {
  union timer_status_field_val val;

  switch (field) {
    case tsf_all:
      val.byte = st;
      break;
    case tsf_initial:
      val.in_mode = (st & (TIMER_LSB_MSB)) >> 4;
      break;
    case tsf_mode:
      val.count_mode = (st & (0x0E)) >> 1;
      if (val.count_mode > 5) {
        val.count_mode -= 4;
      }
      break;
    case tsf_base:
      val.bcd = st & TIMER_BCD;
      break;
    default:
      return 1;
  }

  return timer_print_config(timer, field, val);
}

uint32_t no_interrupts = 0;

void timer_int_handler(void) {
  no_interrupts++;
}
