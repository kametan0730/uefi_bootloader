ARCH            = $(shell uname -m | sed s,i[3456789]86,ia32,)

OBJS            = main.o
TARGET          = main.efi

EFIINC          = /usr/local/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = /usr/lib64
EFILIB          = /usr/local/lib
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
		  -fshort-wchar -mno-red-zone -Wall
ifeq ($(ARCH),x86_64)
  CFLAGS += -DEFI_FUNCTION_WRAPPER
endif

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
		  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS)

all: $(TARGET)

main.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym  -j .rel -j .rela -j .reloc \
		--target=efi-app-$(ARCH) $^ $@

qemu: main.efi fs/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 -m 2G -nodefaults -nographic -serial mon:stdio \
		-bios /usr/share/ovmf/OVMF.fd -hda fat:rw:fs

fs/EFI/BOOT/BOOTX64.EFI:
	mkdir -p fs/EFI/BOOT
	ln -sf ../../../main.efi fs/EFI/BOOT/BOOTX64.EFI

clean:
	rm -f main.efi main.so main.o
	rm -f -r fs