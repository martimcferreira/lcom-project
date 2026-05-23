#pragma once

#include <lcom/lcf.h>
#include <stdint.h>

// Estrutura expandida para suportar também as Horas e Minutos
typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} rtc_timestamp;

/**
 * Lê a data e hora atual do RTC e preenche a estrutura `rtc_timestamp`.
 * Retorna 0 em caso de sucesso, não-zero em caso de erro.
 */
int rtc_read_time(rtc_timestamp *timestamp);
