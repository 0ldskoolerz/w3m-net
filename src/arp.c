/* w3m-arp — tabla ARP y vecinos de la red (net-tools arp / ip neigh):
   quién está en la caché ARP con su MAC, y detección de conflictos de
   IP (misma IP, MACs distintas). */

#include "ui.h"
#include "applet.h"

#define MAX_ENTRIES 64
#define LINE_LEN 220
#define VISIBLE 20

typedef struct ArpEntry {
    char ip[24];
    char mac[24];
    char dev[16];
    bool complete;
} ArpEntry;

static ArpEntry entries[MAX_ENTRIES];
static int nentries = 0;
static char status[128] = "Pulsa Refrescar";
static int conflict_idx[8];
static int nconflicts = 0;

static void reload(void) {
    nentries = 0;
    nconflicts = 0;
    /* net-tools arp -n; fallback: ip neigh */
    char *out = applet_run("arp", (char *const[]){(char *)"-n", NULL});
    bool used_ip = false;
    if (!out) {
        out = applet_run("ip", (char *const[]){(char *)"neigh", NULL});
        used_ip = true;
    }
    if (!out) { snprintf(status, sizeof status, "arp/ip no disponibles"); return; }
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nentries < MAX_ENTRIES;
         tok = strtok_r(NULL, "\n", &save)) {
        ArpEntry e = {0};
        if (used_ip) {
            /* formato ip neigh: "192.168.1.1 dev eth0 lladdr aa:bb:.. REACHABLE" */
            char ll[24] = "", flags[32] = "";
            if (sscanf(tok, "%23s dev %15s lladdr %23s %31s",
                       e.ip, e.dev, ll, flags) >= 3) {
                snprintf(e.mac, sizeof e.mac, "%s", ll);
                e.complete = strcmp(ll, "(incomplete)") != 0 && ll[0] != '\0';
                if (e.complete || strstr(tok, "FAILED")) {
                    if (e.complete)
                        entries[nentries++] = e;
                }
                continue;
            }
        } else {
            /* formato arp -n: "Address HWtype HWaddress Flags Mask Iface" */
            char hwt[16] = "", fl[16] = "";
            if (sscanf(tok, "%23s %15s %23s %15s", e.ip, hwt, e.mac, fl) >= 3) {
                const char *last = strrchr(tok, ' ');
                char ifc[20] = "";
                if (last && sscanf(last, "%19s", ifc) == 1)
                    snprintf(e.dev, sizeof e.dev, "%s", ifc);
                e.complete = strcmp(e.mac, "(incomplete)") != 0;
                /* salta el header */
                if (strcmp(e.ip, "Address") == 0) continue;
                entries[nentries++] = e;
            }
        }
    }
    free(out);

    /* detección de conflictos: misma IP con distinta MAC */
    for (int i = 0; i < nentries; i++)
        for (int j = i + 1; j < nentries; j++)
            if (!strcmp(entries[i].ip, entries[j].ip) &&
                strcmp(entries[i].mac, entries[j].mac) != 0) {
                if (nconflicts < 8) conflict_idx[nconflicts++] = i;
                if (nconflicts < 8) conflict_idx[nconflicts++] = j;
            }

    snprintf(status, sizeof status, "%d entradas ARP%s", nentries,
             nconflicts > 0 ? " — ¡CONFLICTO de IP detectado!" : "");
}

static void draw(UiCtx *u) {
    ui_clear(u);
    ui_button(u, 4, 4, 90, 24, "Refrescar", false);
    ui_button(u, 98, 4, 90, 24, "Hacer ping a todas", false);
    /* header */
    XSetForeground(u->dpy, u->gc, u->c_fg);
    const char *hdr = "IP               MAC                  Iface";
    XDrawString(u->dpy, u->win, u->gc, 8, 40, hdr, (int)strlen(hdr));
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 54;
    for (int i = 0; i < nentries && i < VISIBLE; i++) {
        char line[LINE_LEN];
        snprintf(line, sizeof line, "%-16s %-20s %s",
                 entries[i].ip, entries[i].mac, entries[i].dev);
        bool conflicted = false;
        for (int k = 0; k < nconflicts; k++)
            if (conflict_idx[k] == i) conflicted = true;
        XSetForeground(u->dpy, u->gc, conflicted ? u->c_accent : u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y, line, (int)strlen(line));
        y += row_h;
    }
    if (nentries == 0) {
        XSetForeground(u->dpy, u->gc, u->c_gray);
        XDrawString(u->dpy, u->win, u->gc, 8, 54, "sin entradas", 12);
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

/* pings rápidos a todas las IPs de la caché para refrescar entradas */
static void ping_all(void) {
    snprintf(status, sizeof status, "refrescando con ping...");
    for (int i = 0; i < nentries; i++) {
        char *out = applet_run("ping", (char *const[]){(char *)"-c", (char *)"1",
                                                        (char *)"-W", (char *)"1",
                                                        (char *)entries[i].ip, NULL});
        free(out);
    }
    reload();
}

int main(void) {
    UiCtx u;
    ui_init(&u, 620, 420, "Tabla ARP — w3m-net");
    reload();
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress && ev.xbutton.y >= 4 && ev.xbutton.y <= 28) {
            if (ev.xbutton.x < 94) reload();
            else if (ev.xbutton.x < 188) ping_all();
            draw(&u);
        }
        if (ev.type == KeyPress) {
            char kb[4]; KeySym ks;
            XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
            if (ks == XK_F5) { reload(); draw(&u); }
        }
    }
    ui_close(&u);
    return 0;
}
