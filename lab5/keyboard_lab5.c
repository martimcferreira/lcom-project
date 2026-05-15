#include "keyboard_lab5.h"

#define KBD_IRQ 1

#define KBC_OUT_BUF 0x60
#define KBC_STATUS_REG 0x64

#define KBC_OBF BIT(0)
#define KBC_AUX BIT(5)
#define KBC_TIMEOUT BIT(6)
#define KBC_PARITY BIT(7)

#define ESC_BREAK 0x81

static int kbd_hook_id = 1;
static uint8_t scancode = 0;

static int kbd_subscribe_int(uint8_t *bit_no) {
  *bit_no = kbd_hook_id;

  if (sys_irqsetpolicy(KBD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id) != OK) {
    return 1;
  }

  return 0;
}

static int kbd_unsubscribe_int(void) {
  if (sys_irqrmpolicy(&kbd_hook_id) != OK) {
    return 1;
  }

  return 0;
}

static void kbc_ih_lab5(void) {
  uint32_t status;
  uint32_t data;

  if (util_sys_inb(KBC_STATUS_REG, &status) != OK) {
    return;
  }

  if ((status & KBC_OBF) == 0) {
    return;
  }

  if (status & (KBC_PARITY | KBC_TIMEOUT | KBC_AUX)) {
    return;
  }

  if (util_sys_inb(KBC_OUT_BUF, &data) != OK) {
    return;
  }

  scancode = (uint8_t) data;
}

int wait_esc_break(void) {
  uint8_t bit_no;
  message msg;
  int ipc_status;

  scancode = 0;

  if (kbd_subscribe_int(&bit_no) != 0) {
    return 1;
  }

  uint32_t irq_set = BIT(bit_no);

  while (scancode != ESC_BREAK) {
    if (driver_receive(ANY, &msg, &ipc_status) != OK) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          if (msg.m_notify.interrupts & irq_set) {
            kbc_ih_lab5();
          }
          break;

        default:
          break;
      }
    }
  }

  if (kbd_unsubscribe_int() != 0) {
    return 1;
  }

  return 0;
}