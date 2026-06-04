#include <lcom/lcf.h>

#include "i8042.h"
#include "kbc.h"
#include "utils.h"

uint8_t scancode_byte = 0;
bool ih_error = false;

static int hook_id_kbc = 1;

int(kbd_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) return 1;

  *bit_no = hook_id_kbc;
  return sys_irqsetpolicy(KBC_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &hook_id_kbc);
}

int(kbd_unsubscribe_int)() {
  return sys_irqrmpolicy(&hook_id_kbc);
}

void(kbc_ih)() {
  uint8_t status;
  ih_error = false;

  if (util_sys_inb(KBC_STAT_REG, &status) != 0) {
    ih_error = true;
    return;
  }

  if ((status & KBC_OBF) == 0) {
    ih_error = true;
    return;
  }

  if (util_sys_inb(KBC_OUT_BUF, &scancode_byte) != 0) {
    ih_error = true;
    return;
  }

  if (status & (KBC_PARITY | KBC_TIMEOUT)) {
    scancode_byte = 0;
    ih_error = true;
  }
}

int(kbc_write_command)(uint8_t cmd) {
  uint8_t status;

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

    if ((status & KBC_IBF) == 0) {
      return sys_outb(KBC_CMD_REG, cmd);
    }

    tickdelay(micros_to_ticks(DELAY_US));
  }

  return 1;
}

int(kbc_write_argument)(uint8_t arg) {
  uint8_t status;

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

    if ((status & KBC_IBF) == 0) {
      return sys_outb(KBC_IN_BUF, arg);
    }

    tickdelay(micros_to_ticks(DELAY_US));
  }

  return 1;
}

int(kbc_read_response)(uint8_t *response) {
  uint8_t status;

  if (response == NULL) return 1;

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

    if (status & KBC_OBF) {
      if (util_sys_inb(KBC_OUT_BUF, response) != 0) return 1;
      return (status & (KBC_PARITY | KBC_TIMEOUT)) ? 1 : 0;
    }

    tickdelay(micros_to_ticks(DELAY_US));
  }

  return 1;
}

int(kbd_enable_interrupts)() {
  uint8_t command_byte = minix_get_dflt_kbc_cmd_byte();

  if (kbc_write_command(KBC_WRITE_CMD) != 0) return 1;
  if (kbc_write_argument(command_byte) != 0) return 1;

  return 0;
}
