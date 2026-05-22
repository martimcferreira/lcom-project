#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"

static uint8_t *video_mem = NULL;
static uint8_t *back_buffer = NULL;
static unsigned int vram_size = 0;

static uint16_t h_res = 0;
static uint16_t v_res = 0;
static uint8_t bits_per_pixel = 0;

void *(vg_init)(uint16_t mode) {
  vbe_mode_info_t vmi;

  if (vbe_get_mode_info(mode, &vmi) != 0) {
    printf("vg_init: vbe_get_mode_info() failed\n");
    return NULL;
  }

  h_res = vmi.XResolution;
  v_res = vmi.YResolution;
  bits_per_pixel = vmi.BitsPerPixel;
  vram_size = h_res * v_res * ((bits_per_pixel + 7) / 8);

  struct minix_mem_range mr;
  mr.mr_base = vmi.PhysBasePtr;
  mr.mr_limit = mr.mr_base + vram_size;

  if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
    printf("vg_init: sys_privctl failed\n");
    return NULL;
  }

  video_mem = vm_map_phys(SELF, (void *) vmi.PhysBasePtr, vram_size);
  if (video_mem == MAP_FAILED) {
    printf("vg_init: vm_map_phys failed\n");
    video_mem = NULL;
    return NULL;
  }

  back_buffer = malloc(vram_size);
  if (back_buffer == NULL) {
    printf("vg_init: malloc failed\n");
    video_mem = NULL;
    return NULL;
  }

  memset(back_buffer, 0, vram_size);

  reg86_t r86;
  memset(&r86, 0, sizeof(r86));
  r86.intno = 0x10;
  r86.ah = 0x4F;
  r86.al = 0x02;
  r86.bx = mode | BIT(14);

  if (sys_int86(&r86) != OK || r86.ah != 0x00 || r86.al != 0x4F) {
    printf("vg_init: sys_int86() failed\n");
    vg_free_buffers();
    return NULL;
  }

  return video_mem;
}

int(vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color) {
  if (back_buffer == NULL || x >= h_res || y >= v_res) return 1;

  unsigned int bytes_per_pixel = (bits_per_pixel + 7) / 8;
  uint8_t *pixel = back_buffer + (y * h_res + x) * bytes_per_pixel;

  for (unsigned int i = 0; i < bytes_per_pixel; i++) {
    pixel[i] = (color >> (8 * i)) & 0xFF;
  }

  return 0;
}

int(vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
  for (uint16_t i = 0; i < len; i++) {
    if (vg_draw_pixel(x + i, y, color) != 0) return 1;
  }

  return 0;
}

int(vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
  for (uint16_t row = 0; row < height; row++) {
    if (vg_draw_hline(x, y + row, width, color) != 0) return 1;
  }

  return 0;
}

int(vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
  xpm_image_t img;
  uint8_t *pixmap = xpm_load(xpm, XPM_INDEXED, &img);

  if (pixmap == NULL) return 1;

  for (uint16_t row = 0; row < img.height; row++) {
    for (uint16_t col = 0; col < img.width; col++) {
      uint8_t color = pixmap[row * img.width + col];
      if (vg_draw_pixel(x + col, y + row, color) != 0) return 1;
    }
  }

  return 0;
}

void(vg_clear_back_buffer)(uint32_t color) {
  if (back_buffer == NULL) return;

  if (color == 0) {
    memset(back_buffer, 0, vram_size);
    return;
  }

  for (uint16_t y = 0; y < v_res; y++) {
    for (uint16_t x = 0; x < h_res; x++) {
      vg_draw_pixel(x, y, color);
    }
  }
}

void(vg_swap_buffers)() {
  if (video_mem != NULL && back_buffer != NULL) {
    memcpy(video_mem, back_buffer, vram_size);
  }
}

void(vg_free_buffers)() {
  free(back_buffer);
  back_buffer = NULL;
}

uint16_t(vg_get_h_res)() {
  return h_res;
}

uint16_t(vg_get_v_res)() {
  return v_res;
}
