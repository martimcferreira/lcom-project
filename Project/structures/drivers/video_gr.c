#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>

static uint8_t *video_mem;
static uint16_t h_res, v_res;
static uint8_t bits_per_pixel;

void *(vg_init)(uint16_t mode) {
    vbe_mode_info_t vmi; //estrutura para armazenar as informações sobre a resoluçao do modo gráfico e responde com o PhysBasePtr, que é o endereço físico da memória de vídeo

    if (vbe_get_mode_info(mode, &vmi) != 0) {
        printf("vg_init: vbe_get_mode_info() failed\n");
        return NULL;
    }

    h_res          = vmi.XResolution;
    v_res          = vmi.YResolution;
    bits_per_pixel = vmi.BitsPerPixel;

    struct minix_mem_range mr;
    unsigned int vram_size = h_res * v_res * ((bits_per_pixel + 7) / 8); //calcula o tamanho da memória de vídeo necessária para armazenar a imagem completa, multiplicando a resolução horizontal (h_res) pela resolução vertical (v_res) e pelo número de bytes por pixel
    //temos que somar +7 para arredondar para cima e nao perder informações,
    mr.mr_base  = vmi.PhysBasePtr; //morada fisica da memória de vídeo, que é o endereço físico onde a memória de vídeo está localizada. Essa informação é fornecida pela estrutura vmi, que foi preenchida pela função vbe_get_mode_info() com as informações do modo gráfico solicitado.
    mr.mr_limit = mr.mr_base + vram_size; //memmoria da placa gráfica onde termina, onde placa grafica comeca + comprimento do terreno, temos de ser especificos e dizer onde começa e onde acaba

    if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) { //pedimos permissao de administrador 
        printf("vg_init: sys_privctl failed\n");
        return NULL;
    }

    video_mem = vm_map_phys(SELF, (void *)vmi.PhysBasePtr, vram_size);
    if (video_mem == MAP_FAILED) {
        printf("vg_init: vm_map_phys failed\n"); 
        return NULL;
    }

    reg86_t r86;
    memset(&r86, 0, sizeof(r86));
    r86.intno = 0x10; //canal da placa de vídeo
    r86.ah    = 0x4F; //função para definir o modo gráfico
    r86.al    = 0x02; //modo de vídeo, 0x02 é o modo de vídeo com suporte a linear frame buffer, que permite acessar a memória de vídeo diretamente, sem precisar usar as funções de acesso à memória do BIOS. Isso é necessário para que possamos desenhar na tela usando o ponteiro video_mem que mapeamos para a memória de vídeo.
    r86.bx    = mode | BIT(14); //fazemos para usar o linear frame buffer
    //usamos linear frame buffer porque a placa de vídeo usa um sistema antigo de "bancos" de memória, desta forma é mais eficiente
    if (sys_int86(&r86) != OK || r86.ah != 0x00 || r86.al != 0x4F) {
        printf("vg_init: sys_int86() failed\n");
        return NULL;
    }

    return video_mem;
}

int (vg_draw_pixel)(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= h_res || y >= v_res)
        return 1;

    unsigned int bytes_per_pixel = (bits_per_pixel + 7) / 8;
    uint8_t *pixel = video_mem + (y * h_res + x) * bytes_per_pixel;


    for (unsigned int i = 0; i < bytes_per_pixel; i++) {
        pixel[i] = (color >> (8 * i)) & 0xFF;
    }

    return 0;
}

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
