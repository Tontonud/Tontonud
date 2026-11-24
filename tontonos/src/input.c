/* TontonOS — pilotes clavier (AZERTY/QWERTY) et souris PS/2 */
#include "kernel.h"

/* ================= Clavier ================= */
int kb_azerty = 1;
static int shift, ctrl, alt, altgr, caps, ext, dead;

#define QSIZE 128
static volatile int kq[QSIZE];
static volatile int kq_head, kq_tail;
static void kpush(int k) { int n = (kq_head + 1) % QSIZE; if (n != kq_tail) { kq[kq_head] = k; kq_head = n; } }

/* Tables indexées par code de balayage (jeu 1), de 0x00 à 0x39 + 0x56 */
static const char *us_n = "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char *us_s = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static const char *fr_n = "\0\033&é\"'(-è_çà)=\b\tazertyuiop^$\n\0qsdfghjklmù²\0*wxcvbn,;:!\0*\0 ";
static const char *fr_s = "\0\0331234567890°+\b\tAZERTYUIOP¨£\n\0QSDFGHJKLM%³\0µWXCVBN?./§\0*\0 ";
static const char *fr_g = "\0\0\0~#{[|`\\^@]}\0\0\0\0\0\0\0\0\0\0\0\0\0¤\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static int compose(int d, int c) {
    static const char *base = "aeiouAEIOU";
    static const char *circ = "âêîôûÂÊÎÔÛ";
    static const char *trem = "äëïöüÄËÏÖÜ";
    for (int i = 0; base[i]; i++)
        if (base[i] == c) return (u8)(d == '^' ? circ[i] : trem[i]);
    return -1;
}

static void kb_irq(regs_t *r) {
    u8 sc = inb(0x60);
    if (sc == 0xE0) { ext = 1; return; }
    int rel = sc & 0x80; sc &= 0x7F;
    if (ext) {
        ext = 0;
        if (sc == 0x38) { altgr = !rel; return; }
        if (sc == 0x1D) { ctrl = !rel; return; }
        if (sc == 0x2A || sc == 0x36) return;     /* faux décalages */
        if (rel) return;
        int k = 0;
        switch (sc) {
        case 0x48: k = KEY_UP; break;    case 0x50: k = KEY_DOWN; break;
        case 0x4B: k = KEY_LEFT; break;  case 0x4D: k = KEY_RIGHT; break;
        case 0x47: k = KEY_HOME; break;  case 0x4F: k = KEY_END; break;
        case 0x53: k = KEY_DEL; break;   case 0x49: k = KEY_PGUP; break;
        case 0x51: k = KEY_PGDN; break;  case 0x1C: k = KEY_ENTER; break;
        case 0x35: k = '/'; break;
        }
        if (k) kpush(k | (alt ? KMOD_ALT : 0) | (ctrl ? KMOD_CTRL : 0));
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = !rel; return; }
    if (sc == 0x1D) { ctrl = !rel; return; }
    if (sc == 0x38) { alt = !rel; return; }
    if (rel) return;
    if (sc == 0x3A) { caps = !caps; return; }
    if (sc >= 0x3B && sc <= 0x44) { kpush(KEY_F1 + sc - 0x3B); return; }
    if (sc == 0x57) { kpush(KEY_F11); return; }
    if (sc == 0x58) { kpush(KEY_F12); return; }

    int c = 0;
    if (sc >= 0x47 && sc <= 0x53) {          /* pavé numérique */
        c = (u8)"789-456+1230,"[sc - 0x47];
        if (!kb_azerty && c == ',') c = '.';
        kpush(c);
        return;
    }
    if (sc == 0x56) c = kb_azerty ? (shift ? '>' : '<') : (shift ? '|' : '\\');
    else if (sc <= 0x39) {
        int up = shift;
        const char *tbl;
        if (kb_azerty && altgr) tbl = fr_g;
        else {
            /* Verr. Maj ne touche que les lettres */
            char base = (kb_azerty ? fr_n : us_n)[sc];
            if (caps && ((base >= 'a' && base <= 'z'))) up = !up;
            tbl = kb_azerty ? (up ? fr_s : fr_n) : (up ? us_s : us_n);
        }
        c = (u8)tbl[sc];
    }
    if (!c) return;
    /* Touches mortes ^ et ¨ (AZERTY) */
    if (kb_azerty && !altgr && sc == 0x1A) {
        if (dead) { kpush(dead); dead = 0; }
        dead = c == '^' ? '^' : 0xA8;
        return;
    }
    if (dead) {
        int d = dead; dead = 0;
        int k = compose(d == '^' ? '^' : 0, c);
        if (k >= 0) { kpush(k); return; }
        kpush(d);
        if (c == ' ') return;
    }
    if (ctrl && c >= 'a' && c <= 'z') { kpush(c | KMOD_CTRL); return; }
    kpush(c | (alt ? KMOD_ALT : 0));
}

int keyboard_get(void) {
    if (kq_head == kq_tail) return -1;
    int k = kq[kq_tail]; kq_tail = (kq_tail + 1) % QSIZE;
    return k;
}

/* ================= Contrôleur PS/2 ================= */
static void ps2_wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) ; }
static void ps2_wait_read(void)  { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++) ; }

void keyboard_init(void) {
    while (inb(0x64) & 1) inb(0x60);
    irq_set_handler(1, kb_irq);
}

/* ================= Souris ================= */
static volatile mouse_ev_t mq[QSIZE];
static volatile int mq_head, mq_tail;
static u8 pkt[3];
static int pkt_i;

static void mouse_write(u8 v) {
    ps2_wait_write(); outb(0x64, 0xD4);
    ps2_wait_write(); outb(0x60, v);
    ps2_wait_read(); inb(0x60);          /* acquittement */
}

static void mouse_irq(regs_t *r) {
    u8 st = inb(0x64);
    if (!(st & 0x20)) { inb(0x60); return; }
    u8 b = inb(0x60);
    if (pkt_i == 0 && !(b & 0x08)) return;  /* resynchronisation */
    pkt[pkt_i++] = b;
    if (pkt_i < 3) return;
    pkt_i = 0;
    if (pkt[0] & 0xC0) return;             /* débordement */
    int dx = pkt[1] - ((pkt[0] & 0x10) ? 256 : 0);
    int dy = pkt[2] - ((pkt[0] & 0x20) ? 256 : 0);
    int n = (mq_head + 1) % QSIZE;
    if (n != mq_tail) {
        mq[mq_head].dx = dx; mq[mq_head].dy = -dy; mq[mq_head].buttons = pkt[0] & 7;
        mq_head = n;
    }
}

void mouse_init(void) {
    ps2_wait_write(); outb(0x64, 0xA8);          /* activer le port auxiliaire */
    ps2_wait_write(); outb(0x64, 0x20);
    ps2_wait_read(); u8 cfg = inb(0x60);
    cfg |= 0x02; cfg &= ~0x20;                   /* IRQ12 activée, horloge souris activée */
    ps2_wait_write(); outb(0x64, 0x60);
    ps2_wait_write(); outb(0x60, cfg);
    mouse_write(0xF6);                           /* réglages par défaut */
    mouse_write(0xF4);                           /* envoi des paquets */
    irq_set_handler(12, mouse_irq);
}

int mouse_get(mouse_ev_t *ev) {
    if (mq_head == mq_tail) return 0;
    ev->dx = mq[mq_tail].dx; ev->dy = mq[mq_tail].dy; ev->buttons = mq[mq_tail].buttons;
    mq_tail = (mq_tail + 1) % QSIZE;
    return 1;
}
