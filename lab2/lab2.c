#include <lcom/lcf.h>
#include <lcom/lab2.h>
#include <lcom/timer.h>

#include <stdint.h>
#include <stdio.h>

// O contador está definido em timer.c.
// Aqui apenas dizemos ao compilador que ele existe.
extern volatile int timer_counter;

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/grupo_2leic02_2/lab2/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/grupo_2leic02_2/lab2/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  lcf_cleanup();

  return 0;
}

//Testa a leitura e visualização da configuração de um timer.
int(timer_test_read_config)(uint8_t timer, enum timer_status_field field) {
  uint8_t st;

  // Lê configuração do timer
  if (timer_get_conf(timer, &st) != 0)
    return 1;

  // Mostra a parte pedida dessa configuração
  if (timer_display_conf(timer, st, field) != 0)
    return 1;

  return 0;
}

//Testa a alteração da frequência de um timer.
int(timer_test_time_base)(uint8_t timer, uint32_t freq) {
  return timer_set_frequency(timer, freq);
}

//Testa interrupções do timer durante "time" segundos.
int(timer_test_int)(uint8_t time) {
  uint8_t bit_no;

  // Subscrever interrupções do timer
  if (timer_subscribe_int(&bit_no) != 0)
    return 1;

  // Máscara correspondente ao bit da interrupção
  uint32_t irq_set = BIT(bit_no);

  int ipc_status, r;
  message msg;

  // Reinicia contador global
  timer_counter = 0;

  // Conta quantos segundos já passaram
  uint8_t elapsed = 0;

  // Continuar até atingir o número de segundos pedido
  while (elapsed < time) {
    // Espera por uma mensagem/interrupção
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      printf("driver_receive failed with: %d\n", r);
      continue;
    }

    // Verifica se a mensagem recebida é uma notificação IPC
    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
        case HARDWARE:
          //Verifica se a interrupção recebida foi a do timer.
          //É importante testar a bitmask, porque podem chegar
          //outras notificações de hardware.
          if (msg.m_notify.interrupts & irq_set) {
            // Chama o handler do timer
            timer_int_handler();
            //O timer 0 gera normalmente 60 interrupções por segundo.
            //Portanto, sempre que o contador for múltiplo de 60,
            //passou aproximadamente 1 segundo.
            if (timer_counter % 60 == 0) {
              timer_print_elapsed_time();
              elapsed++;
            }
          }
          break;

        default:
          // Ignorar outras notificações
          break;
      }
    }
  }
  // Cancelar subscrição antes de sair
  if (timer_unsubscribe_int() != 0)
    return 1;

  return 0;
}

