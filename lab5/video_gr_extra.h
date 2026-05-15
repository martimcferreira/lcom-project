#ifndef _VIDEO_GR_EXTRA_H_
#define _VIDEO_GR_EXTRA_H_

#include <lcom/lcf.h>
#include <stdint.h>

int (vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color);
int (vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y);

#endif
