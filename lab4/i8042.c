#ifndef _LCOM_I8042_H_
#define _LCOM_I8042_H_

#define KBC_IRQ           1    /* IRQ do Teclado */

/* Portos do KBC */
#define KBC_STAT_REG      0x64 /* Status Register (Read) */
#define KBC_OUT_BUF       0x60 /* Output Buffer (Read Scancodes) */

#define KBC_OBF           BIT(0) /* Output Buffer Full */
#define KBC_PARITY        BIT(7) /* Erro de Paridade */
#define KBC_TIMEOUT       BIT(6) /* Erro de Timeout */

#define ESC_BREAKCODE     0x81
#define TWO_BYTE_CODE     0xE0

#define KBC_IBF BIT(1)  /* Bit 1 do Status Register */
#define DELAY_US 20000

#define KBC_READ_CMD      0x20    /* Lê o Command Byte (a resposta vem no 0x60) */
#define KBC_WRITE_CMD     0x60    /* Escreve o Command Byte (o argumento vai no 0x60) */

#define KBC_CHECK_KBC     0xAA    /* Auto-teste do KBC */
#define KBC_CHECK_KBD     0xAB    /* Teste da interface do Teclado */
#define KBC_DIS_KBD       0xAD    /* Desativa a interface do Teclado */
#define KBC_ENA_KBD       0xAE    /* Ativa a interface do Teclado */

#define MOUSE_ACK 0xFA
#define MOUSE_NACK 0xFE

#endif
