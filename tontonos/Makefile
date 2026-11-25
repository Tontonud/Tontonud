# TontonOS — compilation
CC      := gcc
AS      := nasm
CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -O2 -Wall -Wextra -Wno-unused-parameter -mno-red-zone \
           -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Isrc
LDFLAGS := -m32 -nostdlib -no-pie -T linker.ld -Wl,--build-id=none

SRC_C   := $(wildcard src/*.c)
OBJ     := build/boot.o $(SRC_C:src/%.c=build/%.o)

all: build/tontonos.iso

build/%.o: src/%.c $(wildcard src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/boot.o: src/boot.asm
	@mkdir -p build
	$(AS) -f elf32 $< -o $@

build/kernel.bin: $(OBJ) linker.ld
	$(CC) $(LDFLAGS) $(OBJ) -o $@ -lgcc

build/tontonos.iso: build/kernel.bin grub.cfg
	@mkdir -p build/iso/boot/grub
	cp build/kernel.bin build/iso/boot/kernel.bin
	cp grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso 2>/dev/null

run: build/tontonos.iso
	qemu-system-i386 -cdrom $< -m 128M

# Test du démarrage UEFI (paquet ovmf)
run-uefi: build/tontonos.iso
	cp /usr/share/OVMF/OVMF_VARS_4M.fd build/ovmf_vars.fd
	qemu-system-x86_64 -m 256M -cdrom $< \
	  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
	  -drive if=pflash,format=raw,file=build/ovmf_vars.fd

clean:
	rm -rf build

.PHONY: all run run-uefi clean
