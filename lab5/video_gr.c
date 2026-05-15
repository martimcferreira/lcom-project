#include "video_gr.h"

#include <machine/int86.h>
#include <sys/mman.h>
#include <string.h>

#define VBE_SET_MODE 0x4F02
#define VBE_LINEAR_FRAMEBUFFER BIT(14)

static uint8_t *video_mem;
static vbe_mode_info_t vmi;
static unsigned bytes_per_pixel;

int set_graphics_mode(uint16_t mode) {
  reg86_t r;
  memset(&r, 0, sizeof(r));

  r.intno = 0x10;
  r.ax = VBE_SET_MODE;
  r.bx = mode | VBE_LINEAR_FRAMEBUFFER;

  if (sys_int86(&r) != OK) {
    printf("set_graphics_mode: sys_int86 failed\n");
    return 1;
  }

  if (r.al != 0x4F || r.ah != 0x00) {
    printf("set_graphics_mode: VBE call failed, AX = 0x%04x\n", r.ax);
    return 1;
  }

  return 0;
}

int map_vram(uint16_t mode) {
  if (vbe_get_mode_info(mode, &vmi) != OK) {
    printf("map_vram: vbe_get_mode_info failed\n");
    return 1;
  }

  bytes_per_pixel = (vmi.BitsPerPixel + 7) / 8;

  phys_bytes vram_base = (phys_bytes) vmi.PhysBasePtr;
  size_t vram_size = vmi.BytesPerScanLine * vmi.YResolution;

  struct minix_mem_range mr;
  mr.mr_base = vram_base;
  mr.mr_limit = mr.mr_base + vram_size;

  int r;
  if ((r = sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr)) != OK) {
    printf("map_vram: sys_privctl failed: %d\n", r);
    return 1;
  }

  video_mem = vm_map_phys(SELF, (void *) mr.mr_base, vram_size);

  if (video_mem == MAP_FAILED) {
    printf("map_vram: vm_map_phys failed\n");
    return 1;
  }

  return 0;
}

int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
  if (x >= vmi.XResolution || y >= vmi.YResolution) {
    return 1;
  }

  uint8_t *pixel_addr = video_mem + y * vmi.BytesPerScanLine + x * bytes_per_pixel;

  memcpy(pixel_addr, &color, bytes_per_pixel);

  return 0;
}

int vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
  for (uint16_t i = 0; i < len; i++) {
    if (vg_draw_pixel(x + i, y, color) != 0) {
      return 1;
    }
  }

  return 0;
}

int vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
  for (uint16_t row = 0; row < height; row++) {
    if (vg_draw_hline(x, y + row, width, color) != 0) {
      return 1;
    }
  }

  return 0;
}

uint16_t get_hres(void) {
  return vmi.XResolution;
}

uint16_t get_vres(void) {
  return vmi.YResolution;
}

unsigned get_bytes_per_pixel(void) {
  return bytes_per_pixel;
}