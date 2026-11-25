; TontonOS — point d'entrée (Multiboot 1, mode protégé 32 bits)
bits 32

MB_MAGIC equ 0x1BADB002
MB_FLAGS equ (1 << 0) | (1 << 1) | (1 << 2)   ; alignement, infos mémoire, mode vidéo

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd -(MB_MAGIC + MB_FLAGS)
    dd 0, 0, 0, 0, 0          ; champs d'adresse (inutilisés)
    dd 0                      ; mode linéaire (framebuffer)
    dd 1024, 768, 32          ; résolution demandée

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top
    push ebx                  ; structure d'infos Multiboot
    push eax                  ; signature magique
    call kmain
.hang:
    cli
    hlt
    jmp .hang

; ---- GDT ----
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; ---- Interruptions ----
extern isr_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 48
  %if i = 8 || (i >= 10 && i <= 14) || i = 17 || i = 21 || i = 29 || i = 30
    ISR_ERR i
  %else
    ISR_NOERR i
  %endif
  %assign i i + 1
%endrep

isr_common:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp                  ; pointeur vers la structure regs
    call isr_handler
    add esp, 4
    pop es
    pop ds
    popa
    add esp, 8
    iret

section .data
global isr_table
isr_table:
%assign i 0
%rep 48
    dd isr %+ i
  %assign i i + 1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
