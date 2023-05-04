#include <linux/kernel.h>
#include <linux/module.h>

#include "cache.h"

#define BARRIER_DSB_ISB() __asm__ __volatile__("DSB SY \t\n ISB \t\n")

/**
 * @brief      Determines whether the specified physical address is cached
 *             in L1 or not.
 *
 * @param[in]  phys_addr  The physical address
 *
 * @return     True if the specified physical address is cached in L1,
 *             False otherwise.
 */
bool is_cached_a72_l1(uint64_t phys_addr) {
	// isolate tag of the physical address for later comparison
	uint64_t input_tag = phys_addr & (0x3FFFFFFFULL << 14);
	volatile uint64_t value;
	BARRIER_DSB_ISB();

	// query the L1 tag ram for each cache way
	for (uint64_t way = 0; way < WAYS_L1; way++) {
		uint64_t cached_tag;
		uint64_t mesi;

		// query tag info
		value = 0;
		value |= (0x08ULL & 0b11111111ULL) << 24;   // RAMID = 0x08 (L1-D Tag RAM)
		value |= (way & 0b1ULL) << 18;              // way select
		value |= (phys_addr & 0b11111111000000ULL); // physical address [13:6] ("L1 set")
		
		asm volatile (
			"SYS #0, C15, C4, #0, %x[input_i]"
			:
			: [input_i] "r" (value)
		);
		BARRIER_DSB_ISB();

		// read tag info
		// DL1DATA0[29:0] contains the physical address tag (bits [43:14]
		// of the physical address). If it equals the upper bits of input
		// phys_addr, we have a match.
		asm volatile (
			"MRS %x[result], S3_0_C15_C1_0" // DL1DATA0
			: [result] "=r" (value)
			:
		);
		
		// isolate the tag of the physical address stored at the current
		// position in cache
		cached_tag = (value & 0x3FFFFFFFULL) << 14;
		
		// extract MESI flag (just to check if it is != 0b00 (i.e. valid))
		// DL1DATA1[1:0] contains the MESI flag (we use it to check the
		// result for validity, i.e., != 0.)
		asm volatile (
			"MRS %x[result], S3_0_C15_C1_1" // DL1DATA1
			: [result] "=r" (value)
			:
		);
		mesi = value & 0b11ULL;

		// if stored tag == expected tag and the entry is valid, the given
		// physical address is currently stored in cache. (the cache set
		// address bits must match because we queried the corresponding
		// cache set when we queried the tag information, so it's
		// sufficient to check just the tag,)
		if (cached_tag == input_tag && mesi != 0) {
			return true;
		}
	}
	return false;
}

bool is_cached_a72_l2(uint64_t phys_addr) {
	// isolate tag of the physical address for later comparison.
	// NOTE: The A72 technical reference manual states (p. 185) that the
	// tag returned in DL1DATA0[30:2] was equal to physical address bits
	// [43:15]. In fact, bit 3 in the response (pysical address bit 15) is
	// always 0, so we ignore it (that's the "e" instead of an "f" in the
	// mask). This is not a problem for correctness since bits 15 and 16
	// must match anyway as they are included in the query (they are part
	// of the "L2 set").
	uint64_t input_tag = phys_addr & (0x1ffffffeULL << 15); // [43:15], forcing bit 15 to 0
	volatile uint64_t value;
	BARRIER_DSB_ISB();

	// query the L1 tag ram for each cache way
	for (uint64_t way = 0; way < WAYS_L2; way++) {
		uint64_t cached_tag;
		uint64_t moesi;

		// query tag info
		value = 0;
		value |= (0x10ULL & 0b11111111ULL) << 24;      // RAMID = 0x10 (L2-D Tag RAM)
		value |= (way & 0b1111ULL) << 18;              // way select
		value |= (phys_addr & 0b11111111111000000ULL); // physical address [16:6] ("L2 set")

		asm volatile (
			"SYS #0, C15, C4, #0, %x[input_i]"
			:
			: [input_i] "r" (value)
		);
		BARRIER_DSB_ISB();

		// read tag info
		// DL1DATA0[1:0] contains the MOESI state (used for validity check).
		// DL1DATA0[30:2] contains the physical address tag (bits [43:15]
		// of the physical address). If it equals the upper bits of input
		// phys_addr, we have a match.
		asm volatile (
			"MRS %x[result], S3_0_C15_C1_0" // DL1DATA0
			: [result] "=r" (value)
			:
		);
		
		// isolate the tag of the physical address stored at the current
		// position in cache
		cached_tag = (value & (0x1FFFFFFFULL << 2)) << (15-2);

		// extract MESI flag (just to check if it is != 0b00 (i.e. valid))
		moesi = value & 0b11ULL;

		// if stored tag == expected tag and the entry is valid, the given
		// physical address is currently stored in cache. (the cache set
		// address bits must match because we queried the corresponding
		// cache set when we queried the tag information, so it's
		// sufficient to check just the tag,)
		if (cached_tag == input_tag && moesi != 0) {
			return true;
		}
	}
	return false;
}

/**
 * @brief      Runs the query: for each of the pages listed in
 *             translation_table, each cache line is checked whether it is
 *             present in L1 cache or not. Each hit is noted in l1_hits.
 *
 * @param      translation_table             The translation table: list of
 *                                           physical addresses of the
 *                                           pages where the setup.pa is
 *                                           stored. The list is expected
 *                                           to be ordered, i.e. the first
 *                                           entry corresponds to the page
 *                                           where the setup.pa memory area
 *                                           begins, and so on.
 * @param[in]  no_translation_table_entries  Number of entries in
 *                                           translation_table
 * @param      l1_hits                       Pointer to an array where the
 *                                           hits can be recorded. The
 *                                           first element will be set to
 *                                           the number of hits, the other
 *                                           elements represent the
 *                                           detected hits by their offset
 *                                           from setup.pa_base_addr. For
 *                                           instance, if the 2nd cache
 *                                           line of that array is cached,
 *                                           the list will contain the
 *                                           value 64.
 *
 * @return     Number of hits.
 */
size_t run_query_l1(uint64_t* translation_table, size_t no_translation_table_entries, size_t* l1_hits) {
	size_t hits = 0;
	for (uint64_t page_idx = 0; page_idx < no_translation_table_entries; page_idx++) {
		for (uint64_t cl_idx = 0; cl_idx < (PAGE_SIZE / CACHE_LINE_WIDTH); cl_idx++) {
			uint64_t phys_addr_to_check = translation_table[page_idx] + cl_idx * CACHE_LINE_WIDTH;
			if (is_cached_a72_l1(phys_addr_to_check)) {
				hits++;
				l1_hits[hits] = (size_t)(page_idx * PAGE_SIZE + cl_idx * CACHE_LINE_WIDTH);
			}
		}
	}
	l1_hits[0] = hits;
	return hits;
}

/**
 * @brief      Runs the query: for each of the pages listed in
 *             translation_table, each cache line is checked whether it is
 *             present in L2 cache or not. Each hit is noted in l2_hits.
 *
 * @param      translation_table             The translation table: list of
 *                                           physical addresses of the
 *                                           pages where the setup.pa is
 *                                           stored. The list is expected
 *                                           to be ordered, i.e. the first
 *                                           entry corresponds to the page
 *                                           where the setup.pa memory area
 *                                           begins, and so on.
 * @param[in]  no_translation_table_entries  Number of entries in
 *                                           translation_table
 * @param      l2_hits                       Pointer to an array where the
 *                                           hits can be recorded. The
 *                                           first element will be set to
 *                                           the number of hits, the other
 *                                           elements represent the
 *                                           detected hits by their offset
 *                                           from setup.pa_base_addr. For
 *                                           instance, if the 2nd cache
 *                                           line of that array is cached,
 *                                           the list will contain the
 *                                           value 64.
 *
 * @return     Number of hits.
 */
size_t run_query_l2(uint64_t* translation_table, size_t no_translation_table_entries, size_t* l2_hits) {
	size_t hits = 0;
	for (uint64_t page_idx = 0; page_idx < no_translation_table_entries; page_idx++) {
		for (uint64_t cl_idx = 0; cl_idx < (PAGE_SIZE / CACHE_LINE_WIDTH); cl_idx++) {
			uint64_t phys_addr_to_check = translation_table[page_idx] + cl_idx * CACHE_LINE_WIDTH;
			if (is_cached_a72_l2(phys_addr_to_check)) {
				hits++;
				l2_hits[hits] = (size_t)(page_idx * PAGE_SIZE + cl_idx * CACHE_LINE_WIDTH);
			}
		}
	}
	l2_hits[0] = hits;
	return hits;
}