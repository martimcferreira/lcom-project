# Nome do executável final
PROG = lab3

# Ficheiros de código fonte (.c) que queres compilar
# Adiciona aqui todos os que criaste
SRCS = lab3.c kbc.c utils.c

# Flags de compilação
# -DLAB3 é obrigatório para a LCF saber que estás no Lab 3
# -I../lab2 diz ao compilador para procurar os headers (.h) na pasta do Lab 2
CFLAGS += -pedantic -DLAB3 -I../lab2

# Bibliotecas e Caminhos (Linker)
# -L../lab2 diz para procurar o ficheiro .a na pasta do Lab 2
# -ltimer liga o teu código à libtimer.a (O prefixo 'lib' e o '.a' são automáticos)
# -llcf é a biblioteca dos professores
DPADD += ${LIBLCF} ${LIBTIMER}
LDADD += -L../lab2 -ltimer -llcf

# Inclui as regras de compilação do sistema Minix
.include <minix.lcom.mk>