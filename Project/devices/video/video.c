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
static unsigned int bytes_per_pixel = 0;

static inline void write_pixel_raw(uint8_t *pixel, uint32_t color) {
  switch (bytes_per_pixel) {
    case 4:
      *((uint32_t *) pixel) = color;
      break;
    case 3:
      pixel[0] = color & 0xFF;
      pixel[1] = (color >> 8) & 0xFF;
      pixel[2] = (color >> 16) & 0xFF;
      break;
    case 2:
      pixel[0] = color & 0xFF;
      pixel[1] = (color >> 8) & 0xFF;
      break;
    default:
      pixel[0] = color & 0xFF;
      break;
  }
}

void *(vg_init)(uint16_t mode) {
  vbe_mode_info_t vmi;

  if (vbe_get_mode_info(mode, &vmi) != 0) {
    printf("vg_init: vbe_get_mode_info() failed\n");
    return NULL;
  }

  h_res = vmi.XResolution;
  v_res = vmi.YResolution;
  bits_per_pixel = vmi.BitsPerPixel;
  bytes_per_pixel = (bits_per_pixel + 7) / 8;
  vram_size = h_res * v_res * bytes_per_pixel;

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

  uint8_t *pixel = back_buffer + (y * h_res + x) * bytes_per_pixel;
  write_pixel_raw(pixel, color);

  return 0;
}

int(vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
  return vg_draw_rectangle(x, y, len, 1, color);
}

int(vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
  if (back_buffer == NULL || x >= h_res || y >= v_res) return 1;

  if (x + width > h_res) width = h_res - x;
  if (y + height > v_res) height = v_res - y;
  if (width == 0 || height == 0) return 0;

  for (uint16_t row = 0; row < height; row++) {
    uint8_t *dst = back_buffer + ((y + row) * h_res + x) * bytes_per_pixel;

    if (bytes_per_pixel == 4) {
      uint32_t *dst32 = (uint32_t *) dst;
      for (uint16_t col = 0; col < width; col++) dst32[col] = color;
    } else {
      for (uint16_t col = 0; col < width; col++) {
        write_pixel_raw(dst + col * bytes_per_pixel, color);
      }
    }
  }

  return 0;
}

int(vg_draw_xpm_image)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, bool use_transparency) {
  if (back_buffer == NULL || pixmap == NULL || width == 0 || height == 0) return 1;

  int src_x = 0;
  int src_y = 0;
  int draw_w = width;
  int draw_h = height;

  if (x < 0) {
    src_x = -x;
    draw_w -= src_x;
    x = 0;
  }
  if (y < 0) {
    src_y = -y;
    draw_h -= src_y;
    y = 0;
  }
  if (x >= h_res || y >= v_res || draw_w <= 0 || draw_h <= 0) return 0;
  if (x + draw_w > h_res) draw_w = h_res - x;
  if (y + draw_h > v_res) draw_h = v_res - y;

  for (int row = 0; row < draw_h; row++) {
    const uint32_t *src = pixmap + (src_y + row) * width + src_x;
    uint8_t *dst = back_buffer + ((y + row) * h_res + x) * bytes_per_pixel;

    if (bytes_per_pixel == 4 && !use_transparency) {
      memcpy(dst, src, draw_w * sizeof(uint32_t));
      continue;
    }

    for (int col = 0; col < draw_w; col++) {
      uint32_t color = src[col];
      if (!use_transparency || color != transparent_color) {
        write_pixel_raw(dst + col * bytes_per_pixel, color);
      }
    }
  }

  return 0;
}

int(vg_draw_xpm_image_tinted)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, uint32_t tint_color) {
  if (back_buffer == NULL || pixmap == NULL || width == 0 || height == 0) return 1;

  int src_x = 0;
  int src_y = 0;
  int draw_w = width;
  int draw_h = height;

  if (x < 0) {
    src_x = -x;
    draw_w -= src_x;
    x = 0;
  }
  if (y < 0) {
    src_y = -y;
    draw_h -= src_y;
    y = 0;
  }
  if (x >= h_res || y >= v_res || draw_w <= 0 || draw_h <= 0) return 0;
  if (x + draw_w > h_res) draw_w = h_res - x;
  if (y + draw_h > v_res) draw_h = v_res - y;

  for (int row = 0; row < draw_h; row++) {
    const uint32_t *src = pixmap + (src_y + row) * width + src_x;
    uint8_t *dst = back_buffer + ((y + row) * h_res + x) * bytes_per_pixel;

    for (int col = 0; col < draw_w; col++) {
      if (src[col] != transparent_color) {
        write_pixel_raw(dst + col * bytes_per_pixel, tint_color);
      }
    }
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
      if (vg_draw_pixel(x + col, y + row, color) != 0) {
        free(pixmap);
        return 1;
      }
    }
  }

  free(pixmap);
  return 0;
}

void(vg_clear_back_buffer)(uint32_t color) {
  if (back_buffer == NULL) return;

  if (color == 0) {
    memset(back_buffer, 0, vram_size);
  } else {
    vg_draw_rectangle(0, 0, h_res, v_res, color);
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
