CCFLAGS=-static

.PHONY: all clean

all: shell

clean:
	rm -f *.elf

%:
	$(CC) $(CCFLAGS) $@.c -o $@.elf