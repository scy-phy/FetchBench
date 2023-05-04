#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../cache_structures.h"
#include "../cache_ioctl.h"

#define PAGE_SIZE (4096)

// file descriptor to the device file that is our ioctl link to the kernel
// module. Initialized at program start.
static int cache_dev_fd;

/**
 * @brief      Memory load
 *
 * @param      p     Address to load from
 */
static inline __attribute__((always_inline)) void maccess(void *p) {
	volatile uint32_t value;
	asm volatile ("LDR %w0, [%1]\n\t"
		: "=r" (value)
		: "r" (p)
	);
}
/**
 * @brief      Memory Barrier
 */
static inline __attribute__((always_inline)) void mfence() {
	asm volatile ("DSB SY");
	asm volatile ("ISB");
}


/**
 * @brief      Flush cache line
 *
 * @param      ptr   Address of the cache line to flush.
 */
static inline __attribute__((always_inline)) void flush(void* ptr) {
	asm volatile ("DC CIVAC, %0" :: "r"(ptr));
	asm volatile ("DSB ISH");
	asm volatile ("ISB");
}

/**
 * @brief      Sends the given setup struct to the kernel module to set it
 *             up and prepare a query.
 *
 * @param      setup_ptr  The pointer to the setup struct
 */
void ioctl_setup(struct cache_request_setup* setup_ptr) {
	int res = ioctl(cache_dev_fd, IOCTL_SETUP, setup_ptr);
	if (res != 0) {
		exit(-1);
	}
	puts("setup done.");
}

/**
 * @brief      Sends a cache query to the kernel module (i.e. the kernel
 *             module will fill the arrays provided at setup.resp_lx
 *             accordingly). Important: run ioctl_setup first, otherwise
 *             the query will fail.
 *
 * @param[in]  level  The cache level to query
 */
void ioctl_query(enum cache_level level) {
	int res = ioctl(cache_dev_fd, IOCTL_QUERY, level);
	if (res != 0) {
		exit(-1);
	}
	puts("query done.");
}

typedef struct {
	uint8_t* base_addr;
	size_t size;
} Mapping;

/**
 * @brief      Allocate a mapping of a given size (in bytes) to store data
 *             or for use as a probe array. The memory will be page-aligned
 *             (i.e. start at the beginning of a page).
 *
 * @param[in]  mem_size  The size of the memory mapping.
 *
 * @return     Mapping struct describing the new mapping.
 */
Mapping allocate_mapping(size_t mem_size) {
	Mapping mapping = {NULL, mem_size};
	mapping.base_addr = (uint8_t*) mmap(
		NULL, mapping.size, PROT_READ | PROT_WRITE,
		MAP_POPULATE | MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0
	);
	if (mapping.base_addr == MAP_FAILED) {
		puts("mmap failed.");
		exit(1);
	}
	mlock(mapping.base_addr, mapping.size);
	return mapping;
}

int main() {
	// Reserve some memory for the kernel module to deliver query results
	// into. The size corresponds to the maximum possible number of results
	// plus one (for a result counter). The contents of these memory
	// structures can be interpreted as follows: after the query is done,
	// the first element ([0]) is a counter that indicates the number of
	// cache hits encountered. All other are offsets in Bytes relative to
	// setup.pa_base_addr. For example, if resp_l1 is {2, 0, 64}, the first
	// number indicates that there were 2 hits, the first at .pa_base_addr,
	// the second 64 bytes later (+ 1 cache line). The detection of hits
	// has cache-line granularity, i.e. all hits are multiples of 64.
	Mapping resp_l1 = allocate_mapping((NUM_LINES_L1+1) * sizeof(size_t));
	Mapping resp_l2 = allocate_mapping((NUM_LINES_L2+1) * sizeof(size_t));

	// allocate a probe array
	Mapping pa = allocate_mapping(3 * PAGE_SIZE);

	// prepare a setup struct (to be sent to the kernel module)
	pid_t my_pid = getpid();
	struct cache_request_setup setup = {
		// process pid, required for the module to work on the right memory
		// map during virtual-to-physical address translation
			.pid = my_pid,
		// probe array information
			.pa_base_addr = pa.base_addr,
			.pa_size = pa.size,
		// result memory information
			.resp_l1 = (size_t*)resp_l1.base_addr,
	    	.resp_l1_size = resp_l1.size,
			.resp_l2 = (size_t*)resp_l2.base_addr,
	    	.resp_l2_size = resp_l2.size
	};

	// open device file for communication with the kernel module
	cache_dev_fd = open(DEVICE_FILE_NAME, 0);
	if (cache_dev_fd < 0) {
		printf ("Can't open device file: %s\n", DEVICE_FILE_NAME);
		exit(-1);
	}

	// send setup struct to kernel module
	ioctl_setup(&setup);

	// perform some memory accesses
	// (NB: the memory area is not flushed, so some entries might be in
	// cache anyway as remeainder of the allocation of the mapping!)
	maccess(pa.base_addr);
	maccess(pa.base_addr + 1 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + 2 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + 3 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + 4 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + 5 * CACHE_LINE_WIDTH);

	maccess(pa.base_addr + PAGE_SIZE);
	maccess(pa.base_addr + PAGE_SIZE + 1 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + PAGE_SIZE + 2 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + PAGE_SIZE + 3 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + PAGE_SIZE + 4 * CACHE_LINE_WIDTH);
	maccess(pa.base_addr + PAGE_SIZE + 5 * CACHE_LINE_WIDTH);
	
	mfence();

	// query the results
	ioctl_query(CACHE_ALL);

	// print the results
	puts("=== L1 ===");
	size_t no_hits_l1 = setup.resp_l1[0];
	printf("Number of hits: %zu\n", no_hits_l1);
	for (uint64_t i = 1; i <= no_hits_l1; i++) {
		printf("resp_l1[%lu] = %zu\n", i, setup.resp_l1[i]);
	}
	puts("=== L2 ===");
	size_t no_hits_l2 = setup.resp_l2[0];
	printf("Number of hits: %zu\n", no_hits_l2);
	for (uint64_t i = 1; i <= no_hits_l2; i++) {
		printf("resp_l2[%lu] = %zu\n", i, setup.resp_l2[i]);
	}
	
	close(cache_dev_fd);
}