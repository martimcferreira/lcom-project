#ifndef _VIDEO_GR_H_
#define _VIDEO_GR_H_

#include <lcom/lcf.h>
#include <stdint.h>

int set_graphics_mode(uint16_t mode);
int map_vram(uint16_t mode);

int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color);
int vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color);
int vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color);

uint16_t get_hres(void);
uint16_t get_vres(void);
unsigned get_bytes_per_pixel(void);

#endif