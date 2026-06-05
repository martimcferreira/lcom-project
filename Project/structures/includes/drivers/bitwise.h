/**
 * @file bitwise.h
 * @brief Operações de manipulação bit-a-bit.
 *
 * @defgroup Bitwise Bitwise Helpers
 * @ingroup Devices
 * @brief Ferramentas de auxílio à manipulação de bits em hardware.
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MSK_END -1

/**
 * @brief Devolve uma nova máscara idêntica a `msk` mas com o bit em `pos` a 0.
 * @param msk Máscara base.
 * @param pos Posição do bit (0 a 7).
 * @return A nova máscara.
 */
uint8_t clear(uint8_t msk, int pos);

/**
 * @brief Devolve uma nova máscara idêntica a `msk` mas com o bit em `pos` a 1.
 * @param msk Máscara base.
 * @param pos Posição do bit (0 a 7).
 * @return A nova máscara.
 */
uint8_t set(uint8_t msk, int pos);

/**
 * @brief Verifica se o bit na posição `pos` da `msk` está ativo (1).
 * @param msk Máscara base.
 * @param pos Posição a testar (0 a 7).
 * @return true se ativo, false em caso contrário.
 */
bool is_set(uint8_t msk, int pos);

/**
 * @brief Retorna o Byte Menos Significativo (LSB) de um número de 16 bits.
 * @param wide_msk O valor de 16 bits.
 * @return O LSB (8 bits inferiores).
 */
uint8_t lsb(uint16_t wide_msk);

/**
 * @brief Retorna o Byte Mais Significativo (MSB) de um número de 16 bits.
 * @param wide_msk O valor de 16 bits.
 * @return O MSB (8 bits superiores).
 */
uint8_t msb(uint16_t wide_msk);

/**
 * @brief Constrói uma máscara com os bits ativos nas posições indicadas.
 * A lista de argumentos variável termina no sentinela `MSK_END`.
 * @param pos Primeira posição a ativar.
 * @param ... Restantes posições a ativar.
 * @return O byte resultante da aplicação lógica OR.
 */
uint8_t mask(int pos, ...);

/** @} */
