/**
 * @file rtc.h
 * @brief Driver para o Real-Time Clock (RTC).
 *
 * Gere a leitura da data e hora atual do sistema, muito útil para
 * o registo cronológico das pontuações na Leaderboard.
 * 
 * @defgroup RTC RTC (Real-Time Clock)
 * @ingroup Devices
 * @brief Leitura de Data e Hora do sistema.
 * @{
 */

#pragma once

#include <lcom/lcf.h>
#include <stdint.h>

/**
 * @brief Estrutura que guarda a data e hora do sistema lida pelo RTC.
 */
typedef struct {
  uint8_t year;     /**< @brief Ano atual. */
  uint8_t month;    /**< @brief Mês atual. */
  uint8_t day;      /**< @brief Dia atual. */
  uint8_t hours;    /**< @brief Hora atual. */
  uint8_t minutes;  /**< @brief Minutos atuais. */
  uint8_t seconds;  /**< @brief Segundos atuais. */
} rtc_timestamp;

/**
 * @brief Lê a data e hora atual do RTC e preenche a estrutura `rtc_timestamp`.
 * 
 * Efetua a leitura dos registos adequados e converte (se aplicável)
 * para um formato em binário utilizável pela aplicação.
 * 
 * @param timestamp Endereço de memória da estrutura a ser preenchida.
 * @return 0 em caso de sucesso, não-zero em caso de erro.
 */
int rtc_read_time(rtc_timestamp *timestamp);

/** @} */
