# Source: https://www.cs.bham.ac.uk/~exr/lectures/systems/07_08/kernelProgramming.php

KERNELDIR=/lib/modules/`uname -r`/build

obj-m := cachemod2.o
cachemod2-objs := cache.o main.o

CFLAGS_cache.o=-std=gnu11
CFLAGS_main.o=-std=gnu11

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install:
	make -C $(KERNELDIR) M=$(PWD) modules_install
