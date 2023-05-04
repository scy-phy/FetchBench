#!/bin/sh

MODULE_NAME="ccr8"

if [ $(id -u) -ne 0 ]
then
	echo "This script must be run with root permissions."
	exit
fi

# unload module
rmmod ${MODULE_NAME}

# Show last few lines of dmesg (module should send some log lines)
dmesg | tail
