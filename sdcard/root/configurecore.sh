# Download latest package lists
apt-get update

# Install stuff you need
apt-get install -y ssh sudo kmod udev ifupdown iproute2 isc-dhcp-client

# Install stuff you probably want
apt-get install vim iputils-ping

# Set up user account
useradd --groups sudo adm --create-home --shell /bin/bash ubuntu
echo ubuntu:ubuntu | chpasswd

echo "ubuntu-zynq" > /etc/hostname
echo "127.0.0.1 localhost" > /etc/hosts
echo "127.0.0.1 ubuntu-zynq" >> /etc/hosts

