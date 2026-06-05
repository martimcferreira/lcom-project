/**
 * @file timer.h
 * @brief Driver do i8254 Timer.
 *
 * @defgroup Timer Timer (i8254)
 * @ingroup Devices
 * @brief Gestão do relógio do sistema e interrupções periódicas.
 * @{
 */

#ifndef _LCOM_TIMER_DRIVER_H_
#define _LCOM_TIMER_DRIVER_H_

#include <lcom/lcf.h>
#include <stdint.h>

/** @brief Variável que conta as interrupções do timer. */
extern uint32_t no_interrupts;

/**
 * @brief Altera a frequência de operação de um timer.
 * @param timer Qual timer configurar (0, 1 ou 2).
 * @param freq Frequência pretendida.
 * @return 0 em sucesso, 1 em erro.
 */
int(timer_set_frequency)(uint8_t timer, uint32_t freq);

/**
 * @brief Subscreve interrupções do Timer 0.
 * @param bit_no Apontador onde será guardado o bit usado no mask.
 * @return bit_no original em sucesso, -1 em erro.
 */
int(timer_subscribe_int)(uint8_t *bit_no);

/**
 * @brief Cancela a subscrição das interrupções do Timer 0.
 * @return 0 em sucesso, 1 em erro.
 */
int(timer_unsubscribe_int)();

/**
 * @brief Rotina de tratamento de interrupção (Interrupt Handler) do Timer.
 * Incrementa a variável `no_interrupts`.
 */
void(timer_int_handler)();

/**
 * @brief Lê o registo de configuração de um timer usando Read-Back.
 * @param timer O timer a ler (0, 1 ou 2).
 * @param st Endereço de memória onde a configuração será guardada.
 * @return 0 em sucesso, 1 em erro.
 */
int(timer_get_conf)(uint8_t timer, uint8_t *st);

/**
 * @brief Mostra a configuração do timer de uma forma amigável no output de LCOM.
 * @param timer Timer cuja configuração vai ser mostrada.
 * @param st Configuração original lida pelo timer_get_conf.
 * @param field Qual parte da configuração se pretende extrair e mostrar.
 * @return 0 em sucesso, 1 em erro.
 */
int(timer_display_conf)(uint8_t timer, uint8_t st, enum timer_status_field field);

#endif /* _LCOM_TIMER_DRIVER_H_ */
/** @} */
