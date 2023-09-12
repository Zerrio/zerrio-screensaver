#!/usr/bin/make -f

TARGET=zerrio-screensaver
CC = gcc
INC = -I src/
CFLAGS += -g -fpie -pie -D_GNU_SOURCE
# CFLAGS += -g -DSPM_DEBUG -DDEVICE_PROTECT_DEBUG

DAEMON_SRC=$(wildcard src/*.c)
DAEMON_OBJS=$(patsubst %.c,%.o,$(DAEMON_SRC))
OBJS=$(DAEMON_OBJS)

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) -g -pie -fpie -o $@ $^

$(OBJS):%.o:%.c
	$(CC) -c $(CFLAGS)  $(INC) $< -o $@

install:
	mkdir -pv ${DESTDIR}/opt/apps/zerrio-screensaver/
	mkdir -pv ${DESTDIR}/opt/apps/zerrio-screensaver/modules/
	mkdir -pv ${DESTDIR}/opt/apps/zerrio-screensaver/conf
	install -v misc/screen_protect.conf ${DESTDIR}/opt/apps/zerrio-screensaver/conf
	install -v $(TARGET) ${DESTDIR}/opt/apps/zerrio-screensaver
	install -v modules/* ${DESTDIR}/opt/apps/zerrio-screensaver/modules

clean:
	rm -f ${OBJS} $(TARGET)
