KER_DIR := /lib/modules/$(shell uname -r)/build
#KER_DIR	:= "/home/gedeon/temp/zynqmp/kernel-source"

PWD := `pwd`

obj-m	:= dma.o cmabuffer.o hwacc.o
ccflags-y := -Wall

dma-objs	:= dma_driver.o dma_help.o dma_queue.o
cmabuffer-objs := cma_driver.o kbuffer.o
streamgen-objs := stream_driver.o
hwacc-objs	:= hwacc_driver.o dma_bufferset.o

all:
	$(MAKE) -C $(KER_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KER_DIR) M=$(PWD) clean
