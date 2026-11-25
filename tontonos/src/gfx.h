/* TontonOS — graphismes (framebuffer + double tampon) */
#ifndef GFX_H
#define GFX_H
#include "kernel.h"

#define RGB(r, g, b) (((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define FONT_W 8
#define FONT_H 16

extern int scr_w, scr_h;

int  gfx_init(u32 addr, u32 pitch, u32 w, u32 h, u32 bpp);
void gfx_clip(int x, int y, int w, int h);
void gfx_noclip(void);
void gfx_pixel(int x, int y, u32 c);
void gfx_fill(int x, int y, int w, int h, u32 c);
void gfx_rect(int x, int y, int w, int h, u32 c);
void gfx_gradient_v(int x, int y, int w, int h, u32 c1, u32 c2);
void gfx_line(int x0, int y0, int x1, int y1, u32 c);
void gfx_disc(int cx, int cy, int r, u32 c);
void gfx_char(int x, int y, u8 ch, u32 fg);
void gfx_char_scaled(int x, int y, u8 ch, u32 fg, int sx, int sy);
void gfx_text(int x, int y, const char *s, u32 fg);
void gfx_text_bold(int x, int y, const char *s, u32 fg);
int  gfx_text_w(const char *s);
void gfx_blit(int x, int y, int w, int h, const u32 *src);
void gfx_shadow(int x, int y, int w, int h);
void gfx_present(void);
void gfx_cursor(int x, int y);
void gfx_cursor_shape(int shape);     /* 0 flèche, 1 diagonale, 2 horizontale, 3 verticale */
u32  gfx_blend(u32 a, u32 b, int alpha);   /* alpha 0..255 de b sur a */

#endif
