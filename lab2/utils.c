#include <lcom/lcf.h>
#include <stdint.h>

//Obtém o byte menos significativo de um valor de 16 bits.
int(util_get_LSB)(uint16_t val, uint8_t *lsb) {
  // Verifica se o ponteiro é válido
  if (lsb == NULL) return 1;

  // Faz máscara com 0x00FF para ficar só com os 8 bits menos significativos
  *lsb = (uint8_t)(val & 0x00FF);

  return 0;
}

//Obtém o byte mais significativo de um valor de 16 bits.
int(util_get_MSB)(uint16_t val, uint8_t *msb) {
  // Verifica se o ponteiro é válido
  if (msb == NULL) return 1;

  // Desloca 8 bits para a direita para trazer o MSB para a posição baixa
  // e faz máscara para garantir que ficam só 8 bits
  *msb = (uint8_t)((val >> 8) & 0x00FF);

  return 0;
}

//Wrapper para sys_inb.
int(util_sys_inb)(int port, uint8_t *byte) {
  // Verifica se o ponteiro de saída é válido
  if (byte == NULL) return 1;

  uint32_t value;

  // Lê 32 bits da porta
  if (sys_inb(port, &value) != OK) return 1;

  // Guarda apenas os 8 bits menos significativos
  *byte = (uint8_t)(value & 0xFF);

  return 0;
}
