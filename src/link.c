/* w3m-link — estado físico del enlace por interfaz (net-tools mii-tool
   + ethtool si existen + /sys/class/net): velocidad, dúplex, portador
   (¿hay cable/reds?), estadísticas de errores. */

#include "ui.h"
#include "applet.h"

#include <dirent.h>

#define MAX_LINES 40
#define LINE_LEN 200

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char status[128] = "Estado del enlace (mii-tool/sysfs)";

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
}

static bool carrier(const char *ifname) {
    char path[128];
    snprintf(path, sizeof path, "/sys/class/net/%s/carrier", ifname);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    int c = fgetc(f);
    fclose(f);
    return c == '1';
}

static void reload(void) {
    nlines = 0;
    /* lista de interfaces desde sysfs (sin deps) */
    DIR *d = opendir("/sys/class/net");
    if (!d) { push("(sin /sys/class/net)"); return; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL && nlines < MAX_LINES - 2) {
        if (de->d_name[0] == '.') continue;
        const char *ifn = de->d_name;
        char path[192];
        char line[LINE_LEN];

        /* carrier */
        bool car = carrier(ifn);
        /* speed */
        int speed = -1;
        snprintf(path, sizeof path, "/sys/class/net/%s/speed", ifn);
        FILE *f = fopen(path, "r");
        if (f) { if (fscanf(f, "%d", &speed) != 1) speed = -1; fclose(f); }
        /* duplex */
        char duplex[24] = "?";
        snprintf(path, sizeof path, "/sys/class/net/%s/duplex", ifn);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%23s", duplex) != 1) duplex[0]='?'; fclose(f); }
        /* stats: rx/tx errors */
        unsigned long rx_err = 0, tx_err = 0, rx_drp = 0;
        snprintf(path, sizeof path, "/sys/class/net/%s/statistics/rx_errors", ifn);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%lu", &rx_err) != 1) rx_err = 0; fclose(f); }
        snprintf(path, sizeof path, "/sys/class/net/%s/statistics/tx_errors", ifn);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%lu", &tx_err) != 1) tx_err = 0; fclose(f); }
        snprintf(path, sizeof path, "/sys/class/net/%s/statistics/rx_dropped", ifn);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%lu", &rx_drp) != 1) rx_drp = 0; fclose(f); }

        char spd[16];
        if (speed > 0) snprintf(spd, sizeof spd, "%d", speed);
        else snprintf(spd, sizeof spd, "?");
        snprintf(line, sizeof line, "%-10s %-8s %5s Mbps %-7s err: rx=%lu tx=%lu drop=%lu",
                 ifn, car ? "CARRIER" : "no-link", spd, duplex, rx_err, tx_err, rx_drp);
        push(line);
    }
    closedir(d);

    /* mii-tool como complemento (negocia/detalla PHY) si existe */
    char *out = applet_run1("mii-tool", "");
    if (out && out[0]) {
        push("--- mii-tool ---");
        char *save = NULL;
        for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
             tok = strtok_r(NULL, "\n", &save))
            push(tok);
        free(out);
    }
    snprintf(status, sizeof status, "%d interfaces (sysfs + mii-tool)", nlines);
}

static void draw(UiCtx *u) {
    ui_clear(u);
    ui_button(u, 4, 4, 90, 24, "Refrescar", false);
    int row_h = u->font->ascent + u->font->descent + 2;
    int y = 40;
    for (int i = 0; i < nlines; i++) {
        XSetForeground(u->dpy, u->gc, strstr(lines[i], "CARRIER") ? u->c_fg : u->c_gray);
        if (strncmp(lines[i], "---", 3) == 0)
            XSetForeground(u->dpy, u->gc, u->c_accent);
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i]));
        y += row_h;
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 680, 400, "Estado del enlace — w3m-net");
    reload();
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress && ev.xbutton.y >= 4 && ev.xbutton.y <= 28
            && ev.xbutton.x < 94) { reload(); draw(&u); }
        if (ev.type == KeyPress) {
            char kb[4]; KeySym ks;
            XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
            if (ks == XK_F5) { reload(); draw(&u); }
        }
    }
    ui_close(&u);
    return 0;
}
