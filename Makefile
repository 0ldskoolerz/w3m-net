CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS ?=

X11_CFLAGS := $(shell pkg-config --cflags x11 2>/dev/null)
X11_LIBS   := $(shell pkg-config --libs x11 2>/dev/null || echo -lX11)

COMMON := src/ui.c src/applet.c
APPS   := w3m-ping w3m-ifaces w3m-ports w3m-scan w3m-sniff w3m-traf w3m-nc w3m-dns w3m-route w3m-arp w3m-link

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

w3m-sniff: src/sniff.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/sniff.c $(COMMON) $(X11_LIBS)

w3m-traf: src/traf.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/traf.c $(COMMON) $(X11_LIBS)

w3m-nc: src/nc.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/nc.c $(COMMON) $(X11_LIBS)

w3m-dns: src/dns.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/dns.c $(COMMON) $(X11_LIBS)

w3m-route: src/route.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/route.c $(COMMON) $(X11_LIBS)

w3m-arp: src/arp.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/arp.c $(COMMON) $(X11_LIBS)

w3m-link: src/link.c $(COMMON) src/ui.h src/applet.h
	$(CC) $(CFLAGS) $(X11_CFLAGS) -o $@ src/link.c $(COMMON) $(X11_LIBS)

clean:
	rm -f $(APPS)
