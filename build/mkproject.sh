#!/usr/bin/env bash

#Usage ./generate_tcl.py PATH_TO_HLS_CORE PATH_TO_HW_YAML_CONFIG


if [ -z "$1" ]; then
	echo "Wrong usage. Expected: sh mkproject.sh PATH_TO_HLS_IP_CORE_DIRECTORY"
else
	make clean

	./generate_tcl.py $1 
	
	vivado -mode batch -source mkproject.tcl

	# Waits for the vivado build flow to finish.
	while [ ! -f ./uz3eg_iocc_dp/uz3eg_iocc_dp.sdk/uz3eg_iocc_dp_wrapper.hdf ]; do sleep 1; done

	mkdir petalinux
	cd petalinux	

	petalinux-create -t project -s ../source_files/uz3eg_iocc_dp_2017_2.bsp --name uz3eg_iocc_dp
	
	cd ../../drivers/
	rm driver.c dma_bufferset.h

	# Regenerates the parameterized driver files
	python ./parameterize.py ./hwconfig.yml driver.c.mako:driver.c
	python ./parameterize.py ./hwconfig.yml dma_bufferset.h.mako:dma_bufferset.h

	# Copies over all the petalinux kernel configs and device tree configs to actual petalinux build directory
	cp -rf ../build/source_files/project-spec ../build/petalinux/uz3eg_iocc_dp/

	# Copies over the latest and greatest driver files to build directory
	cp {buffer.c,buffer.h,cmabuf.c,common.h,ioctl_cmds.h} ../build/petalinux/uz3eg_iocc_dp/project-spec/meta-user/recipes-modules/cmabuffer/files
	cp {buffer.h,common.h,dma_bufferset.c,dma_bufferset.h,driver.c,ioctl_cmds.h} ../build/petalinux/uz3eg_iocc_dp/project-spec/meta-user/recipes-modules/hwacc/files
	cp {buffer.h,common.h,ioctl_cmds.h,vdma.c} ../build/petalinux/uz3eg_iocc_dp/project-spec/meta-user/recipes-modules/xilcam/files

	cd ../build/petalinux/uz3eg_iocc_dp/

	petalinux-config --get-hw-description=../../uz3eg_iocc_dp/uz3eg_iocc_dp.sdk/ --oldconfig

	petalinux-build

	cp ../../source_files/bootgen_static.bif ./
	cp ../../source_files/regs.init ./
	petalinux-package --boot --force --bif bootgen_static.bif
fi
