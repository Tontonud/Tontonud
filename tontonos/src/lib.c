/* TontonOS — fonctions de base (chaînes, mémoire, nombres) */
#include "kernel.h"

void *memset(void *d, int c, size_t n) {
    u8 *p = d;
    while (n--) *p++ = (u8)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    void *ret = d;
    size_t words = n / 4, rest = n % 4;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(words) :: "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(rest) :: "memory");
    return ret;
}
void *memmove(void *d, const void *s, size_t n) {
    u8 *dp = d; const u8 *sp = s;
    if (dp < sp) while (n--) *dp++ = *sp++;
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (u8)*a - (u8)*b : 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
void strlcpy(char *d, const char *s, size_t n) {
    if (!n) return;
    while (--n && *s) *d++ = *s++;
    *d = 0;
}
void itoa(int v, char *out) {
    char tmp[12]; int i = 0, neg = v < 0;
    u32 u = neg ? (u32)(-(v + 1)) + 1 : (u32)v;
    do { tmp[i++] = '0' + u % 10; u /= 10; } while (u);
    if (neg) *out++ = '-';
    while (i) *out++ = tmp[--i];
    *out = 0;
}
void utoa_pad(u32 v, char *out, int width) {
    char tmp[12]; int i = 0;
    do { tmp[i++] = '0' + v % 10; v /= 10; } while (v);
    while (i < width) tmp[i++] = '0';
    while (i) *out++ = tmp[--i];
    *out = 0;
}
void ftoa(double v, char *out) {
    if (v != v) { strcpy(out, "Erreur"); return; }
    if (v > 1e15 || v < -1e15) { strcpy(out, "Trop grand"); return; }
    if (v < 0) { *out++ = '-'; v = -v; }
    long long ip = (long long)v;
    double frac = v - (double)ip;
    long long fp = (long long)(frac * 1000000.0 + 0.5);
    if (fp >= 1000000) { ip++; fp -= 1000000; }
    char tmp[24]; int i = 0;
    do { tmp[i++] = '0' + (int)(ip % 10); ip /= 10; } while (ip);
    while (i) *out++ = tmp[--i];
    if (fp) {
        char f[7]; for (int k = 5; k >= 0; k--) { f[k] = '0' + (int)(fp % 10); fp /= 10; }
        int len = 6; while (len > 0 && f[len - 1] == '0') len--;
        *out++ = ',';
        for (int k = 0; k < len; k++) *out++ = f[k];
    }
    *out = 0;
}
static u32 rnd_state = 0x12345678;
void srand(u32 s) { rnd_state = s ? s : 1; }
u32 rand(void) {
    rnd_state ^= rnd_state << 13; rnd_state ^= rnd_state >> 17; rnd_state ^= rnd_state << 5;
    return rnd_state;
}
int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

/* ---- Allocateur : liste chaînée de blocs, fusion des voisins libres ---- */
typedef struct block { u32 size; u32 free; struct block *next; u32 pad; } block_t;
static block_t *heap_head;
u32 mem_total_kb;

void heap_init(u32 start, u32 size) {
    start = (start + 15) & ~15u;
    heap_head = (block_t *)start;
    heap_head->size = size - sizeof(block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
}
void *kmalloc(size_t n) {
    n = (n + 15) & ~15u;
    for (block_t *b = heap_head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof(block_t) + 64) {
            block_t *nb = (block_t *)((u8 *)(b + 1) + n);
            nb->size = b->size - n - sizeof(block_t);
            nb->free = 1;
            nb->next = b->next;
            b->next = nb;
            b->size = n;
        }
        b->free = 0;
        return b + 1;
    }
    return NULL;
}
void *kzalloc(size_t n) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }
void kfree(void *p) {
    if (!p) return;
    ((block_t *)p - 1)->free = 1;
    for (block_t *b = heap_head; b; b = b->next)
        while (b->free && b->next && b->next->free) {
            b->size += sizeof(block_t) + b->next->size;
            b->next = b->next->next;
        }
}
u32 heap_free_bytes(void) {
    u32 t = 0;
    for (block_t *b = heap_head; b; b = b->next) if (b->free) t += b->size;
    return t;
}
