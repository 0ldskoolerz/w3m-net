/* w3m-dns — análisis DNS: consultas de registros (A/AAAA/CNAME/MX/NS/PTR)
   contra tu resolver o uno específico (8.8.8.8, 1.1.1.1...).
   Motor: dig si está disponible; fallback a busybox nslookup. */

#include "ui.h"
#include "applet.h"

#define MAX_LINES 60
#define LINE_LEN 200
#define VISIBLE 22

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char qname[200] = "";
static char server[64] = "";      /* vacío = resolver del sistema */
static char qtype[8] = "A";
static char status[128] = "dominio + tipo + Consultar";

static void clear_out(void) { nlines = 0; }

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
}

/* dig: dig [@server] name TYPE +noall +answer  (o -x IP para PTR) */
static bool dig_available(void) {
    char *out = applet_run1("which", "dig");
    bool ok = out && out[0] && out[0] != '\n';
    free(out);
    return ok;
}

static void query_dig(void) {
    char at[80] = "";
    if (server[0]) snprintf(at, sizeof at, "@%s", server);
    char *args[8];
    int n = 0;
    args[n++] = (char *)at;
    if (!strcmp(qtype, "PTR")) {
        /* reverse: dig -x IP */
        args[0] = (char *)"-x";
        args[n++] = qname;
    } else {
        args[n++] = qname;
        args[n++] = qtype;
    }
    args[n++] = (char *)"+noall";
    args[n++] = (char *)"+answer";
    args[n] = NULL;
    /* sin server: primer arg vacío molesta a dig; reconstruimos */
    char *clean[8];
    int m = 0;
    for (int i = 0; i < n; i++)
        if (args[i][0]) clean[m++] = args[i];
    clean[m] = NULL;
    char *out = applet_run("dig", clean);
    if (!out) { push("(dig falló)"); return; }
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save))
        push(tok);
    free(out);
}

static void query_nslookup(void) {
    char *args[5];
    int n = 0;
    if (!strcmp(qtype, "PTR")) {
        /* nslookup hace reverse automáticamente con la IP */
        args[n++] = qname;
    } else {
        args[n++] = (char *)"-type=";
        /* busybox: nslookup -type=MX host; llamamos con query completa */
        static char typearg[24];
        snprintf(typearg, sizeof typearg, "-type=%s", qtype);
        args[n - 1] = typearg;
        args[n++] = qname;
    }
    if (server[0]) args[n++] = server;
    args[n] = NULL;
    char *out = applet_run("nslookup", args);
    if (!out) { push("(nslookup falló — prueba instalar dig)"); return; }
    char *save = NULL;
    for (char *tok = strtok_r(out, "\n", &save); tok && nlines < MAX_LINES;
         tok = strtok_r(NULL, "\n", &save))
        push(tok);
    free(out);
}

static void run_query(void) {
    if (!qname[0]) { snprintf(status, sizeof status, "escribe un dominio o IP"); return; }
    clear_out();
    push("--- consulta ---");
    if (dig_available()) query_dig();
    else query_nslookup();
    snprintf(status, sizeof status, "%s %s @%s — %d líneas",
             qtype, qname, server[0] ? server : "sistema", nlines);
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* query field */
    ui_bevel(u, 4, 4, u->w - 160, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 6, 6, (unsigned)u->w - 168, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 10, 20, qname, (int)strlen(qname));
    ui_button(u, u->w - 152, 4, 74, 24, "Consultar", false);
    ui_button(u, u->w - 74, 4, 70, 24, "Limpiar", false);
    /* type buttons */
    const char *types[] = {"A", "AAAA", "CNAME", "MX", "NS", "PTR"};
    int bx = 4;
    for (int i = 0; i < 6; i++) {
        ui_button(u, bx, 32, 56, 24, types[i], !strcmp(qtype, types[i]));
        bx += 60;
    }
    /* server field */
    ui_text(u, bx + 4, 48, "server:");
    ui_bevel(u, bx + 58, 32, 150, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, bx + 60, 34, 146, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, bx + 64, 48, server, (int)strlen(server));
    /* output */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 66;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    XSetForeground(u->dpy, u->gc, u->c_fg);
    for (int i = first; i < nlines && i - first < VISIBLE; i++)
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i])),
            y += row_h;
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

static int focus = 0;   /* 0=qname 1=server */

int main(void) {
    UiCtx u;
    ui_init(&u, 700, 440, "Análisis DNS — w3m-net");
    XEvent ev;
    push("w3m-dns: dominio, tipo de registro, Consultar");
    for (;;) {
        if (!ui_next_event(&u, &ev)) break;
        if (ev.type == Expose) { draw(&u); continue; }
        if (ev.type == ButtonPress) {
            int x = ev.xbutton.x, y = ev.xbutton.y;
            const char *types[] = {"A", "AAAA", "CNAME", "MX", "NS", "PTR"};
            if (y >= 4 && y <= 28) {
                if (x < u.w - 160) focus = 0;
                else if (x >= u.w - 152 && x < u.w - 78) run_query();
                else if (x >= u.w - 74) { clear_out(); nlines = 0; }
            } else if (y >= 32 && y <= 56 && x < 364) {
                int ti = (x - 4) / 60;
                if (ti >= 0 && ti < 6) snprintf(qtype, sizeof qtype, "%s", types[ti]);
            } else if (y >= 32 && y <= 56) {
                focus = 1;
            }
            draw(&u);
        }
        if (ev.type == KeyPress) {
            char kb[8]; KeySym ks;
            int n = XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
            char *field = focus == 0 ? qname : server;
            size_t cap = focus == 0 ? sizeof qname : sizeof server;
            size_t fl = strlen(field);
            if (ks == XK_Tab) focus = (focus + 1) % 2;
            else if (ks == XK_Return) run_query();
            else if (ks == XK_BackSpace) { if (fl) field[fl-1] = '\0'; }
            else if (n == 1 && kb[0] >= ' ' && kb[0] < 127 && fl < cap - 1)
                { field[fl] = kb[0]; field[fl+1] = '\0'; }
            draw(&u);
        }
    }
    ui_close(&u);
    return 0;
}
