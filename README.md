# Automated tools for building Zynq Ultrascale designs

This is one backend designed to be used with f4graph and Halide-HLS.

# Files
- **hw**: Scripts to build a hardware system which contains an accelerator, cameras, and DisplayPort output
- **ubuntu**: scripts to build a root filesystem from Ubuntu base.  This is a convenient base system for development, since it makes it easy to install lots of development packages and other utilities without recompiling the whole rootfs in Petalinux.
- **drivers**: Kernel drivers for pushing images through accelerators created with Halide-HLS.
- **hwutils**: tcl scripts and other utilities that we've created for debugging and testing FPGA designs.
- **petalinux**: petalinux project for building all the boot collateral.
- **realtime**: Real-time code for controlling cameras and hardware accelerators, which interfaces with the f4graph runtime.
- **doc**: various bits of documentation and notes.

# Supported devices
At the moment, this supports the UltraZed-EG with either the IO carrier card or the PCIe carrier card.

Currently supported version of Vivado is 2017.2.

# Build flow
## Hardware
1. (Optional) Create one or more hardware accelerators with Halide, and ensure that HLS has packaged them as IP cores.
2. Copy `hwconfig.example` to `hwconfig.user` and edit it to specify the design you want.
3. Run `python extractparams.py` to extract the parameters from the IP cores and place them in the configuration file.
4. In the `hw` directory, run `python ../parameterize.py ../hwconfig.all mkproject.tcl.mako:mkproject.tcl` to generate a Tcl script that will create the design.
5. Open Vivado and in the Tcl console run `source mkproject.tcl`. Or run `vivado -mode batch -source mkproject.tcl` to avoid running the GUI.

The design will be created, and Vivado should produce a bitstream.  From here you can use the petalinux build script to create the boot collateral.

## PetaLinux
1. In the `petalinux` directory, run `./build_petalinux.sh VIVADO_EXPORT_DIR PROJECT_DIR`, where `VIVADO_EXPORT_DIR` points to the SDK export, and `PROJECT_DIR` is the project directory name. Based on your vivado version, you may need to manually launch Vivado SDK to set up SDK files after generating bitstream. Currently the script will create `PROJECT_DIR` for you.
2. Once the build finishes, copy `images/linux/BOOT.BIN` and `images/linux/image.ub` to the device's boot partition

## Ubuntu (Optional)
It will setup a Ubuntu file system, including its package repositories.
1. Modify `build_rootfs.sh` so that `TARGET` is pointing to the device's root partition
2. Run `build_rootfs.sh` with root privilege. Because it uses `chroot`, during the execution anything like <kbd>CTRL</kbd> + <kbd>C</kbd> will have unexpected results. You can use a virtual machine if that's the concern.

3. Copy libraries to the Ubuntu root filesystem.  The easiest way to do this is to extract the PetaLinux-built root filesystem and copy the entire `/lib/modules/` directory.

