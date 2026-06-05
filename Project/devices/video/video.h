/**
 * @file video.h
 * @brief Funções e estruturas para gestão da Placa Gráfica (Video Card).
 *
 * Utiliza o modo VBE (VESA BIOS Extensions) para alterar o modo de vídeo
 * e desenhar píxeis, retângulos, XPMs e sprites, com suporte a double buffering.
 * 
 * @defgroup VideoCard Video Card
 * @ingroup Devices
 * @brief Operações de desenho no ecrã (VBE).
 * @{
 */

#ifndef _LCOM_VIDEO_H_
#define _LCOM_VIDEO_H_

#include <lcom/lcf.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Inicializa o modo de vídeo especificado usando VBE.
 * @param mode Modo de vídeo (ex: 0x115).
 * @return Apontador para o início do VRAM (Video RAM).
 */
void *(vg_init)(uint16_t mode);

/**
 * @brief Desenha um píxel com uma determinada cor.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @param color Cor do píxel (RGB/Indexada dependendo do modo).
 * @return 0 em sucesso, 1 em erro.
 */
int(vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief Desenha uma linha horizontal.
 * @param x Coordenada X inicial.
 * @param y Coordenada Y.
 * @param len Comprimento da linha.
 * @param color Cor da linha.
 * @return 0 em sucesso, 1 em erro.
 */
int(vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color);

/**
 * @brief Desenha um retângulo preenchido.
 * @param x Coordenada X do canto superior esquerdo.
 * @param y Coordenada Y do canto superior esquerdo.
 * @param width Largura.
 * @param height Altura.
 * @param color Cor do preenchimento.
 * @return 0 em sucesso, 1 em erro.
 */
int(vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color);

/**
 * @brief Desenha um XPM (formato antigo/padrão) no ecrã.
 * @param xpm Mapa XPM.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @return 0 em sucesso.
 */
int(vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y);

/**
 * @brief Desenha um Pixmap (imagem pré-carregada) suportando transparência.
 * @param pixmap Array de píxeis da imagem.
 * @param width Largura da imagem.
 * @param height Altura da imagem.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @param transparent_color Cor considerada como transparente.
 * @param use_transparency 1 para ativar transparência, 0 para ignorar.
 * @return 0 em sucesso.
 */
int(vg_draw_xpm_image)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, int use_transparency);

/**
 * @brief Desenha um Pixmap aplicando um tint (filtro de cor).
 * @param pixmap Array de píxeis.
 * @param width Largura.
 * @param height Altura.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @param transparent_color Cor considerada transparente.
 * @param tint_color Cor de tint a sobrepor.
 * @return 0 em sucesso.
 */
int(vg_draw_xpm_image_tinted)(const uint32_t *pixmap, uint16_t width, uint16_t height, int x, int y, uint32_t transparent_color, uint32_t tint_color);

/**
 * @brief Limpa o buffer de desenho (back buffer) com uma cor sólida.
 * @param color Cor de fundo (geralmente preto: 0x000000).
 */
void(vg_clear_back_buffer)(uint32_t color);

/**
 * @brief Troca os buffers (Double Buffering).
 * Copia o conteúdo do back buffer para o front buffer (VRAM real).
 */
void(vg_swap_buffers)();

/**
 * @brief Liberta a memória alocada para os buffers.
 */
void(vg_free_buffers)();

/**
 * @brief Obtém a resolução horizontal.
 * @return Largura do ecrã em píxeis.
 */
uint16_t(vg_get_h_res)();

/**
 * @brief Obtém a resolução vertical.
 * @return Altura do ecrã em píxeis.
 */
uint16_t(vg_get_v_res)();

/**
 * @brief Desenha um sprite baseando-se no mapa de píxeis e struct xpm_image_t.
 * @param map Array de píxeis do sprite.
 * @param img Estrutura com metadata da imagem.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 */
void (vg_draw_sprite)(uint32_t *map, xpm_image_t img, int x, int y);

#endif /* _LCOM_VIDEO_H_ */
/** @} */
