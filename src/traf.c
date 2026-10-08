/* w3m-traf — monitor de tráfico LAN en tiempo real estilo iptraf/ntop:
   lee /proc/net/dev cada segundo y dibuja tasas RX/TX por interfaz
   con barras de ancho de banda. Sin dependencias: /proc puro. */

#include "ui.h"

#include <unistd.h>
#include <time.h>

#define IFACES_MAX 6
#define HISTORY 120

typedef struct IfTraf {
    char name[16];
    unsigned long long rx_bytes, tx_bytes;
    unsigned long long rx_prev, tx_prev;
    unsigned rx_hist[HISTORY], tx_hist[HISTORY];
    int hist_n;
} IfTraf;

static IfTraf ifs[IFACES_MAX];
static int nifs = 0;
static char status[128] = "Leyendo /proc/net/dev...";

static void read_proc(void) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) { snprintf(status, sizeof status, "sin /proc/net/dev"); return; }
    char line[256];
    fgets(line, sizeof line, f);   /* headers */
    fgets(line, sizeof line, f);
    nifs = 0;
    while (fgets(line, sizeof line, f) && nifs < IFACES_MAX) {
        char name[16];
        unsigned long long rx, tx;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = ' ';
        if (sscanf(line, "%15s %llu %*s %*s %*s %*s %*s %*s %*s %llu",
                   name, &rx, &tx) >= 3) {
            IfTraf *it = NULL;
            for (int i = 0; i < nifs; i++)
                if (!strcmp(ifs[i].name, name)) { it = &ifs[i]; break; }
            if (!it && nifs < IFACES_MAX) {
                it = &ifs[nifs++];
                memset(it, 0, sizeof *it);
                snprintf(it->name, sizeof it->name, "%s", name);
                it->rx_prev = rx; it->tx_prev = tx;   /* primera lectura: base */
            }
            if (it) {
                it->rx_bytes = rx; it->tx_bytes = tx;
                unsigned rx_rate = (unsigned)(rx - it->rx_prev);
                unsigned tx_rate = (unsigned)(tx - it->tx_prev);
                it->rx_prev = rx; it->tx_prev = tx;
                if (it->hist_n < HISTORY) {
                    it->rx_hist[it->hist_n] = rx_rate;
                    it->tx_hist[it->hist_n] = tx_rate;
                    it->hist_n++;
                } else {
                    memmove(it->rx_hist, it->rx_hist + 1, (HISTORY-1)*sizeof(unsigned));
                    memmove(it->tx_hist, it->tx_hist + 1, (HISTORY-1)*sizeof(unsigned));
                    it->rx_hist[HISTORY-1] = rx_rate;
                    it->tx_hist[HISTORY-1] = tx_rate;
                }
            }
        }
    }
    fclose(f);
    snprintf(status, sizeof status, "%d interfaces — actualizando cada 1s", nifs);
}

static void human(unsigned long long bytes, char *out, size_t n) {
    if (bytes > 1024ULL*1024*1024) snprintf(out, n, "%.1fG", bytes/1073741824.0);
    else if (bytes > 1024*1024) snprintf(out, n, "%.1fM", bytes/1048576.0);
    else if (bytes > 1024) snprintf(out, n, "%.1fK", bytes/1024.0);
    else snprintf(out, n, "%lluB", bytes);
}

static void draw(UiCtx *u) {
    ui_clear(u);
    int row_h = (u->h - 40) / (nifs > 0 ? nifs : 1);
    for (int i = 0; i < nifs; i++) {
        IfTraf *it = &ifs[i];
        int y = 30 + i * row_h;
        /* name + totals */
        char rx_s[24], tx_s[24];
        human(it->rx_bytes, rx_s, sizeof rx_s);
        human(it->tx_bytes, tx_s, sizeof tx_s);
        char line[96];
        snprintf(line, sizeof line, "%-8s RX:%-8s TX:%-8s", it->name, rx_s, tx_s);
        XSetForeground(u->dpy, u->gc, u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y + 14, line, (int)strlen(line));
        /* sparkline: últimas tasas */
        int gw = u->w - 220;
        unsigned peak = 1;
        for (int k = 0; k < it->hist_n; k++) {
            if (it->rx_hist[k] > peak) peak = it->rx_hist[k];
            if (it->tx_hist[k] > peak) peak = it->tx_hist[k];
        }
        int gh = row_h - 24;
        if (gh > 4) {
            for (int k = 0; k < it->hist_n; k++) {
                int x = 200 + (k * gw) / HISTORY;
                int rh = (int)(it->rx_hist[k] * (unsigned)gh / peak);
                int th = (int)(it->tx_hist[k] * (unsigned)gh / peak);
                XSetForeground(u->dpy, u->gc, u->c_accent);
                XDrawLine(u->dpy, u->win, u->gc, x, y + 16, x, y + 16 + rh);
                XSetForeground(u->dpy, u->gc, u->c_gray);
                XDrawLine(u->dpy, u->win, u->gc, x, y + 16 + gh, x, y + 16 + gh - th);
            }
        }
        /* leyenda */
        XSetForeground(u->dpy, u->gc, u->c_accent);
        XDrawString(u->dpy, u->win, u->gc, 200, y + 14, "RX", 2);
        XSetForeground(u->dpy, u->gc, u->c_gray);
        XDrawString(u->dpy, u->win, u->gc, 222, y + 14, "TX", 2);
    }
    if (nifs == 0) {
        XSetForeground(u->dpy, u->gc, u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, 30, "sin interfaces", 14);
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 700, 380, "Monitor de tráfico — w3m-net");
    read_proc();
    draw(&u);
    XEvent ev;
    time_t last = time(NULL);
    for (;;) {
        while (XPending(u.dpy)) {
            XNextEvent(u.dpy, &ev);
            if (ev.type == ClientMessage) { ui_close(&u); return 0; }
            if (ev.type == Expose) draw(&u);
        }
        if (time(NULL) != last) {
            last = time(NULL);
            read_proc();
            draw(&u);
        }
        usleep(100000);
    }
}
