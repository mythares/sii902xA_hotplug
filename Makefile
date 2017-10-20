KDIR = /root/Hi3536_SDK/osdrv/opensource/kernel/linux-3.10.y

obj-m += sil9024.o

all:
	make -C $(KDIR) ARCH=arm CROSS_COMPILE=arm-hisiv400-linux- M=`pwd` modules
.PHONY: clean
clean:
	make -C $(KDIR) ARCH=arm CROSS_COMPILE=arm-hisiv400-linux- M=`pwd` modules clean
