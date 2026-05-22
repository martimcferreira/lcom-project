PROG = proj

# Adicionada a pasta devices/video aos caminhos de pesquisa
.PATH.c: structures structures/drivers devices/video

# Adicionado o video.c à lista de ficheiros fonte
SRCS = main.c game.c timer.c utils.c video.c

CFLAGS += -Wall -Wextra -pedantic

DPADD += ${LIBLCF}
LDADD += -llcf

.include <minix.lcom.mk>