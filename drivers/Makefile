KER_DIR := /lib/modules/`uname -r`/build

# There are three modules created from this source code
#   cmabuffer - Provides access to large contiguous memory buffers from Linux CMA
#   hwacc - Driver for hardware accelerator, using one or more DMA engines
obj-m := hwacc.o cmabuffer.o
ccflags-y := -Wall

# Dependencies for each of the modules
# Note that some code related to buffer handling and ioctl numbers is shared
hwacc-objs := driver.o dma_bufferset.o
cmabuffer-objs := cmabuf.o buffer.o

# Call the Linux source makefiles to do the dirty work
all:
	$(MAKE) -C $(KER_DIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KER_DIR) M=$(PWD) modules_install
	sudo insmod cmabuffer
	sudo insmod hwacc

clean:
	make -C $(KER_DIR) M=$(PWD) clean

uninstall:
	make -C $(KER_DIR) M=$(PWD) clean
	sudo rmmod hwacc
	sudo rmmod cmabuffer
