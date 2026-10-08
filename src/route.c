/* w3m-route — análisis de ruteo: tabla completa (busybox route -n / ip
   route), gateway por defecto, métricas, y traza de ruta propia:
   verifica por dónde SALDRÁ un destino concreto antes de enviarlo. */

#include "ui.h"
#include "applet.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_LINES 40
#define LINE_LEN 220
#define VISIBLE 18

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char dest[128] = "";
static char gateway[64] = "";
static char iface[16] = "";
static char status[128] = "Tabla de rutas (route -n)";
static char match_line[200] = "";

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
}

/* parse "Destination  Gateway  Genmask  Flags  Metric  Ref  Use  Iface" */
static void parse_route(const char *line, char *dest_o, size_t dn,
                        char *gw_o, size_t gn, char *if_o, size_t in,
                        int *metric) {
    char d[64] = "", g[64] = "", gen[64] = "", fl[16] = "";
    int met = 0;
    /* busybox route -n: DEST GW GENMASK FLAGS METRIC REF USE IFACE */
    if (sscanf(line, "%63s %63s %63s %15s %d", d, g, gen, fl, &met) >= 4) {
        /* la iface es la última columna */
        const char *last = strrchr(line, ' ');
        char ifc[20] = "";
        if (last && sscanf(last, "%19s", ifc) == 1) {
            snprintf(if_o, in, "%s", ifc);
        }
        snprintf(dest_o, dn, "%s", d);
        snprintf(gw_o, gn, "%s", g);
        *metric = met;
    }
}

static void reload(void) {
    nlines = 0;
    gateway[0] = iface[0] = '\0';
    char *out = applet_run("route", (char *const[]){(char *)"-n", NULL});
    if (!out)
        out = applet_run("ip", (char *const[]){(char *)"route", (char *)"show", NULL});
    if (!out) { snprintf(status, sizeof status, "route/ip no disponibles"); return; }
    char *save = NULL;
    int first = 1;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save)) {
        push(tok);
        if (first) { first = 0; continue; }   /* header */
        if (strstr(tok, "UG") || strstr(tok, "default")) {
            char d[64], g[64], ifc[20];
            int met = 0;
            parse_route(tok, d, sizeof d, g, sizeof g, ifc, sizeof ifc, &met);
            /* ruta por defecto: 0.0.0.0 o "default" */
            if (!strcmp(d, "0.0.0.0") || !strcmp(d, "default")) {
                snprintf(gateway, sizeof gateway, "%s", g);
                snprintf(iface, sizeof iface, "%s", ifc);
            }
        }
    }
    free(out);
    snprintf(status, sizeof status, "%d rutas — default gw: %s (%s)",
              nlines - 1 > 0 ? nlines - 1 : 0,
              gateway[0] ? gateway : "?", iface[0] ? iface : "?");
}

/* ¿por qué interfaz y gateway saldría `dest`? Simula la búsqueda de
   ruta: la ruta más específica (máscara más larga) que la contiene. */
static void check_route(void) {
    if (!dest[0]) { snprintf(status, sizeof status, "escribe una IP destino"); return; }
    struct in_addr da;
    if (inet_aton(dest, &da) == 0) {
        snprintf(status, sizeof status, "IP inválida (usa IPv4 numérica)");
        match_line[0] = '\0';
        return;
    }
    unsigned int di = ntohl(da.s_addr);
    /* recorre las rutas parseadas buscando la mejor coincidencia */
    char best_line[LINE_LEN] = "";
    int best_bits = -1;
    for (int i = 1; i < nlines; i++) {
        char d[64], g[64], gen[64];
        char line_copy[LINE_LEN];
        snprintf(line_copy, sizeof line_copy, "%s", lines[i]);
        if (sscanf(line_copy, "%63s %63s %63s", d, g, gen) < 3) continue;
        if (!strcmp(d, "default")) {
            if (best_bits < 0) {
                snprintf(best_line, sizeof best_line, "%s", lines[i]);
                best_bits = 0;
            }
            continue;
        }
        struct in_addr na, nm;
        if (inet_aton(d, &na) == 0 || inet_aton(gen, &nm) == 0) continue;
        unsigned int net = ntohl(na.s_addr);
        unsigned int mask = ntohl(nm.s_addr);
        if ((di & mask) == (net & mask)) {
            int bits = __builtin_popcount(mask);
            if (bits > best_bits) {
                best_bits = bits;
                snprintf(best_line, sizeof best_line, "%s", lines[i]);
            }
        }
    }
    if (best_bits >= 0) {
        snprintf(match_line, sizeof match_line, "→ usará: %s", best_line);
        snprintf(status, sizeof status, "salida por %s%s",
                 strstr(best_line, "0.0.0.0") ? "default gw" : "ruta específica",
                 best_bits == 0 ? " (default)" : "");
    } else {
        snprintf(match_line, sizeof match_line, "→ sin ruta conocida (usaría default)");
        snprintf(status, sizeof status, "destino sin ruta específica");
    }
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* dest field + button */
    ui_bevel(u, 4, 4, u->w - 120, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 6, 6, (unsigned)u->w - 128, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 10, 20, dest, (int)strlen(dest));
    ui_button(u, u->w - 112, 4, 108, 24, "¿Por dónde?", false);
    /* match result */
    if (match_line[0]) {
        ui_bevel(u, 4, 32, u->w - 8, 22, true);
        XSetForeground(u->dpy, u->gc, u->c_accent);
        XDrawString(u->dpy, u->win, u->gc, 8, 47, match_line,
                    (int)strlen(match_line));
    }
    /* table */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 62;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    for (int i = first; i < nlines && i - first < VISIBLE; i++) {
        bool is_default = strstr(lines[i], "UG") || strstr(lines[i], "default");
        XSetForeground(u->dpy, u->gc, is_default ? u->c_accent : u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i]));
        y += row_h;
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 720, 440, "Ruteo — w3m-net");
    reload();
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress && ev.xbutton.y >= 4 && ev.xbutton.y <= 28) {
            if (ev.xbutton.x >= u.w - 112) check_route();
            draw(&u);
        }
        if (ev.type == KeyPress) {
            char kb[8]; KeySym ks;
            int n = XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
            size_t dl = strlen(dest);
            if (ks == XK_Return) { check_route(); draw(&u); }
            else if (ks == XK_F5) { reload(); draw(&u); }
            else if (ks == XK_BackSpace) { if (dl) dest[dl-1] = '\0'; draw(&u); }
            else if (n == 1 && kb[0] >= ' ' && kb[0] < 127 && dl < sizeof dest - 1)
                { dest[dl] = kb[0]; dest[dl+1] = '\0'; draw(&u); }
        }
    }
    ui_close(&u);
    return 0;
}
