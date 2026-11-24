/* TontonOS — point d'entrée C du noyau */
#include "kernel.h"
#include "gfx.h"
#include "wm.h"

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

void kmain(u32 magic, struct multiboot_info *mb) {
    if (magic != 0x2BADB002) for (;;) __asm__ volatile("hlt");

    /* Mémoire : le tas commence après le noyau et occupe la RAM restante */
    mem_total_kb = (mb->flags & 1) ? mb->mem_upper + 1024 : 32 * 1024;
    u32 heap_start = ((u32)kernel_end + 0xFFF) & ~0xFFFu;
    u32 heap_end = 0x100000 + (mem_total_kb - 1024) * 1024;
    heap_init(heap_start, heap_end - heap_start);

    if (!(mb->flags & (1 << 12)) || mb->fb_type != 1) for (;;) __asm__ volatile("hlt");
    gfx_init((u32)mb->fb_addr, mb->fb_pitch, mb->fb_width, mb->fb_height, mb->fb_bpp);

    interrupts_init();
    keyboard_init();
    mouse_init();
    srand(ticks + 12345);
    sti();

    wm_init();
    wm_run();
}
