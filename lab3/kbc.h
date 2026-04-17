#ifndef KBC_H
#define KBC_H

#include <stdbool.h>
#include <stdint.h>

int (kbd_subscribe_int)(uint8_t *irq_set);
int (kbd_unsubscribe_int)(void);

int (kbc_read_data_poll)(uint8_t *data, bool *has_data);
int (kbc_enable_keyboard_interrupts)(void);

void (kbc_reset_sys_inb_calls)(void);
uint32_t (kbc_get_no_sys_inb_calls)(void);

uint8_t (kbc_get_scancode_byte)(void);
bool (kbc_is_scancode_valid)(void);

#endif
