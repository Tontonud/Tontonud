/* TontonOS — définitions communes du noyau */
#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed int         i32;
typedef u32                size_t;
#define NULL ((void *)0)

/* ---- Ports d'entrée/sortie ---- */
static inline void outb(u16 p, u8 v)  { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8   inb(u16 p)         { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void)      { outb(0x80, 0); }
static inline void cli(void)          { __asm__ volatile("cli"); }
static inline void sti(void)          { __asm__ volatile("sti"); }

/* ---- Bibliothèque ---- */
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
void   strlcpy(char *d, const char *s, size_t n);
void   itoa(int v, char *out);
void   utoa_pad(u32 v, char *out, int width);
void   ftoa(double v, char *out);
u32    rand(void);
void   srand(u32 s);
int    min(int a, int b);
int    max(int a, int b);

/* ---- Mémoire (allocateur) ---- */
void   heap_init(u32 start, u32 size);
void  *kmalloc(size_t n);
void  *kzalloc(size_t n);
void   kfree(void *p);
u32    heap_free_bytes(void);
extern u32 mem_total_kb;

/* ---- Interruptions / temps ---- */
typedef struct {
    u32 es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
} regs_t;

void interrupts_init(void);
void irq_set_handler(int irq, void (*h)(regs_t *));
extern volatile u32 ticks;          /* 100 par seconde */
void rtc_read(int *h, int *m, int *s, int *d, int *mo, int *y);
void reboot(void);
void shutdown(void);
void panic(const char *msg, regs_t *r);

/* ---- Clavier ---- */
enum {
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_DEL, KEY_PGUP, KEY_PGDN,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};
#define KEY_BACKSPACE 8
#define KEY_TAB       9
#define KEY_ENTER     '\n'
#define KEY_ESC       27
#define KMOD_ALT  0x10000
#define KMOD_CTRL 0x20000

void keyboard_init(void);
int  keyboard_get(void);            /* -1 si aucune touche */
extern int kb_azerty;

/* ---- Souris ---- */
typedef struct { int dx, dy, buttons; } mouse_ev_t;
void mouse_init(void);
int  mouse_get(mouse_ev_t *ev);     /* 0 si vide */

#endif
