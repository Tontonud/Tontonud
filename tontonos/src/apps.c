/* TontonOS - applications intégrées */
#include "wm.h"

#define DARK  RGB(30, 35, 45)
#define MUTED RGB(110, 118, 132)

/* =========================== Terminal =========================== */
#define T_COLS 78
#define T_ROWS 300
typedef struct {
    char line[T_ROWS][T_COLS + 1];
    u32  color[T_ROWS];
    int  n;
    char in[T_COLS + 1];
    int  ilen;
    int  blink;
    char hist[8][T_COLS + 1];
    int  nhist, hpos;
} term_t;

static void t_newline(term_t *t, u32 c) {
    if (t->n == T_ROWS) {
        memmove(t->line[0], t->line[1], (T_ROWS - 1) * (T_COLS + 1));
        memmove(&t->color[0], &t->color[1], (T_ROWS - 1) * sizeof(u32));
        t->n--;
    }
    t->line[t->n][0] = 0; t->color[t->n] = c; t->n++;
}
static void t_print(term_t *t, const char *s, u32 c) {
    t_newline(t, c);
    int col = 0;
    for (; *s; s++) {
        if (*s == '\n' || col == T_COLS) { t_newline(t, c); col = 0; if (*s == '\n') continue; }
        t->line[t->n - 1][col++] = *s; t->line[t->n - 1][col] = 0;
    }
}
#define T_OK    RGB(200, 210, 225)
#define T_GREEN RGB(110, 230, 140)
#define T_BLUE  RGB(120, 180, 255)
#define T_YEL   RGB(250, 210, 90)
#define T_RED   RGB(250, 110, 110)

static void t_draw(window_t *w, int x, int y) {
    term_t *t = w->data;
    gfx_fill(x, y, w->w, w->h, RGB(18, 20, 28));
    int rows = (w->h - 8) / FONT_H;
    int total = t->n + 1, first = total > rows ? total - rows : 0;
    for (int r = first; r < t->n; r++)
        gfx_text(x + 6, y + 4 + (r - first) * FONT_H, t->line[r], t->color[r]);
    int py = y + 4 + (t->n - first) * FONT_H;
    gfx_text(x + 6, py, "tonton>", T_GREEN);
    gfx_text(x + 6 + 8 * FONT_W, py, t->in, 0xFFFFFF);
    if (t->blink < 5) gfx_fill(x + 6 + (8 + t->ilen) * FONT_W, py + 2, FONT_W, FONT_H - 3, RGB(200, 200, 200));
}
static void t_tick(window_t *w) { term_t *t = w->data; t->blink = (t->blink + 1) % 10; if (t->blink == 0 || t->blink == 5) wm_invalidate(); }

static const char *arg_after(const char *s, const char *cmd) {
    size_t n = strlen(cmd);
    if (strncmp(s, cmd, n)) return NULL;
    if (s[n] == 0) return "";
    if (s[n] != ' ') return NULL;
    s += n; while (*s == ' ') s++;
    return s;
}

static void t_exec(term_t *t, const char *c) {
    const char *a;
    char buf[96], tmp[16];
    while (*c == ' ') c++;
    if (!*c) return;
    if (!strcmp(c, "aide") || !strcmp(c, "help")) {
        t_print(t, "Commandes disponibles :", T_YEL);
        t_print(t, "  aide            affiche cette aide", T_OK);
        t_print(t, "  effacer         vide l'écran", T_OK);
        t_print(t, "  echo <texte>    répète le texte", T_OK);
        t_print(t, "  heure / date    heure et date actuelles", T_OK);
        t_print(t, "  mem             mémoire disponible", T_OK);
        t_print(t, "  uptime          temps depuis le démarrage", T_OK);
        t_print(t, "  ver             version du système", T_OK);
        t_print(t, "  clavier <fr|us> change la disposition (ou F12)", T_OK);
        t_print(t, "  fond <1-4>      change le fond d'écran", T_OK);
        t_print(t, "  ouvrir <appli>  notes, calc, peinture, mines, param", T_OK);
        t_print(t, "  calc <a> <op> <b>  petit calcul (+ - * /)", T_OK);
        t_print(t, "  redemarrer / eteindre", T_OK);
    } else if (!strcmp(c, "effacer") || !strcmp(c, "cls") || !strcmp(c, "clear")) {
        t->n = 0;
    } else if ((a = arg_after(c, "echo"))) {
        t_print(t, a, T_OK);
    } else if (!strcmp(c, "heure")) {
        int h, m, s; rtc_read(&h, &m, &s, NULL, NULL, NULL);
        strcpy(buf, "Il est "); utoa_pad(h, tmp, 2); strcpy(buf + 7, tmp); strcpy(buf + 9, ":");
        utoa_pad(m, tmp, 2); strcpy(buf + 10, tmp); strcpy(buf + 12, ":"); utoa_pad(s, tmp, 2); strcpy(buf + 13, tmp);
        strcpy(buf + 15, " (UTC)");
        t_print(t, buf, T_OK);
    } else if (!strcmp(c, "date")) {
        static const char *mois[] = { "janvier", "février", "mars", "avril", "mai", "juin", "juillet",
                                      "août", "septembre", "octobre", "novembre", "décembre" };
        int d, mo, y; rtc_read(NULL, NULL, NULL, &d, &mo, &y);
        itoa(d, buf); strcpy(buf + strlen(buf), " ");
        strcpy(buf + strlen(buf), mois[(mo - 1) % 12]); strcpy(buf + strlen(buf), " ");
        itoa(y, tmp); strcpy(buf + strlen(buf), tmp);
        t_print(t, buf, T_OK);
    } else if (!strcmp(c, "mem")) {
        strcpy(buf, "Mémoire totale : "); itoa(mem_total_kb / 1024, tmp); strcpy(buf + strlen(buf), tmp);
        strcpy(buf + strlen(buf), " Mo   libre : "); itoa(heap_free_bytes() / 1024, tmp);
        strcpy(buf + strlen(buf), tmp); strcpy(buf + strlen(buf), " Ko");
        t_print(t, buf, T_OK);
    } else if (!strcmp(c, "uptime")) {
        u32 s = ticks / 100;
        strcpy(buf, "Allumé depuis "); itoa(s / 60, tmp); strcpy(buf + strlen(buf), tmp);
        strcpy(buf + strlen(buf), " min "); itoa(s % 60, tmp); strcpy(buf + strlen(buf), tmp);
        strcpy(buf + strlen(buf), " s");
        t_print(t, buf, T_OK);
    } else if (!strcmp(c, "ver")) {
        t_print(t, "TontonOS version 1.0 - noyau 32 bits écrit à partir de zéro", T_BLUE);
    } else if ((a = arg_after(c, "clavier"))) {
        if (!strcmp(a, "fr") || !strcmp(a, "azerty")) kb_azerty = 1;
        else if (!strcmp(a, "us") || !strcmp(a, "qwerty")) kb_azerty = 0;
        t_print(t, kb_azerty ? "Clavier : AZERTY (français)" : "Clavier : QWERTY (américain)", T_OK);
    } else if ((a = arg_after(c, "fond"))) {
        if (*a >= '1' && *a <= '4') { wallpaper = *a - '1'; t_print(t, "Fond d'écran changé.", T_OK); }
        else t_print(t, "Usage : fond <1-4>", T_RED);
    } else if ((a = arg_after(c, "ouvrir"))) {
        if (!strcmp(a, "notes")) app_notes();
        else if (!strcmp(a, "calc")) app_calc();
        else if (!strcmp(a, "peinture")) app_paint();
        else if (!strcmp(a, "mines")) app_mines();
        else if (!strcmp(a, "param")) app_settings();
        else if (!strcmp(a, "terminal")) app_terminal();
        else t_print(t, "Application inconnue.", T_RED);
    } else if ((a = arg_after(c, "calc"))) {
        int x = 0, y = 0, sx = 1, sy = 1; char op = 0;
        if (*a == '-') { sx = -1; a++; }
        while (*a >= '0' && *a <= '9') x = x * 10 + *a++ - '0';
        while (*a == ' ') a++;
        op = *a; if (op) a++;
        while (*a == ' ') a++;
        if (*a == '-') { sy = -1; a++; }
        while (*a >= '0' && *a <= '9') y = y * 10 + *a++ - '0';
        double r, X = sx * x, Y = sy * y;
        switch (op) {
        case '+': r = X + Y; break; case '-': r = X - Y; break;
        case '*': case 'x': r = X * Y; break;
        case '/': if (!y) { t_print(t, "Division par zéro !", T_RED); return; } r = X / Y; break;
        default: t_print(t, "Usage : calc 12 * 7", T_RED); return;
        }
        strcpy(buf, "= "); ftoa(r, buf + 2);
        t_print(t, buf, T_GREEN);
    } else if (!strcmp(c, "redemarrer")) {
        reboot();
    } else if (!strcmp(c, "eteindre")) {
        shutdown();
        t_print(t, "Extinction impossible sur cette machine.", T_RED);
    } else {
        strcpy(buf, "Commande inconnue : "); strlcpy(buf + strlen(buf), c, 60);
        t_print(t, buf, T_RED);
        t_print(t, "Tapez « aide » pour la liste des commandes.", MUTED);
    }
}

static void t_key(window_t *w, int k) {
    term_t *t = w->data;
    t->blink = 0;
    if (k == KEY_ENTER) {
        char line[T_COLS + 12];
        strcpy(line, "tonton> "); strcpy(line + 8, t->in);
        t_print(t, line, RGB(150, 160, 180));
        if (t->ilen) {
            if (t->nhist == 8) { memmove(t->hist[0], t->hist[1], 7 * (T_COLS + 1)); t->nhist--; }
            strcpy(t->hist[t->nhist++], t->in);
        }
        t->hpos = t->nhist;
        char cmd[T_COLS + 1]; strcpy(cmd, t->in);
        t->ilen = 0; t->in[0] = 0;
        t_exec(t, cmd);
    } else if (k == KEY_BACKSPACE) {
        if (t->ilen) t->in[--t->ilen] = 0;
    } else if (k == KEY_UP || k == KEY_DOWN) {
        if (!t->nhist) return;
        t->hpos += k == KEY_UP ? -1 : 1;
        if (t->hpos < 0) t->hpos = 0;
        if (t->hpos >= t->nhist) { t->hpos = t->nhist; t->in[0] = 0; }
        else strcpy(t->in, t->hist[t->hpos]);
        t->ilen = strlen(t->in);
    } else if ((k & 0xFFFF) == ('l' | 0) && (k & KMOD_CTRL)) {
        t->n = 0;
    } else if (k >= 32 && k < 256 && t->ilen < T_COLS - 9) {
        t->in[t->ilen++] = k; t->in[t->ilen] = 0;
    }
}

void app_terminal(void) {
    window_t *w = wm_open("Terminal", 640, 400, ICON_TERM);
    if (!w) return;
    term_t *t = kzalloc(sizeof(term_t));
    if (!t) { wm_close(w); return; }
    w->data = t; w->draw = t_draw; w->key = t_key; w->tick = t_tick;
    t_print(t, "TontonOS [version 1.0]", T_BLUE);
    t_print(t, "Tapez « aide » pour voir les commandes.", MUTED);
}

/* =========================== Bloc-notes =========================== */
#define N_MAX 16000
typedef struct { char text[N_MAX]; int len, cur, top, blink; } notes_t;

static void n_curpos(notes_t *n, int *line, int *col) {
    int l = 0, c = 0;
    for (int i = 0; i < n->cur; i++) { if (n->text[i] == '\n') { l++; c = 0; } else c++; }
    *line = l; *col = c;
}
static int n_line_start(notes_t *n, int line) {
    int i = 0;
    while (line > 0 && i < n->len) { if (n->text[i++] == '\n') line--; }
    return i;
}
static int n_line_len(notes_t *n, int start) { int i = start; while (i < n->len && n->text[i] != '\n') i++; return i - start; }
static int n_lines(notes_t *n) { int l = 1; for (int i = 0; i < n->len; i++) if (n->text[i] == '\n') l++; return l; }

static void n_draw(window_t *w, int x, int y) {
    notes_t *n = w->data;
    gfx_fill(x, y, w->w, 24, RGB(238, 241, 246));
    gfx_fill(x, y + 24, w->w, 1, RGB(210, 216, 226));
    gfx_text(x + 8, y + 4, "Fichier   Édition   Format", RGB(60, 66, 78));
    int rows = (w->h - 50) / FONT_H, cl, cc; n_curpos(n, &cl, &cc);
    if (cl < n->top) n->top = cl;
    if (cl >= n->top + rows) n->top = cl - rows + 1;
    int ty = y + 30, i = n_line_start(n, n->top);
    char lb[90];
    for (int r = 0; r < rows && i <= n->len; r++) {
        int ll = n_line_len(n, i), k;
        for (k = 0; k < ll && k < 88; k++) lb[k] = n->text[i + k];
        lb[k] = 0;
        gfx_text(x + 8, ty + r * FONT_H, lb, DARK);
        i += ll + 1;
        if (i > n->len + 1) break;
    }
    if (n->blink < 5) gfx_fill(x + 8 + cc * FONT_W, ty + (cl - n->top) * FONT_H, 2, FONT_H, DARK);
    /* barre d'état */
    gfx_fill(x, y + w->h - 20, w->w, 20, RGB(238, 241, 246));
    char st[64], tmp[12];
    strcpy(st, "Ligne "); itoa(cl + 1, tmp); strcpy(st + strlen(st), tmp);
    strcpy(st + strlen(st), ", colonne "); itoa(cc + 1, tmp); strcpy(st + strlen(st), tmp);
    strcpy(st + strlen(st), "   |   "); itoa(n->len, tmp); strcpy(st + strlen(st), tmp);
    strcpy(st + strlen(st), " caractères");
    gfx_text(x + 8, y + w->h - 18, st, MUTED);
}
static void n_tick(window_t *w) { notes_t *n = w->data; n->blink = (n->blink + 1) % 10; if (n->blink == 0 || n->blink == 5) wm_invalidate(); }
static void n_key(window_t *w, int k) {
    notes_t *n = w->data;
    int l, c; n_curpos(n, &l, &c);
    n->blink = 0;
    switch (k) {
    case KEY_LEFT:  if (n->cur) n->cur--; return;
    case KEY_RIGHT: if (n->cur < n->len) n->cur++; return;
    case KEY_HOME:  n->cur -= c; return;
    case KEY_END:   n->cur += n_line_len(n, n->cur - c) - c; return;
    case KEY_UP: case KEY_DOWN: {
        int nl = l + (k == KEY_UP ? -1 : 1);
        if (nl < 0 || nl >= n_lines(n)) return;
        int s = n_line_start(n, nl);
        n->cur = s + min(c, n_line_len(n, s));
        return; }
    case KEY_BACKSPACE:
        if (!n->cur) return;
        memmove(n->text + n->cur - 1, n->text + n->cur, n->len - n->cur);
        n->cur--; n->len--; return;
    case KEY_DEL:
        if (n->cur >= n->len) return;
        memmove(n->text + n->cur, n->text + n->cur + 1, n->len - n->cur - 1);
        n->len--; return;
    }
    if (k == KEY_TAB) { for (int i = 0; i < 4; i++) n_key(w, ' '); return; }
    if ((k >= 32 && k < 256) || k == KEY_ENTER) {
        if (n->len >= N_MAX - 1) return;
        if (k != KEY_ENTER && n_line_len(n, n->cur - c) >= (w->w - 16) / FONT_W - 1) return;
        memmove(n->text + n->cur + 1, n->text + n->cur, n->len - n->cur);
        n->text[n->cur++] = k; n->len++;
    }
}
void app_notes(void) {
    window_t *w = wm_open("Sans titre - Bloc-notes", 600, 420, ICON_NOTES);
    if (!w) return;
    notes_t *n = kzalloc(sizeof(notes_t));
    if (!n) { wm_close(w); return; }
    w->data = n; w->draw = n_draw; w->key = n_key; w->tick = n_tick;
    const char *hello = "Bienvenue dans le Bloc-notes de TontonOS !\n\nÉcrivez ce que vous voulez ici.\nLes accents marchent : é è à ç ù ê â ô.\n";
    strcpy(n->text, hello); n->len = strlen(hello); n->cur = n->len;
}

/* =========================== Calculatrice =========================== */
typedef struct { double acc; char disp[24]; char op; int fresh; char hist[40]; int pressed; } calc_t;
static const char *calc_keys[20] = { "C", "±", "%", "÷", "7", "8", "9", "×", "4", "5", "6", "-", "1", "2", "3", "+", "0", "00", ",", "=" };
#define CB_W 62
#define CB_H 48
#define CB_Y 92

static double c_value(calc_t *c) {
    double v = 0, f = 0.1; int neg = 0, frac = 0;
    for (const char *p = c->disp; *p; p++) {
        if (*p == '-') neg = 1;
        else if (*p == ',') frac = 1;
        else if (*p >= '0' && *p <= '9') { if (frac) { v += (*p - '0') * f; f /= 10; } else v = v * 10 + (*p - '0'); }
    }
    return neg ? -v : v;
}
static void c_apply(calc_t *c) {
    double v = c_value(c);
    switch (c->op) {
    case '+': c->acc += v; break; case '-': c->acc -= v; break;
    case '*': c->acc *= v; break;
    case '/': if (v == 0) { strcpy(c->disp, "Erreur"); c->op = 0; c->fresh = 1; return; } c->acc /= v; break;
    default: c->acc = v;
    }
    ftoa(c->acc, c->disp);
}
static void c_press(calc_t *c, const char *k) {
    if (!strcmp(k, "C")) { c->acc = 0; strcpy(c->disp, "0"); c->op = 0; c->fresh = 1; c->hist[0] = 0; return; }
    if (k[0] >= '0' && k[0] <= '9') {
        if (c->fresh || !strcmp(c->disp, "0")) { c->disp[0] = 0; c->fresh = 0; }
        if (strlen(c->disp) + strlen(k) < 16) strcpy(c->disp + strlen(c->disp), k);
        if (!strcmp(c->disp, "00")) strcpy(c->disp, "0");
        return;
    }
    if (!strcmp(k, ",")) {
        if (c->fresh) { strcpy(c->disp, "0"); c->fresh = 0; }
        for (char *p = c->disp; *p; p++) if (*p == ',') return;
        strcpy(c->disp + strlen(c->disp), ","); return;
    }
    if (!strcmp(k, "±")) {
        if (c->disp[0] == '-') memmove(c->disp, c->disp + 1, strlen(c->disp));
        else if (strcmp(c->disp, "0")) { memmove(c->disp + 1, c->disp, strlen(c->disp) + 1); c->disp[0] = '-'; }
        return;
    }
    if (!strcmp(k, "%")) { ftoa(c_value(c) / 100.0, c->disp); return; }
    char op = !strcmp(k, "+") ? '+' : !strcmp(k, "-") ? '-' : !strcmp(k, "×") ? '*' : !strcmp(k, "÷") ? '/' : '=';
    if (!c->fresh || op == '=') c_apply(c);
    if (op == '=') { c->op = 0; c->hist[0] = 0; }
    else {
        c->op = op;
        strlcpy(c->hist, c->disp, 30); strcpy(c->hist + strlen(c->hist), " "); strcpy(c->hist + strlen(c->hist), k);
    }
    c->fresh = 1;
}
static void c_draw(window_t *w, int x, int y) {
    calc_t *c = w->data;
    gfx_fill(x, y, w->w, w->h, RGB(243, 245, 249));
    gfx_text(x + w->w - 12 - gfx_text_w(c->hist), y + 12, c->hist, MUTED);
    /* grand affichage (police doublée) */
    int len = strlen(c->disp), sx = x + w->w - 14 - len * 16;
    for (int i = 0; i < len; i++) gfx_char_scaled(sx + i * 16, y + 36, (u8)c->disp[i], DARK, 2, 3);
    for (int i = 0; i < 20; i++) {
        int bx = x + 8 + (i % 4) * (CB_W + 4), by = y + CB_Y + (i / 4) * (CB_H + 4);
        u32 bg = i == 19 ? accent_color : (i % 4 == 3 || i < 3) ? RGB(228, 232, 240) : RGB(255, 255, 255);
        if (c->pressed == i) bg = gfx_blend(bg, 0, 40);
        gfx_fill(bx, by, CB_W, CB_H, bg);
        gfx_rect(bx, by, CB_W, CB_H, RGB(214, 220, 230));
        gfx_text_bold(bx + (CB_W - gfx_text_w(calc_keys[i])) / 2, by + 16, calc_keys[i], i == 19 ? 0xFFFFFF : DARK);
    }
}
static void c_mouse(window_t *w, int x, int y, int ev, int b) {
    calc_t *c = w->data;
    if (ev == MEV_UP) { c->pressed = -1; return; }
    if (ev != MEV_DOWN) return;
    for (int i = 0; i < 20; i++) {
        int bx = 8 + (i % 4) * (CB_W + 4), by = CB_Y + (i / 4) * (CB_H + 4);
        if (ui_hit(x, y, bx, by, CB_W, CB_H)) { c->pressed = i; c_press(c, calc_keys[i]); }
    }
}
static void c_key(window_t *w, int k) {
    calc_t *c = w->data;
    char s[2] = { (char)k, 0 };
    if (k >= '0' && k <= '9') c_press(c, s);
    else if (k == '+' || k == '-') c_press(c, s);
    else if (k == '*') c_press(c, "×");
    else if (k == '/') c_press(c, "÷");
    else if (k == ',' || k == '.') c_press(c, ",");
    else if (k == KEY_ENTER || k == '=') c_press(c, "=");
    else if (k == KEY_ESC || k == KEY_DEL) c_press(c, "C");
    else if (k == KEY_BACKSPACE && !c->fresh) {
        int l = strlen(c->disp);
        if (l > 1) c->disp[l - 1] = 0; else strcpy(c->disp, "0");
    }
}
void app_calc(void) {
    window_t *w = wm_open("Calculatrice", 8 * 2 + 4 * CB_W + 12, CB_Y + 5 * (CB_H + 4) + 6, ICON_CALC);
    if (!w) return;
    calc_t *c = kzalloc(sizeof(calc_t));
    if (!c) { wm_close(w); return; }
    strcpy(c->disp, "0"); c->fresh = 1; c->pressed = -1;
    w->data = c; w->draw = c_draw; w->mouse = c_mouse; w->key = c_key;
}

/* =========================== Peinture =========================== */
#define P_W 600
#define P_H 380
#define P_BAR 44
typedef struct { u32 px[P_W * P_H]; u32 color; int size, lx, ly, down; } paint_t;
static const u32 p_colors[] = { 0x000000, 0x6B7280, 0xFFFFFF, 0xEF4444, 0xF97316, 0xFACC15,
                                0x22C55E, 0x14B8A6, 0x3B82F6, 0x8B5CF6, 0xEC4899, 0x92400E };
#define P_NC 12
static void p_draw(window_t *w, int x, int y) {
    paint_t *p = w->data;
    gfx_fill(x, y, w->w, P_BAR, RGB(238, 241, 246));
    for (int i = 0; i < P_NC; i++) {
        int bx = x + 8 + i * 28;
        gfx_fill(bx, y + 10, 24, 24, p_colors[i]);
        gfx_rect(bx, y + 10, 24, 24, p->color == p_colors[i] ? accent_color : RGB(180, 188, 204));
        if (p->color == p_colors[i]) gfx_rect(bx - 2, y + 8, 28, 28, accent_color);
    }
    int sx = x + 8 + P_NC * 28 + 12;
    for (int i = 0; i < 3; i++) {
        int r = 2 + i * 3, bx = sx + i * 32;
        gfx_fill(bx, y + 8, 28, 28, p->size == r ? RGB(205, 214, 235) : RGB(255, 255, 255));
        gfx_rect(bx, y + 8, 28, 28, RGB(180, 188, 204));
        gfx_disc(bx + 14, y + 22, r, DARK);
    }
    ui_button(x + w->w - 90, y + 8, 80, 28, "Effacer", 0);
    gfx_blit(x, y + P_BAR, P_W, P_H, p->px);
}
static void p_stroke(paint_t *p, int x0, int y0, int x1, int y1) {
    int steps = max(x1 > x0 ? x1 - x0 : x0 - x1, y1 > y0 ? y1 - y0 : y0 - y1) + 1;
    for (int s = 0; s <= steps; s++) {
        int cx = x0 + (x1 - x0) * s / steps, cy = y0 + (y1 - y0) * s / steps;
        for (int j = -p->size; j <= p->size; j++) for (int i = -p->size; i <= p->size; i++) {
            int px = cx + i, py = cy + j;
            if (i * i + j * j <= p->size * p->size && px >= 0 && py >= 0 && px < P_W && py < P_H) p->px[py * P_W + px] = p->color;
        }
    }
}
static void p_mouse(window_t *w, int x, int y, int ev, int b) {
    paint_t *p = w->data;
    if (ev == MEV_UP) { p->down = 0; return; }
    if (ev == MEV_DOWN && y < P_BAR) {
        for (int i = 0; i < P_NC; i++) if (ui_hit(x, y, 8 + i * 28, 10, 24, 24)) p->color = p_colors[i];
        int sx = 8 + P_NC * 28 + 12;
        for (int i = 0; i < 3; i++) if (ui_hit(x, y, sx + i * 32, 8, 28, 28)) p->size = 2 + i * 3;
        if (ui_hit(x, y, w->w - 90, 8, 80, 28)) for (int i = 0; i < P_W * P_H; i++) p->px[i] = 0xFFFFFF;
        return;
    }
    y -= P_BAR;
    if (ev == MEV_DOWN) { p->down = 1; p->lx = x; p->ly = y; }
    if (p->down) { p_stroke(p, p->lx, p->ly, x, y); p->lx = x; p->ly = y; }
}
void app_paint(void) {
    window_t *w = wm_open("Peinture", P_W, P_H + P_BAR, ICON_PAINT);
    if (!w) return;
    paint_t *p = kmalloc(sizeof(paint_t));
    if (!p) { wm_close(w); return; }
    for (int i = 0; i < P_W * P_H; i++) p->px[i] = 0xFFFFFF;
    p->color = 0x3B82F6; p->size = 5; p->down = 0;
    w->data = p; w->draw = p_draw; w->mouse = p_mouse;
}

/* =========================== Mines =========================== */
#define M_N 10
#define M_CELL 30
#define M_MINES 14
#define M_TOP 46
typedef struct { u8 mine[M_N][M_N], open[M_N][M_N], flag[M_N][M_N]; int state, opened, start, secs; } mines_t;

static void m_reset(mines_t *m) { memset(m, 0, sizeof(*m)); }
/* Les mines sont posées au premier clic, jamais autour de la case cliquée */
static void m_place(mines_t *m, int ci, int cj) {
    srand(ticks * 2654435761u + 7);
    for (int k = 0; k < M_MINES; ) {
        int i = rand() % M_N, j = rand() % M_N;
        if (m->mine[i][j] || (i >= ci - 1 && i <= ci + 1 && j >= cj - 1 && j <= cj + 1)) continue;
        m->mine[i][j] = 1; k++;
    }
}
static int m_count(mines_t *m, int i, int j) {
    int c = 0;
    for (int a = -1; a <= 1; a++) for (int b = -1; b <= 1; b++) {
        int x = i + a, y = j + b;
        if (x >= 0 && y >= 0 && x < M_N && y < M_N) c += m->mine[x][y];
    }
    return c;
}
static void m_open(mines_t *m, int i, int j) {
    if (i < 0 || j < 0 || i >= M_N || j >= M_N || m->open[i][j] || m->flag[i][j]) return;
    m->open[i][j] = 1; m->opened++;
    if (m_count(m, i, j) == 0 && !m->mine[i][j])
        for (int a = -1; a <= 1; a++) for (int b = -1; b <= 1; b++) m_open(m, i + a, j + b);
}
static const u32 m_numcol[9] = { 0, 0x2563EB, 0x16A34A, 0xDC2626, 0x7C3AED, 0xB45309, 0x0891B2, 0x111827, 0x6B7280 };
static void m_draw(window_t *w, int x, int y) {
    mines_t *m = w->data;
    gfx_fill(x, y, w->w, M_TOP, RGB(238, 241, 246));
    int flags = 0;
    for (int i = 0; i < M_N; i++) for (int j = 0; j < M_N; j++) flags += m->flag[i][j];
    char buf[32], tmp[8];
    strcpy(buf, "Mines : "); itoa(M_MINES - flags, tmp); strcpy(buf + 8, tmp);
    gfx_text(x + 10, y + 15, buf, DARK);
    itoa(m->secs, tmp); strcpy(buf, tmp); strcpy(buf + strlen(buf), " s");
    gfx_text(x + w->w - 10 - gfx_text_w(buf), y + 15, buf, DARK);
    ui_button(x + w->w / 2 - 44, y + 8, 88, 30, m->state == 0 ? "Nouveau" : m->state == 1 ? "Gagné !" : "Perdu...", 0);
    for (int i = 0; i < M_N; i++) for (int j = 0; j < M_N; j++) {
        int cx = x + 5 + i * M_CELL, cy = y + M_TOP + j * M_CELL;
        int reveal = m->open[i][j] || (m->state == 2 && m->mine[i][j]);
        if (!reveal) {
            gfx_fill(cx, cy, M_CELL - 2, M_CELL - 2, ((i + j) & 1) ? RGB(134, 190, 102) : RGB(146, 202, 112));
            if (m->flag[i][j]) {
                gfx_fill(cx + 10, cy + 6, 2, 17, DARK);
                for (int k = 0; k < 7; k++) gfx_fill(cx + 12, cy + 6 + k, 9 - k, 1, RGB(220, 38, 38));
            }
        } else {
            gfx_fill(cx, cy, M_CELL - 2, M_CELL - 2, ((i + j) & 1) ? RGB(226, 214, 190) : RGB(236, 224, 200));
            if (m->mine[i][j]) { gfx_disc(cx + 14, cy + 14, 7, DARK); gfx_disc(cx + 12, cy + 12, 2, 0xFFFFFF); }
            else { int n = m_count(m, i, j); if (n) { char s[2] = { '0' + n, 0 }; gfx_text_bold(cx + 10, cy + 6, s, m_numcol[n]); } }
        }
    }
}
static void m_mouse(window_t *w, int x, int y, int ev, int b) {
    mines_t *m = w->data;
    if (ev != MEV_DOWN && ev != MEV_RDOWN) return;
    if (y < M_TOP) { if (ev == MEV_DOWN && ui_hit(x, y, w->w / 2 - 44, 8, 88, 30)) m_reset(m); return; }
    if (m->state) return;
    int i = (x - 5) / M_CELL, j = (y - M_TOP) / M_CELL;
    if (x < 5 || i >= M_N || j >= M_N) return;
    if (ev == MEV_RDOWN && !m->start) return;
    if (!m->start) { m->start = ticks; m_place(m, i, j); }
    if (ev == MEV_RDOWN) { if (!m->open[i][j]) m->flag[i][j] ^= 1; return; }
    if (m->flag[i][j]) return;
    if (m->mine[i][j]) { m->state = 2; return; }
    m_open(m, i, j);
    if (m->opened == M_N * M_N - M_MINES) m->state = 1;
}
static void m_tick(window_t *w) {
    mines_t *m = w->data;
    if (m->start && !m->state) { int s = (ticks - m->start) / 100; if (s != m->secs) { m->secs = s; wm_invalidate(); } }
}
void app_mines(void) {
    window_t *w = wm_open("Mines", 10 + M_N * M_CELL, M_TOP + M_N * M_CELL + 4, ICON_MINES);
    if (!w) return;
    mines_t *m = kmalloc(sizeof(mines_t));
    if (!m) { wm_close(w); return; }
    m_reset(m);
    w->data = m; w->draw = m_draw; w->mouse = m_mouse; w->tick = m_tick;
}

/* =========================== Paramètres =========================== */
static const u32 accents[] = { RGB(37, 99, 235), RGB(220, 38, 38), RGB(22, 163, 74), RGB(147, 51, 234), RGB(234, 88, 12), RGB(8, 145, 178) };
static const u32 wall_prev[] = { RGB(60, 46, 136), RGB(15, 125, 110), RGB(195, 115, 50), RGB(42, 47, 59) };
static void s_draw(window_t *w, int x, int y) {
    gfx_text_bold(x + 20, y + 16, "Fond d'écran", DARK);
    for (int i = 0; i < 4; i++) {
        int bx = x + 20 + i * 100;
        gfx_fill(bx, y + 40, 88, 56, wall_prev[i]);
        gfx_rect(bx - 2, y + 38, 92, 60, wallpaper == i ? accent_color : RGB(214, 220, 230));
        if (wallpaper == i) gfx_rect(bx - 3, y + 37, 94, 62, accent_color);
    }
    gfx_text_bold(x + 20, y + 120, "Couleur d'accentuation", DARK);
    for (int i = 0; i < 6; i++) {
        int cx = x + 38 + i * 50;
        if (accent_color == accents[i]) gfx_disc(cx, y + 166, 19, DARK);
        gfx_disc(cx, y + 166, 16, accents[i]);
    }
    gfx_text_bold(x + 20, y + 206, "Disposition du clavier", DARK);
    ui_button(x + 20, y + 230, 160, 32, "AZERTY (français)", kb_azerty);
    ui_button(x + 190, y + 230, 160, 32, "QWERTY (US)", !kb_azerty);
    gfx_text(x + 20, y + 274, "Astuce : F12 bascule la disposition à tout moment.", MUTED);
}
static void s_mouse(window_t *w, int x, int y, int ev, int b) {
    if (ev != MEV_DOWN) return;
    for (int i = 0; i < 4; i++) if (ui_hit(x, y, 20 + i * 100, 40, 88, 56)) wallpaper = i;
    for (int i = 0; i < 6; i++) if (ui_hit(x, y, 22 + i * 50, 150, 32, 32)) accent_color = accents[i];
    if (ui_hit(x, y, 20, 230, 160, 32)) kb_azerty = 1;
    if (ui_hit(x, y, 190, 230, 160, 32)) kb_azerty = 0;
}
void app_settings(void) {
    window_t *w = wm_open("Paramètres", 430, 300, ICON_SETTINGS);
    if (!w) return;
    w->draw = s_draw; w->mouse = s_mouse;
}

/* =========================== À propos =========================== */
static void a_draw(window_t *w, int x, int y) {
    gfx_gradient_v(x, y, w->w, 90, accent_color, gfx_blend(accent_color, 0xFFFFFF, 90));
    gfx_text_bold(x + 24, y + 26, "TontonOS", 0xFFFFFF);
    gfx_text(x + 24, y + 48, "Version 1.0 - un système écrit à partir de zéro", RGB(235, 240, 255));
    char buf[80], tmp[12];
    int ly = y + 110;
    gfx_text(x + 24, ly, "Noyau : 32 bits, mode protégé x86", DARK);
    strcpy(buf, "Écran : "); itoa(scr_w, tmp); strcpy(buf + strlen(buf), tmp); strcpy(buf + strlen(buf), " × ");
    itoa(scr_h, tmp); strcpy(buf + strlen(buf), tmp); strcpy(buf + strlen(buf), " en 32 bits");
    gfx_text(x + 24, ly + 22, buf, DARK);
    strcpy(buf, "Mémoire : "); itoa(mem_total_kb / 1024, tmp); strcpy(buf + strlen(buf), tmp); strcpy(buf + strlen(buf), " Mo");
    gfx_text(x + 24, ly + 44, buf, DARK);
    gfx_text(x + 24, ly + 66, "Clavier, souris, fenêtres, 7 applications", DARK);
    gfx_text(x + 24, ly + 100, "Double-cliquez sur une icône du bureau", MUTED);
    gfx_text(x + 24, ly + 118, "ou ouvrez le menu Démarrer pour commencer.", MUTED);
    ui_button(x + w->w - 110, y + w->h - 44, 90, 30, "OK", 0);
}
static void a_mouse(window_t *w, int x, int y, int ev, int b) {
    if (ev == MEV_DOWN && ui_hit(x, y, w->w - 110, w->h - 44, 90, 30)) wm_close(w);
}
static void a_key(window_t *w, int k) { if (k == KEY_ENTER || k == KEY_ESC) wm_close(w); }
void app_about(void) {
    window_t *w = wm_open("À propos de TontonOS", 440, 300, ICON_ABOUT);
    if (!w) return;
    w->draw = a_draw; w->mouse = a_mouse; w->key = a_key;
}
