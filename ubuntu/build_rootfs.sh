#! /bin/bash -e

# Build a minimal Ubuntu root file system for Zynq boards
# This installs a handful of packages that make life on the board easier,
# and adds some basic stuff like networking.

# This requires the package qemu-user-static for the chroot to work
# Must be run as root

# Steven Bell <sebell@stanford.edu>
# 12 October 2017

RELEASE=16.04
MINOR=$RELEASE.3
ARCH=arm64

# Assumes the SD card partition is labeled "ROOT". Edit this as appropriate.
#TARGET=/media/`whoami`/ROOT
TARGET=/media/steven/ROOT

# Get the Ubuntu core filesystem
RELNAME=ubuntu-base-$MINOR-base-$ARCH.tar.gz
if [ ! -e $RELNAME ]; then
    wget http://cdimage.ubuntu.com/ubuntu-base/releases/$RELEASE/release/$RELNAME
fi

# Extract it to the target directory
tar -xf $RELNAME --directory=$TARGET

# Set up networking for the chroot
mkdir -p $TARGET/etc/network
cp /etc/network/interfaces $TARGET/etc/network
cp /etc/resolv.conf $TARGET/etc/resolv.conf

# Copy qemu-arm-static
# Before copy, make sure the user has it installed
# -e will make sure to stop at $?==1
dpkg -s qemu-user-static > /dev/null
dpkg -s qemu-system-arm > /dev/null

# This is necessary for running arm binaries once we chroot
cp /usr/bin/qemu-arm-static $TARGET/usr/bin/
cp /usr/bin/qemu-aarch64-static $TARGET/usr/bin/

# Copy the configuration script so we can run it inside the chroot
cp configurecore.sh $TARGET

# Mount the target
for m in `echo 'sys dev proc'`; do mount /$m $TARGET/$m -o bind; done
LC_ALL=C chroot $TARGET /configurecore.sh # Automated configuration
#LC_ALL=C chroot $TARGET /bin/bash # Just run a shell

# Clean up
for m in `echo 'sys dev proc'`; do sudo umount $TARGET/$m; done
rm $TARGET/etc/resolv.conf
rm $TARGET/configurecore.sh

# Configure networking for device
# This overwrites the file we made for the chroot
echo "auto lo" > $TARGET/etc/network/interfaces
echo "iface lo inet loopback" >> $TARGET/etc/network/interfaces
echo "auto eth0" >> $TARGET/etc/network/interfaces
echo "iface eth0 inet dhcp" >> $TARGET/etc/network/interfaces

