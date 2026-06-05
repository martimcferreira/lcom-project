/**
 * @file mouse.h
 * @brief Driver para interagir com o Rato (PS/2).
 *
 * Gere a subscrição de interrupções, receção de bytes, montagem
 * de pacotes (packets) e comandos escritos no rato.
 * 
 * @defgroup Mouse Mouse
 * @ingroup Devices
 * @brief Leitura e processamento de dados do Rato.
 * @{
 */

#ifndef _LCOM_MOUSE_H_
#define _LCOM_MOUSE_H_

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>

/** @brief Último byte lido pelo interrupt handler do rato. */
extern uint8_t mouse_byte;

/** @brief Flag que indica se houve um erro (parity, timeout) durante a leitura. */
extern bool mouse_error;

/**
 * @brief Subscreve as interrupções do Rato.
 * @param bit_no Apontador onde será guardado o bit usado no mask.
 * @return bit_no original em sucesso, -1 em erro.
 */
int (mouse_subscribe_int)(uint8_t *bit_no);

/**
 * @brief Cancela a subscrição das interrupções do Rato.
 * @return 0 em sucesso, 1 em erro.
 */
int (mouse_unsubscribe_int)();

/**
 * @brief Interrupt Handler do Rato. Lê o byte do OUT_BUF.
 */
void (mouse_ih)();

/**
 * @brief Escreve um comando no rato.
 * @param command O comando a escrever (ex: Enable Data Reporting).
 * @return 0 em sucesso, 1 em erro.
 */
int (mouse_write_command)(uint8_t command);

/**
 * @brief Transforma um array de 3 bytes na estrutura padronizada de pacotes do Rato.
 * @param bytes Array com os 3 bytes do rato.
 * @param pp Estrutura do tipo `packet` que será preenchida.
 */
void (mouse_parse_packet)(uint8_t *bytes, struct packet *pp);

#endif /* _LCOM_MOUSE_H_ */
/** @} */
