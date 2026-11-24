/* TontonOS — moteur graphique */
#include "gfx.h"
#include "font.h"

int scr_w, scr_h;
static u8  *fb;
static u32  fb_pitch, fb_bpp;
static u32 *back;                 /* tampon arrière (scène) */
static int  cx0, cy0, cx1, cy1;   /* rectangle de découpe */

int gfx_init(u32 addr, u32 pitch, u32 w, u32 h, u32 bpp) {
    fb = (u8 *)addr; fb_pitch = pitch; fb_bpp = bpp;
    scr_w = w; scr_h = h;
    back = kmalloc(w * h * 4);
    if (!back) return 0;
    gfx_noclip();
    return 1;
}
void gfx_clip(int x, int y, int w, int h) {
    cx0 = max(x, 0); cy0 = max(y, 0);
    cx1 = min(x + w, scr_w); cy1 = min(y + h, scr_h);
}
void gfx_noclip(void) { cx0 = 0; cy0 = 0; cx1 = scr_w; cy1 = scr_h; }

void gfx_pixel(int x, int y, u32 c) {
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) back[y * scr_w + x] = c;
}
void gfx_fill(int x, int y, int w, int h, u32 c) {
    int x0 = max(x, cx0), y0 = max(y, cy0), x1 = min(x + w, cx1), y1 = min(y + h, cy1);
    for (int j = y0; j < y1; j++) {
        u32 *p = back + j * scr_w + x0;
        for (int i = x0; i < x1; i++) *p++ = c;
    }
}
void gfx_rect(int x, int y, int w, int h, u32 c) {
    gfx_fill(x, y, w, 1, c); gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y, 1, h, c); gfx_fill(x + w - 1, y, 1, h, c);
}
u32 gfx_blend(u32 a, u32 b, int al) {
    u32 r = (((a >> 16) & 255) * (255 - al) + ((b >> 16) & 255) * al) / 255;
    u32 g = (((a >> 8) & 255) * (255 - al) + ((b >> 8) & 255) * al) / 255;
    u32 bl = ((a & 255) * (255 - al) + (b & 255) * al) / 255;
    return RGB(r, g, bl);
}
void gfx_gradient_v(int x, int y, int w, int h, u32 c1, u32 c2) {
    for (int j = 0; j < h; j++) gfx_fill(x, y + j, w, 1, gfx_blend(c1, c2, h > 1 ? j * 255 / (h - 1) : 0));
}
void gfx_line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void gfx_disc(int cx, int cy, int r, u32 c) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r + r) gfx_pixel(cx + i, cy + j, c);
}
void gfx_char(int x, int y, u8 ch, u32 fg) {
    const u8 *g = font8x16[ch];
    if (x + FONT_W <= cx0 || x >= cx1 || y + FONT_H <= cy0 || y >= cy1) return;
    for (int j = 0; j < FONT_H; j++)
        for (int i = 0; i < FONT_W; i++)
            if (g[j] & (0x80 >> i)) gfx_pixel(x + i, y + j, fg);
}
void gfx_text(int x, int y, const char *s, u32 fg) {
    for (; *s; s++, x += FONT_W) gfx_char(x, y, (u8)*s, fg);
}
void gfx_text_bold(int x, int y, const char *s, u32 fg) { gfx_text(x, y, s, fg); gfx_text(x + 1, y, s, fg); }
int gfx_text_w(const char *s) { return strlen(s) * FONT_W; }

void gfx_blit(int x, int y, int w, int h, const u32 *src) {
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < cy0 || yy >= cy1) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx >= cx0 && xx < cx1) back[yy * scr_w + xx] = src[j * w + i];
        }
    }
}
void gfx_shadow(int x, int y, int w, int h) {
    for (int k = 1; k <= 6; k++) {
        int al = 60 - k * 9;
        int x0 = max(x + k, cx0), x1 = min(x + w + k, cx1);
        int yy = y + h + k - 1;
        if (yy >= cy0 && yy < cy1)
            for (int i = x0; i < x1; i++) back[yy * scr_w + i] = gfx_blend(back[yy * scr_w + i], 0, al);
        int xx = x + w + k - 1;
        if (xx >= cx0 && xx < cx1)
            for (int j = max(y + k, cy0); j < min(y + h + k - 1, cy1); j++)
                back[j * scr_w + xx] = gfx_blend(back[j * scr_w + xx], 0, al);
    }
}

/* ---- Copie vers l'écran ---- */
static void copy_rect(int x, int y, int w, int h) {
    int x0 = max(x, 0), y0 = max(y, 0), x1 = min(x + w, scr_w), y1 = min(y + h, scr_h);
    if (x0 >= x1) return;
    for (int j = y0; j < y1; j++) {
        u32 *src = back + j * scr_w + x0;
        if (fb_bpp == 32) memcpy(fb + j * fb_pitch + x0 * 4, src, (x1 - x0) * 4);
        else {
            u8 *d = fb + j * fb_pitch + x0 * 3;
            for (int i = x0; i < x1; i++, src++) { d[0] = *src; d[1] = *src >> 8; d[2] = *src >> 16; d += 3; }
        }
    }
}
static void fb_pixel(int x, int y, u32 c) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) return;
    if (fb_bpp == 32) *(u32 *)(fb + y * fb_pitch + x * 4) = c;
    else { u8 *d = fb + y * fb_pitch + x * 3; d[0] = c; d[1] = c >> 8; d[2] = c >> 16; }
}

/* Curseur flèche : 1 = contour noir, 2 = blanc */
static const char *cursor_img[] = {
    "1", "11", "121", "1221", "12221", "122221", "1222221", "12222221",
    "122222221", "1222222221", "12222211111", "1221221", "121 1221", "11  1221",
    "1    1221", "     1221", "      11",
};
#define CUR_H 17
#define CUR_W 11
static int cur_x = -100, cur_y = -100;

static void draw_cursor_fb(void) {
    for (int j = 0; j < CUR_H; j++)
        for (int i = 0; cursor_img[j][i]; i++) {
            char c = cursor_img[j][i];
            if (c == '1') fb_pixel(cur_x + i, cur_y + j, 0);
            else if (c == '2') fb_pixel(cur_x + i, cur_y + j, 0xFFFFFF);
        }
}
void gfx_present(void) { copy_rect(0, 0, scr_w, scr_h); draw_cursor_fb(); }
void gfx_cursor(int x, int y) {
    if (x == cur_x && y == cur_y) return;
    copy_rect(cur_x, cur_y, CUR_W, CUR_H);
    cur_x = x; cur_y = y;
    draw_cursor_fb();
}
void gfx_char_scaled(int x, int y, u8 ch, u32 fg, int sx, int sy) {
    const u8 *g = font8x16[ch];
    for (int j = 0; j < FONT_H; j++)
        for (int i = 0; i < FONT_W; i++)
            if (g[j] & (0x80 >> i)) gfx_fill(x + i * sx, y + j * sy, sx, sy, fg);
}
