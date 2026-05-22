#include <lcom/lcf.h>
#include "video.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // NOVO: Necessário para o malloc()
#include <string.h> // NOVO: Necessário para o memcpy() e memset()

static uint8_t *video_mem;
static uint8_t *back_buffer; // NOVO: O nosso palco falso onde vamos desenhar
static unsigned int vram_size_global; // NOVO: Variável global para guardar o tamanho da memória para o memcpy

static uint16_t h_res, v_res;
static uint8_t bits_per_pixel;

void *(vg_init)(uint16_t mode) {
    vbe_mode_info_t vmi; 

    if (vbe_get_mode_info(mode, &vmi) != 0) {
        printf("vg_init: vbe_get_mode_info() failed\n");
        return NULL;
    }

    h_res          = vmi.XResolution;
    v_res          = vmi.YResolution;
    bits_per_pixel = vmi.BitsPerPixel;

    struct minix_mem_range mr;
    
    // Guardamos o tamanho da VRAM na variável global para a função de swap
    vram_size_global = h_res * v_res * ((bits_per_pixel + 7) / 8); 
    
    mr.mr_base  = vmi.PhysBasePtr; 
    mr.mr_limit = mr.mr_base + vram_size_global; 

    if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {  
        printf("vg_init: sys_privctl failed\n");
        return NULL;
    }

    video_mem = vm_map_phys(SELF, (void *)vmi.PhysBasePtr, vram_size_global);
    if (video_mem == MAP_FAILED) {
        printf("vg_init: vm_map_phys failed\n"); 
        return NULL;
    }

    // NOVO: Alocar a memória dinâmica para o nosso back buffer
    // Tem de ter exatamente o mesmo tamanho da VRAM real
    back_buffer = (uint8_t *) malloc(vram_size_global);
    if (back_buffer == NULL) {
        printf("vg_init: malloc falhou a alocar o back_buffer\n");
        return NULL;
    }

    reg86_t r86;
    memset(&r86, 0, sizeof(r86));
    r86.intno = 0x10; 
    r86.ah    = 0x4F; 
    r86.al    = 0x02; 
    r86.bx    = mode | BIT(14); 
    
    if (sys_int86(&r86) != OK || r86.ah != 0x00 || r86.al != 0x4F) {
        printf("vg_init: sys_int86() failed\n");
        free(back_buffer); // Boa prática: libertar a memória se houver erro
        return NULL;
    }

    return video_mem;
}

int (vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= h_res || y >= v_res)
        return 1;

    unsigned int bytes_per_pixel = (bits_per_pixel + 7) / 8;
    
    // ALTERAÇÃO VITAL: Em vez de apontar para video_mem, apontamos para back_buffer!
    // A partir de agora, o ecrã não atualiza em tempo real quando chamas esta função.
    uint8_t *pixel = back_buffer + (y * h_res + x) * bytes_per_pixel;

    for (unsigned int i = 0; i < bytes_per_pixel; i++) {
        pixel[i] = (color >> (8 * i)) & 0xFF;
    }

    return 0;
}

// Repara na magia: Como as funções abaixo chamam a TUA vg_draw_pixel, 
// não precisamos de alterar mais nada nelas! Elas já estão a desenhar no back_buffer indiretamente.

int (vg_draw_hline)(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
    for (uint16_t i = 0; i < len; i++) {
        if (vg_draw_pixel(x + i, y, color) != 0)
            return 1;
    }
    return 0;
}

int (vg_draw_rectangle)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
    for (uint16_t row = 0; row < height; row++) {
        if (vg_draw_hline(x, y + row, width, color) != 0)
            return 1;
    }
    return 0;
}

int (vg_draw_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
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

// NOVA FUNÇÃO: A Cortina Abre!
// Copia tudo o que preparámos nos bastidores para a placa gráfica num só instante.
void (vg_swap_buffers)() {
    memcpy(video_mem, back_buffer, vram_size_global);
}
