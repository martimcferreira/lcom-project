#include <lcom/lcf.h>
#include "video_gr.h"
#include "keyboard_lab5.h"

static int draw_xpm(uint8_t *pixmap, xpm_image_t img, uint16_t x, uint16_t y) {
  for (uint16_t row = 0; row < img.height; row++) {
    for (uint16_t col = 0; col < img.width; col++) {
      uint32_t color = pixmap[row * img.width + col];

      if (vg_draw_pixel(x + col, y + row, color) != 0) {
        return 1;
      }
    }
  }

  return 0;
}

int(video_test_init)(uint16_t mode, uint8_t delay) {
  if (set_graphics_mode(mode) != 0) {
    return 1;
  }

  sleep(delay);

  if (vg_exit() != 0) {
    return 1;
  }

  return 0;
}

int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint32_t color) {
  if (map_vram(mode) != 0) {
    return 1;
  }

  if (set_graphics_mode(mode) != 0) {
    return 1;
  }

  if (vg_draw_rectangle(x, y, width, height, color) != 0) {
    vg_exit();
    return 1;
  }

  if (wait_esc_break() != 0) {
    vg_exit();
    return 1;
  }

  if (vg_exit() != 0) {
    return 1;
  }

  return 0;
}

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
  uint16_t mode = 0x105;

  if (map_vram(mode) != 0) {
    return 1;
  }

  if (set_graphics_mode(mode) != 0) {
    return 1;
  }

  xpm_image_t img;
  uint8_t *pixmap = xpm_load(xpm, XPM_INDEXED, &img);

  if (pixmap == NULL) {
    vg_exit();
    return 1;
  }

  if (draw_xpm(pixmap, img, x, y) != 0) {
    free(pixmap);
    vg_exit();
    return 1;
  }

  free(pixmap);

  if (wait_esc_break() != 0) {
    vg_exit();
    return 1;
  }

  if (vg_exit() != 0) {
    return 1;
  }

  return 0;
}