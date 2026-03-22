#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdint.h>

#include "i8254.h"

static int timer_hook_id = 0;
static uint32_t timer_interrupts = 0;

int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  if (timer > 2 || freq == 0 || freq > TIMER_FREQ) {
    return 1;
  }

  uint8_t st = 0;
  if (timer_get_conf(timer, &st) != OK) {
    return 1;
  }

  uint8_t cw = (st & 0x0F) | TIMER_LSB_MSB;
  switch (timer) {
    case 0:
      cw |= TIMER_SEL0;
      break;
    case 1:
      cw |= TIMER_SEL1;
      break;
    case 2:
      cw |= TIMER_SEL2;
      break;
    default:
      return 1;
  }

  if (sys_outb(TIMER_CTRL, cw) != OK) {
    return 1;
  }

  uint16_t div = (uint16_t) (TIMER_FREQ / freq);
  uint8_t lsb = 0;
  uint8_t msb = 0;
  if (util_get_LSB(div, &lsb) != OK || util_get_MSB(div, &msb) != OK) {
    return 1;
  }

  uint8_t port = (uint8_t) (TIMER_0 + timer);
  if (sys_outb(port, lsb) != OK || sys_outb(port, msb) != OK) {
    return 1;
  }

  return 0;
}

int (timer_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) {
    return 1;
  }

  *bit_no = (uint8_t) timer_hook_id;
  if (sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &timer_hook_id) != OK) {
    return 1;
  }

  return 0;
}

int (timer_unsubscribe_int)() {
  if (sys_irqrmpolicy(&timer_hook_id) != OK) {
    return 1;
  }

  return 0;
}

void (timer_int_handler)() {
  timer_interrupts++;
}

int (timer_get_conf)(uint8_t timer, uint8_t *st) {
  if (timer > 2 || st == NULL) {
    return 1;
  }

  uint8_t rb = TIMER_RB_CMD | TIMER_RB_COUNT_ | TIMER_RB_SEL(timer);
  if (sys_outb(TIMER_CTRL, rb) != OK) {
    return 1;
  }

  if (util_sys_inb(TIMER_0 + timer, st) != OK) {
    return 1;
  }

  return 0;
}

int (timer_display_conf)(uint8_t timer, uint8_t st,
                        enum timer_status_field field) {
  union timer_status_field_val value;

  switch (field) {
    case tsf_all:
      value.byte = st;
      break;
    case tsf_initial:
      value.in_mode = (uint8_t) ((st & (TIMER_LSB | TIMER_MSB)) >> 4);
      break;
    case tsf_mode:
      value.count_mode = (uint8_t) ((st & (BIT(3) | BIT(2) | BIT(1))) >> 1);
      if (value.count_mode > 5) {
        value.count_mode -= 4;
      }
      break;
    case tsf_base:
      value.bcd = (st & TIMER_BCD) != 0;
      break;
    default:
      return 1;
  }

  if (timer_print_config(timer, field, value) != OK) {
    return 1;
  }

  return 0;
}
