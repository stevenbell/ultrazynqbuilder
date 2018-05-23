#! /bin/bash

# Assumes petalinux tools are in the path

if [ -z "$2" ]; then
	echo "USAGE: build_petalinux.sh VIVADO_EXPORT_DIR PROJECT_DIR"
  exit
fi

VIVADO_EXPORT_DIR=`realpath $1`
PROJECT_DIR=`realpath $2`

if [ -e $PROJECT_DIR ]; then
	echo "Project directory $PROJECT_DIR already exists.  Please choose another location."
  exit
fi

# Copy the files to the project directory
cp -r project_skeleton $PROJECT_DIR
cp bootgen_static.bif $PROJECT_DIR
cp regs.init $PROJECT_DIR
cp ../hwconfig.user $PROJECT_DIR
cp ../dtconfig.py $PROJECT_DIR

# get the petalinux dir
PETALINUX_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DRIVER_DIR=$PETALINUX_DIR/../drivers

pushd $PROJECT_DIR

# link every driver files over
for i in $DRIVER_DIR/*; do
    if [ "${i}" != "${i%.c}" ] || [ "${i}" != "${i%.h}" ] ; then
        ln -s $i project-spec/meta-user/recipes-modules/hwacc/files/
        ln -s $i project-spec/meta-user/recipes-modules/cmabuffer/files/
    fi
done

# Fix the config TMPDIR
sed -i "s,<PETALINUX_PROJECT_ROOT>,$PROJECT_DIR," project-spec/configs/config

# Create device tree overlay
python dtconfig.py hwconfig.user project-spec/meta-user/recipes-bsp/device-tree/files/system-user-overlay.dtsi

# Make an empty directory to Petalinux doesn't choke on the import
mkdir project-spec/hw-description

petalinux-config --get-hw-description=$VIVADO_EXPORT_DIR --oldconfig
petalinux-build
petalinux-package --boot --force --bif bootgen_static.bif

echo "Done!"
echo "BOOT.BIN and images/linux/image.ub can now be copied to the boot partition of your SD card."

popd

