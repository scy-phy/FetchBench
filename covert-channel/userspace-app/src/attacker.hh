#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <bitset>
#include <map>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <semaphore.h>

#include "nops.h"
#include "common_test.h"
#include "cache_structures.h"
#include "cache_ioctl.h"
#include "smc_ioctl.h"
#include "secret.hh"

#define CACHE_DEVICE_FILE_PATH ("/dev/" DEVICE_FILE_NAME)
#define SMC_DEVICE_FILE_PATH ("/dev/" SMC_DEVICE_FILE_NAME)

#define DEBUG 0

#define debug_print(fmt, ...) \
            do { if (DEBUG) printf(fmt, ## __VA_ARGS__); } while (0)

// ##################################################
// TEST VARIANTS
// ##################################################

#define CORE_SAME
// #define CORE_DIFFERENT

#define REGION_SIZE (1024)
#define SMS_ENTRIES 4
#define MAPPING_SIZE (SMS_ENTRIES * 4 * REGION_SIZE)
#define PATTERN_SIZE 2 // 2 bytes
#define PAYLOAD_SIZE (SMS_ENTRIES * PATTERN_SIZE)
#define TRIGGER_OFFSET 8

#define MAX_RETRY 5
// ##################################################

using std::cout;
using std::endl;
using std::ofstream;
using std::pair;
using std::string;
using std::vector;
using std::map;

typedef struct data_info {
	size_t seq;
	uint8_t data[TX_DATA][MAX_RETRY] = {0};
} data_t;

void ioctl_query(enum cache_level level);

// match first load in trustzone
void padding1() {
	NOP512
//	NOP256
	NOP128
//	NOP64
	NOP32
//	NOP16
	NOP8
	NOP4
	NOP4
	NOP2
	NOP2
}

void aligned_access1(uint8_t* ptr) {
	maccess(ptr);
	mfence();

	// check cache for cached values
	ioctl_query(CACHE_L2);
}

void padding2() {
//	NOP1024
	NOP512
	NOP256
	NOP128
	NOP64
	NOP32
	NOP16
	NOP8
//	NOP4
//	NOP2
//	NOP1
}

void aligned_access2(uint8_t* ptr) {
	maccess(ptr);
	mfence();

	// check cache for cached values
	ioctl_query(CACHE_L2);
}

void padding3() {
//	NOP1024
	NOP512
	NOP256
	NOP128
	NOP64
	NOP32
	NOP16
	NOP8
//	NOP4
//	NOP2
//	NOP1
}

void aligned_access3(uint8_t* ptr) {
	maccess(ptr);
	mfence();

	// check cache for cached values
	ioctl_query(CACHE_L2);
}

void padding4() {
//	NOP1024
	NOP512
	NOP256
	NOP128
	NOP64
	NOP32
	NOP16
	NOP8
//	NOP4
//	NOP2
//	NOP1
}

void aligned_access4(uint8_t* ptr) {
	maccess(ptr);
	mfence();

	// check cache for cached values
	ioctl_query(CACHE_L2);
}

void padding5() {
//	NOP1024
	NOP512
	NOP256
	NOP128
	NOP64
	NOP32
	NOP16
	NOP8
//	NOP4
//	NOP2
//	NOP1
}

void aligned_access5(uint8_t* ptr) {
	maccess(ptr);
	mfence();

	// check cache for cached values
	ioctl_query(CACHE_L2);
}

// Allocate continguous memory
Mapping allocate_mapping(size_t mem_size) {
	uint8_t* m = (uint8_t*) mmap(
		NULL, mem_size, PROT_READ | PROT_WRITE,
		MAP_POPULATE | MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0
	);
	if (m == MAP_FAILED) {
		cout << "mmap failed" << endl;
		exit(1);
	}
	return Mapping {m, mem_size};
}


static void (*aligned_access_array [SMS_ENTRIES]) (uint8_t *p) = {aligned_access1,
							aligned_access2,
							aligned_access3,
							aligned_access4};


