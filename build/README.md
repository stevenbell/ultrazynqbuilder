Running the following shell script will accomplish the following:
	1. Generate the FrankenCamera hardware design and Vivado project bitfile with the embedded HLS core
	2. Generates the parameterized driver files driver.c and dma_bufferset.h based off of hwconfig.yml.
	3. Create a new petalinux project and imports the hardware definition (.hdf) file from the Vivado project.
	3. Sets all the proper kernel configurations.
	4. Builds the petalinux project and generates the boot collateral (boot.bin, image.ub).

To run: sh ./mkproject.sh PATH_TO_UNZIPPED_HLS_CORE

PATH_TO_UNZIPPED_HLS_CORE must contain the component.xml file in the main directory.


