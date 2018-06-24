# Usage make -c cross_platform.make
#KER_DIR := /lib/modules/$(shell uname -r)/build
KER_DIR	:= "/home/gedeon/temp/zynqmp/kernel-source"
arch = arm64
COMP = aarch64-linux-gnu-

obj-m	:= dma.o cmabuffer.o hwacc.o

dma-objs	:= dma_driver.o dma_help.o dma_queue.o
cmabuffer-objs := cma_driver.o kbuffer.o
streamgen-objs := stream_driver.o
hwacc-objs	:= hwacc_driver.o dma_bufferset.o

all:
	$(MAKE) ARCH=$(arch) CROSS_COMPILE=$(COMP) -C $(KER_DIR) M=$(PWD) modules
	@$(MAKE) ARCH=$(arch) CROSS_COMPILE=$(COMP) clean_partial

clean:
	$(MAKE) ARCH=$(arch) CROSS_COMPILE=$(COMP) -C $(KER_DIR) M=$(PWD) clean

clean_partial:
	@rm -f *.o *~ core .depend .*.cmd *.mod.c
	@rm -f Module.markers Module.symvers modules.order
	@rm -rf .tmp_versions Modules.symvers
