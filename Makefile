KERNEL_PATH ?= /lib/modules/$(shell uname -r)/build
CC=gcc

obj-m += evdev_mirror.o
evdev_mirror-objs := evdev-mirror.o kallsyms.o

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules
	gcc -o read_example read_example.c
	gcc -o write_example write_example.c

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
	rm read_example
	rm write_example
