#
# Makefile for the linux location based filesystem
#

# Below line to be used in conjunction with the Linux kernel build.
#obj-$(CONFIG_LOCFS) += locfs.o

obj-m := locfs.o
locfs-objs := main.o super.o inode.o file.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
