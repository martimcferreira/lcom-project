#include <lcom/lcf.h>
#include "i8042.h"
#include "mouse.h"
#include "kbc.h"

int mouse_hook_id = 2; // Diferente do teclado (1) e timer (0)
uint8_t mouse_byte;    // Byte global lido em cada interrupção
bool mouse_error = false;

int(mouse_subscribe_int)(uint8_t *bit_no)
{
  if (bit_no == NULL)
    return 1;
  *bit_no = mouse_hook_id;
  // IRQ 12 é a linha dedicada ao rato PS/2
  return sys_irqsetpolicy(12, IRQ_REENABLE | IRQ_EXCLUSIVE, &mouse_hook_id);
}

int(mouse_unsubscribe_int)()
{
  return sys_irqrmpolicy(&mouse_hook_id);
}

void(mouse_ih)()
{
  uint8_t status;
  mouse_error = false;

  if (util_sys_inb(KBC_STAT_REG, &status) != 0)
  {
    mouse_error = true;
    return;
  }

  // Verifica se há dados (OBF) E se esses dados vêm do Rato (BIT(5) ou AUX)
  if ((status & KBC_OBF) && (status & BIT(5)))
  {
    if (util_sys_inb(KBC_OUT_BUF, &mouse_byte) != 0)
    {
      mouse_error = true;
      return;
    }
    // Verifica erros de paridade ou timeout
    if (status & (KBC_PARITY | KBC_TIMEOUT))
      mouse_error = true;
  }
  else
  {
    mouse_error = true;
  }
}

void(mouse_parse_packet)(uint8_t *bytes, struct packet *pp)
{

  pp->bytes[0] = bytes[0];
  pp->bytes[1] = bytes[1];
  pp->bytes[2] = bytes[2];

  pp->lb = bytes[0] & BIT(0);
  pp->rb = bytes[0] & BIT(1);
  pp->mb = bytes[0] & BIT(2);

  pp->delta_x = bytes[1];
  pp->delta_y = bytes[2];

  if (bytes[0] & BIT(4))
    pp->delta_x |= 0xFF00;
  if (bytes[0] & BIT(5))
    pp->delta_y |= 0xFF00;

  pp->x_ov = bytes[0] & BIT(6);
  pp->y_ov = bytes[0] & BIT(7);
}
int(mouse_write_command)(uint8_t command)
{
  uint8_t ack_byte;
  uint8_t max_retries = 10;

  while (max_retries > 0)
  {
    if (kbc_write_command(0xD4) != 0)
    {
      max_retries--;
      continue;
    }
    if (kbc_write_argument(command) != 0)
    {
      max_retries--;
      continue;
    }
    if (kbc_read_response(&ack_byte) != 0)
    {
      max_retries--;
      continue;
    }

    if (ack_byte == 0xFA)
    {
      return 0; // Sucesso absoluto!
    }

    // Se a resposta for FE, FC, ou qualquer outro lixo, tenta de novo
    max_retries--;
  }

  return 1;
}
