#!/bin/sh

MODULE_PATH=./smcmod.ko

if [ ! -f ${MODULE_PATH} ]
then
	echo "You have to compile that module first (just run make)."
fi

if [ $(id -u) -ne 0 ]
then
	echo "This script must be run with root permissions."
	exit
fi

# Load Module
insmod ${MODULE_PATH}

# Show last few lines of dmesg (module should send some log lines)
dmesg | tail

mknod /dev/smc_dev c 102 0