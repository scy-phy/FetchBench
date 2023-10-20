#pragma once

#include <stddef.h>
#include <inttypes.h>
#include <time.h>

#include "common_test.hh"

#define REGION_SIZE (1024)

#define FNR_THRESHOLD (217) // Flush+Reload threshold
#define FNF_THRESHOLD (240) // Flush+Flush threshold

static inline size_t permute(size_t upper_bound, size_t original_idx) {
    return ((original_idx * 167u) + 13u) & (upper_bound - 1);
}
void cache_query_setup();
void cache_query();
void print_hits(uint64_t _cache_state);
void print_hits();
uint64_t get_cache_state();
