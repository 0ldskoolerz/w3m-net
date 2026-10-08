/* w3m-ifaces — interfaces de red estilo Trinux: estado (busybox ifconfig
   -a / ip addr), DHCP (udhcpc), subir/bajar (ifconfig up/down), wifi
   (iwlist scan si existe). */

#include "ui.h"
#include "applet.h"

#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINES 40
#define LINE_LEN 200

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char sel_if[16] = "eth0";
static char status[128] = "Listo";

static void clear_lines(void) { nlines = 0; }

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
}

static void reload(void) {
    clear_lines();
    char *out = applet_run("ifconfig", (char *const[]){(char *)"-a", NULL});
    if (!out) {
        push("(ifconfig no disponible — probando ip addr)");
        out = applet_run("ip", (char *const[]){(char *)"addr", NULL});
    }
    if (!out) { push("(sin datos de red)"); return; }
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save))
        push(tok);
    free(out);
    snprintf(status, sizeof status, "%d líneas", nlines);
}

static void op_dhcp(void) {
    int st = applet_status("udhcpc",
        (char *const[]){(char *)"-i", (char *)sel_if, (char *)"-n", (char *)"-q", NULL});
    if (st == 0) snprintf(status, sizeof status, "DHCP OK en %s", sel_if);
    else snprintf(status, sizeof status, "DHCP falló (%d) en %s", st, sel_if);
    reload();
}

static void op_updown(bool up) {
    char *args[] = {(char *)sel_if, up ? (char *)"up" : (char *)"down", NULL};
    int st = applet_status("ifconfig", args);
    snprintf(status, sizeof status, "%s %s: %s", sel_if, up ? "up" : "down",
             st == 0 ? "OK" : "falló");
    reload();
}

static void op_wifi_scan(void) {
    clear_lines();
    char *out = applet_run("iwlist", (char *const[]){(char *)"scan", NULL});
    if (!out) { push("(iwlist no disponible — instala wireless_tools)"); return; }
    /* solo líneas con ESSID o calidad, para no inundar */
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save))
        if (strstr(tok, "ESSID") || strstr(tok, "Quality") || strstr(tok, "Cell"))
            push(tok);
    free(out);
    snprintf(status, sizeof status, "wifi: %d entradas", nlines);
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* selector de interfaz: los primeros 4 campos de las líneas que
       empiezan sin espacio (nombre de iface) */
    int bx = 4;
    const char *names[8]; int nn = 0;
    for (int i = 0; i < nlines && nn < 8; i++) {
        if (lines[i][0] && lines[i][0] != ' ' && lines[i][0] != '(') {
            char n[16];
            sscanf(lines[i], "%15[^:]", n);
            names[nn] = strdup(n); nn++;
        }
    }
    /* botones por iface */
    for (int i = 0; i < nn; i++) {
        ui_button(u, bx, 4, 84, 24, names[i], strcmp(names[i], sel_if) == 0);
        bx += 88;
    }
    ui_button(u, 4, 32, 70, 24, "DHCP", false);
    ui_button(u, 78, 32, 70, 24, "Up", false);
    ui_button(u, 152, 32, 70, 24, "Down", false);
    ui_button(u, 226, 32, 90, 24, "Escanear wifi", false);
    ui_button(u, 320, 32, 90, 24, "Refrescar", false);
    /* salida */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 66;
    for (int i = 0; i < nlines && i < MAX_LINES - 2; i++) {
        XSetForeground(u->dpy, u->gc, u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i]));
        y += row_h;
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
    for (int i = 0; i < nn; i++) free((void *)names[i]);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 720, 460, "Interfaces de red — w3m-net");
    reload();
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress) {
            int x = ev.xbutton.x, y = ev.xbutton.y;
            if (y >= 4 && y <= 28) {
                /* click en un botón de iface: extraer el nombre de la línea */
                int idx = (x - 4) / 88;
                int li = -1, seen = -1;
                for (int i = 0; i < nlines; i++)
                    if (lines[i][0] && lines[i][0] != ' ' && lines[i][0] != '(') {
                        seen++;
                        if (seen == idx) { li = i; break; }
                    }
                if (li >= 0) {
                    char n[16];
                    sscanf(lines[li], "%15[^:]", n);
                    snprintf(sel_if, sizeof sel_if, "%s", n);
                    draw(&u);
                }
            } else if (y >= 32 && y <= 56) {
                if (x < 74) op_dhcp();
                else if (x < 148) op_updown(true);
                else if (x < 222) op_updown(false);
                else if (x < 316) op_wifi_scan();
                else if (x < 410) reload();
                draw(&u);
            }
        }
    }
    ui_close(&u);
    return 0;
}
