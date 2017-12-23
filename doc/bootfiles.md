
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
  -> Boot args?
  -> Root filesystem on SD card

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

