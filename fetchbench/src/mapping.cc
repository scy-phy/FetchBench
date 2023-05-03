#include <sys/mman.h>

#include "mapping.hh"
#include "logger.hh"
#include "cacheutils.hh"

/**
 * Allocates a mapping using mmap. The mapping will be page aligned.
 *
 * @param[in]  mem_size  Size of the mapping.
 *
 * @return     Mapping.
 */
Mapping allocate_mapping(size_t mem_size) {
	uint8_t* m = (uint8_t*) mmap(
		NULL, mem_size, PROT_READ | PROT_WRITE,
		MAP_POPULATE | MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0
	);
	if (m == MAP_FAILED) {
		L::err("mmap failed");
		exit(1);
	}
	return Mapping {m, mem_size};
}

/**
 * Unmaps a mapping that was previously allocated via `allocate_mapping()`.
 *
 * @param      mapping  The mapping
 */
void unmap_mapping(Mapping const& mapping) {
	munmap(mapping.base_addr, mapping.size);
}

/**
 * Flushs all cache lines of the mapping.
 *
 * @param      mapping  The mapping
 */
void flush_mapping(Mapping const& mapping) {
	for (uint8_t* ptr = mapping.base_addr; ptr < mapping.base_addr + mapping.size; ptr += CACHE_LINE_SIZE) {
		flush(ptr);
	}
	mfence();
}

/**
 * Run some random memory activity within the provided mapping. The goal is
 * to reset the internal data structure of the prefetcher.
 *
 * @param      mapping  The mapping to work in
 */
void random_activity(Mapping const& mapping) {
	for (size_t i = 0; i < 100000; i++) {
		flush_mapping(mapping);
		size_t cls_per_page = PAGE_SIZE/CACHE_LINE_SIZE;
		for (size_t page = 0; page < mapping.size / PAGE_SIZE; page++) {
			for (size_t cl = 0; cl < cls_per_page; cl += 2) {
				uint8_t* addr = mapping.base_addr + (page * PAGE_SIZE) + permute(cls_per_page, cl) * CACHE_LINE_SIZE;
				maccess(addr);
			}
			mfence();
		}
	}
}
