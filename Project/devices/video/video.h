#ifndef _LCOM_VIDEO_H_
#define _LCOM_VIDEO_H_

#include <lcom/lcf.h>
#include <stdbool.h>
#include <stdint.h>

void *(vg_init)(uint16_t mode);
int(vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color);
int(vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color);
int(vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color);
int(vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y);
int(vg_draw_xpm_image)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, int use_transparency);
int(vg_draw_xpm_image_tinted)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, uint32_t tint_color);
void(vg_clear_back_buffer)(uint32_t color);
void(vg_swap_buffers)();
void(vg_free_buffers)();
uint16_t(vg_get_h_res)();
uint16_t(vg_get_v_res)();
void (vg_draw_sprite)(uint32_t *map, xpm_image_t img, int x, int y);

#endif /* _LCOM_VIDEO_H_ */
