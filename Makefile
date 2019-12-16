# export SOURCE_DATE_EPOCH := $(shell git show --no-patch --format=%ct HEAD)

CFLAGS  = -g -Wall -Werror -std=c99 -fPIC
LDFLAGS = -shared -ldl

.PHONY: demo demo-https demo-http clean

SRC    := override.c
TARGET := override.so

demo: demo-https demo-http

demo-https: override.so
	@printf '\x1b[32m[+] Trying out HTTPS traffic\x1b[39m\n'
	LD_PRELOAD=./override.so curl --verbose https://wikipedia.org
	@echo

demo-http: override.so
	@printf '\x1b[32m[+] Trying out HTTP traffic\x1b[39m\n'
	LD_PRELOAD=./override.so curl --verbose http://wikipedia.org
	@echo

%.so: %.o
	$(CC) $(LDFLAGS) $< -o $@

clean:
	rm -f $(TARGET)
