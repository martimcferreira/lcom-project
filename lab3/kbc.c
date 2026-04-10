#include <lcom/lcf.h>

#include <stdbool.h>
#include <stdint.h>

#include "i8042.h"
#include "kbc.h"
#include "utils.h"

static int kbd_hook_id = KBD_IRQ;
static uint8_t scancode_byte = 0;
static bool scancode_ready = false;

static int kbc_write(uint8_t port, uint8_t value) {
  uint8_t status = 0;

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (util_sys_inb(KBC_ST_REG, &status) != 0) {
      return 1;
    }

    if ((status & KBC_IBF) == 0) {
      if (sys_outb(port, value) != 0) {
        return 1;
      }
      return 0;
    }

    tickdelay(micros_to_ticks(KBC_DELAY_US));
  }

  return 1;
}

static int kbc_read_response(uint8_t *data) {
  uint8_t status = 0;
  uint8_t value = 0;

  if (data == NULL) {
    return 1;
  }

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (util_sys_inb(KBC_ST_REG, &status) != 0) {
      return 1;
    }

    if (status & KBC_OBF) {
      if (util_sys_inb(KBC_OUT_BUF, &value) != 0) {
        return 1;
      }

      if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) {
        return 1;
      }

      *data = value;
      return 0;
    }

    tickdelay(micros_to_ticks(KBC_DELAY_US));
  }

  return 1;
}

static int kbc_read_cmd_byte(uint8_t *cmd_byte) {
  if (kbc_write(KBC_CMD_REG, KBC_READ_CMD) != 0) {
    return 1;
  }

  return kbc_read_response(cmd_byte);
}

static int kbc_write_cmd_byte(uint8_t cmd_byte) {
  if (kbc_write(KBC_CMD_REG, KBC_WRITE_CMD) != 0) {
    return 1;
  }

  return kbc_write(KBC_IN_BUF, cmd_byte);
}

int keyboard_subscribe_int(uint8_t *bit_no) {
  if (bit_no == NULL) {
    return 1;
  }

  kbd_hook_id = KBD_IRQ;
  *bit_no = (uint8_t) kbd_hook_id;

  return sys_irqsetpolicy(KBD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id);
}

int keyboard_unsubscribe_int(void) {
  return sys_irqrmpolicy(&kbd_hook_id);
}

void (kbc_ih)(void) {
  uint8_t status = 0;
  uint8_t value = 0;

  scancode_ready = false;

  if (util_sys_inb(KBC_ST_REG, &status) != 0) {
    return;
  }

  if ((status & KBC_OBF) == 0) {
    return;
  }

  if (util_sys_inb(KBC_OUT_BUF, &value) != 0) {
    return;
  }

  if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) {
    return;
  }

  scancode_byte = value;
  scancode_ready = true;
}

bool kbc_scancode_available(void) {
  return scancode_ready;
}

uint8_t kbc_get_scancode_byte(void) {
  scancode_ready = false;
  return scancode_byte;
}

int kbc_poll_read_byte(uint8_t *byte) {
  uint8_t status = 0;
  uint8_t value = 0;

  if (byte == NULL) {
    return 1;
  }

  while (true) {
    if (util_sys_inb(KBC_ST_REG, &status) != 0) {
      return 1;
    }

    if (status & KBC_OBF) {
      if (util_sys_inb(KBC_OUT_BUF, &value) != 0) {
        return 1;
      }

      if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) {
        return 1;
      }

      *byte = value;
      return 0;
    }

    tickdelay(micros_to_ticks(KBC_DELAY_US));
  }
}

int kbc_enable_keyboard_interrupts(void) {
  uint8_t cmd_byte = 0;

  if (kbc_write(KBC_CMD_REG, KBC_DISABLE_KBD_IF) != 0) {
    return 1;
  }

  if (kbc_read_cmd_byte(&cmd_byte) != 0) {
    (void) kbc_write(KBC_CMD_REG, KBC_ENABLE_KBD_IF);
    return 1;
  }

  cmd_byte |= KBC_INT_EN;

  if (kbc_write_cmd_byte(cmd_byte) != 0) {
    (void) kbc_write(KBC_CMD_REG, KBC_ENABLE_KBD_IF);
    return 1;
  }

  return kbc_write(KBC_CMD_REG, KBC_ENABLE_KBD_IF);
}
