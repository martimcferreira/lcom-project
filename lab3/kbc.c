#include <lcom/lcf.h>

#include <lcom/lab3.h>

#include <stdbool.h>
#include <stdint.h>

#include "i8042.h"
#include "kbc.h"

static int kbd_hook_id = KBD_IRQ;
static uint8_t scancode_byte = 0;
static bool scancode_valid = false;
static uint32_t no_sys_inb_calls = 0;

static int(kbc_read_status)(uint8_t *status) {
  if (status == NULL) return 1;

#ifdef LAB3
  no_sys_inb_calls++;
#endif
  return util_sys_inb(KBC_ST_REG, status);
}

static int(kbc_read_output)(uint8_t *data) {
  if (data == NULL) return 1;

#ifdef LAB3
  no_sys_inb_calls++;
#endif
  return util_sys_inb(KBC_OUT_BUF, data);
}

static int(kbc_wait_input_empty)(void) {
  uint8_t status;

  for (int i = 0; i < KBC_MAX_TRIES; i++) {
    if (kbc_read_status(&status) != 0) return 1;
    if ((status & KBC_IBF) == 0) return 0;
    tickdelay(micros_to_ticks(KBC_DELAY_US));
  }

  return 1;
}

static int(kbc_read_cmd_byte)(uint8_t *cmd_byte) {
  if (cmd_byte == NULL) return 1;
  if (kbc_wait_input_empty() != 0) return 1;
  if (sys_outb(KBC_CMD_REG, KBC_READ_CMD_BYTE) != 0) return 1;

  uint8_t status;

  for (int i = 0; i < KBC_MAX_TRIES; i++) {
    if (kbc_read_status(&status) != 0) return 1;

    if ((status & KBC_OBF) != 0) {
      if (kbc_read_output(cmd_byte) != 0) return 1;
      if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) != 0) return 1;
      return 0;
    }

    tickdelay(micros_to_ticks(KBC_DELAY_US));
  }

  return 1;
}

static int(kbc_write_cmd_byte)(uint8_t cmd_byte) {
  if (kbc_wait_input_empty() != 0) return 1;
  if (sys_outb(KBC_CMD_REG, KBC_WRITE_CMD_BYTE) != 0) return 1;
  if (kbc_wait_input_empty() != 0) return 1;
  if (sys_outb(KBC_IN_BUF, cmd_byte) != 0) return 1;
  return 0;
}

int(kbd_subscribe_int)(uint8_t *irq_set) {
  if (irq_set == NULL) return 1;

  *irq_set = BIT(kbd_hook_id);
  if (sys_irqsetpolicy(KBD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id) != 0) return 1;

  return 0;
}

int(kbd_unsubscribe_int)(void) {
  if (sys_irqrmpolicy(&kbd_hook_id) != 0) return 1;
  return 0;
}

int(kbc_read_data_poll)(uint8_t *data, bool *has_data) {
  if (data == NULL || has_data == NULL) return 1;

  uint8_t status;
  *has_data = false;

  if (kbc_read_status(&status) != 0) return 1;
  if ((status & KBC_OBF) == 0) return 0;

  if (kbc_read_output(data) != 0) return 1;
  if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) return 0;

  *has_data = true;
  return 0;
}

int(kbc_enable_keyboard_interrupts)(void) {
  uint8_t cmd_byte;
  if (kbc_read_cmd_byte(&cmd_byte) != 0) return 1;

  cmd_byte |= KBC_INT;
  if (kbc_write_cmd_byte(cmd_byte) != 0) return 1;

  return 0;
}

void(kbc_reset_sys_inb_calls)(void) {
  no_sys_inb_calls = 0;
}

uint32_t(kbc_get_no_sys_inb_calls)(void) {
  return no_sys_inb_calls;
}

uint8_t(kbc_get_scancode_byte)(void) {
  return scancode_byte;
}

bool(kbc_is_scancode_valid)(void) {
  return scancode_valid;
}

void(kbc_ih)(void) {
  scancode_valid = false;

  uint8_t status;
  if (kbc_read_status(&status) != 0) return;
  if ((status & KBC_OBF) == 0) return;

  uint8_t data;
  if (kbc_read_output(&data) != 0) return;
  if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) return;

  scancode_byte = data;
  scancode_valid = true;
}
