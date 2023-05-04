#pragma once

#include <stdbool.h>
#include "cache_structures.h"

size_t run_query_l1(uint64_t* translation_table, size_t no_translation_table_entries, size_t* l1_hits);
size_t run_query_l2(uint64_t* translation_table, size_t no_translation_table_entries, size_t* l2_hits);