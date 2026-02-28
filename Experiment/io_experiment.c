#include <minix/syslib.h>
#include <stdio.h>

int main() {
    sef_startup(); // Importante: regista o programa como serviço
    freopen("/dev/console", "w", stdout); // Redireciona o texto para a consola

    int ret = sys_outb(0x70, 0xA); // Tenta escrever no RTC (Real-Time Clock)
    printf("sys_outb returned: %d\n", ret);
    return 0;
}