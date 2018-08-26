#!/bin/sh

unset HOSTCC CC LD OBJCOPY STRIP

cd build

../configure CC="$(HOSTCC)" LD="$(LD)" \
	TARGET_CC="$(CC)" TARGET_OBJCOPY="$(OBJCOPY)" TARGET_STRIP="$(STRIP)" \
	CFLAGS=-O2 TARGET_CFLAGS=-Os \
	--target=i386 --with-platform=coreboot --disable-werror \
	--disable-nls --disable-efiemu --disable-grub-emu-sdl \
	--disable-grub-mkfont --disable-grub-mount --disable-device-mapper \
	--disable-libzfs --enable-boot-time

patch Makefile < ../a.patch

make
