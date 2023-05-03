#pragma once
#include <cinttypes>
#include <unistd.h>

typedef struct {
	uint8_t* base_addr;
	size_t size;
} Mapping;

Mapping allocate_mapping(size_t mem_size);
void unmap_mapping(Mapping const& mapping);
void flush_mapping(Mapping const& mapping);

// upper_bound must be power of 2.
static inline size_t permute(size_t upper_bound, size_t original_idx) {
    return ((original_idx * 167u) + 13u) & (upper_bound - 1);
}
void random_activity(Mapping const& mapping);