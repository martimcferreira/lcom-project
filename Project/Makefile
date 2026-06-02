PROG = proj

.PATH.c: structures structures/drivers devices/video devices/uart


SRCS = main.c game.c beatmap_loader.c bitwise.c kbc.c mouse.c rtc.c timer.c utils.c video.c uart.c menu.c leaderboard.c

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -I.
CFLAGS += -I./structures/includes
CFLAGS += -I./structures/includes/drivers
CFLAGS += -I./devices/video
CFLAGS += -I./devices/uart

DPADD += ${LIBLCF}
LDADD += -llcf

.BEGIN:
	@echo "A copiar beatmaps para /tmp/beatmaps..."
	@mkdir -p /tmp/beatmaps
	@cp -f beatmaps/* /tmp/beatmaps/ 2>/dev/null || true

.include <minix.lcom.mk>

