#include <lcom/lcf.h>
#include <lcom/video_gr.h>

#include <minix/sysutil.h>
#include <sys/mman.h>

#include <stdio.h>
#include <string.h>

static char *video_mem;          /* Frame-buffer VM address */
static vbe_mode_info_t vmi;      /* VBE mode info */
static unsigned h_res;           /* Horizontal resolution */
static unsigned v_res;           /* Vertical resolution */
static unsigned bytes_per_pixel; /* Bytes per pixel */
static unsigned bytes_per_line;  /* Bytes per scan line */

static int set_vbe_mode(uint16_t mode) {
    reg86_t r86;

    memset(&r86, 0, sizeof(r86));
    r86.intno = 0x10;
    r86.ax = 0x4F02;
    r86.bx = mode | BIT(14);
    r86.bx &= ~BIT(15);

    if (sys_int86(&r86) != OK) {
        return 1;
    }

    if (r86.al != 0x4F || r86.ah != 0x00) {
        return 1;
    }

    return 0;
}

void *(vg_init)(uint16_t mode) {
    int r;
    struct minix_mem_range mr;
    unsigned int vram_base;
    unsigned int vram_size;

    if (vbe_get_mode_info(mode, &vmi) != 0) {
        printf("vg_init: vbe_get_mode_info(0x%03x) failed\n", mode);
        return NULL;
    }

    h_res = vmi.XResolution;
    v_res = vmi.YResolution;
    bytes_per_pixel = (vmi.BitsPerPixel + 7) / 8;
    bytes_per_line = (vmi.LinBytesPerScanLine != 0) ? vmi.LinBytesPerScanLine : vmi.BytesPerScanLine;

    vram_base = vmi.PhysBasePtr;
    vram_size = bytes_per_line * v_res;

    mr.mr_base = (phys_bytes)vram_base;
    mr.mr_limit = mr.mr_base + vram_size;

    if (OK != (r = sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr))) {
        printf("vg_init: sys_privctl failed: %d\n", r);
        return NULL;
    }

    video_mem = vm_map_phys(SELF, (void *)mr.mr_base, vram_size);
    if (video_mem == MAP_FAILED) {
        printf("vg_init: vm_map_phys failed\n");
        return NULL;
    }

    if (set_vbe_mode(mode) != 0) {
        printf("vg_init: set_vbe_mode(0x%03x) failed\n", mode);
        return NULL;
    }

    return video_mem;
}

static int draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
    unsigned int offset;
    unsigned int byte;

    if (x >= h_res || y >= v_res) {
        return 1;
    }

    offset = y * bytes_per_line + x * bytes_per_pixel;

    for (byte = 0; byte < bytes_per_pixel; ++byte) {
        video_mem[offset + byte] = (uint8_t)(color >> (8 * byte));
    }

    return 0;
}

int (vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
    unsigned int col;

    if (x >= h_res || y >= v_res || x + len > h_res) {
        return 1;
    }

    for (col = 0; col < len; ++col) {
        if (draw_pixel(x + col, y, color) != 0) {
            return 1;
        }
    }

    return 0;
}

int (vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
    unsigned int row;

    if (x >= h_res || y >= v_res || x + width > h_res || y + height > v_res) {
        return 1;
    }

    for (row = 0; row < height; ++row) {
        if (vg_draw_hline(x, y + row, width, color) != 0) {
            return 1;
        }
    }

    return 0;
}
