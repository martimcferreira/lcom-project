#include <lcom/lcf.h>
#include "mouse.h"
#include <stdint.h>
#include <stdio.h>


int main(int argc, char *argv[]) {
  lcf_set_language("EN-US");
 
  lcf_trace_calls("/home/lcom/labs/lab4/trace.txt");

  lcf_log_output("/home/lcom/labs/lab4/output.txt");

  if (lcf_start(argc, argv))
    return 1;

  lcf_cleanup();

  return 0;
}int (mouse_test_packet)(uint32_t cnt) {
    uint8_t bit_no;
    if (mouse_subscribe_int(&bit_no) != 0) return 1;
    uint32_t irq_set = BIT(bit_no);

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

    if (mouse_write_command(0xF5) != 0) return 1;

    if (mouse_unsubscribe_int() != 0) return 1;

    return 0;
}
int (mouse_test_async)(uint8_t idle_time) {
    uint8_t mouse_bit_no, timer_bit_no;

    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1;

    uint32_t irq_set_mouse = BIT(mouse_bit_no);
    uint32_t irq_set_timer = BIT(timer_bit_no);

    if (mouse_write_command(0xF4) != 0) return 1;

    int r, ipc_status;
    message msg;
    uint8_t mouse_bytes[3];
    uint8_t byte_index = 0;

    uint32_t timer_ticks = 0;
    uint32_t freq = sys_hz();

    while (timer_ticks < idle_time * freq) {
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
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

                            timer_ticks = 0; 
                        }
                    }
                    
                    if (msg.m_notify.interrupts & irq_set_timer) {
                        timer_int_handler();
                        timer_ticks++;
                    }
                    break;
            }
        }
    }

    if (mouse_write_command(0xF5) != 0) return 1;
    if (mouse_unsubscribe_int() != 0) return 1;
    if (timer_unsubscribe_int() != 0) return 1;
    
    

    return 0;
}
