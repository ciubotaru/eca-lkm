obj-m+=eca30.o
KERNEL_BUILD  := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(PWD) clean
install:
	$(MAKE) -C $(KERNEL_BUILD) M=$(PWD) modules_install
