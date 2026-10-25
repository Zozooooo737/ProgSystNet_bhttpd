CC     = gcc
CFLAGS = -Wall -Werror
PREFIX = $(HOME)/opt/bhttpd

all: bhttpd

bhttpd: src/bhttpd.c
	$(CC) $(CFLAGS) -o bhttpd src/bhttpd.c

install: bhttpd
	install -d $(PREFIX)/sbin $(PREFIX)/etc $(PREFIX)/srv $(PREFIX)/var/log
	install -m 755 bhttpd $(PREFIX)/sbin/bhttpd

clean:
	rm -f bhttpd

.PHONY: all install clean
