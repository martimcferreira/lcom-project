#include <lcom/lcf.h>

#include <stdbool.h>
#include <stdint.h>

#include "rtc.h"

#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B
#define RTC_REG_DAY 0x07
#define RTC_REG_MONTH 0x08
#define RTC_REG_YEAR 0x09
#define RTC_UIP_MSK BIT(7)
#define RTC_DM_MSK BIT(2)

static uint8_t bcd_to_bin(uint8_t bcd) {
  return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static int rtc_read_register(uint8_t reg, uint8_t *value) {
  uint32_t temp;

  if (value == NULL) return 1;
  if (sys_outb(RTC_ADDR_REG, reg) != 0) return 1;
  if (sys_inb(RTC_DATA_REG, &temp) != 0) return 1;

  *value = (uint8_t) temp;
  return 0;
}

int rtc_read_date(rtc_date *date) {
  if (date == NULL) return 1;

  uint8_t reg_a;
  do {
    if (rtc_read_register(RTC_REG_A, &reg_a) != 0) return 1;
    if (reg_a & RTC_UIP_MSK) tickdelay(micros_to_ticks(2000));
  } while (reg_a & RTC_UIP_MSK);

  uint8_t reg_b;
  if (rtc_read_register(RTC_REG_B, &reg_b) != 0) return 1;

  bool is_binary = reg_b & RTC_DM_MSK;

  uint8_t day, month, year;
  if (rtc_read_register(RTC_REG_DAY, &day) != 0) return 1;
  if (rtc_read_register(RTC_REG_MONTH, &month) != 0) return 1;
  if (rtc_read_register(RTC_REG_YEAR, &year) != 0) return 1;

  date->day = is_binary ? day : bcd_to_bin(day);
  date->month = is_binary ? month : bcd_to_bin(month);
  date->year = is_binary ? year : bcd_to_bin(year);

  return 0;
}
