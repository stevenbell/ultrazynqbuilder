OUTPUT	= dma-mod

obj-m += $(OUTPUT).o
$(OUTPUT)-objs += dma_driver.o dma_help.o dma_queue.o
PWD = $(shell pwd)

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	sudo dmesg -c
	sudo insmod ./$(OUTPUT).ko

unload:
	sudo rmmod $(OUTPUT)