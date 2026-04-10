#ifndef KBC_H
#define KBC_H

#include <stdbool.h>
#include <stdint.h>

int keyboard_subscribe_int(uint8_t *bit_no);
int keyboard_unsubscribe_int(void);

void (kbc_ih)(void);

bool kbc_scancode_available(void);
uint8_t kbc_get_scancode_byte(void);

int kbc_poll_read_byte(uint8_t *byte);
int kbc_enable_keyboard_interrupts(void);

#endif
