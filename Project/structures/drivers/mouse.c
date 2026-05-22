#include <lcom/lcf.h>

#include "i8042.h"
#include "kbc.h"
#include "mouse.h"
#include "utils.h"

static int mouse_hook_id = 2;

uint8_t mouse_byte = 0;
bool mouse_error = false;

int(mouse_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) return 1;

  *bit_no = mouse_hook_id;
  return sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &mouse_hook_id);
}

int(mouse_unsubscribe_int)() {
  return sys_irqrmpolicy(&mouse_hook_id);
}

void(mouse_ih)() {
  uint8_t status;
  mouse_error = false;

  if (util_sys_inb(KBC_STAT_REG, &status) != 0) {
    mouse_error = true;
    return;
  }

  if ((status & KBC_OBF) == 0 || (status & KBC_AUX) == 0) {
    mouse_error = true;
    return;
  }

  if (util_sys_inb(KBC_OUT_BUF, &mouse_byte) != 0) {
    mouse_error = true;
    return;
  }

  if (status & (KBC_PARITY | KBC_TIMEOUT)) {
    mouse_error = true;
  }
}

int(mouse_write_command)(uint8_t command) {
  uint8_t ack_byte;

  for (int i = 0; i < KBC_RETRIES; i++) {
    if (kbc_write_command(KBC_WRITE_MOUSE) != 0) continue;
    if (kbc_write_argument(command) != 0) continue;
    if (kbc_read_response(&ack_byte) != 0) continue;

    if (ack_byte == MOUSE_ACK) return 0;
    if (ack_byte == MOUSE_ERROR) return 1;
  }

  return 1;
}

void(mouse_parse_packet)(uint8_t *bytes, struct packet *pp) {
  if (bytes == NULL || pp == NULL) return;

  pp->bytes[0] = bytes[0];
  pp->bytes[1] = bytes[1];
  pp->bytes[2] = bytes[2];

  pp->lb = bytes[0] & MOUSE_LB;
  pp->rb = bytes[0] & MOUSE_RB;
  pp->mb = bytes[0] & MOUSE_MB;

  pp->delta_x = bytes[1];
  pp->delta_y = bytes[2];

  if (bytes[0] & MOUSE_X_SIGN) pp->delta_x |= 0xFF00;
  if (bytes[0] & MOUSE_Y_SIGN) pp->delta_y |= 0xFF00;

  pp->x_ov = bytes[0] & MOUSE_X_OVF;
  pp->y_ov = bytes[0] & MOUSE_Y_OVF;
}
