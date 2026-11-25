/* TontonOS - bureau, fenêtres, barre des tâches, menu Démarrer */
#include "wm.h"

#define MAXWIN 16
static window_t wins[MAXWIN];
static int zorder[MAXWIN], zcount;     /* zorder[zcount-1] = premier plan */
static int dirty = 1;
static int mx, my, mbtn;
static int drag_win = -1, drag_dx, drag_dy, capture_win = -1;
static int rs_win = -1, rs_mode, rs_sx, rs_sy, rs_w, rs_h;   /* redimensionnement en cours */
static int title_click_win = -1; static u32 title_click_tick;
#define GRIP 6        /* épaisseur de la zone de saisie des bords */
static int start_open, sm_sel = -1;   /* sm_sel : élément choisi au clavier */

static int last_click_tick, last_click_icon = -1, sel_icon = -1;
static int clock_min = -1;
u32 accent_color = RGB(37, 99, 235);
int wallpaper;

void wm_invalidate(void) { dirty = 1; }

/* ---- Icônes dessinées à la main ---- */
void draw_icon(int icon, int x, int y, int s) {
    int u = s / 16; if (u < 1) u = 1;
    switch (icon) {
    case ICON_TERM:
        gfx_fill(x, y + 2*u, s, s - 4*u, RGB(30, 34, 44));
        gfx_fill(x, y + 2*u, s, 2*u, RGB(80, 90, 110));
        gfx_line(x + 3*u, y + 6*u, x + 6*u, y + 9*u, RGB(80, 230, 120));
        gfx_line(x + 3*u, y + 12*u, x + 6*u, y + 9*u, RGB(80, 230, 120));
        gfx_fill(x + 7*u, y + 11*u, 5*u, u, RGB(80, 230, 120));
        break;
    case ICON_NOTES:
        gfx_fill(x + 2*u, y, s - 4*u, s, RGB(255, 255, 255));
        gfx_rect(x + 2*u, y, s - 4*u, s, RGB(120, 130, 150));
        gfx_fill(x + 2*u, y, s - 4*u, 3*u, RGB(250, 200, 60));
        for (int i = 0; i < 4; i++) gfx_fill(x + 4*u, y + (5 + 3*i)*u, s - 8*u, u, RGB(140, 160, 200));
        break;
    case ICON_CALC:
        gfx_fill(x + 2*u, y, s - 4*u, s, RGB(60, 70, 90));
        gfx_fill(x + 4*u, y + 2*u, s - 8*u, 3*u, RGB(170, 230, 190));
        for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++)
            gfx_fill(x + (4 + 3*i)*u, y + (7 + 3*j)*u, 2*u, 2*u, i == 2 ? RGB(250, 150, 50) : RGB(220, 225, 235));
        break;
    case ICON_PAINT:
        gfx_disc(x + s/2, y + s/2, s/2 - u, RGB(245, 235, 215));
        gfx_disc(x + 5*u, y + 6*u, 2*u, RGB(230, 60, 60));
        gfx_disc(x + 9*u, y + 4*u, 2*u, RGB(60, 120, 230));
        gfx_disc(x + 12*u, y + 8*u, 2*u, RGB(60, 190, 90));
        gfx_disc(x + 6*u, y + 11*u, 2*u, RGB(250, 200, 40));
        break;
    case ICON_MINES:
        gfx_fill(x + u, y + u, s - 2*u, s - 2*u, RGB(190, 200, 215));
        gfx_disc(x + s/2, y + s/2 + u, 4*u, RGB(30, 30, 30));
        gfx_fill(x + s/2 - u/2, y + 2*u, u > 1 ? u : 1, 4*u, RGB(30, 30, 30));
        gfx_disc(x + s/2 - u, y + s/2, u, RGB(255, 255, 255));
        break;
    case ICON_SETTINGS:
        gfx_disc(x + s/2, y + s/2, s/2 - u, RGB(120, 130, 150));
        for (int i = 0; i < 4; i++) {
            int d = s/2 - u;
            gfx_fill(x + s/2 - u, y + s/2 - d - u + (i == 1 ? 2*d : 0) * (i < 2), 2*u, 2*u, RGB(120, 130, 150));
        }
        gfx_disc(x + s/2, y + s/2, s/4, RGB(230, 235, 245));
        gfx_disc(x + s/2, y + s/2, s/8, RGB(120, 130, 150));
        break;
    case ICON_FOLDER:
        gfx_fill(x + u, y + 3*u, 6*u, 2*u, RGB(230, 170, 40));
        gfx_fill(x + u, y + 4*u, s - 2*u, s - 6*u, RGB(245, 190, 60));
        gfx_fill(x + u, y + 6*u, s - 2*u, s - 8*u, RGB(252, 211, 90));
        break;
    case ICON_FILE:
        gfx_fill(x + 3*u, y + u, s - 6*u, s - 2*u, 0xFFFFFF);
        gfx_rect(x + 3*u, y + u, s - 6*u, s - 2*u, RGB(140, 150, 170));
        for (int i = 0; i < 4; i++) gfx_fill(x + 5*u, y + (4 + 3*i)*u, s - 10*u, u, RGB(150, 170, 210));
        break;
    case ICON_ABOUT:
        gfx_disc(x + s/2, y + s/2, s/2 - u, accent_color);
        gfx_fill(x + s/2 - u, y + 7*u, 2*u, 6*u, 0xFFFFFF);
        gfx_fill(x + s/2 - u, y + 4*u, 2*u, 2*u, 0xFFFFFF);
        break;
    }
}

/* Logo TontonOS : pastille arrondie en dégradé avec un « T » blanc */
static void draw_logo(int x, int y, int s) {
    int r = s / 4;
    for (int j = 0; j < s; j++) {
        u32 c = gfx_blend(RGB(255, 140, 50), RGB(236, 72, 153), j * 255 / s);
        for (int i = 0; i < s; i++) {
            int dx = i < r ? r - i : (i >= s - r ? i - (s - r - 1) : 0);
            int dy = j < r ? r - j : (j >= s - r ? j - (s - r - 1) : 0);
            if (dx * dx + dy * dy <= r * r) gfx_pixel(x + i, y + j, c);
        }
    }
    int t = s / 7 > 0 ? s / 7 : 1;
    gfx_fill(x + s / 5, y + s / 5, s - 2 * (s / 5), t + 1, 0xFFFFFF);
    gfx_fill(x + s / 2 - t / 2 - 1, y + s / 5, t + 1, s - 2 * (s / 5), 0xFFFFFF);
}

/* ---- Widgets ---- */
int ui_hit(int px, int py, int x, int y, int w, int h) { return px >= x && py >= y && px < x + w && py < y + h; }
void ui_button(int x, int y, int w, int h, const char *label, int pressed) {
    gfx_fill(x, y, w, h, pressed ? RGB(205, 212, 225) : RGB(238, 241, 246));
    gfx_rect(x, y, w, h, RGB(180, 188, 204));
    gfx_text(x + (w - gfx_text_w(label)) / 2, y + (h - FONT_H) / 2, label, RGB(30, 35, 45));
}

/* ---- Fenêtres ---- */
static int win_index(window_t *w) { return w - wins; }
static void z_remove(int i) {
    for (int k = 0; k < zcount; k++)
        if (zorder[k] == i) { memmove(&zorder[k], &zorder[k + 1], (zcount - k - 1) * sizeof(int)); zcount--; return; }
}
static void z_top(int i) { z_remove(i); zorder[zcount++] = i; }
static int top_win(void) {
    for (int k = zcount - 1; k >= 0; k--) if (!wins[zorder[k]].minimized) return zorder[k];
    return -1;
}

window_t *wm_open(const char *title, int w, int h, int icon) {
    for (int i = 0; i < MAXWIN; i++) if (!wins[i].used) {
        window_t *win = &wins[i];
        memset(win, 0, sizeof(*win));
        win->used = 1; win->w = w; win->h = h; win->icon = icon;
        static int cascade;
        win->x = 120 + (cascade % 8) * 28;
        win->y = 50 + (cascade % 8) * 24;
        cascade++;
        if (win->x + w > scr_w) win->x = max(0, scr_w - w - 10);
        if (win->y + h + TITLE_H > scr_h - TASKBAR_H) win->y = max(0, scr_h - TASKBAR_H - h - TITLE_H - 10);
        strlcpy(win->title, title, sizeof(win->title));
        z_top(i);
        dirty = 1;
        return win;
    }
    return NULL;
}
void wm_close(window_t *w) {
    int i = win_index(w);
    if (w->close) w->close(w);
    if (w->data) kfree(w->data);
    w->used = 0;
    z_remove(i);
    if (drag_win == i) drag_win = -1;
    if (rs_win == i) rs_win = -1;
    if (capture_win == i) capture_win = -1;
    dirty = 1;
}

static void frame_rects(window_t *w, int *bx, int *by, int *bw, int *bh) {
    *bx = w->x; *by = w->y; *bw = w->w + 2 * BORDER; *bh = w->h + TITLE_H + BORDER;
}
#define BTN_W 40
static int nbtn(window_t *w) { return w->resizable ? 3 : 2; }

void wm_set_resizable(window_t *w, int min_w, int min_h) {
    w->resizable = 1; w->min_w = min_w; w->min_h = min_h;
}
static void toggle_max(window_t *w) {
    if (!w->resizable) return;
    if (w->maximized) {
        w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh; w->maximized = 0;
    } else {
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        w->x = 0; w->y = 0; w->w = scr_w - 2 * BORDER; w->h = scr_h - TASKBAR_H - TITLE_H - BORDER;
        w->maximized = 1;
    }
    dirty = 1;
}
/* Bords saisissables : 1 = droit, 2 = bas (3 = coin) */
static int resize_zone(window_t *w, int px, int py) {
    if (!w->resizable || w->maximized || w->minimized) return 0;
    int bx, by, bw, bh; frame_rects(w, &bx, &by, &bw, &bh);
    if (px < bx || py < by + TITLE_H || px >= bx + bw + GRIP || py >= by + bh + GRIP) return 0;
    int z = 0;
    if (px >= bx + bw - GRIP) z |= 1;
    if (py >= by + bh - GRIP) z |= 2;
    /* le coin est plus large pour être facile à attraper */
    if (px >= bx + bw - 16 && py >= by + bh - 16) z = 3;
    return z;
}
static void draw_window(int i, int active) {
    window_t *w = &wins[i];
    int x, y, fw, fh; frame_rects(w, &x, &y, &fw, &fh);
    gfx_shadow(x, y, fw, fh);
    u32 tb = active ? RGB(255, 255, 255) : RGB(236, 239, 244);
    gfx_fill(x, y, fw, TITLE_H, tb);
    gfx_rect(x, y, fw, fh, active ? accent_color : RGB(170, 178, 192));
    gfx_fill(x, y, fw, 2, active ? accent_color : RGB(170, 178, 192));
    draw_icon(w->icon, x + 8, y + 5, 16);
    gfx_clip(x, y, fw - nbtn(w) * BTN_W, TITLE_H);
    gfx_text(x + 32, y + 6, w->title, active ? RGB(20, 24, 32) : RGB(110, 118, 132));
    gfx_noclip();
    /* boutons réduire / fermer */
    int bx = x + fw - BTN_W;
    int hover_close = ui_hit(mx, my, bx, y + 2, BTN_W - 1, TITLE_H - 2);
    if (hover_close) gfx_fill(bx, y + 2, BTN_W - 1, TITLE_H - 2, RGB(220, 38, 38));
    u32 xc = hover_close ? 0xFFFFFF : RGB(60, 66, 78);
    for (int k = 0; k < 9; k++) { gfx_pixel(bx + 15 + k, y + 9 + k, xc); gfx_pixel(bx + 23 - k, y + 9 + k, xc);
                                  gfx_pixel(bx + 16 + k, y + 9 + k, xc); gfx_pixel(bx + 24 - k, y + 9 + k, xc); }
    int mbx = bx - BTN_W;
    if (w->resizable) {
        if (ui_hit(mx, my, mbx, y + 2, BTN_W, TITLE_H - 2)) gfx_fill(mbx, y + 2, BTN_W, TITLE_H - 2, RGB(222, 226, 234));
        if (w->maximized) {
            gfx_rect(mbx + 17, y + 7, 9, 9, RGB(60, 66, 78));
            gfx_fill(mbx + 14, y + 10, 9, 9, active ? RGB(255, 255, 255) : RGB(236, 239, 244));
            gfx_rect(mbx + 14, y + 10, 9, 9, RGB(60, 66, 78));
        } else gfx_rect(mbx + 15, y + 8, 11, 11, RGB(60, 66, 78));
        mbx -= BTN_W;
    }
    if (ui_hit(mx, my, mbx, y + 2, BTN_W, TITLE_H - 2)) gfx_fill(mbx, y + 2, BTN_W, TITLE_H - 2, RGB(222, 226, 234));
    gfx_fill(mbx + 15, y + 14, 10, 2, RGB(60, 66, 78));
    /* zone cliente */
    int cx = x + BORDER, cy = y + TITLE_H;
    gfx_clip(cx, cy, w->w, w->h);
    gfx_fill(cx, cy, w->w, w->h, RGB(250, 251, 253));
    if (w->draw) w->draw(w, cx, cy);
    gfx_noclip();
    /* poignée de redimensionnement dans le coin */
    if (w->resizable && !w->maximized)
        for (int k = 0; k < 3; k++)
            for (int j = 0; j <= k; j++)
                gfx_fill(x + fw - 5 - (k - j) * 4, y + fh - 5 - j * 4, 2, 2, RGB(150, 158, 176));
}

/* ---- Bureau ---- */
static const struct { const char *name; int icon; void (*open)(void); } desk[] = {
    { "Explorateur",  ICON_FOLDER,   app_explorer },
    { "Terminal",     ICON_TERM,     app_terminal },
    { "Bloc-notes",   ICON_NOTES,    app_notes },
    { "Calculatrice", ICON_CALC,     app_calc },
    { "Peinture",     ICON_PAINT,    app_paint },
    { "Mines",        ICON_MINES,    app_mines },
    { "Paramètres",   ICON_SETTINGS, app_settings },
    { "À propos",     ICON_ABOUT,    app_about },
};
#define NDESK (int)(sizeof(desk) / sizeof(desk[0]))
#define DI_W 88
#define DI_H 80
static void desk_icon_pos(int i, int *x, int *y) { *x = 16; *y = 16 + i * DI_H; }

static const u32 wallpapers[][2] = {
    { RGB(14, 42, 110), RGB(110, 50, 160) },
    { RGB(10, 90, 100), RGB(20, 160, 120) },
    { RGB(150, 60, 40), RGB(240, 170, 60) },
    { RGB(25, 28, 36),  RGB(60, 66, 82) },
};
#define NWALL 4

static void draw_desktop(void) {
    int h = scr_h - TASKBAR_H;
    gfx_gradient_v(0, 0, scr_w, h, wallpapers[wallpaper % NWALL][0], wallpapers[wallpaper % NWALL][1]);
    /* Lueur décorative et logo en filigrane */
    int lx = scr_w / 2 + 120, ly = h / 2 - 90;
    draw_logo(lx + 30, ly + 20, 120);
    gfx_text_bold(lx + 54, ly + 160, "TontonOS", 0xFFFFFF);
    gfx_text(lx + 54 + 72, ly + 160, "1.2", RGB(200, 210, 240));

    for (int i = 0; i < NDESK; i++) {
        int x, y; desk_icon_pos(i, &x, &y);
        if (i == sel_icon) {
            for (int j = 0; j < DI_H - 6; j++) for (int k = 0; k < DI_W; k++)
                gfx_pixel(x + k, y + j, gfx_blend(RGB(60, 70, 120), 0xFFFFFF, 60));
        }
        draw_icon(desk[i].icon, x + (DI_W - 40) / 2, y + 6, 40);
        int tw = gfx_text_w(desk[i].name);
        gfx_text(x + (DI_W - tw) / 2 + 1, y + 51, desk[i].name, 0);
        gfx_text(x + (DI_W - tw) / 2, y + 50, desk[i].name, 0xFFFFFF);
    }
}

/* ---- Barre des tâches ---- */
#define START_W 120
#define TB_BTN_W 170
static void task_btn_rect(int n, int *x) { *x = START_W + 10 + n * (TB_BTN_W + 4); }

static void draw_taskbar(void) {
    int y = scr_h - TASKBAR_H;
    gfx_fill(0, y, scr_w, TASKBAR_H, RGB(22, 26, 36));
    gfx_fill(0, y, scr_w, 1, RGB(60, 68, 88));
    /* bouton Démarrer */
    int hov = ui_hit(mx, my, 0, y, START_W, TASKBAR_H) || start_open;
    if (hov) gfx_fill(0, y + 1, START_W, TASKBAR_H - 1, RGB(48, 56, 76));
    draw_logo(10, y + 8, 20);
    gfx_text_bold(38, y + 10, "Démarrer", 0xFFFFFF);
    /* fenêtres ouvertes */
    int top = top_win(), n = 0;
    for (int i = 0; i < MAXWIN; i++) if (wins[i].used) {
        int x; task_btn_rect(n++, &x);
        if (x + TB_BTN_W > scr_w - 130) break;
        int act = i == top;
        gfx_fill(x, y + 4, TB_BTN_W, TASKBAR_H - 8, act ? RGB(58, 68, 92) : RGB(36, 42, 56));
        if (act) gfx_fill(x, y + TASKBAR_H - 6, TB_BTN_W, 2, accent_color);
        draw_icon(wins[i].icon, x + 6, y + 10, 16);
        char t[18]; strlcpy(t, wins[i].title, sizeof(t));
        gfx_text(x + 28, y + 10, t, wins[i].minimized ? RGB(150, 158, 176) : 0xFFFFFF);
    }
    /* horloge */
    int h, m, d, mo, yr; rtc_read(&h, &m, NULL, &d, &mo, &yr);
    char buf[16], tmp[8];
    utoa_pad(h, buf, 2); strcpy(buf + 2, ":"); utoa_pad(m, tmp, 2); strcpy(buf + 3, tmp);
    gfx_text(scr_w - 20 - gfx_text_w(buf), y + 2, buf, 0xFFFFFF);
    utoa_pad(d, buf, 2); buf[2] = '/'; utoa_pad(mo, buf + 3, 2); buf[5] = '/'; utoa_pad(yr, buf + 6, 4);
    gfx_text(scr_w - 20 - gfx_text_w(buf), y + 18, buf, RGB(170, 178, 196));
    char kb[8]; strcpy(kb, kb_azerty ? "FR" : "US");
    gfx_text(scr_w - 130, y + 10, kb, RGB(170, 178, 196));
}

/* ---- Menu Démarrer ---- */
#define SM_W 280
#define SM_ITEM 38
#define SM_N (NDESK + 2)
static int sm_y(void) { return scr_h - TASKBAR_H - (SM_N * SM_ITEM + 70); }
static void draw_start_menu(void) {
    int x = 4, y = sm_y(), h = SM_N * SM_ITEM + 66;
    gfx_shadow(x, y, SM_W, h);
    gfx_fill(x, y, SM_W, h, RGB(32, 38, 52));
    gfx_rect(x, y, SM_W, h, RGB(70, 80, 104));
    gfx_fill(x, y, SM_W, 50, accent_color);
    draw_logo(x + 12, y + 10, 30);
    gfx_text_bold(x + 54, y + 9, "TontonOS", 0xFFFFFF);
    gfx_text(x + 54, y + 26, "Bonjour, Tonton !", RGB(220, 230, 255));
    for (int i = 0; i < SM_N; i++) {
        int iy = y + 58 + i * SM_ITEM;
        if (i == sm_sel || (sm_sel < 0 && ui_hit(mx, my, x, iy, SM_W, SM_ITEM))) gfx_fill(x + 4, iy, SM_W - 8, SM_ITEM - 2, RGB(56, 66, 90));
        if (i < NDESK) {
            draw_icon(desk[i].icon, x + 14, iy + 7, 24);
            gfx_text(x + 50, iy + 11, desk[i].name, 0xFFFFFF);
        } else {
            if (i == NDESK) gfx_fill(x + 10, iy - 2, SM_W - 20, 1, RGB(70, 80, 104));
            u32 col = i == NDESK ? RGB(250, 180, 60) : RGB(240, 80, 80);
            gfx_disc(x + 26, iy + 18, 9, col);
            gfx_disc(x + 26, iy + 18, 5, RGB(32, 38, 52));
            gfx_fill(x + 25, iy + 7, 3, 9, col);
            gfx_text(x + 50, iy + 11, i == NDESK ? "Redémarrer" : "Éteindre", 0xFFFFFF);
        }
    }
}
static void start_menu_activate(int i) {
    start_open = 0; sm_sel = -1; dirty = 1;
    if (i < NDESK) desk[i].open();
    else if (i == NDESK) reboot();
    else shutdown();
}
static int start_menu_click(void) {
    int x = 4, y = sm_y();
    if (!ui_hit(mx, my, x, y, SM_W, SM_N * SM_ITEM + 66)) return 0;
    int i = (my - y - 58) / SM_ITEM;
    if (my >= y + 58 && i >= 0 && i < SM_N) start_menu_activate(i);
    return 1;
}

/* ---- Rendu complet ---- */
static void render(void) {
    gfx_noclip();
    draw_desktop();
    int top = top_win();
    for (int k = 0; k < zcount; k++) {
        int i = zorder[k];
        if (!wins[i].minimized) draw_window(i, i == top);
    }
    draw_taskbar();
    if (start_open) draw_start_menu();
    gfx_present();
}

/* ---- Souris ---- */
static int win_at(int x, int y) {
    for (int k = zcount - 1; k >= 0; k--) {
        window_t *w = &wins[zorder[k]];
        if (w->minimized) continue;
        int bx, by, bw, bh; frame_rects(w, &bx, &by, &bw, &bh);
        if (ui_hit(x, y, bx, by, bw, bh) || resize_zone(w, x, y)) return zorder[k];
    }
    return -1;
}

static void on_press(int button) {
    if (start_open) {
        if (start_menu_click()) { dirty = 1; return; }
        start_open = 0; dirty = 1;
        if (my >= scr_h - TASKBAR_H && mx < START_W) return;
    }
    if (my >= scr_h - TASKBAR_H) {
        if (button != 1) return;
        if (mx < START_W) { start_open = 1; sm_sel = -1; dirty = 1; return; }
        int n = 0;
        for (int i = 0; i < MAXWIN; i++) if (wins[i].used) {
            int x; task_btn_rect(n++, &x);
            if (ui_hit(mx, my, x, scr_h - TASKBAR_H, TB_BTN_W, TASKBAR_H)) {
                if (wins[i].minimized) { wins[i].minimized = 0; z_top(i); }
                else if (top_win() == i) wins[i].minimized = 1;
                else z_top(i);
                dirty = 1; return;
            }
        }
        return;
    }
    int i = win_at(mx, my);
    if (i >= 0) {
        window_t *w = &wins[i];
        if (top_win() != i) { z_top(i); dirty = 1; }
        int bx, by, bw, bh; frame_rects(w, &bx, &by, &bw, &bh);
        int rz = button == 1 ? resize_zone(w, mx, my) : 0;
        if (rz) {
            rs_win = i; rs_mode = rz; rs_sx = mx; rs_sy = my; rs_w = w->w; rs_h = w->h;
            return;
        }
        if (my < by + TITLE_H) {
            if (button != 1) return;
            if (mx >= bx + bw - BTN_W) { wm_close(w); return; }
            if (w->resizable && mx >= bx + bw - 2 * BTN_W) { toggle_max(w); return; }
            if (mx >= bx + bw - nbtn(w) * BTN_W) { w->minimized = 1; dirty = 1; return; }
            /* double-clic sur la barre de titre : agrandir / restaurer */
            if (title_click_win == i && ticks - title_click_tick < 60) {
                title_click_win = -1; toggle_max(w); return;
            }
            title_click_win = i; title_click_tick = ticks;
            drag_win = i; drag_dx = mx - w->x; drag_dy = my - w->y;
            return;
        }
        if (mx >= bx + bw || my >= by + bh) return;   /* marge de saisie hors de la fenêtre */
        int cx = mx - bx - BORDER, cy = my - by - TITLE_H;
        if (w->mouse && cx >= 0 && cy >= 0 && cx < w->w && cy < w->h) {
            capture_win = i;
            w->mouse(w, cx, cy, button == 1 ? MEV_DOWN : MEV_RDOWN, mbtn);
            dirty = 1;
        }
        return;
    }
    /* clic sur le bureau */
    int hit = -1;
    for (int k = 0; k < NDESK; k++) {
        int x, y; desk_icon_pos(k, &x, &y);
        if (ui_hit(mx, my, x, y, DI_W, DI_H - 6)) hit = k;
    }
    if (hit >= 0 && hit == last_click_icon && ticks - last_click_tick < 60) {
        desk[hit].open(); last_click_icon = -1;
    } else { last_click_icon = hit; last_click_tick = ticks; }
    if (sel_icon != hit) { sel_icon = hit; dirty = 1; }
}

static void handle_mouse(mouse_ev_t *e) {
    int old = mbtn;
    mx += e->dx; my += e->dy;
    if (mx < 0) mx = 0; if (my < 0) my = 0;
    if (mx >= scr_w) mx = scr_w - 1; if (my >= scr_h) my = scr_h - 1;
    mbtn = e->buttons;
    if ((mbtn & 1) && !(old & 1)) on_press(1);
    if ((mbtn & 2) && !(old & 2)) on_press(2);
    if (!(mbtn & 1) && (old & 1)) {
        drag_win = -1;
        if (rs_win >= 0) dirty = 1;
        rs_win = -1;
        if (capture_win >= 0 && wins[capture_win].used && wins[capture_win].mouse) {
            window_t *w = &wins[capture_win];
            w->mouse(w, mx - w->x - BORDER, my - w->y - TITLE_H, MEV_UP, mbtn);
            dirty = 1;
        }
        capture_win = -1;
    }
    if (e->dx || e->dy) {
        if (rs_win >= 0) {
            window_t *w = &wins[rs_win];
            if (rs_mode & 1) w->w = max(w->min_w, min(rs_w + mx - rs_sx, scr_w - w->x - 2 * BORDER));
            if (rs_mode & 2) w->h = max(w->min_h, min(rs_h + my - rs_sy, scr_h - TASKBAR_H - w->y - TITLE_H - BORDER));
            dirty = 1;
        } else if (drag_win >= 0) {
            window_t *w = &wins[drag_win];
            if (w->maximized) {
                /* tirer une fenêtre agrandie la restaure sous le curseur */
                int fw = w->w + 2 * BORDER;
                toggle_max(w);
                drag_dx = drag_dx * (w->w + 2 * BORDER) / fw;
            }
            title_click_win = -1;
            w->x = mx - drag_dx; w->y = my - drag_dy;
            if (w->y < 0) w->y = 0;
            if (w->y > scr_h - TASKBAR_H - TITLE_H) w->y = scr_h - TASKBAR_H - TITLE_H;
            dirty = 1;
        } else if (capture_win >= 0 && (mbtn & 1)) {
            window_t *w = &wins[capture_win];
            w->mouse(w, mx - w->x - BORDER, my - w->y - TITLE_H, MEV_MOVE, mbtn);
            dirty = 1;
        } else if (my < 60 + scr_h || start_open) {
            /* survol : la barre de titre et le menu réagissent */
            static int last_zone = -2;
            int zone = start_open ? (my - sm_y()) / SM_ITEM + 100 : win_at(mx, my) * 1000 + (my / 8) * 0;
            int over_btns = 0;
            int t = win_at(mx, my);
            if (t >= 0) { window_t *w = &wins[t]; int bx = w->x + w->w + 2 * BORDER;
                if (my < w->y + TITLE_H && mx >= bx - nbtn(w) * BTN_W) over_btns = 1 + (bx - mx - 1) / BTN_W; }
            zone = zone * 8 + over_btns + (my >= scr_h - TASKBAR_H && mx < START_W) * 3;
            if (zone != last_zone) { last_zone = zone; dirty = 1; }
        }
    }
}

/* ---- Clavier : raccourcis du système ---- */
static void set_rect(window_t *w, int x, int y, int cw, int ch) {
    if (!w->maximized) { w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h; }
    w->x = x; w->y = y; w->w = cw; w->h = ch; w->maximized = 1;
}
static int wm_shortcut(int k) {
    int base = k & 0xFFFF, t = top_win();
    /* Menu Démarrer : touche Windows, puis flèches / Entrée / Échap */
    if (k == KEY_WIN || k == (KEY_ESC | KMOD_CTRL)) {
        start_open = !start_open; sm_sel = start_open ? 0 : -1; return 1;
    }
    if (start_open) {
        if (base == KEY_DOWN) sm_sel = (sm_sel + 1) % SM_N;
        else if (base == KEY_UP) sm_sel = (sm_sel + SM_N - 1) % SM_N;
        else if (base == KEY_ENTER && sm_sel >= 0) start_menu_activate(sm_sel);
        else if (base == KEY_ESC) { start_open = 0; sm_sel = -1; }
        return 1;
    }
    /* Alt+Tab : passer à la fenêtre suivante (la plus ancienne au premier plan) */
    if (k == (KEY_TAB | KMOD_ALT)) {
        if (zcount < 1) return 1;
        int i = zorder[0];
        if (zcount > 1 && i == t) i = zorder[1];
        wins[i].minimized = 0; z_top(i);
        return 1;
    }
    if (k & KMOD_WIN) {
        /* Windows + D : afficher le bureau / revenir */
        if (base == 'd') {
            int any = 0;
            for (int i = 0; i < MAXWIN; i++) if (wins[i].used && !wins[i].minimized) any = 1;
            for (int i = 0; i < MAXWIN; i++) if (wins[i].used) wins[i].minimized = any;
            if (any && sel_icon < 0) sel_icon = 0;
            return 1;
        }
        if (t < 0) return 1;
        window_t *w = &wins[t];
        int full_h = scr_h - TASKBAR_H - TITLE_H - BORDER;
        if (base == KEY_UP) { if (!w->maximized) toggle_max(w); else if (w->resizable && w->w < scr_w - 2 * BORDER) { toggle_max(w); toggle_max(w); } }
        else if (base == KEY_DOWN) { if (w->maximized) toggle_max(w); else w->minimized = 1; }
        else if ((base == KEY_LEFT || base == KEY_RIGHT) && w->resizable) {
            int half = scr_w / 2;
            set_rect(w, base == KEY_LEFT ? 0 : half, 0, half - 2 * BORDER, full_h);
        }
        return 1;
    }
    return 0;
}

/* Bureau sans fenêtre : flèches pour choisir une icône, Entrée pour l'ouvrir */
static void desktop_key(int k) {
    if (k == KEY_DOWN || k == KEY_RIGHT) sel_icon = sel_icon < 0 ? 0 : (sel_icon + 1) % NDESK;
    else if (k == KEY_UP || k == KEY_LEFT) sel_icon = sel_icon < 0 ? 0 : (sel_icon + NDESK - 1) % NDESK;
    else if (k == KEY_ENTER && sel_icon >= 0) desk[sel_icon].open();
    else return;
    dirty = 1;
}

/* ---- Boucle principale ---- */
void wm_init(void) {
    mx = scr_w / 2; my = scr_h / 2;
    app_about();
}

void wm_run(void) {
    u32 last_tick = 0;
    for (;;) {
        mouse_ev_t e; int k, any = 0;
        while (mouse_get(&e)) { handle_mouse(&e); any = 1; }
        while ((k = keyboard_get()) >= 0) {
            any = 1;
            if (k == KEY_F12) { kb_azerty = !kb_azerty; dirty = 1; continue; }
            if ((k & KMOD_ALT) && (k & 0xFFFF) == KEY_F4) {
                int t = top_win(); if (t >= 0) wm_close(&wins[t]); continue;
            }
            if (wm_shortcut(k)) { dirty = 1; continue; }
            int t = top_win();
            if (t >= 0 && wins[t].key) { wins[t].key(&wins[t], k); dirty = 1; }
            else if (t < 0) desktop_key(k);
        }
        if (ticks != last_tick) {
            u32 now = ticks;
            if (now / 10 != last_tick / 10) {
                for (int i = 0; i < MAXWIN; i++) if (wins[i].used && wins[i].tick) wins[i].tick(&wins[i]);
                int m; rtc_read(NULL, &m, NULL, NULL, NULL, NULL);
                if (m != clock_min) { clock_min = m; dirty = 1; }
            }
            last_tick = now;
        }
        if (dirty) { dirty = 0; render(); }
        {
            int shape = 0, t = win_at(mx, my);
            int rz = rs_win >= 0 ? rs_mode : (drag_win < 0 && capture_win < 0 && !start_open && t >= 0) ? resize_zone(&wins[t], mx, my) : 0;
            if (rz) shape = rz == 3 ? 1 : rz == 1 ? 2 : 3;
            gfx_cursor_shape(shape);
        }
        gfx_cursor(mx, my);
        if (!any) __asm__ volatile("sti; hlt");
    }
}
