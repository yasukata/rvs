PROGS = a.out

CD := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CLEANFILES = $(PROGS) *.o

CFLAGS += -O3 -pipe -g -rdynamic
CFLAGS += -Werror -Wextra -Wall
CFLAGS += -I$(CD)../../include

LDFLAGS += -lpthread

RVS_CFLAGS += -O3 -pipe -g -rdynamic
RVS_CFLAGS += -Werror -Wextra -Wall
RVS_CFLAGS += -std=c89 -nostdlib -nostdinc
RVS_CFLAGS += -I$(CD)../../include

RVS_LDFLAGS +=

C_SRCS = main.c

C_OBJS = $(C_SRCS:.c=.o) rvs.o

OBJS = $(C_OBJS)

.PHONY: all
all: $(PROGS)

rvs.o: ../../rvs.c
	$(CC) $(RVS_CFLAGS) -c -o $@ $^ $(RVS_LDFLAGS)

$(PROGS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	-@rm -rf $(CLEANFILES)
