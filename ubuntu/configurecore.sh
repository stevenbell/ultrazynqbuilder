# Download latest package lists
apt-get update

# Install stuff you need
apt-get install -y ssh sudo kmod udev ifupdown iproute2 isc-dhcp-client

# Install stuff you probably want
apt-get install -y vim iputils-ping

# Stuff you may want
apt-get install -y usbutils

# Set up user account
useradd --groups sudo,adm --create-home --shell /bin/bash ubuntu
echo ubuntu:ubuntu | chpasswd

echo "ubuntu-zynq" > /etc/hostname
echo "127.0.0.1 localhost" > /etc/hosts
echo "127.0.0.1 ubuntu-zynq" >> /etc/hosts

# Set up auto-mounting of boot partition
mkdir /mnt/boot
# sh echo doesn't handle -e, so use the gnu one in /bin/echo
/bin/echo -e "/dev/mmcblk1p1\t/mnt/boot\tauto\tuser,umask=000\t0\t0" | sudo tee /etc/fstab

