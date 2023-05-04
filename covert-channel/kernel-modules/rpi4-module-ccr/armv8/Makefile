# Source: https://www.cs.bham.ac.uk/~exr/lectures/systems/07_08/kernelProgramming.php

KERNELDIR=/lib/modules/`uname -r`/build

MODULES = ccr8.c
obj-m += ccr8.o

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install:
	make -C $(KERNELDIR) M=$(PWD) modules_install
