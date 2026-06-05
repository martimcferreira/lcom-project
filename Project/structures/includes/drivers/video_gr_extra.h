/**
 * @file video_gr_extra.h
 * @brief Funções gráficas extra.
 * 
 * @defgroup VideoExtra Video Card Extra
 * @ingroup Devices
 * @brief Funções gráficas não usadas ou redundantes.
 * @{
 */

#ifndef _VIDEO_GR_EXTRA_H_
#define _VIDEO_GR_EXTRA_H_

#include <lcom/lcf.h>
#include <stdint.h>

/**
 * @brief (Redundante) Desenha um píxel.
 */
int (vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief (Redundante) Desenha um XPM padrão.
 */
int (vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y);

#endif
/** @} */
