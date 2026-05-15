
#include <lcom/lcf.h>
#include "i8042.h"
#include <lcom/lab5.h>
#include "video_gr_extra.h"
#include <stdint.h>
#include <stdio.h>
#include "kbc.h"
#include "i8042.h"

extern uint8_t scancode_byte;   // definida no teu kbc.c
extern bool ih_error;
// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
    lcf_set_language("EN-US");
    lcf_trace_calls("/home/lcom/labs/lab5/trace.txt");
    lcf_log_output("/home/lcom/labs/lab5/output.txt");
    if (lcf_start(argc, argv))
        return 1;
    lcf_cleanup();
    return 0;
}
int(video_test_init)(uint16_t mode, uint8_t delay) {

    if (vg_init(mode) == NULL) {
        printf("video_test_init: vg_init() failed\n");
        return 1;
    }

    
    tickdelay(micros_to_ticks((uint32_t)delay * 1000000));

    if (vg_exit() != 0) return 1;

    return 0;
}

int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint32_t color) {

    if (vg_init(mode) == NULL) return 1;

    if (vg_draw_rectangle(x, y, width, height, color) != 0) return 1;

    uint8_t kbc_irq_set;
    if (kbd_subscribe_int(&kbc_irq_set) != 0) return 1;

    int ipc_status;
    message msg;
    uint8_t kbc_irq_bit = BIT(kbc_irq_set);  /* <-- BIT() aqui */
    bool esc_received = false;

    while (!esc_received) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_notify.interrupts & kbc_irq_bit) {  /* <-- kbc_irq_bit */
                kbc_ih();
                if (!ih_error && scancode_byte == ESC_BREAKCODE)
                    esc_received = true;
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) return 1;
    if (vg_exit() != 0) return 1;
    return 0;
 }

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {

    if (vg_init(0x105) == NULL) return 1;

    if (vg_draw_xpm(xpm, x, y) != 0) return 1;

    uint8_t kbc_irq_set;
    if (kbd_subscribe_int(&kbc_irq_set) != 0) return 1;

    int ipc_status;
    message msg;
    bool esc_received = false;

    while (!esc_received) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_notify.interrupts & BIT(kbc_irq_set)) {
                kbc_ih();
                if (!ih_error && scancode_byte == ESC_BREAKCODE)
                    esc_received = true;
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) return 1;
    if (vg_exit() != 0) return 1;
    return 0;
}
