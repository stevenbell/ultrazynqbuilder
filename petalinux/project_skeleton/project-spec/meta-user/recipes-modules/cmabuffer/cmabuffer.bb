SUMMARY = "Recipe for  build an external cmabuffer Linux kernel module"
SECTION = "PETALINUX/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=d41d8cd98f00b204e9800998ecf8427e"

inherit module

SRC_URI = "file://Makefile \
           file://buffer.h \
           file://buffer.c \
           file://common.h \
           file://ioctl_cmds.h \
           file://cmabuf.c \
           file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
