#include <lcom/lcf.h>
#include "i8042.h"
#include "kbc.h"
#define KBC_RETRIES 10

uint8_t scancode_byte = 0; // Declares a global variable to store the scancode byte
bool ih_error = false; // Initializes a global variable to indicate if an error occurred in the interrupt handler
int hook_id_kbc = 1; // ID usado pelo MINIX para identificar a subscrição da IRQ

int (kbd_subscribe_int)(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;

    *bit_no = 1;

    if (sys_irqsetpolicy(KBC_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &hook_id_kbc) != 0) return 1;

    return 0;
}

int (kbd_unsubscribe_int)() {
    if (sys_irqrmpolicy(&hook_id_kbc) != 0) return 1;
    return 0;
}

void (kbc_ih)() {
    uint8_t status; // Declares a variable to store the status of the keyboard controller
    ih_error = false; // Resets the error flag at the start of the interrupt handler

    if (util_sys_inb(KBC_STAT_REG, &status) != 0) {
        ih_error = true;
        return;
    }

    if (status & KBC_OBF) {
        uint8_t data; 
        if (util_sys_inb(KBC_OUT_BUF, &data) != 0) {
            ih_error = true;
            return;
        }

        if ((status & (KBC_PARITY | KBC_TIMEOUT)) == 0) {
            scancode_byte = data;
        } else {
            scancode_byte = 0;
            ih_error = true;
        }
    } else {
        ih_error = true;
    }
}

int (kbc_write_command)(uint8_t cmd) {
    uint8_t status; // Declares a variable to store the status of the keyboard controller

    for (int i = 0; i < KBC_RETRIES; i++) {
        if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

        if ((status & KBC_IBF) == 0) {
            return sys_outb(KBC_STAT_REG, cmd);
        }
        tickdelay(micros_to_ticks(DELAY_US));
    }
    return 1;
}

int (kbc_write_argument)(uint8_t arg) {
    uint8_t status; // Declares a variable to store the status of the keyboard controller

    for (int i = 0; i < KBC_RETRIES; i++) {
        if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

        if ((status & KBC_IBF) == 0) {
            return sys_outb(KBC_OUT_BUF, arg);
        }
        tickdelay(micros_to_ticks(DELAY_US));
    }
    return 1;
}

int (kbc_read_response)(uint8_t *response) {
    uint8_t status; // Declares a variable to store the status of the keyboard controller

    for (int i = 0; i < KBC_RETRIES; i++) {
        if (util_sys_inb(KBC_STAT_REG, &status) != 0) return 1;

        if (status & KBC_OBF) {
            if (util_sys_inb(KBC_OUT_BUF, response) != 0) return 1;

            if ((status & (KBC_PARITY | KBC_TIMEOUT)) == 0) {
                return 0; // Successfully read the response
            } else {
                return 1; // Indicates an error in the response
            }
        }
        tickdelay(micros_to_ticks(DELAY_US));
    }
    return 1;
}

int (kbd_enable_interrupts)() {
    uint8_t command_byte;

    if (kbc_write_command(KBC_READ_CMD) != 0) return 1;

    if (kbc_read_response(&command_byte) != 0) return 1;

    command_byte |= BIT(0);

    if (kbc_write_command(KBC_WRITE_CMD) != 0) return 1;

    if (kbc_write_argument(command_byte) != 0) return 1;

    return 0;
}
