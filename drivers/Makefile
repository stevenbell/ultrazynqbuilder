KER_DIR := /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

obj-m	:= dma.o cmabuffer.o streamgen.o

dma-objs	:= dma_driver.o dma_help.o dma_queue.o
cmabuffer-objs := cma_driver.o kbuffer.o
streamgen-objs := stream_driver.o

all:
	$(MAKE) -C $(KER_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KER_DIR) M=$(PWD) clean
