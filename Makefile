KBUILD=/opt/linux/
KMOD=$(PWD)/src/rootkit
EXPLOITS=$(PWD)/src/exploits
SHELLS=$(PWD)/src/shell

.PHONY: all run debug build initramfs clean

all: clean build

run: clean build initramfs
	./run.sh

debug: clean build initramfs
	./run.sh '-s -S'

build:
	$(MAKE) -C $(KBUILD) M=$(KMOD) modules
	$(MAKE) -C $(EXPLOITS)
	$(MAKE) -C $(SHELLS)

initramfs: build
	cp $(KMOD)/rootkit.ko initramfs
	cp $(EXPLOITS)/*.elf initramfs
	cp $(SHELLS)/*.elf initramfs
	cd initramfs && find . | cpio --quiet -o -H newc | gzip -9 > ../initrd

clean:
	$(MAKE) -C $(KBUILD) M=$(KMOD) clean
	$(MAKE) -C $(EXPLOITS) clean
	$(MAKE) -C $(SHELLS) clean
	rm -f initrd initramfs/rootkit.ko initramfs/*.elf
