# Automated tools for building Zynq Ultrascale designs

This is one backend designed to be used with f4graph and Halide-HLS.

# Files
- **ubuntu**: scripts to build a root filesystem from Ubuntu base.  This is a convenient base system for development, since it makes it easy to install lots of development packages and other utilities without recompiling the whole rootfs in Petalinux.
- **drivers**: Kernel drivers for pushing images through accelerators created with Halide-HLS.
- **hwutils**: tcl scripts and other utilities that we've created for debugging and testing hardware.
- **petalinux**: petalinux project for building all the boot collateral.
- **doc**: various bits of documentation and notes.

# Supported devices
At the moment, we support only the UltraZed-EG.

# Build flow

