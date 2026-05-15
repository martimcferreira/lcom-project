#include "rtc.h"
#include <minix/syslib.h>
#include "bitwise.h"
#include <minix/sysutil.h>


#define TODO return -1

#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B
#define RTC_REG_DAY 0x07
#define RTC_REG_MONTH 0x08
#define RTC_REG_YEAR 0x09
#define RTC_UIP_MSK (1 << 7)
#define RTC_DM_MSK (1 << 2)

static int bdc_to_bin(uint8_t bcd) { 
  return ((bcd >> 4)*10 + (bcd & 0x0F)); }

int rtc_read_date(rtc_date *date) {
    uint32_t regA, regB, temp;

 
    while (true) {
        sys_outb(RTC_ADDR_REG, RTC_REG_A);
        sys_inb(RTC_DATA_REG, &regA);
        if (!(regA & RTC_UIP_MSK)) break;  
        tickdelay(micros_to_ticks(244));
    }


    sys_outb(RTC_ADDR_REG, RTC_REG_B);
    sys_inb(RTC_DATA_REG, &regB);
    bool is_binary = (regB & RTC_DM_MSK);

    sys_outb(RTC_ADDR_REG, RTC_REG_DAY);
    sys_inb(RTC_DATA_REG, &temp);
    date->day = is_binary ? (uint8_t)temp : (uint8_t)bdc_to_bin(temp);

    // Mês
    sys_outb(RTC_ADDR_REG, RTC_REG_MONTH);
    sys_inb(RTC_DATA_REG, &temp);
    date->month = is_binary ? (uint8_t)temp : (uint8_t)bdc_to_bin(temp);

    // Ano
    sys_outb(RTC_ADDR_REG, RTC_REG_YEAR);
    sys_inb(RTC_DATA_REG, &temp);
    date->year = is_binary ? (uint8_t)temp : (uint8_t)bdc_to_bin(temp);

    return 0;
} 
