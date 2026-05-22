PROG = proj

.PATH.c: structures structures/drivers devices/video

SRCS = main.c \
       game.c \
       bitwise.c \
       kbc.c \
       mouse.c \
       rtc.c \
       timer.c \
       utils.c \
       video.c

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -I./structures/includes
CFLAGS += -I./structures/includes/drivers
CFLAGS += -I./devices/video

DPADD += ${LIBLCF}
LDADD += -llcf

.include <minix.lcom.mk>
