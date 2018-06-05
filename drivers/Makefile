KER_DIR := /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

obj-m	:= dma.o cmabuffer.o streamgen.o hwacc.o

dma-objs	:= dma_driver.o dma_help.o dma_queue.o
cmabuffer-objs := cma_driver.o kbuffer.o
streamgen-objs := stream_driver.o
hwacc-objs := driver.o dma_bufferset.o

all:
	$(MAKE) -C $(KER_DIR) M=$(PWD) modules
	@$(MAKE) clean_partial

clean:
	$(MAKE) -C $(KER_DIR) M=$(PWD) clean

clean_partial:
	@rm -f *.o *~ core .depend .*.cmd *.mod.c
	@rm -f Module.markers Module.symvers modules.order
	@rm -rf .tmp_versions Modules.symvers
