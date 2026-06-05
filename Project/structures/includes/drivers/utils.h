/**
 * @file utils.h
 * @brief Funções utilitárias partilhadas de baixo nível.
 *
 * @defgroup Utils Utilities
 * @ingroup Devices
 * @brief Manipulação de LSB/MSB e chamadas I/O base.
 * @{
 */

#ifndef _LCOM_UTILS_H_
#define _LCOM_UTILS_H_

#include <stdint.h>

/**
 * @brief Extrai o LSB de um valor de 16 bits.
 * @param val Valor original de 16 bits.
 * @param lsb Apontador onde será colocado o byte menos significativo.
 * @return 0 em sucesso.
 */
int(util_get_LSB)(uint16_t val, uint8_t *lsb);

/**
 * @brief Extrai o MSB de um valor de 16 bits.
 * @param val Valor original de 16 bits.
 * @param msb Apontador onde será colocado o byte mais significativo.
 * @return 0 em sucesso.
 */
int(util_get_MSB)(uint16_t val, uint8_t *msb);

/**
 * @brief Chamada wrapper do sys_inb. Transforma a receção 32-bit numa receção de 8-bits.
 * @param port Endereço do porto a ser lido.
 * @param value Variável de 8 bits onde o valor lido será gravado.
 * @return 0 em sucesso, 1 em erro.
 */
int(util_sys_inb)(int port, uint8_t *value);

#endif /* _LCOM_UTILS_H_ */
/** @} */
