#pragma once

#include <stdint.h>

/* -----------------------------------------------------------------------
 * UART (COM1) - Driver para comunicação série com o Windows via Python
 * Base Address: 0x3F8
 * Protocolo de pacotes: [0xAA (START)] [CMD] [ARG] [0xFF (END)]
 * ----------------------------------------------------------------------- */

/* --- Endereço base da COM1 --- */
#define UART_COM1_BASE      0x3F8

/* --- Offsets dos registos UART (relativos à base) --- */
#define UART_THR            0   /* Transmit Holding Register  (DLAB=0, escrita) */
#define UART_DLL            0   /* Divisor Latch LSB          (DLAB=1) */
#define UART_DLM            1   /* Divisor Latch MSB          (DLAB=1) */
#define UART_IER            1   /* Interrupt Enable Register  (DLAB=0) */
#define UART_LCR            3   /* Line Control Register */
#define UART_LSR            5   /* Line Status Register */

/* --- Bits do LCR --- */
#define UART_DLAB_BIT       BIT(7)  /* Divisor Latch Access Bit */
#define UART_8N1            0x03    /* 8 bits de dados, sem paridade, 1 stop bit */

/* --- Bits do LSR --- */
#define UART_THRE_BIT       BIT(5)  /* Transmit Holding Register Empty */

/* --- Divisor para 115200 bps (clock = 1.8432 MHz / (16 * 115200) = 1) --- */
#define UART_BAUD_115200_DLL  0x01
#define UART_BAUD_115200_DLM  0x00

/* --- Protocolo de pacotes legado --- */
#define UART_PKT_START      0xAA
#define UART_PKT_END        0xFF

/* --- Eventos simples acordados com o módulo Python de áudio --- */
#define UART_EVENT_GAME_START_SONG1 0x01  /* Iniciar Música 1 */
#define UART_EVENT_GAME_START_SONG2 0x02  /* Iniciar Música 2 */
#define UART_EVENT_GAME_END         0x03  /* Parar Música / Fim de Jogo */
#define UART_EVENT_HIT              0x0A  /* Nota acertada */
#define UART_EVENT_MISS             0x0E  /* Erro / nota falhada */



/* -----------------------------------------------------------------------
 * Interface pública
 * ----------------------------------------------------------------------- */

/**
 * @brief Inicializa a COM1 a 115200 bps, 8N1, sem interrupções.
 * @return 0 em sucesso, 1 em erro.
 */
int uart_init(void);

/**
 * @brief Envia um byte via polling (aguarda THRE antes de escrever no THR).
 *
 * Usado pelo Membro 3 para enviar eventos simples ao Python:
 *  - UART_EVENT_GAME_START (0x01) no início do jogo;
 *  - UART_EVENT_HIT        (0x0A) quando a nota é acertada;
 *  - UART_EVENT_MISS       (0x0E) quando há erro/miss.
 *
 * @param byte Byte a enviar.
 * @return 0 em sucesso, 1 em erro.
 */
int uart_send_byte(uint8_t byte);

/**
 * @brief Envia um pacote de 4 bytes: [0xAA] [cmd] [arg] [0xFF].
 * @param cmd  Byte de comando (ex: 0x10 = iniciar música, 0x20 = acerto).
 * @param arg  Argumento do comando (ex: ID da música, ID da pista).
 * @return 0 em sucesso, 1 em erro.
 */
int uart_send_packet(uint8_t cmd, uint8_t arg);
