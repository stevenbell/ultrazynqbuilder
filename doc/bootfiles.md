
Building the boot collateral for a minimal ubuntu system
Needs to have:
  - rootfs on a separate partition
  - Serial console output

# Install tools
Download petalinux
  https://www.xilinx.com/products/design-tools/embedded-software/petalinux-sdk.html
Petalinux is not part of the SDK; there are separate downloads on the page.

I'm using version 2017.2, since that's the version which has a BSP for the UltraZed

# Build a hardware design
Build a hardware design in Vivado, and export the HDF with the bitstream.

# Download BSPs
The ZCU102 BSPs are hosted by Xilinx: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools/2017-2.html

The UltraZed BSPs are hosted by Avnet/Zedboard: http://zedboard.org/content/ultrazed-io-carrier-card-petalinux-20172-compressed-bsp

# Create the project with the BSP
Create project using pre-built BSP:
  `petalinux-create -t project -s <BSP path> --name <project name>`

Import the HDF:
  `petalinux-config --get-hw-description <hdf path> -p <path to the project you created in 1>`

Configure the project
  -> Set boot args: Main->Kernel Bootargs
"earlycon=cdns,mmio,0xFF000000,115200n8 root=/dev/mmcblk1p2 rw rootwait cma=256m"
  -> Put the root filesystem on SD card: Main->Image Packaging Configuration->Root fs type
  -> Change device node to /dev/mmcblk1p2

Configure the kernel
  `petalinux-config -c kernel -p <path to project>`
  -> Disable ramdisk support (optimization; shouldn't be necessary): General setup
  -> Disaple PCI support (optimization; shouldn't be necessary): Bus Support
  -> Set boot args (why is this necessary?): Boot options
  -> Enable CMA debugfs (useful for debugging): Kernel features > CMA

Build it
  `cd <PROJECT>`
  `petalinux-build`

Create the boot collateral:
  `petalinux-package --boot --fsbl <fsbl path> --fpga <fpga path> --u-boot


Check that all of the necessary files are included:
INFO: File in BOOT BIN: ".../images/linux/zynqmp_fsbl.elf"
INFO: File in BOOT BIN: ".../images/linux/pmufw.elf"
INFO: File in BOOT BIN: ".../images/linux/design_1_wrapper.bit"
INFO: File in BOOT BIN: ".../images/linux/bl31.elf"
INFO: File in BOOT BIN: ".../images/linux/u-boot.elf"

The pmufw and fsbl are included by default.

