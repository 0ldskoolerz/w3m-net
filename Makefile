CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS ?=

X11_CFLAGS := $(shell pkg-config --cflags x11 2>/dev/null)
X11_LIBS   := $(shell pkg-config --libs x11 2>/dev/null || echo -lX11)

COMMON := src/ui.c src/applet.c
APPS   := w3m-ping w3m-ifaces w3m-ports w3m-scan

.PHONY: all clean

all: $(APPS)

w3m-ping: src/ping.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/ping.c $(COMMON) $(X11_LIBS)

w3m-ifaces: src/ifaces.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/ifaces.c $(COMMON) $(X11_LIBS)

w3m-ports: src/ports.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/ports.c $(COMMON) $(X11_LIBS)

w3m-scan: src/scan.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/scan.c $(COMMON) $(X11_LIBS)

clean:
	rm -f $(APPS)
