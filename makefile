CC=gcc
CFLAGS=-g -Wall
DEPS=defn.h
SRCS=expand.c builtin.c ush.c
OBJS=${SRCS:.c=.o}

ush: $(OBJS)
	$(CC) $(CFLAGS) -o ush $(OBJS)

$(OBJS): $(DEPS)

clean:
	rm ush $(OBJS)
