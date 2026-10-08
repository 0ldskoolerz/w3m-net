/* w3m-scan — inventario de tu LAN estilo Trinux: detecta el gateway
   (busybox route), escanea la subred /24 con nmap si está instalado
   (si no, hace un barrido ping básico con applets busybox).
   Uso legítimo: auditoría de TU propia red. */

#include "ui.h"
#include "applet.h"

#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MAX_HOSTS 256
#define LINE_LEN 160
#define VISIBLE 24

typedef struct HostEntry {
    char ip[20];
    char info[LINE_LEN];
    bool up;
} HostEntry;

static HostEntry hosts[MAX_HOSTS];
static int nhosts = 0;
static char subnet[32] = "";     /* ej: 192.168.1 */
static char status[128] = "Pulsa Escanear LAN";
static bool scanning = false;

static void detect_subnet(void) {
    char *out = applet_run("route", (char *const[]){(char *)"-n", NULL});
    if (!out) out = applet_run("ip", (char *const[]){(char *)"route", NULL});
    if (!out) { snprintf(subnet, sizeof subnet, "?"); return; }
    /* busca el gateway default: 0.0.0.0/0 o "default via X" */
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok;
         tok = strtok_r(NULL, "\n", &save)) {
        char gw[32] = "";
        /* busybox route -n: ... 0.0.0.0  GW  0.0.0.0  UG ... */
        if (strstr(tok, "UG") && sscanf(tok, "%*s %31s", gw) == 1 && strcmp(gw, "0.0.0.0") != 0) {
            /* subred = gateway sin último octeto */
            char *dot = strrchr(gw, '.');
            if (dot) {
                size_t n = (size_t)(dot - gw);
                snprintf(subnet, sizeof subnet, "%.*s", (int)n, gw);
                free(out);
                return;
            }
        }
        /* ip route: "default via 192.168.1.1 dev ..." */
        if (sscanf(tok, "default via %31s", gw) == 1) {
            char *dot = strrchr(gw, '.');
            if (dot) {
                size_t n = (size_t)(dot - gw);
                snprintf(subnet, sizeof subnet, "%.*s", (int)n, gw);
                free(out);
                return;
            }
        }
    }
    free(out);
    snprintf(subnet, sizeof subnet, "?");
}

static bool nmap_available(void) {
    char *out = applet_run1("which", "nmap");
    bool ok = out && out[0] && out[0] != '\n';
    free(out);
    return ok;
}

/* barrido ping básico con busybox (sin nmap): ping -c1 -W1 por host */
static void ping_sweep(void) {
    nhosts = 0;
    for (int i = 1; i <= 254 && nhosts < MAX_HOSTS; i++) {
        char ip[24];
        snprintf(ip, sizeof ip, "%s.%d", subnet, i);
        char *out = applet_run("ping", (char *const[]){(char *)"-c", (char *)"1",
                                                        (char *)"-W", (char *)"1",
                                                        (char *)ip, NULL});
        if (out && strstr(out, "1 received")) {
            snprintf(hosts[nhosts].ip, 20, "%s", ip);
            snprintf(hosts[nhosts].info, LINE_LEN, "up (ping)");
            hosts[nhosts].up = true;
            nhosts++;
        }
        free(out);
    }
}

/* con nmap: una sola pasada, salida formateada */
static void nmap_sweep(void) {
    char target[64];
    snprintf(target, sizeof target, "%s.0/24", subnet);
    char *out = applet_run("nmap", (char *const[]){(char *)"-sn", (char *)target, NULL});
    if (!out) { ping_sweep(); return; }
    nhosts = 0;
    char *save = NULL;
    char cur_ip[20] = "";
    for (char *tok = strtok_r(out, "\n", &save); tok && nhosts < MAX_HOSTS;
         tok = strtok_r(NULL, "\n", &save)) {
        char ip[24];
        if (sscanf(tok, "Nmap scan report for %23s", ip) == 1 ||
            sscanf(tok, "Nmap scan report for %*s (%23s", ip) == 1) {
            /* quita '(' si vino con formato hostname (ip) */
            char *par = strrchr(ip, ')');
            if (par) *par = '\0';
            snprintf(cur_ip, sizeof cur_ip, "%s", ip);
        } else if (strstr(tok, "MAC Address") && cur_ip[0]) {
            snprintf(hosts[nhosts].ip, 20, "%s", cur_ip);
            snprintf(hosts[nhosts].info, LINE_LEN, "%s", tok);
            hosts[nhosts].up = true;
            nhosts++;
            cur_ip[0] = '\0';
        }
    }
    free(out);
}

static void scan_lan(void) {
    if (scanning) return;
    scanning = true;
    snprintf(status, sizeof status, "detectando subred...");
    detect_subnet();
    if (subnet[0] == '?' || !subnet[0]) {
        snprintf(status, sizeof status, "no se pudo detectar la subred");
        scanning = false;
        return;
    }
    snprintf(status, sizeof status, "escaneando %s.0/24 ...", subnet);
    if (nmap_available()) nmap_sweep();
    else ping_sweep();
    snprintf(status, sizeof status, "%d hosts activos en %s.0/24", nhosts, subnet);
    scanning = false;
}

static void draw(UiCtx *u) {
    ui_clear(u);
    ui_button(u, 4, 4, 110, 24, "Escanear LAN", scanning);
    ui_text(u, 124, 20, status);
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 40;
    for (int i = 0; i < nhosts && i < VISIBLE; i++) {
        char line[200];
        snprintf(line, sizeof line, "%-16s %s", hosts[i].ip, hosts[i].info);
        XSetForeground(u->dpy, u->gc, u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y, line, (int)strlen(line));
        y += row_h;
    }
    if (nhosts == 0) {
        XSetForeground(u->dpy, u->gc, u->c_gray);
        XDrawString(u->dpy, u->win, u->gc, 8, 40,
                    "Sin resultados — pulsa Escanear LAN", 33);
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, subnet[0] ? subnet : "-");
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 640, 480, "Escáner de LAN — w3m-net");
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress && ev.xbutton.y >= 4 && ev.xbutton.y <= 28
            && ev.xbutton.x < 114) {
            scan_lan();
            draw(&u);
        }
    }
    ui_close(&u);
    return 0;
}
