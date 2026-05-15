
#include <lcom/lcf.h>
#include "i8042.h"
#include <lcom/lab5.h>
#include "video_gr_extra.h"
#include <stdint.h>
#include <stdio.h>
#include "kbc.h"
#include "i8042.h"

extern uint8_t scancode_byte;   // definida no teu kbc.c
extern bool ih_error;
// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
    lcf_set_language("EN-US");
    lcf_trace_calls("/home/lcom/labs/lab5/trace.txt");
    lcf_log_output("/home/lcom/labs/lab5/output.txt");
    if (lcf_start(argc, argv))
        return 1;
    lcf_cleanup();
    return 0;
}
int(video_test_init)(uint16_t mode, uint8_t delay) {

    if (vg_init(mode) == NULL) { //usa a interrupção de 0x10 do BIOS para mudar a resolução e mapeia a memória física da placa gráfica para um ponteiro na RAM.
        printf("video_test_init: vg_init() failed\n");
        return 1;
    }

    
    tickdelay(micros_to_ticks((uint32_t)delay * 1000000)); //manter o ecrã no modo escolhido antes de sair

    if (vg_exit() != 0) return 1; //é necessária para restaurar o modo de texto através da função 0x03 do BIOS e para liberar os recursos alocados durante a inicialização gráfica. Se não for chamada, o sistema pode permanecer em um estado gráfico indesejado ou ter recursos alocados que não são mais necessários.

    return 0;
}

int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint32_t color) {

    if (vg_init(mode) == NULL) return 1; //muda paara o modo gráfico

    if (vg_draw_rectangle(x, y, width, height, color) != 0) return 1; //desenha um retângulo preenchido com a cor especificada, usando a função vg_draw_rectangle se a função retornar um valor diferente de 0, indica que houve um erro ao desenhar o retângulo.

    uint8_t kbc_irq_set;
    if (kbd_subscribe_int(&kbc_irq_set) != 0) return 1;

    int ipc_status;
    message msg; //variavel para enviar informações sobre o programa
    uint8_t kbc_irq_bit = BIT(kbc_irq_set);  
    bool esc_received = false;

    while (!esc_received) { //bloquear num ciclo while até o utilizador carregar na tecla esc
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue; //driver_receive é uma função que bloqueia até receber uma mensagem, e armazena a mensagem recebida na variável msg e o status da mensagem em ipc_status. Se a função retornar um valor diferente de 0, indica que houve um erro ao receber a mensagem, e o loop continua para tentar receber outra mensagem.

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_notify.interrupts & kbc_irq_bit) { //verificar se a interrupção recebida é a do teclado, usando a variável kbc_irq_bit que representa o bit correspondente à interrupção do teclado. Se a condição for verdadeira, significa que a interrupção do teclado foi recebida.
                kbc_ih();
                if (!ih_error && scancode_byte == ESC_BREAKCODE) //verifica se a leitura correu bem, verifica se o scancode lido é o código de da tecla ESC, se ambas as condições forem verdadeiras, a variável esc_received é definida como true, o que fará com que o loop while seja encerrado e o programa continue para a próxima etapa.
                    esc_received = true;
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) return 1; //libertar a interrupção do teclado, usando a função kbd_unsubscribe_int. Se a função retornar um valor diferente de 0, indica que houve um erro ao liberar a interrupção do teclado.
    if (vg_exit() != 0) return 1;
    return 0;
 }

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {

    if (vg_init(0x105) == NULL) return 1; //usamos 0x105 porque é o moodo de 8bits por píxel, cada píxel é apenas um número de 0 a 255, que aponta para uma tabela de cores.
    //imagem aparece desfeita se nao fosse 0x105
    if (vg_draw_xpm(xpm, x, y) != 0) return 1;

    uint8_t kbc_irq_set; //variável para armazenar o número do bit correspondente à interrupção do teclado, que será usada para verificar se a interrupção recebida é a do teclado.
    if (kbd_subscribe_int(&kbc_irq_set) != 0) return 1;

    int ipc_status;
    message msg;
    bool esc_received = false;

    while (!esc_received) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;

        if (is_ipc_notify(ipc_status)) {
            if (msg.m_notify.interrupts & BIT(kbc_irq_set)) {
                kbc_ih();
                if (!ih_error && scancode_byte == ESC_BREAKCODE)
                    esc_received = true;
            }
        }
    }

    if (kbd_unsubscribe_int() != 0) return 1;
    if (vg_exit() != 0) return 1;
    return 0;
}
