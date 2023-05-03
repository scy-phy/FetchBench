#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <limits>

#include "testcase.hh"
#include "cacheutils.hh"
#include "mapping.hh"
#include "logger.hh"

using std::string;


/**
 * Test case for Adjacent Cache Line Prefetcher.
 *
 * Tests if an adjacent cache line prefetcher exists, and if so
 * which cache line is prefetched. Given four adjacent cache
 * lines A, B, C, with the address of A a multiple of 128 bytes
 * (2 * cache line size):
 * 
 *     +---------+---------+---------+
 *     |    A    |    B    |    C    |
 *     +---------+---------+---------+
 *     0        63        127       191
 * 
 * We know of three different implementations for adjacent
 * cache line prefetcher.
 *  - **No prefetch:** accessing any of the cache lines only caches
 *    the accessed cache line, nothing more
 *  - **Next line**: accessing A additionally caches B, accessing B
 *    additionally caches C. Implemented since Intel Ice Lake.
 *  - **128-byte block:** the prefetcher ensures that always a
 *    128-byte aligned block is cached -> accessing A
 *    additionally caches B, accessing B additionally caches A.
 */
class TestCaseAdjacent : public TestCaseBase {
private:
	size_t const fr_thresh;
	size_t const noise_thresh;
	bool const use_nanosleep;
	// structs for nanosleep
	struct timespec const t_req;
	struct timespec t_rem;

public:
	TestCaseAdjacent(size_t fr_thresh, size_t noise_thresh, bool use_nanosleep)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	, use_nanosleep {use_nanosleep}
	, t_req { .tv_sec = 0, .tv_nsec = 1000 /* 1µs */ }
	{}

	virtual string id() override {
		return "adjacent";
	}
private:
	size_t access_measure(uint8_t* ptr1, uint8_t* ptr2) {
		size_t sum = 0, min = std::numeric_limits<size_t>::max();
		size_t repeat = 10000;

		for(size_t i = 0; i < repeat; i++) {
			for(int f = 0; f < 512; f += CACHE_LINE_SIZE) {
				flush(ptr1 + f);
				flush(ptr2 + f);
			}
			mfence();

			maccess(ptr1);
			mfence();

			// sleep a while to give the prefetcher some time to work
			if (use_nanosleep) {
				nanosleep(&t_req, &t_rem);
			}
			
			size_t start = rdtsc();
			maccess(ptr2);
			size_t end = rdtsc();
			size_t delta = end - start;
			sum += delta;
			if(delta < min) min = delta;
			mfence();
		}
		return sum / repeat;
	}

	Json test_adjacent() {
		Mapping mapping = allocate_mapping(2 * PAGE_SIZE);
		flush_mapping(mapping);
			
		int patterns[2] = {0,};
		
		L::debug("[ Even cache line ]\n");
		for(int offset = 0; offset < 2; offset++) {
			uint8_t* d = mapping.base_addr + 2048 + offset * CACHE_LINE_SIZE;

			int pos = 0;
			for(int range = -3; range <= 2; range++) {
				if(((size_t)d + range * CACHE_LINE_SIZE) % (2 * CACHE_LINE_SIZE) == 0) L::debug("\n");
				size_t t = access_measure(d, d + range * CACHE_LINE_SIZE);
				L::debug("%2d:     %zd\n", range, t);
				patterns[offset] |= (t > noise_thresh && t < fr_thresh) << pos;
				pos++;
			}
			if(offset == 0) L::debug("\n[ Odd cache line ]\n");
		}
		L::debug("%x - %x\n", patterns[0], patterns[1]);

		string result = "";

		if (patterns[0] == 0b001000 && patterns[1] == 0b001000) {
			result = "none";
		} else if (patterns[0] == 0b011000 && patterns[1] == 0b001100) {
			result = "2CL block";
		} else if (patterns[0] == 0b011000 && patterns[1] == 0b011000) {
			result = "next";
		} else {
			result = "unknown";
		}   

		unmap_mapping(mapping);

		return Json::object {
			{ "result", result },
			{ "success", true }
		};
	}

protected:
	virtual Json pre_test() override {
		architecture_t arch = get_arch();
		if (arch == ARCH_INTEL) {
			// OBSERVATION (originally on i7-2620M): The adjacent CL
			// prefetcher only works when the L2 HW prefetcher is enabled
			// as well. If only one of both is enabled, there is no
			// adjacent cache line prefetching. That's why we enable both
			// for this test.
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, false);
		} else if (arch == ARCH_ARM) {
		}
		return Json::object {
			{"architecture", arch},
		};
	}

	virtual Json post_test() override {
		if (get_arch() == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, true);
		}
		return Json::object {};
	}

	
	virtual Json identify() override {
		Json test_results = test_adjacent();
		bool identified = (test_results["result"].string_value() != "none");
		return Json::object {
			{ "identified", identified },
			{ "test_adjacent", test_results },
		};
	}
	
	virtual Json characterize() override {
		return {};
	}
};