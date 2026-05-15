#ifndef _LCOM_MOUSE_H_
#define _LCOM_MOUSE_H_

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>

extern uint8_t mouse_byte;
extern bool mouse_error;

int (mouse_subscribe_int)(uint8_t *bit_no);
int (mouse_unsubscribe_int)();
void (mouse_ih)();

int (mouse_write_command)(uint8_t command);

void (mouse_parse_packet)(uint8_t *bytes, struct packet *pp);

#endif /* _LCOM_MOUSE_H_ */

