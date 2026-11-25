/* TontonOS — point d'entrée C du noyau */
#include "kernel.h"
#include "gfx.h"
#include "wm.h"
#include "fs.h"

extern u8 kernel_end[];

struct multiboot_info {
    u32 flags, mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr;
    u32 syms[4], mmap_length, mmap_addr, drives_length, drives_addr, config_table;
    u32 boot_loader_name, apm_table, vbe_control_info, vbe_mode_info;
    u16 vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    u64 fb_addr;
    u32 fb_pitch, fb_width, fb_height;
    u8  fb_bpp, fb_type;
} __attribute__((packed));

struct mmap_entry { u32 size; u64 base; u64 len; u32 type; } __attribute__((packed));

void kmain(u32 magic, struct multiboot_info *mb) {
    if (magic != 0x2BADB002) for (;;) __asm__ volatile("hlt");

    /* On copie d'abord les infos d'écran : le tas peut recouvrir la structure Multiboot */
    if (!(mb->flags & (1 << 12)) || mb->fb_type != 1) for (;;) __asm__ volatile("hlt");
    u32 fb_addr = (u32)mb->fb_addr, fb_pitch = mb->fb_pitch, fb_w = mb->fb_width, fb_h = mb->fb_height, fb_bpp = mb->fb_bpp;

    /* Mémoire : on cherche le plus grand bloc libre au-dessus de 1 Mo dans la carte mémoire.
       En UEFI, la mémoire est morcelée ; mem_upper ne donne que le premier morceau. */
    u32 kend = ((u32)kernel_end + 0xFFF) & ~0xFFFu;
    u32 best_start = kend, best_size = 0;
    u64 total = 0;
    if (mb->flags & (1 << 6)) {
        u32 p = mb->mmap_addr, end = mb->mmap_addr + mb->mmap_length;
        while (p < end) {
            struct mmap_entry *e = (struct mmap_entry *)p;
            if (e->type == 1) {
                total += e->len;
                u64 lo = e->base, hi = e->base + e->len;
                if (hi > 0xFFFFF000ull) hi = 0xFFFFF000ull;       /* 32 bits seulement */
                if (lo < kend) lo = kend;                          /* jamais sur le noyau */
                if (lo < 0x100000) lo = 0x100000;
                /* évite la structure Multiboot et la carte mémoire elles-mêmes */
                if (lo <= (u32)mb && (u32)mb < hi && (u32)mb - lo < 0x10000) lo = ((u32)mb + 0x1000) & ~0xFFFu;
                if (lo <= mb->mmap_addr && mb->mmap_addr < hi && mb->mmap_addr - lo < 0x10000) lo = (end + 0xFFF) & ~0xFFFu;
                if (hi > lo && hi - lo > best_size) { best_start = (u32)lo; best_size = (u32)(hi - lo); }
            }
            p += e->size + 4;
        }
        mem_total_kb = (u32)(total / 1024);
    }
    if (!best_size) {   /* pas de carte : on se rabat sur mem_upper */
        mem_total_kb = (mb->flags & 1) ? mb->mem_upper + 1024 : 32 * 1024;
        best_start = kend;
        best_size = 0x100000 + (mem_total_kb - 1024) * 1024 - kend;
    }
    heap_init(best_start, best_size);

    gfx_init(fb_addr, fb_pitch, fb_w, fb_h, fb_bpp);

    fs_init();
    interrupts_init();
    keyboard_init();
    mouse_init();
    srand(ticks + 12345);
    sti();

    wm_init();
    wm_run();
}
