#include <lcom/lcf.h>

#include <stdint.h>

#include "i8042.h"
#include "timer_local.h"

static int timer_hook_id = TIMER0_IRQ;
static uint32_t timer_counter = 0;

int timer_subscribe_int(uint8_t *bit_no) {
  if (bit_no == NULL) {
    return 1;
  }

  timer_hook_id = TIMER0_IRQ;
  *bit_no = (uint8_t) timer_hook_id;

  return sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &timer_hook_id);
}

int timer_unsubscribe_int(void) {
  return sys_irqrmpolicy(&timer_hook_id);
}

void timer_int_handler(void) {
  timer_counter++;
}

void timer_reset_counter(void) {
  timer_counter = 0;
}

uint32_t timer_get_counter(void) {
  return timer_counter;
}
