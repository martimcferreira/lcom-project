#include <lcom/lcf.h>

#include <lcom/lab3.h>
#include <lcom/timer.h>

#include <stdbool.h>
#include <stdint.h>

#include "i8042.h"
#include "kbc.h"

static int(handle_scancode_byte)(uint8_t byte, uint8_t *scancode_bytes, uint8_t *size, bool *done) {
  if (scancode_bytes == NULL || size == NULL || done == NULL) return 1;

  *done = false;

  if (*size == 1 && scancode_bytes[0] == TWO_BYTE_SCANCODE_PREFIX) {
    scancode_bytes[1] = byte;
    *size = 2;
  }
  else if (byte == TWO_BYTE_SCANCODE_PREFIX) {
    scancode_bytes[0] = byte;
    *size = 1;
    return 0;
  }
  else {
    scancode_bytes[0] = byte;
    *size = 1;
  }

  bool make = (scancode_bytes[*size - 1] & BIT(7)) == 0;
  if (kbd_print_scancode(make, *size, scancode_bytes) != 0) return 1;

  if (*size == 1 && scancode_bytes[0] == ESC_BREAK_CODE) *done = true;

  *size = 0;
  return 0;
}

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
  uint8_t kbd_irq_set = 0;
  if (kbd_subscribe_int(&kbd_irq_set) != 0) return 1;

  kbc_reset_sys_inb_calls();

  int ipc_status, r;
  message msg;
  uint8_t scancode_bytes[2] = {0};
  uint8_t size = 0;
  bool done = false;

  while (!done) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      printf("driver_receive failed with: %d", r);
      continue;
    }

    if (!is_ipc_notify(ipc_status)) continue;
    if (_ENDPOINT_P(msg.m_source) != HARDWARE) continue;
    if ((msg.m_notify.interrupts & kbd_irq_set) == 0) continue;

    kbc_ih();
    if (!kbc_is_scancode_valid()) continue;

    if (handle_scancode_byte(kbc_get_scancode_byte(), scancode_bytes, &size, &done) != 0) {
      if (kbd_unsubscribe_int() != 0) return 1;
      return 1;
    }
  }

  if (kbd_unsubscribe_int() != 0) return 1;
  if (kbd_print_no_sysinb(kbc_get_no_sys_inb_calls()) != 0) return 1;

  return 0;
}

int(kbd_test_poll)() {
  kbc_reset_sys_inb_calls();

  uint8_t scancode_bytes[2] = {0};
  uint8_t size = 0;
  bool done = false;
  int ret = 0;

  while (!done) {
    uint8_t data;
    bool has_data;

    if (kbc_read_data_poll(&data, &has_data) != 0) {
      ret = 1;
      break;
    }

    if (!has_data) {
      tickdelay(micros_to_ticks(KBC_DELAY_US));
      continue;
    }

    if (handle_scancode_byte(data, scancode_bytes, &size, &done) != 0) {
      ret = 1;
      break;
    }
  }

  if (kbc_enable_keyboard_interrupts() != 0) ret = 1;
  if (kbd_print_no_sysinb(kbc_get_no_sys_inb_calls()) != 0) ret = 1;

  return ret;
}

int(kbd_test_timed_scan)(uint8_t n) {
  uint8_t kbd_irq_set = 0;
  if (kbd_subscribe_int(&kbd_irq_set) != 0) return 1;

  uint8_t timer_irq_set = 0;
  if (timer_subscribe_int(&timer_irq_set) != 0) {
    if (kbd_unsubscribe_int() != 0) return 1;
    return 1;
  }

  kbc_reset_sys_inb_calls();

  int ipc_status, r;
  message msg;
  uint8_t scancode_bytes[2] = {0};
  uint8_t size = 0;
  bool done = false;
  uint32_t idle_ticks = 0;
  const uint32_t timeout_ticks = n * TIMER_FREQ_HZ;

  while (!done && idle_ticks < timeout_ticks) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      printf("driver_receive failed with: %d", r);
      continue;
    }

    if (!is_ipc_notify(ipc_status)) continue;
    if (_ENDPOINT_P(msg.m_source) != HARDWARE) continue;

    if ((msg.m_notify.interrupts & timer_irq_set) != 0) {
      timer_int_handler();
      idle_ticks++;
    }

    if ((msg.m_notify.interrupts & kbd_irq_set) == 0) continue;

    kbc_ih();
    if (!kbc_is_scancode_valid()) continue;

    idle_ticks = 0;
    if (handle_scancode_byte(kbc_get_scancode_byte(), scancode_bytes, &size, &done) != 0) {
      if (timer_unsubscribe_int() != 0) return 1;
      if (kbd_unsubscribe_int() != 0) return 1;
      return 1;
    }
  }

  if (timer_unsubscribe_int() != 0) return 1;
  if (kbd_unsubscribe_int() != 0) return 1;
  if (kbd_print_no_sysinb(kbc_get_no_sys_inb_calls()) != 0) return 1;

  return 0;
}
