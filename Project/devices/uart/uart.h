/**
 * @file uart.h
 * @brief Driver para a Porta Série (UART - COM1).
 *
 * Gere a comunicação com periféricos ou programas externos (ex: Python para áudio).
 * 
 * @defgroup UART Serial Port (UART)
 * @ingroup Devices
 * @brief Envio e receção de dados via Serial Port.
 * @{
 */

#pragma once

#include <stdint.h>

/* --- Endereço base da COM1 --- */
#define UART_COM1_BASE      0x3F8

/* --- Offsets dos registos UART (relativos à base) --- */
#define UART_THR            0   /**< @brief Transmit Holding Register (DLAB=0, escrita) */
#define UART_DLL            0   /**< @brief Divisor Latch LSB (DLAB=1) */
#define UART_DLM            1   /**< @brief Divisor Latch MSB (DLAB=1) */
#define UART_IER            1   /**< @brief Interrupt Enable Register (DLAB=0) */
#define UART_LCR            3   /**< @brief Line Control Register */
#define UART_LSR            5   /**< @brief Line Status Register */

/* --- Bits do LCR --- */
#define UART_DLAB_BIT       BIT(7)  /**< @brief Divisor Latch Access Bit */
#define UART_8N1            0x03    /**< @brief 8 bits de dados, sem paridade, 1 stop bit */

/* --- Bits do LSR --- */
#define UART_THRE_BIT       BIT(5)  /**< @brief Transmit Holding Register Empty */

/* --- Divisor para 115200 bps --- */
#define UART_BAUD_115200_DLL  0x01
#define UART_BAUD_115200_DLM  0x00

/* --- Protocolo de pacotes legado --- */
#define UART_PKT_START      0xAA
#define UART_PKT_END        0xFF

/* --- Eventos de Áudio via UART --- */
#define UART_EVENT_GAME_START_SONG1 0x01  /**< @brief Iniciar Música 1 */
#define UART_EVENT_GAME_START_SONG2 0x02  /**< @brief Iniciar Música 2 */
#define UART_EVENT_GAME_END         0x03  /**< @brief Fim de Jogo/Música */
#define UART_EVENT_MUSIC_PAUSE      0x04  /**< @brief Pausar Música */
#define UART_EVENT_MUSIC_RESUME     0x05  /**< @brief Retomar Música */
#define UART_EVENT_GAME_START_SONG3 0x06  /**< @brief Iniciar Música 3 */
#define UART_EVENT_GAME_START_SONG4 0x07  /**< @brief Iniciar Música 4 */
#define UART_EVENT_HIT              0x0A  /**< @brief Nota Acertada */
#define UART_EVENT_MISS             0x0E  /**< @brief Nota Falhada */
#define UART_EVENT_VOLUME_UP        0x10  /**< @brief Aumentar Volume */
#define UART_EVENT_VOLUME_DOWN      0x11  /**< @brief Diminuir Volume */

/* --- Resultado do envio --- */
#define UART_SEND_OK                0
#define UART_SEND_ERROR             1
#define UART_SEND_BUSY              2

/**
 * @brief Inicializa a COM1 a 115200 bps, 8N1.
 * @return 0 em sucesso, 1 em erro.
 */
int uart_init(void);

/**
 * @brief Envia um byte de forma bloqueante (espera que THRE esteja livre).
 * @param byte Byte a enviar.
 * @return 0 em sucesso, 1 em erro.
 */
int uart_send_byte(uint8_t byte);

/**
 * @brief Tenta enviar um byte sem bloquear o ciclo de jogo.
 * @param byte Byte a enviar.
 * @return UART_SEND_OK, UART_SEND_ERROR ou UART_SEND_BUSY.
 */
int uart_try_send_byte(uint8_t byte);

/**
 * @brief Envia um pacote de comando completo.
 * @param cmd Byte de Comando.
 * @param arg Byte de Argumento.
 * @return 0 em sucesso, 1 em erro.
 */
int uart_send_packet(uint8_t cmd, uint8_t arg);

/** @} */
