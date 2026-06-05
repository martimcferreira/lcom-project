/**
 * @file kbc.h
 * @brief Driver para o Keyboard Controller (KBC).
 *
 * Contém a lógica de leitura do teclado via polling ou interrupções,
 * permitindo controlar as teclas primárias do jogo.
 * 
 * @defgroup KBC Keyboard Controller
 * @ingroup Devices
 * @brief Controlador do teclado (PS/2).
 * @{
 */

#ifndef _LCOM_KBC_H_
#define _LCOM_KBC_H_

#include <stdbool.h>
#include <stdint.h>

/** @brief Último scancode lido pelo interrupt handler do teclado. */
extern uint8_t scancode_byte;

/** @brief Flag que indica se houve um erro de paridade ou timeout durante a leitura. */
extern bool ih_error;

/**
 * @brief Subscreve as interrupções do Teclado (IRQ 1).
 * @param bit_no Apontador onde será guardado o bit usado no mask.
 * @return bit_no original em sucesso, -1 em erro.
 */
int(kbd_subscribe_int)(uint8_t *bit_no);

/**
 * @brief Cancela a subscrição das interrupções do Teclado.
 * @return 0 em sucesso, 1 em erro.
 */
int(kbd_unsubscribe_int)();

/**
 * @brief Interrupt Handler do Teclado. Lê o scancode do OUT_BUF.
 */
void(kbc_ih)();

/**
 * @brief Escreve um comando diretamente para o registo de comandos do KBC.
 * @param cmd Comando a escrever.
 * @return 0 em sucesso, 1 em erro.
 */
int(kbc_write_command)(uint8_t cmd);

/**
 * @brief Escreve um argumento num comando prévio do KBC.
 * @param arg Argumento a passar.
 * @return 0 em sucesso, 1 em erro.
 */
int(kbc_write_argument)(uint8_t arg);

/**
 * @brief Lê a resposta (retorno) proveniente do OUT_BUF.
 * @param response Apontador onde será guardada a resposta.
 * @return 0 em sucesso, 1 em erro.
 */
int(kbc_read_response)(uint8_t *response);

/**
 * @brief Reativa as interrupções do teclado que possam ter sido desativadas.
 * @return 0 em sucesso, 1 em erro.
 */
int(kbd_enable_interrupts)();

#endif /* _LCOM_KBC_H_ */
/** @} */
