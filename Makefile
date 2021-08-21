ccflags-y=-O3
obj-m+=eca30.o
KERNEL_BUILD  := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(shell pwd) modules
clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(shell pwd) clean
install:
	$(MAKE) -C $(KERNEL_BUILD) M=$(shell pwd) modules_install
