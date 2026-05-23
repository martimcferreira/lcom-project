PROG = proj

.PATH.c: structures structures/drivers devices/video

SRCS = main.c game.c timer.c utils.c video.c mouse.c kbc.c _rtc.c

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -I./structures/includes
CFLAGS += -I./structures/includes/drivers
CFLAGS += -I./devices/video

DPADD += ${LIBLCF}
LDADD += -llcf

.include <minix.lcom.mk>

