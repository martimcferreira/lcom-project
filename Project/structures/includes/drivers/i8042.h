#ifndef _LCOM_I8042_H_
#define _LCOM_I8042_H_

#include <lcom/lcf.h>

#define KBC_IRQ 1
#define MOUSE_IRQ 12

#define KBC_OUT_BUF 0x60
#define KBC_IN_BUF 0x60
#define KBC_STAT_REG 0x64
#define KBC_CMD_REG 0x64

#define KBC_OBF BIT(0)
#define KBC_IBF BIT(1)
#define KBC_AUX BIT(5)
#define KBC_TIMEOUT BIT(6)
#define KBC_PARITY BIT(7)

#define DELAY_US 20000
#define KBC_RETRIES 10

#define ESC_BREAKCODE 0x81
#define TWO_BYTE_CODE 0xE0

#define KBC_READ_CMD 0x20
#define KBC_WRITE_CMD 0x60
#define KBC_CHECK_KBC 0xAA
#define KBC_CHECK_KBD 0xAB
#define KBC_DIS_KBD 0xAD
#define KBC_ENA_KBD 0xAE

#define KBC_WRITE_MOUSE 0xD4

#define MOUSE_LB BIT(0)
#define MOUSE_RB BIT(1)
#define MOUSE_MB BIT(2)
#define MOUSE_SYNC BIT(3)
#define MOUSE_X_SIGN BIT(4)
#define MOUSE_Y_SIGN BIT(5)
#define MOUSE_X_OVF BIT(6)
#define MOUSE_Y_OVF BIT(7)

#define MOUSE_ACK 0xFA
#define MOUSE_NACK 0xFE
#define MOUSE_ERROR 0xFC
#define MOUSE_ENABLE_DATA_REPORTING 0xF4
#define MOUSE_DISABLE_DATA_REPORTING 0xF5

#endif /* _LCOM_I8042_H_ */
