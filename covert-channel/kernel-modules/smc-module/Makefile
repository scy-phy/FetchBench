# Source: https://www.cs.bham.ac.uk/~exr/lectures/systems/07_08/kernelProgramming.php

KERNELDIR=/media/amit/Workspace_drv/Prefetcher/TZ/optee-project/linux/

MODULES = smcmod.c
obj-m += smcmod.o

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install:
	make -C $(KERNELDIR) M=$(PWD) modules_install
