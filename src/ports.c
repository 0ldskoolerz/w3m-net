/* w3m-ports — conexiones y puertos abiertos estilo Trinux: netstat -tulpn
   y netstat -tn, refrescable, con resaltado de LISTEN. */

#include "ui.h"
#include "applet.h"

#define MAX_LINES 60
#define LINE_LEN 200
#define VISIBLE 24

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char mode = 'a';       /* a=todo, l=listen, c=conexiones */
static char status[128] = "Listo";

static void reload(void) {
    nlines = 0;
    char *out = NULL;
    if (mode == 'l')
        out = applet_run("netstat", (char *const[]){(char *)"-tulpn", NULL});
    else if (mode == 'c')
        out = applet_run("netstat", (char *const[]){(char *)"-tn", NULL});
    else
        out = applet_run("netstat", (char *const[]){(char *)"-tulpn", NULL});
    if (!out) {
        /* fallback: ss si netstat no está */
        out = applet_run("ss", (char *const[]){(char *)"-tulpn", NULL});
        if (!out) { snprintf(status, sizeof status, "netstat/ss no disponibles"); return; }
    }
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save))
        snprintf(lines[nlines++], LINE_LEN, "%s", tok);
    free(out);
    snprintf(status, sizeof status, "%d líneas (modo %c)", nlines, mode);
}

static void draw(UiCtx *u) {
    ui_clear(u);
    ui_button(u, 4, 4, 110, 24, "Puertos abiertos", mode == 'a');
    ui_button(u, 118, 4, 90, 24, "Conexiones", mode == 'c');
    ui_button(u, 212, 4, 90, 24, "Refrescar", false);
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 40;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    for (int i = first; i < nlines && i - first < VISIBLE; i++) {
        /* resalta líneas con LISTEN */
        bool is_listen = strstr(lines[i], "LISTEN") != NULL;
        XSetForeground(u->dpy, u->gc, is_listen ? u->c_accent : u->c_fg);
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i]));
        y += row_h;
    }
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 720, 480, "Puertos y conexiones — w3m-net");
    reload();
    draw(&u);
    XEvent ev;
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress && ev.xbutton.y >= 4 && ev.xbutton.y <= 28) {
            int x = ev.xbutton.x;
            if (x < 114) mode = 'a';
            else if (x < 208) mode = 'c';
            reload();
            draw(&u);
        }
        if (ev.type == KeyPress) {
            char kb[4]; KeySym ks;
            XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
            if (ks == XK_r || ks == XK_F5) { reload(); draw(&u); }
        }
    }
    ui_close(&u);
    return 0;
}
