/* TontonOS — gestionnaire de fenêtres */
#ifndef WM_H
#define WM_H
#include "kernel.h"
#include "gfx.h"

#define TITLE_H   26
#define BORDER    1
#define TASKBAR_H 36

enum { MEV_DOWN, MEV_UP, MEV_MOVE, MEV_RDOWN };

typedef struct window window_t;
struct window {
    int  used, x, y, w, h;          /* w,h = zone cliente */
    int  minimized;
    char title[40];
    u32  accent;                    /* couleur de l'icône */
    int  icon;
    void *data;
    void (*draw)(window_t *, int cx, int cy);           /* cx,cy = origine cliente */
    void (*key)(window_t *, int k);
    void (*mouse)(window_t *, int x, int y, int ev, int buttons);
    void (*tick)(window_t *);
    void (*close)(window_t *);
};

window_t *wm_open(const char *title, int w, int h, int icon);
void      wm_close(window_t *w);
void      wm_invalidate(void);
void      wm_init(void);
void      wm_run(void);
void      draw_icon(int icon, int x, int y, int size);
extern u32 accent_color;
extern int wallpaper;

/* Icônes */
enum { ICON_TERM, ICON_NOTES, ICON_CALC, ICON_PAINT, ICON_MINES, ICON_SETTINGS, ICON_ABOUT, ICON_FOLDER, ICON_FILE, ICON_COUNT };

/* Applications (apps.c) */
void app_terminal(void);
void app_notes(void);
struct fnode;
void app_notes_open(struct fnode *f);
void app_explorer(void);
void app_calc(void);
void app_paint(void);
void app_mines(void);
void app_settings(void);
void app_about(void);

/* Widgets */
void ui_button(int x, int y, int w, int h, const char *label, int pressed);
int  ui_hit(int px, int py, int x, int y, int w, int h);

#endif
