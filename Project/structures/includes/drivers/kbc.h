#ifndef _LCOM_KBC_H_
#define _LCOM_KBC_H_

#include <stdbool.h>
#include <stdint.h>

extern uint8_t scancode_byte;
extern bool ih_error;

int(kbd_subscribe_int)(uint8_t *bit_no);
int(kbd_unsubscribe_int)();
void(kbc_ih)();

int(kbc_write_command)(uint8_t cmd);
int(kbc_write_argument)(uint8_t arg);
int(kbc_read_response)(uint8_t *response);
int(kbd_enable_interrupts)();

#endif /* _LCOM_KBC_H_ */
