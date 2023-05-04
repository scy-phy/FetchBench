#pragma once

#include <linux/ioctl.h>
#include "cache_structures.h"

// data structures to represet an ioctl cache request
enum cache_level { CACHE_L1, CACHE_L2, CACHE_ALL };

struct cache_request_setup {
    // process pid, required for the module to work on the right memory
    // map during virtual-to-physical address translation
    pid_t pid;
    
    // probe array information
    uint8_t* pa_base_addr;
    size_t pa_size;

    // result memory information
    size_t* resp_l1;
    size_t resp_l1_size;
    size_t* resp_l2;
    size_t resp_l2_size;
};

// major device number
#define MAJOR_NUM 101

// definition of sub-commands supported by the ioctl device
#define IOCTL_SETUP _IOR(MAJOR_NUM, 0, struct cache_request_setup*)
#define IOCTL_QUERY _IOR(MAJOR_NUM, 1, enum cache_level)

// name of the device file
#define DEVICE_FILE_NAME "cache_dev_v2"