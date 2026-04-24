// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>
#include "mouse.h"
#include <stdint.h>
#include <stdio.h>

// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need/ it]
  lcf_trace_calls("/home/lcom/labs/lab4/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab4/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}int (mouse_test_packet)(uint32_t cnt) {
    // 1º: SUBSCREVER PRIMEIRO (Para o Minix não roubar as respostas)
    uint8_t bit_no;
    if (mouse_subscribe_int(&bit_no) != 0) return 1;
    uint32_t irq_set = BIT(bit_no);

    // 2º: ATIVAR O RATO
    if (mouse_write_command(0xF4) != 0) return 1;

    int r, ipc_status;
    message msg;
    uint8_t mouse_bytes[3];
    uint8_t byte_index = 0;
    uint32_t packets_read = 0;

    while (packets_read < cnt) {
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & irq_set) {
                        mouse_ih(); // Lê 1 byte
                        if (mouse_error) continue;

                        // REGRA DA SINCRONIZAÇÃO
                        if (byte_index == 0 && (mouse_byte & BIT(3)) == 0) {
                            continue;
                        }

                        mouse_bytes[byte_index] = mouse_byte;
                        byte_index++;

                        if (byte_index == 3) {
                            struct packet pp;
                            mouse_parse_packet(mouse_bytes, &pp);
                            mouse_print_packet(&pp);
                            
                            byte_index = 0;
                            packets_read++;
                        }
                    }
                    break;
            }
        }
    }

    // 3º: DESATIVAR O RATO
    if (mouse_write_command(0xF5) != 0) return 1;

    // 4º: CANCELAR A SUBSCRIÇÃO
    if (mouse_unsubscribe_int() != 0) return 1;

    return 0;
}
int (mouse_test_async)(uint8_t idle_time) {
    uint8_t mouse_bit_no, timer_bit_no;

    // 1º: Subscrever o Rato e o Timer
    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1;

    uint32_t irq_set_mouse = BIT(mouse_bit_no);
    uint32_t irq_set_timer = BIT(timer_bit_no);

    // 2º: Ativar o envio de dados do rato
    if (mouse_write_command(0xF4) != 0) return 1;

    int r, ipc_status;
    message msg;
    uint8_t mouse_bytes[3];
    uint8_t byte_index = 0;

    // Variáveis para contar o tempo
    uint32_t timer_ticks = 0;
    uint32_t freq = sys_hz(); // Frequência normal do Minix (60Hz)

    // O ciclo corre enquanto os ticks forem menores que os segundos pedidos * 60
    while (timer_ticks < idle_time * freq) {
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    // A) Foi o RATO que interrompeu?
                    if (msg.m_notify.interrupts & irq_set_mouse) {
                        mouse_ih();
                        if (mouse_error) continue;

                        if (byte_index == 0 && (mouse_byte & BIT(3)) == 0) continue;

                        mouse_bytes[byte_index] = mouse_byte;
                        byte_index++;

                        if (byte_index == 3) {
                            struct packet pp;
                            mouse_parse_packet(mouse_bytes, &pp);
                            mouse_print_packet(&pp);
                            byte_index = 0;

                            // HOUVE MOVIMENTO: O relógio volta a zero!
                            timer_ticks = 0; 
                        }
                    }
                    
                    // B) Foi o TIMER que interrompeu?
                    if (msg.m_notify.interrupts & irq_set_timer) {
                        timer_int_handler();
                        timer_ticks++; // Soma 1/60 de segundo
                    }
                    break;
            }
        }
    }

    // 3º: Desativar tudo pela ordem correta
    if (mouse_write_command(0xF5) != 0) return 1;
    if (mouse_unsubscribe_int() != 0) return 1;
    if (timer_unsubscribe_int() != 0) return 1;
    
    

    return 0;
}
