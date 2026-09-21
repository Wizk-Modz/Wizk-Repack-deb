# Makefile - Biên dịch và cài đặt wizk-repack
CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Werror -O2
CPPFLAGS ?=
LDFLAGS  ?=
PREFIX   ?= /usr
DESTDIR  ?=

SRCS     = src/main.c src/config.c src/utils.c src/fs_ops.c src/dpkg.c src/payload.c
OBJS     = $(SRCS:.c=.o)
TARGET   = repack
INCLUDES = -Iinclude

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
