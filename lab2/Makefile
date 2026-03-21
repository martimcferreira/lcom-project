# Nome do programa final
PROG = lab2

# Ficheiros fonte APENAS do programa de teste
SRCS = lab2.c

# Flags de compilação
CFLAGS += -pedantic

# Configuração da Biblioteca
LIB_NAME = libtimer.a
LIB_SRCS = timer.c utils.c
LIB_OBJS = ${LIB_SRCS:.c=.o}

# Dependências e Linkagem
# O -L. diz para procurar bibliotecas na pasta atual
# O -ltimer diz para ligar o ficheiro libtimer.a
DPADD += ${LIBLCF} ${LIB_NAME}
LDADD += -llcf -L. -ltimer

# Limpeza: garante que o 'make clean' apaga a biblioteca e os .o
CLEANFILES += ${LIB_NAME} ${LIB_OBJS}

# Alvo principal: obriga a correr o alvo 'lib' antes do programa
all: lib

# Regra para criar a biblioteca (.a) a partir dos objetos (.o)
lib: ${LIB_OBJS}
	ar rcs ${LIB_NAME} ${LIB_OBJS}
# ^ ATENÇÃO: A linha acima TEM de começar com um TAB, não espaços!

# Inclui as regras "pesadas" do sistema LCOM
.include <minix.lcom.mk>