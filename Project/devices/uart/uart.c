#include <lcom/lcf.h>
#include <stdint.h>

#include "uart.h"

/* -----------------------------------------------------------------------
 * uart_init
 *
 * Configura a COM1 para 115200 bps, 8N1, sem interrupções de hardware.
 * Sequência obrigatória:
 *   1. Desativar interrupções (IER = 0)
 *   2. Ativar DLAB para aceder aos divisores de baud rate
 *   3. Escrever DLL e DLM com o divisor correto (1 para 115200 bps)
 *   4. Desativar DLAB e configurar formato da trama (8N1) no LCR
 * ----------------------------------------------------------------------- */
int uart_init(void) {
  /* 1. Desativar todas as interrupções da UART */
  if (sys_outb(UART_COM1_BASE + UART_IER, 0x00) != OK) return 1;

  /* 2. Ativar DLAB (bit 7 do LCR) para aceder aos registos de divisão */
  if (sys_outb(UART_COM1_BASE + UART_LCR, UART_DLAB_BIT) != OK) return 1;

  /* 3. Programar o divisor para 115200 bps: DLL = 0x01, DLM = 0x00 */
  if (sys_outb(UART_COM1_BASE + UART_DLL, UART_BAUD_115200_DLL) != OK) return 1;
  if (sys_outb(UART_COM1_BASE + UART_DLM, UART_BAUD_115200_DLM) != OK) return 1;

  /* 4. Configurar formato 8N1 e desativar DLAB (DLAB=0 implícito em 0x03) */
  if (sys_outb(UART_COM1_BASE + UART_LCR, UART_8N1) != OK) return 1;

  return 0;
}

/* -----------------------------------------------------------------------
 * uart_send_byte
 *
 * Envia um único byte por polling: aguarda que o bit THRE (bit 5) do LSR
 * esteja a 1 (THR vazio e pronto a aceitar dados) antes de escrever no THR.
 * ----------------------------------------------------------------------- */
int uart_send_byte(uint8_t byte) {
  uint32_t lsr;

  /* Polling até ao THRE: aguarda o transmissor estar livre */
  do {
    if (sys_inb(UART_COM1_BASE + UART_LSR, &lsr) != OK) return 1;
  } while ((lsr & UART_THRE_BIT) == 0);

  /* Escreve o byte no Transmit Holding Register */
  return (sys_outb(UART_COM1_BASE + UART_THR, (uint32_t)byte) != OK) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * uart_send_packet
 *
 * Envia um pacote de 4 bytes com o protocolo definido:
 *   [0xAA (START)] [CMD] [ARG] [0xFF (END)]
 * ----------------------------------------------------------------------- */
int uart_send_packet(uint8_t cmd, uint8_t arg) {
  if (uart_send_byte(UART_PKT_START) != 0) return 1;
  if (uart_send_byte(cmd)            != 0) return 1;
  if (uart_send_byte(arg)            != 0) return 1;
  if (uart_send_byte(UART_PKT_END)   != 0) return 1;
  return 0;
}
