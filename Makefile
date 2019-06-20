ifneq ($(KERNELRELEASE),)
obj-m := pistahtu21d.o
else
KDIR := $(HOME)/linux-kernel-labs/src/linux
all:
	$(MAKE) -C $(KDIR) M=$$PWD
	arm-linux-gnueabi-gcc test_pistahtu21d.c -o test
endif
