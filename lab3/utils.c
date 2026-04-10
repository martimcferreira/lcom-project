#include <lcom/lcf.h>

#include "utils.h"

static uint32_t sys_inb_count = 0;

int (util_sys_inb)(int port, uint8_t *value) {
  uint32_t raw_value = 0;

  if (value == NULL) {
    return 1;
  }

  if (sys_inb(port, &raw_value) != 0) {
    return 1;
  }

#ifdef LAB3
  sys_inb_count++;
#endif

  *value = (uint8_t) raw_value;
  return 0;
}

void reset_sys_inb_count(void) {
  sys_inb_count = 0;
}

uint32_t get_sys_inb_count(void) {
  return sys_inb_count;
}
