ARCH = arm64
CROSS_COMPILE = /home/student/Downloads/rpi-buildroot-2026.08/output/host/bin/aarobj-m += mymod.o

KDIR := /home/student/Downloads/rpi-buildroot-2026.08/output/build/linux-custom

ARCH := arm64
CROSS_COMPILE := /home/student/Downloads/rpi-buildroot-2026.08/output/host/bin/aarch64-linux-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
