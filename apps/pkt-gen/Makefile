PROGS = a.out

CD := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CLEANFILES = $(PROGS) *.o

CFLAGS += -O3 -pipe -g -rdynamic
CFLAGS += -Werror -Wextra -Wall
CFLAGS += -I$(CD)../..//include

LDFLAGS += -lpthread

C_SRCS = main.c

C_OBJS = $(C_SRCS:.c=.o)

OBJS = $(C_OBJS)

.PHONY: all
all: $(PROGS)

$(PROGS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	-@rm -rf $(CLEANFILES)
