# name of the program (Minix service)
PROG = lab5

# source code files to be compiled
SRCS = lac5.c video_gr.c utils.c

# additional compilation flags
CFLAGS += -pedantic -I../lab2

# list of library dependencies (for Lab 5, only LCF library)
DPADD += ${LIBLCF}
LDADD += -llcf

.include <minix.lcom.mk>
