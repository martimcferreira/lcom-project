/**
 * @file i8042.h
 * @brief Constantes e macros para programação do KBC (Keyboard Controller).
 *
 * @addtogroup KBC
 * @{
 */

#ifndef _LCOM_I8042_H_
#define _LCOM_I8042_H_

#include <lcom/lcf.h>

#define KBC_IRQ 1          /**< @brief Linha de IRQ do Teclado */
#define MOUSE_IRQ 12       /**< @brief Linha de IRQ do Rato */

#define KBC_OUT_BUF 0x60   /**< @brief Registo de Output Buffer (Leitura) */
#define KBC_IN_BUF 0x60    /**< @brief Registo de Input Buffer (Escrita) */
#define KBC_STAT_REG 0x64  /**< @brief Registo de Estado (Status) */
#define KBC_CMD_REG 0x64   /**< @brief Registo de Comandos */

/* --- Status Register Bits --- */
#define KBC_OBF BIT(0)     /**< @brief Output Buffer Full */
#define KBC_IBF BIT(1)     /**< @brief Input Buffer Full */
#define KBC_AUX BIT(5)     /**< @brief Auxiliar (dados vêm do Rato) */
#define KBC_TIMEOUT BIT(6) /**< @brief Erro de Timeout */
#define KBC_PARITY BIT(7)  /**< @brief Erro de Paridade */

#define DELAY_US 20000     /**< @brief Atraso para operações do KBC */
#define KBC_RETRIES 10     /**< @brief Número máximo de tentativas */

#define ESC_BREAKCODE 0x81 /**< @brief Breakcode da tecla ESC */
#define TWO_BYTE_CODE 0xE0 /**< @brief Primeiro byte para códigos de 2 bytes */

/* --- Comandos KBC --- */
#define KBC_READ_CMD 0x20
#define KBC_WRITE_CMD 0x60
#define KBC_CHECK_KBC 0xAA
#define KBC_CHECK_KBD 0xAB
#define KBC_DIS_KBD 0xAD
#define KBC_ENA_KBD 0xAE

#define KBC_WRITE_MOUSE 0xD4

/* --- Mouse Packets --- */
#define MOUSE_LB BIT(0)       /**< @brief Botão Esquerdo premido */
#define MOUSE_RB BIT(1)       /**< @brief Botão Direito premido */
#define MOUSE_MB BIT(2)       /**< @brief Botão Central premido */
#define MOUSE_SYNC BIT(3)     /**< @brief Bit de sincronização (sempre 1 no primeiro byte) */
#define MOUSE_X_SIGN BIT(4)   /**< @brief Sinal do delta X */
#define MOUSE_Y_SIGN BIT(5)   /**< @brief Sinal do delta Y */
#define MOUSE_X_OVF BIT(6)    /**< @brief Overflow em X */
#define MOUSE_Y_OVF BIT(7)    /**< @brief Overflow em Y */

/* --- Comandos Rato --- */
#define MOUSE_ACK 0xFA        /**< @brief Acknowledged (Tudo ok) */
#define MOUSE_NACK 0xFE       /**< @brief Não Acknowledged (Tentar novamente) */
#define MOUSE_ERROR 0xFC      /**< @brief Erro na comunicação com o Rato */
#define MOUSE_ENABLE_DATA_REPORTING 0xF4
#define MOUSE_DISABLE_DATA_REPORTING 0xF5

#endif /* _LCOM_I8042_H_ */
/** @} */
