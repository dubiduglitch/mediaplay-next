# mediaplay-next Makefile
# Use `make` for a host syntax check; use Docker (build.sh) for the real armhf binary.

CC      ?= gcc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra
SRCS    = src/player.c src/ui.c src/music.c src/video.c src/browser.c \
          src/settings.c src/config.c src/input.c
OBJS    = $(SRCS:.c=.o)
TARGET  = mediaplay

.PHONY: all clean syntax docker

all: syntax

syntax:
	@$(CC) -fsyntax-only $(CFLAGS) $(SRCS)

clean:
	rm -f $(OBJS) $(TARGET)

docker:
	docker buildx build --platform linux/arm/v7 --target out --output type=local,dest=dist .
