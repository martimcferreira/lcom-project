#ifndef I8042_H
#define I8042_H

#include <lcom/lcf.h>

#define KBD_IRQ              1
#define TIMER0_IRQ           0

#define KBC_OUT_BUF          0x60
#define KBC_IN_BUF           0x60
#define KBC_ST_REG           0x64
#define KBC_CMD_REG          0x64

#define KBC_OBF              BIT(0)
#define KBC_IBF              BIT(1)
#define KBC_AUX              BIT(5)
#define KBC_TIMEOUT_ERR      BIT(6)
#define KBC_PARITY_ERR       BIT(7)

#define KBC_READ_CMD         0x20
#define KBC_WRITE_CMD        0x60
#define KBC_DISABLE_KBD_IF   0xAD
#define KBC_ENABLE_KBD_IF    0xAE

#define KBC_INT_EN           BIT(0)

#define TWO_BYTE_SCANCODE    0xE0
#define ESC_BREAK_CODE       0x81
#define BREAK_CODE_BIT       BIT(7)

#define KBC_RETRIES          10
#define KBC_DELAY_US         20000

#endif
