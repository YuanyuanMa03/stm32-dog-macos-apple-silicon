CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all test clean

all: build/su03t-flash

build:
	mkdir -p build

build/su03t-flash: tools/su03t-flash.c tools/su03t_xmodem.c tools/su03t_xmodem.h | build
	$(CC) $(CFLAGS) tools/su03t-flash.c tools/su03t_xmodem.c -o $@

build/test-su03t: tests/test_su03t.c tools/su03t_xmodem.c tools/su03t_xmodem.h | build
	$(CC) $(CFLAGS) tests/test_su03t.c tools/su03t_xmodem.c -o $@

test: build/test-su03t
	./build/test-su03t

clean:
	rm -f build/su03t-flash build/test-su03t
	rmdir build 2>/dev/null || true
