#ifndef I8042_H
#define I8042_H

#include <lcom/lcf.h>

#define KBC_OUT_BUF 0x60
#define KBC_IN_BUF 0x60
#define KBC_ST_REG 0x64
#define KBC_CMD_REG 0x64

#define KBD_IRQ 1

#define KBC_OBF BIT(0)
#define KBC_IBF BIT(1)
#define KBC_AUX BIT(5)
#define KBC_TIMEOUT_ERR BIT(6)
#define KBC_PARITY_ERR BIT(7)

#define KBC_READ_CMD_BYTE 0x20
#define KBC_WRITE_CMD_BYTE 0x60
#define KBC_INT BIT(0)

#define ESC_BREAK_CODE 0x81
#define TWO_BYTE_SCANCODE_PREFIX 0xE0

#define KBC_DELAY_US 20000
#define KBC_MAX_TRIES 10
#define TIMER_FREQ_HZ 60

#endif
