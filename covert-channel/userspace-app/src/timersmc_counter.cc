#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common_test.h"
#include "timersmc_ioctl.h"

#define TIMERSMC_DEVICE_FILE_PATH ("/dev/" TIMERSMC_DEVICE_FILE_NAME)

static int smc_dev_fd;

void ioctl_setup(uint64_t ctr_phy) {
	int res = ioctl(smc_dev_fd, TIMERSMC_IOCTL_SET_UP_COUNTER, ctr_phy);
	if (res != 0) {
		exit(-1);
	}
}

void ioctl_start_experiment() {
	int res = ioctl(smc_dev_fd, TIMERSMC_IOCTL_START_EXPERIMENT);
	if (res != 0) {
		exit(-1);
	}
}

int main(int argc, char* argv[]) {
	// move to CPU
	move_process_to_cpu(getpid(), 4);
	int pagemap = open_pagemap();

	// shared counter between EL0 and EL3
	uint64_t volatile ctr = 1;
	
	// get physical address of the counter to pass it to EL3
	uint64_t ctr_phy = get_physical_addr(pagemap, (uint64_t)&ctr);
	printf("log: %p\nphy: %lx\n", &ctr, ctr_phy);

	// open device file for communication with the smc kernel module
	smc_dev_fd = open(TIMERSMC_DEVICE_FILE_PATH, 0);
	if (smc_dev_fd < 0) {
		printf ("Can't open device file: %s\n", TIMERSMC_DEVICE_FILE_PATH);
		perror("");
		exit(-1);
	}

	// pass the address of the counter to EL3 and let it increment it
	ioctl_setup(ctr_phy);
	ioctl_start_experiment();
	
	printf("Updated value: %lu\n", ctr);

	return 0;
}
