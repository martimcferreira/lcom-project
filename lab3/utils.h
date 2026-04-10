#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

int (util_sys_inb)(int port, uint8_t *value);
void reset_sys_inb_count(void);
uint32_t get_sys_inb_count(void);

#endif
