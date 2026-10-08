/* w3m-ping — ping/traceroute continuo estilo Trinux: campo de host,
   botones Iniciar/Parar, salida de busybox ping/traceroute en scroll. */

#include "ui.h"
#include "applet.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>

#define MAX_LINES 300
#define LINE_LEN 160
#define VISIBLE 20

static char lines[MAX_LINES][LINE_LEN];
static int nlines = 0;
static char host[128] = "";
static char last_result[64] = "—";
static bool running = false;
static pid_t child = -1;
static int outfd = -1;

static void push(const char *s) {
    if (nlines < MAX_LINES) snprintf(lines[nlines++], LINE_LEN, "%s", s);
    else {
        memmove(lines[0], lines[1], (MAX_LINES - 1) * LINE_LEN);
        snprintf(lines[MAX_LINES - 1], LINE_LEN, "%s", s);
    }
}

static void stop_ping(void) {
    if (!running) return;
    if (child > 0) kill(child, SIGTERM);
    if (outfd >= 0) close(outfd);
    outfd = -1; child = -1; running = false;
    push("--- detenido ---");
}

/* lanza: ping -c 4 host  (o traceroute -m 15 host) */
static void start(int trace) {
    if (!host[0]) { snprintf(last_result, sizeof last_result, "escribe un host"); return; }
    if (running) stop_ping();
    int pipefd[2];
    if (pipe(pipefd) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        if (trace) execlp("traceroute", "traceroute", "-m", "15", host, (char *)NULL);
        else execlp("ping", "ping", "-c", "4", host, (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    child = pid; outfd = pipefd[0]; running = true;
    push(trace ? "--- traceroute ---" : "--- ping ---");
}

static void pump_output(void) {
    if (!running || outfd < 0) return;
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(outfd, &rf);
    struct timeval tv = {0, 0};
    if (select(outfd + 1, &rf, NULL, NULL, &tv) > 0) {
        char buf[2048];
        ssize_t n = read(outfd, buf, sizeof buf - 1);
        if (n <= 0) { stop_ping(); return; }
        buf[n] = '\0';
        /* line-oriented */
        char *save = NULL;
        for (char *tok = strtok_r(buf, "\n", &save); tok;
             tok = strtok_r(NULL, "\n", &save)) {
            push(tok);
            /* resumen: última línea de estadísticas de ping */
            float loss;
            if (sscanf(tok, "%*d packets transmitted, %*d received, %f%% packet loss", &loss) == 1)
                snprintf(last_result, sizeof last_result, "pérdida: %.0f%%", loss);
        }
    }
}

static void draw(UiCtx *u) {
    ui_clear(u);
    /* input row */
    ui_bevel(u, 4, 4, u->w - 8, 24, true);
    XSetForeground(u->dpy, u->gc, u->c_white);
    XFillRectangle(u->dpy, u->win, u->gc, 6, 6, (unsigned)u->w - 12, 20);
    XSetForeground(u->dpy, u->gc, u->c_fg);
    XDrawString(u->dpy, u->win, u->gc, 10, 20, host, (int)strlen(host));
    ui_button(u, u->w - 210, 4, 64, 24, "Ping", false);
    ui_button(u, u->w - 142, 4, 90, 24, "Traceroute", false);
    ui_button(u, u->w - 48, 4, 44, 24, running ? "Stop" : "—", false);
    /* result badge */
    ui_text(u, 8, 42, last_result);
    /* output */
    int row_h = u->font->ascent + u->font->descent + 1;
    int y = 52;
    int first = nlines > VISIBLE ? nlines - VISIBLE : 0;
    for (int i = first; i < nlines && i - first < VISIBLE; i++)
        XDrawString(u->dpy, u->win, u->gc, 8, y, lines[i], (int)strlen(lines[i])),
            y += row_h;
    ui_flush(u);
}

int main(void) {
    UiCtx u;
    ui_init(&u, 640, 420, "Ping / Traceroute — w3m-net");
    XEvent ev;
    push("w3m-ping: escribe un host y pulsa Ping o Enter");

    for (;;) {
        while (XPending(u.dpy)) {
            XNextEvent(u.dpy, &ev);
            if (ev.type == ClientMessage) { stop_ping(); ui_close(&u); return 0; }
            if (ev.type == Expose) { draw(&u); continue; }
            if (ev.type == ButtonPress) {
                int x = ev.xbutton.x;
                if (ev.xbutton.y >= 4 && ev.xbutton.y <= 28) {
                    if (x >= u.w - 210) start(0);
                    else if (x >= u.w - 142) start(1);
                    else if (x >= u.w - 48) stop_ping();
                    draw(&u);
                }
            }
            if (ev.type == KeyPress) {
                char kb[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, kb, sizeof kb, &ks, NULL);
                size_t hl = strlen(host);
                if (ks == XK_Return) start(0);
                else if (ks == XK_BackSpace) { if (hl) host[hl-1] = '\0'; }
                else if (n == 1 && kb[0] >= ' ' && kb[0] < 127 && hl < sizeof host - 1)
                    { host[hl] = kb[0]; host[hl+1] = '\0'; }
                draw(&u);
            }
        }
        pump_output();
        usleep(20000);
    }
}
