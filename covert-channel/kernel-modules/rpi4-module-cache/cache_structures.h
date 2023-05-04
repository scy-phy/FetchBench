#pragma once

#include <stdbool.h>

// Cortex A72: 32 KiB L1D cache, 2 ways
#define WAYS_L1 (2)
#define SETS_L1 (32 * 1024 / 64 / WAYS_L1)

// Cortex A72: 1 MiB L2 cache, 16 ways
#define WAYS_L2 (16)
#define SETS_L2 (1024 * 1024 / 64 / WAYS_L2)
#define CACHE_LINE_WIDTH (64)

#define NUM_LINES_L1 (SETS_L1 * WAYS_L1)
#define NUM_LINES_L2 (SETS_L2 * WAYS_L2)

// #define SET_OF_ADDR_L1(x) ((((uint64_t)(x))/64) % SETS_L1)
// #define SET_OF_ADDR_L2(x) ((((uint64_t)(x))/64) % SETS_L2)
// #define TAG_OF_ADDR(x) (((uint64_t)(x)) / SETS / 64)

