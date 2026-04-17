# name of the program (Minix service)
PROG = lab3

# source code files to be compiled
SRCS = lab3.c kbc.c ../lab2/timer.c utils.c

# additional compilation flags
CFLAGS += -pedantic -DLAB3

# list of library dependencies
DPADD += ${LIBLCF}
LDADD += -llcf

# include LCOM's makefile that does all the heavy lifting
.include <minix.lcom.mk>
