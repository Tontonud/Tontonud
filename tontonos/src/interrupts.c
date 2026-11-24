/* TontonOS - GDT, IDT, PIC, minuterie, horloge, écran bleu */
#include "kernel.h"
#include "gfx.h"

/* ---- GDT ---- */
struct gdt_entry { u16 lim; u16 base_lo; u8 base_mid; u8 access; u8 gran; u8 base_hi; } __attribute__((packed));
struct dt_ptr { u16 limit; u32 base; } __attribute__((packed));
static struct gdt_entry gdt[3];
static struct dt_ptr gdtp;
extern void gdt_flush(struct dt_ptr *);

static void gdt_set(int i, u8 access, u8 gran) {
    gdt[i] = (struct gdt_entry){ 0xFFFF, 0, 0, access, gran, 0 };
}

/* ---- IDT ---- */
struct idt_entry { u16 off_lo; u16 sel; u8 zero; u8 flags; u16 off_hi; } __attribute__((packed));
static struct idt_entry idt[256];
static struct dt_ptr idtp;
extern void idt_load(struct dt_ptr *);
extern u32 isr_table[48];
static void (*irq_handlers[16])(regs_t *);

volatile u32 ticks;

static void pic_remap(void) {
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, 0x00); outb(0xA1, 0x00);
}

static void timer_irq(regs_t *r) { ticks++; }

void irq_set_handler(int irq, void (*h)(regs_t *)) { irq_handlers[irq] = h; }

static const char *exc_names[] = {
    "Division par zéro", "Débogage", "NMI", "Point d'arrêt", "Dépassement",
    "Hors limites", "Instruction invalide", "Coprocesseur absent", "Double faute",
    "Segment coprocesseur", "TSS invalide", "Segment absent", "Faute de pile",
    "Protection générale", "Défaut de page", "Réservé", "Erreur virgule flottante",
    "Alignement", "Vérification machine", "Erreur SIMD",
};

void isr_handler(regs_t *r) {
    if (r->int_no < 32) {
        panic(r->int_no < 20 ? exc_names[r->int_no] : "Exception processeur", r);
    } else if (r->int_no < 48) {
        int irq = r->int_no - 32;
        if (irq_handlers[irq]) irq_handlers[irq](r);
        if (irq >= 8) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}

void interrupts_init(void) {
    gdt_set(0, 0, 0);
    gdt_set(1, 0x9A, 0xCF);   /* code noyau */
    gdt_set(2, 0x92, 0xCF);   /* données noyau */
    gdtp = (struct dt_ptr){ sizeof(gdt) - 1, (u32)&gdt };
    gdt_flush(&gdtp);

    pic_remap();
    for (int i = 0; i < 48; i++)
        idt[i] = (struct idt_entry){ isr_table[i] & 0xFFFF, 0x08, 0, 0x8E, isr_table[i] >> 16 };
    idtp = (struct dt_ptr){ sizeof(idt) - 1, (u32)&idt };
    idt_load(&idtp);

    /* Minuterie (PIT) à 100 Hz */
    u32 div = 1193182 / 100;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, div >> 8);
    irq_set_handler(0, timer_irq);

    /* Unité de calcul flottant */
    __asm__ volatile("fninit");
}

/* ---- Horloge temps réel (CMOS) ---- */
static u8 cmos(u8 reg) { outb(0x70, reg); return inb(0x71); }
static int bcd(int v) { return (v & 15) + (v >> 4) * 10; }
void rtc_read(int *h, int *m, int *s, int *d, int *mo, int *y) {
    while (cmos(0x0A) & 0x80) ;
    int sec = cmos(0), min = cmos(2), hr = cmos(4), day = cmos(7), mon = cmos(8), yr = cmos(9);
    if (!(cmos(0x0B) & 4)) {
        sec = bcd(sec); min = bcd(min); hr = bcd(hr & 0x7F) | (hr & 0x80);
        day = bcd(day); mon = bcd(mon); yr = bcd(yr);
    }
    if (h) *h = hr; if (m) *m = min; if (s) *s = sec;
    if (d) *d = day; if (mo) *mo = mon; if (y) *y = 2000 + yr;
}

void reboot(void) {
    cli();
    while (inb(0x64) & 2) ;
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("hlt");
}
void shutdown(void) {
    outw(0x604, 0x2000);   /* QEMU */
    outw(0xB004, 0x2000);  /* Bochs / anciens QEMU */
    outw(0x4004, 0x3400);  /* VirtualBox */
}

/* ---- Écran bleu ---- */
static void hex(u32 v, char *o) {
    for (int i = 7; i >= 0; i--) { o[i] = "0123456789ABCDEF"[v & 15]; v >>= 4; }
    o[8] = 0;
}
void panic(const char *msg, regs_t *r) {
    cli();
    gfx_noclip();
    gfx_fill(0, 0, scr_w, scr_h, RGB(16, 60, 140));
    int x = 120, y = 180;
    gfx_text_bold(x, y, ":(", 0xFFFFFF);
    gfx_text_bold(x, y + 40, "TontonOS a rencontré un problème et doit redémarrer.", 0xFFFFFF);
    gfx_text(x, y + 80, "Erreur :", 0xC8D8FF);
    gfx_text_bold(x + 80, y + 80, msg, 0xFFFFFF);
    if (r) {
        char b[9], line[64];
        hex(r->eip, b); strcpy(line, "EIP = 0x"); strcpy(line + 8, b);
        gfx_text(x, y + 110, line, 0xC8D8FF);
        hex(r->err_code, b); strcpy(line, "Code = 0x"); strcpy(line + 9, b);
        gfx_text(x, y + 130, line, 0xC8D8FF);
    }
    gfx_text(x, y + 180, "Appuyez sur le bouton de réinitialisation de votre machine.", 0xC8D8FF);
    gfx_present();
    for (;;) __asm__ volatile("cli; hlt");
}
