#include "structures/includes/drivers/_rtc.h"
#include <minix/sysutil.h>

// Endereços Base
#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71

// Registos de Controlo e Dados
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B

#define RTC_REG_SEC   0x00
#define RTC_REG_MIN   0x02
#define RTC_REG_HOURS 0x04
#define RTC_REG_DAY   0x07
#define RTC_REG_MONTH 0x08
#define RTC_REG_YEAR  0x09

// Máscaras
#define RTC_UIP_MSK BIT(7)
#define RTC_DM_MSK  BIT(2)

// Corrigido o nome para "bcd" (Binary-Coded Decimal)
static int bcd_to_bin(uint8_t bcd) { 
  return ((bcd >> 4) * 10 + (bcd & 0x0F)); 
}

int rtc_read_time(rtc_timestamp *timestamp) {
    uint32_t regA, regB, temp;

    // 1. Esperar que o RTC não esteja a atualizar (UIP == 0)
    while (true) {
        sys_outb(RTC_ADDR_REG, RTC_REG_A);
        sys_inb(RTC_DATA_REG, &regA);
        if (!(regA & RTC_UIP_MSK)) break;  
        tickdelay(micros_to_ticks(2000));
    }

    // 2. Ler o Registo B para saber se os dados estão em BCD ou Binário puro
    sys_outb(RTC_ADDR_REG, RTC_REG_B);
    sys_inb(RTC_DATA_REG, &regB);
    bool is_binary = (regB & RTC_DM_MSK);

    // 3. Ler Data
    sys_outb(RTC_ADDR_REG, RTC_REG_DAY);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->day = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    sys_outb(RTC_ADDR_REG, RTC_REG_MONTH);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->month = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    sys_outb(RTC_ADDR_REG, RTC_REG_YEAR);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->year = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    // 4. Ler Hora (NOVO)
    sys_outb(RTC_ADDR_REG, RTC_REG_HOURS);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->hours = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    sys_outb(RTC_ADDR_REG, RTC_REG_MIN);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->minutes = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    sys_outb(RTC_ADDR_REG, RTC_REG_SEC);
    sys_inb(RTC_DATA_REG, &temp);
    timestamp->seconds = is_binary ? (uint8_t)temp : (uint8_t)bcd_to_bin(temp);

    return 0;
}
