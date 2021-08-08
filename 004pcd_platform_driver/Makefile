obj-m := pcd_device_setup.o pcd_platform_driver.o

KERN_DIR=/home/leonardo/Development/linux-stable

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help
copy-drv:
	scp pcd_device_setup.ko root@192.68.50.2:/home/root/
