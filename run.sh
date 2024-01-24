#!/bin/sh

qemu-system-x86_64                                          \
	-cpu qemu64,+smep,+smap                                 \
	-m 512                                                  \
	-kernel ./bzImage                                       \
	-initrd initrd                                          \
	-nographic                                              \
	-serial stdio                                           \
	-append 'nokaslr console=ttyS0 boot=ctf quiet=y'        \
	-monitor telnet::45454,server,nowait                    \
	"$@"
