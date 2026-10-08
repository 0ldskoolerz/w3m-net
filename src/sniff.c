/* w3m-sniff — captura de paquetes en vivo (tcpdump) con filtro.
   Diagnóstico de tu propia red: el filtro pasa a tcpdump como argv
   (sin shell). Sin tcpdump instalado, avisa en la barra de estado. */

#include "ui.h"
#include "applet.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>

#define MAX_LINES 400
#define LINE_LEN 200
#define VISIBLE 22

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char filter[128] = "";
static char iface[16] = "eth0";
static char status[128] = "Filtro opcional (tcpdump syntax), Start para capturar";
static bool running = false;
static pid_t child = -1;
static int outfd = -1;
static int count = 0;

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
    else {
        memmove(lines[0], lines[1], (MAX_LINES - 1) * LINE_LEN);
        snprintf(lines[MAX_LINES - 1], LINE_LEN, "%s", s);
    }
}

static void stop(void) {
    if (!running) return;
    if (child > 0) kill(child, SIGTERM);
    if (outfd >= 0) close(outfd);
    outfd = -1; child = -1; running = false;
    push("--- captura detenida ---");
}

/* el filtro se pasa como UN solo argv: tcpdump lo interpreta su
   lenguaje propio, nunca pasa por un shell */
static void start(void) {
    if (running) stop();
    int pipefd[2];
    if (pipe(pipefd) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        if (filter[0])
            execlp("tcpdump", "tcpdump", "-i", iface, "-l", "-tttt",
                   filter, (char *)NULL);
        else
            execlp("tcpdump", "tcpdump", "-i", iface, "-l", "-tttt",
                   (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    child = pid; outfd = pipefd[0]; running = true; count = 0;
    push("--- capturando en %s ---");
}

static void pump(void) {
    if (!running || outfd < 0) return;
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(outfd, &rf);
    struct timeval tv = {0, 0};
    if (select(outfd + 1, &rf, NULL, NULL, &tv) > 0) {
        char buf[8192];
        ssize_t n = read(outfd, buf, sizeof buf - 1);
        if (n <= 0) { stop(); return; }
        buf[n] = '\0';
        char *save = NULL;
        for (char *tok = strtok_r(buf, "\n", &save); tok;
             tok = strtok_r(NULL, "\n", &save)) {
            push(tok);
            count++;
        }
        snprintf(status, sizeof status, "%d paquetes capturados", count);
    }
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* iface selector */
    ui_text(u, 6, 20, "iface:");
    ui_button(u, 52, 4, 60, 24, iface, false);
    /* filter field */
    ui_bevel(u, 120, 4, u->w - 240, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 122, 6, (unsigned)u->w - 244, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 126, 20, filter, (int)strlen(filter));
    ui_button(u, u->w - 112, 4, 52, 24, running ? "Stop" : "Start", running);
    ui_button(u, u->w - 56, 4, 52, 24, "Limpiar", false);
    /* output */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 40;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    XSetForeground(u->dpy, u->gc, u->c_fg);
    for (int i = first; i < nlines && i - first < VISIBLE; i++)
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i])),
            y += row_h;
    ui_bevel(u, 4, u->h - 22, u->w - 8, 18, true);
    ui_text(u, 8, u->h - 9, status);
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 720, 460, "Sniffer (tcpdump) — w3m-net");
    XEvent ev;
    for (;;) {
        while (XPending(u.dpy)) {
            XNextEvent(u.dpy, &ev);
            if (ev.type == ClientMessage) { stop(); ui_close(&u); return 0; }
            if (ev.type == Expose) { draw(&u); continue; }
            if (ev.type == ButtonPress) {
                int x = ev.xbutton.x, y = ev.xbutton.y;
                if (y >= 4 && y <= 28) {
                    if (x >= 52 && x < 112) {
                        /* cicla interfaces comunes */
                        if (!strcmp(iface, "eth0")) snprintf(iface, sizeof iface, "wlan0");
                        else if (!strcmp(iface, "wlan0")) snprintf(iface, sizeof iface, "any");
                        else snprintf(iface, sizeof iface, "eth0");
                    } else if (x >= u.w - 112 && x < u.w - 60) {
                        if (running) stop(); else start();
                    } else if (x >= u.w - 56) {
                        nlines = 0; count = 0;
                        snprintf(status, sizeof status, "limpio");
                    }
                    draw(&u);
                }
            }
            if (ev.type == KeyPress) {
                char kb[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
                size_t fl = strlen(filter);
                if (ks == XK_BackSpace) { if (fl) filter[fl-1] = '\0'; }
                else if (n == 1 && kb[0] >= ' ' && kb[0] < 127 && fl < sizeof filter - 1)
                    { filter[fl] = kb[0]; filter[fl+1] = '\0'; }
                draw(&u);
            }
        }
        pump();
        usleep(20000);
    }
}
