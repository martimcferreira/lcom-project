#include <lcom/lab3.h>
#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdbool.h>
#include <stdint.h>

#include "i8042.h"
#include "kbc.h"
#include "utils.h"

static int print_scancode_byte(uint8_t byte, bool *awaiting_second_byte, bool *done) {
  static uint8_t scancode[2] = {0, 0};

  if (awaiting_second_byte == NULL || done == NULL) {
    return 1;
  }

  if (*awaiting_second_byte) {
    scancode[1] = byte;
    *awaiting_second_byte = false;

    if (kbd_print_scancode((byte & BREAK_CODE_BIT) == 0, 2, scancode) != 0) {
      return 1;
    }

    return 0;
  }

  if (byte == TWO_BYTE_SCANCODE) {
    scancode[0] = byte;
    *awaiting_second_byte = true;
    return 0;
  }

  scancode[0] = byte;
  if (kbd_print_scancode((byte & BREAK_CODE_BIT) == 0, 1, scancode) != 0) {
    return 1;
  }

  if (byte == ESC_BREAK_CODE) {
    *done = true;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  lcf_set_language("EN-US");
  lcf_trace_calls("/home/lcom/labs/lab3/trace.txt");
  lcf_log_output("/home/lcom/labs/lab3/output.txt");

  if (lcf_start(argc, argv) != 0) {
    return 1;
  }

  lcf_cleanup();
  return 0;
}

int (kbd_test_scan)() {
  int ipc_status = 0;
  int r = 0;
  message msg;
  uint8_t kbd_bit_no = 0;
  bool done = false;
  bool awaiting_second_byte = false;
  int status = 0;

  reset_sys_inb_count();

  if (keyboard_subscribe_int(&kbd_bit_no) != 0) {
    return 1;
  }

  while (!done) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      printf("driver_receive failed with: %d\n", r);
      continue;
    }

    if (!is_ipc_notify(ipc_status)) {
      continue;
    }

    if (_ENDPOINT_P(msg.m_source) != HARDWARE) {
      continue;
    }

    if (msg.m_notify.interrupts & BIT(kbd_bit_no)) {
      kbc_ih();

      if (kbc_scancode_available()) {
        uint8_t byte = kbc_get_scancode_byte();
        if (print_scancode_byte(byte, &awaiting_second_byte, &done) != 0) {
          status = 1;
          break;
        }
      }
    }
  }

  if (keyboard_unsubscribe_int() != 0) {
    status = 1;
  }

  if (kbd_print_no_sysinb(get_sys_inb_count()) != 0) {
    status = 1;
  }

  return status;
}

int (kbd_test_poll)() {
  bool done = false;
  bool awaiting_second_byte = false;
  int status = 0;

  reset_sys_inb_count();

  while (!done) {
    uint8_t byte = 0;

    if (kbc_poll_read_byte(&byte) != 0) {
      continue;
    }

    if (print_scancode_byte(byte, &awaiting_second_byte, &done) != 0) {
      status = 1;
      break;
    }
  }

  if (kbc_enable_keyboard_interrupts() != 0) {
    status = 1;
  }

  if (kbd_print_no_sysinb(get_sys_inb_count()) != 0) {
    status = 1;
  }

  return status;
}

int (kbd_test_timed_scan)(uint8_t n) {
  int ipc_status = 0;
  int r = 0;
  message msg;
  uint8_t kbd_bit_no = 0;
  uint8_t timer_bit_no = 0;
  bool done = false;
  bool awaiting_second_byte = false;
  int status = 0;
  const uint32_t timeout_ticks = (uint32_t) n * sys_hz();
  uint32_t idle_ticks = 0;

  if (keyboard_subscribe_int(&kbd_bit_no) != 0) {
    return 1;
  }

  if (timer_subscribe_int(&timer_bit_no) != 0) {
    (void) keyboard_unsubscribe_int();
    return 1;
  }
  
  while (!done && idle_ticks < timeout_ticks) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      printf("driver_receive failed with: %d\n", r);
      continue;
    }

    if (!is_ipc_notify(ipc_status)) {
      continue;
    }

    if (_ENDPOINT_P(msg.m_source) != HARDWARE) {
      continue;
    }

    if (msg.m_notify.interrupts & BIT(kbd_bit_no)) {
      kbc_ih();

      if (kbc_scancode_available()) {
        uint8_t byte = kbc_get_scancode_byte();
        idle_ticks = 0;

        if (print_scancode_byte(byte, &awaiting_second_byte, &done) != 0) {
          status = 1;
          break;
        }
      }
    }

    if (msg.m_notify.interrupts & BIT(timer_bit_no)) {
      timer_int_handler();
      idle_ticks++;
    }
  }

  if (timer_unsubscribe_int() != 0) {
    status = 1;
  }

  if (keyboard_unsubscribe_int() != 0) {
    status = 1;
  }

  return status;
}
