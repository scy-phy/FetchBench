#pragma once

#include <linux/ioctl.h>

// major device number
#define SMC_MAJOR_NUM 102

// definition of sub-commands supported by the ioctl device
#define SMC_IOCTL_INDUCE_PATTERN	_IO(SMC_MAJOR_NUM, 0)
#define SMC_IOCTL_SET_UP_COUNTER	_IOR(SMC_MAJOR_NUM, 1, uint64_t)
#define SMC_IOCTL_INCREMENT_COUNTER	_IO(SMC_MAJOR_NUM, 2)

// name of the device file
#define SMC_DEVICE_FILE_NAME "smc_dev"