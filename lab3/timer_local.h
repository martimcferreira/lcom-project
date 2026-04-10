#ifndef TIMER_LOCAL_H
#define TIMER_LOCAL_H

#include <stdint.h>

int timer_subscribe_int(uint8_t *bit_no);
int timer_unsubscribe_int(void);
void timer_int_handler(void);
void timer_reset_counter(void);
uint32_t timer_get_counter(void);

#endif
