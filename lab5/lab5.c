// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <lcom/lab5.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

// Any header files included below this line should have been created by you

#define KBC_OUT_BUF 0x60
#define KBC_ST_REG 0x64

#define KBD_IRQ 1

#define KBC_OBF BIT(0)
#define KBC_AUX BIT(5)
#define KBC_TIMEOUT_ERR BIT(6)
#define KBC_PARITY_ERR BIT(7)

#define ESC_BREAK_CODE 0x81


static int kbd_hook_id = KBD_IRQ;
static uint8_t scancode_byte = 0;
static bool scancode_valid = false;

static int kbd_subscribe_int(uint8_t *irq_set) {
    if (irq_set == NULL) {
        return 1;
    }

    *irq_set = BIT(kbd_hook_id);
    if (sys_irqsetpolicy(KBD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id) != 0) {
        return 1;
    }

    return 0;
}

static int kbd_unsubscribe_int(void) {
    if (sys_irqrmpolicy(&kbd_hook_id) != 0) {
        return 1;
    }

    return 0;
}

static void kbd_ih(void) {
    scancode_valid = false;

    uint8_t status;
    if (util_sys_inb(KBC_ST_REG, &status) != 0) {
        return;
    }

    if ((status & KBC_OBF) == 0) {
        return;
    }

    if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR | KBC_AUX)) != 0) {
        return;
    }

    uint8_t data;
    if (util_sys_inb(KBC_OUT_BUF, &data) != 0) {
        return;
    }

    scancode_byte = data;
    scancode_valid = true;
}

static int draw_xpm(uint16_t x, uint16_t y, const xpm_image_t *img, const uint8_t *pixmap) {
    for (uint16_t row = 0; row < img->height; ++row) {
        for (uint16_t col = 0; col < img->width; ++col) {
            uint32_t color = pixmap[row * img->width + col];
            if (vg_draw_hline(x + col, y + row, 1, color) != 0) {
                return 1;
            }
        }
    }

    return 0;
}


int main(int argc, char *argv[]) {
    // sets the language of LCF messages (can be either EN-US or PT-PT)
    lcf_set_language("EN-US");

    // enables to log function invocations that are being "wrapped" by LCF
    // [comment this out if you don't want/need it]
    lcf_trace_calls("/home/lcom/labs/lab5/trace.txt");

    // enables to save the output of printf function calls on a file
    // [comment this out if you don't want/need it]
    lcf_log_output("/home/lcom/labs/lab5/output.txt");

    // handles control over to LCF
    // [LCF handles command line arguments and invokes the right function]
    if (lcf_start(argc, argv))
        return 1;

    // LCF clean up tasks
    // [must be the last statement before return]
    lcf_cleanup();

    return 0;
}

int(video_test_init)(uint16_t mode, uint8_t delay) {
    if (vg_init(mode) == NULL) {
        printf("%s: vg_init(0x%03x) failed\n", __func__, mode);
        return 1;
    }

    sleep(delay);

    if (vg_exit() != 0) {
        printf("%s: vg_exit() failed\n", __func__);
        return 1;
    }

    return 0;
}

int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint32_t color) {
    if (vg_init(mode) == NULL) {
        printf("%s: vg_init(0x%03x) failed\n", __func__, mode);
        return 1;
    }

    if (vg_draw_rectangle(x, y, width, height, color) != 0) {
        printf("%s: vg_draw_rectangle failed\n", __func__);
        vg_exit();
        printf("\n");
        return 1;
    }

    uint8_t irq_set;
    if (kbd_subscribe_int(&irq_set) != 0) {
        printf("%s: kbd_subscribe_int failed\n", __func__);
        vg_exit();
        printf("\n");
        return 1;
    }

    scancode_byte = 0;
    scancode_valid = false;

    int ipc_status;
    message msg;

    while (scancode_byte != ESC_BREAK_CODE) {
        int r = driver_receive(ANY, &msg, &ipc_status);
        if (r != 0) {
            continue;
        }

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_source == HARDWARE) {
                if (msg.m_notify.interrupts & irq_set) {
                    kbd_ih();
                }
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) {
        printf("%s: kbd_unsubscribe_int failed\n", __func__);
        vg_exit();
        printf("\n");
        return 1;
    }

    if (vg_exit() != 0) {
        printf("%s: vg_exit() failed\n", __func__);
        return 1;
    }

    return 0;
}

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
    if (vg_init(0x105) == NULL) {
        printf("%s: vg_init(0x105) failed\n", __func__);
        return 1;
    }

    xpm_image_t img;
    uint8_t *pixmap = xpm_load(xpm, XPM_INDEXED, &img);
    if (pixmap == NULL) {
        printf("%s: xpm_load failed\n", __func__);
        vg_exit();
        return 1;
    }

    if (draw_xpm(x, y, &img, pixmap) != 0) {
        printf("%s: draw_xpm failed\n", __func__);
        vg_exit();
        return 1;
    }

    uint8_t irq_set;
    if (kbd_subscribe_int(&irq_set) != 0) {
        printf("%s: kbd_subscribe_int failed\n", __func__);
        vg_exit();
        return 1;
    }

    scancode_byte = 0;
    scancode_valid = false;

    int ipc_status;
    message msg;

    while (scancode_byte != ESC_BREAK_CODE) {
        int r = driver_receive(ANY, &msg, &ipc_status);
        if (r != 0) {
            continue;
        }

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_source == HARDWARE) {
                if (msg.m_notify.interrupts & irq_set) {
                    kbd_ih();
                }
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) {
        printf("%s: kbd_unsubscribe_int failed\n", __func__);
        vg_exit();
        return 1;
    }

    if (vg_exit() != 0) {
        printf("%s: vg_exit() failed\n", __func__);
        return 1;
    }

    return 0;
}
