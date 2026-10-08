/* w3m-nc — netcat gráfico: conectar a host:puerto, enviar líneas,
   ver respuesta. Usos legítimos: probar tus propios servicios
   (¿escucha el puerto? ¿responde el protocolo?), debug de servidores. */

#include "ui.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define MAX_LINES 300
#define LINE_LEN 200
#define VISIBLE 20

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char host[128] = "";
static char port[8] = "80";
static char input_line[200] = "";
static int sockfd = -1;
static char status[128] = "host + puerto, Conectar";

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
    else {
        memmove(lines[0], lines[1], (MAX_LINES - 1) * LINE_LEN);
        snprintf(lines[MAX_LINES - 1], LINE_LEN, "%s", s);
    }
}

static void disconnect(void) {
    if (sockfd >= 0) close(sockfd);
    sockfd = -1;
    snprintf(status, sizeof status, "desconectado");
}

static void connect_host(void) {
    if (!host[0] || !port[0]) { snprintf(status, sizeof status, "falta host o puerto"); return; }
    disconnect();
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        snprintf(status, sizeof status, "getaddrinfo: %s", gai_strerror(rc));
        return;
    }
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) { snprintf(status, sizeof status, "socket: %s", strerror(errno)); freeaddrinfo(res); return; }
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        snprintf(status, sizeof status, "connect: %s", strerror(errno));
        close(sockfd); sockfd = -1;
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);
    char msg[160];
    snprintf(msg, sizeof msg, "--- conectado a %s:%s ---", host, port);
    push(msg);
    snprintf(status, sizeof status, "conectado a %s:%s", host, port);
}

static void pump(void) {
    if (sockfd < 0) return;
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(sockfd, &rf);
    struct timeval tv = {0, 0};
    if (select(sockfd + 1, &rf, NULL, NULL, &tv) > 0) {
        char buf[4096];
        ssize_t n = recv(sockfd, buf, sizeof buf - 1, MSG_DONTWAIT);
        if (n == 0) { push("--- cerrado por el remoto ---"); disconnect(); return; }
        if (n < 0) return;
        buf[n] = '\0';
        char *save = NULL;
        for (char *tok = strtok_r(buf, "\r\n", &save); tok;
             tok = strtok_r(NULL, "\r\n", &save))
            push(tok);
    }
}

static void send_line(void) {
    if (sockfd < 0) { snprintf(status, sizeof status, "no conectado"); return; }
    char msg[220];
    snprintf(msg, sizeof msg, "> %s", input_line);
    push(msg);
    size_t len = strlen(input_line);
    if (send(sockfd, input_line, len, 0) < 0)
        push("(error de envío)");
    else if (send(sockfd, "\n", 1, 0) < 0)
        push("(error de envío)");
    input_line[0] = '\0';
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* host field */
    ui_bevel(u, 4, 4, 300, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 6, 6, 296, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 10, 20, host, (int)strlen(host));
    /* port field */
    ui_bevel(u, 310, 4, 70, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 312, 6, 66, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 316, 20, port, (int)strlen(port));
    ui_button(u, 386, 4, 80, 24, sockfd >= 0 ? "Cerrar" : "Conectar", false);
    /* log */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 40;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    XSetForeground(u->dpy, u->gc, u->c_fg);
    for (int i = first; i < nlines && i - first < VISIBLE; i++)
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i])),
            y += row_h;
    /* input line */
    int iy = u->h - 50;
    ui_bevel(u, 4, iy, u->w - 8, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 6, iy + 2, (unsigned)u->w - 12, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 10, iy + 18, input_line, (int)strlen(input_line));
    /* status */
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

/* focus: 0=host, 1=port, 2=input */
static int focus = 0;

int main(void) {
    UiCtx u;
    ui_init(&u, 640, 440, "Netcat — w3m-net");
    XEvent ev;
    push("w3m-nc: prueba tus propios servicios (host, puerto, Conectar)");
    for (;;) {
        while (XPending(u.dpy)) {
            XNextEvent(u.dpy, &ev);
            if (ev.type == ClientMessage) { disconnect(); ui_close(&u); return 0; }
            if (ev.type == Expose) { draw(&u); continue; }
            if (ev.type == ButtonPress) {
                int x = ev.xbutton.x, y = ev.xbutton.y;
                if (y >= 4 && y <= 28) {
                    if (x < 304) focus = 0;
                    else if (x < 380) focus = 1;
                    else if (x < 466) {
                        if (sockfd >= 0) disconnect();
                        else connect_host();
                    }
                } else if (y >= u.h - 50 && y <= u.h - 26) {
                    focus = 2;
                }
                draw(&u);
            }
            if (ev.type == KeyPress) {
                char kb[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
                char *field = focus == 0 ? host : focus == 1 ? port : input_line;
                size_t cap = focus == 0 ? sizeof host : focus == 1 ? sizeof port : sizeof input_line;
                size_t fl = strlen(field);
                if (ks == XK_Tab) focus = (focus + 1) % 3;
                else if (ks == XK_Return) {
                    if (focus == 2 && sockfd >= 0) send_line();
                    else if (sockfd < 0) connect_host();
                }
                else if (ks == XK_BackSpace) { if (fl) field[fl-1] = '\0'; }
                else if (n == 1 && kb[0] >= ' ' && kb[0] < 127 && fl < cap - 1)
                    { field[fl] = kb[0]; field[fl+1] = '\0'; }
                draw(&u);
            }
        }
        pump();
        usleep(20000);
    }
}
