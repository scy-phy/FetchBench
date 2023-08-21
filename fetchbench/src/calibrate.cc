#include <algorithm>

#include "calibrate.hh"
#include "mapping.hh"
#include "cacheutils.hh"
#include "testcase_stride_strideexperiment.hh"

/**
 * Helper function to determine the Flush+Reload threshold.
 *
 * @param      ptr1    The first pointer to work with
 * @param      ptr2    The second pointer to work with
 * @param[in]  repeat  The number of repetitions to perform
 * @param[in]  median  The median
 *
 * @return     The average timing value
 */
size_t access_measure(uint8_t* ptr1, uint8_t* ptr2, size_t repeat, size_t median) {
	size_t sum = 0;
	size_t avg = 0;
	size_t j = 0;

	for(size_t i = 0; i < repeat; i++) {
		for(size_t f = 0; f < 512; f += CACHE_LINE_SIZE) {
			flush(ptr1 + f);
			flush(ptr2 + f);
		}
		mfence();

		maccess(ptr1);
		mfence();
		size_t start = rdtsc();
		maccess(ptr2);
		size_t end = rdtsc();
		size_t delta = end - start;

		// printf("%s: %zu\n", label.c_str(), delta);
		if(median && (delta < median/1.25 || delta > 1.25*median))
			continue;
		sum += delta;
		j++;

		mfence();
	}
	avg = sum / j;
	return avg;
}

/**
 * Calibrates the Flush+Reload threshold
 *
 * @param      mapping  The mapping to work in
 *
 * @return     The recommended Flush+Reload threshold
 */
size_t calibrate_thresh(Mapping const& mapping) {
	assert(mapping.size >= 2 * PAGE_SIZE);

	// find median
	size_t repeat = 100000;
	size_t thresh;
	flush_mapping(mapping);
	size_t hit_median = access_measure(mapping.base_addr + 1024, mapping.base_addr + 1024, repeat, 0);
	flush_mapping(mapping);
	size_t miss_median = access_measure(mapping.base_addr + 1024, mapping.base_addr + PAGE_SIZE + 512 + 1024, repeat, 0);
	L::debug("Median: Hit(%zu) Miss(%zu)\n", hit_median, miss_median);
	
	// use median to remove outliers
	repeat = 10000000;
	flush_mapping(mapping);
	size_t hit = access_measure(mapping.base_addr + 1024, mapping.base_addr + 1024, repeat, hit_median);
	flush_mapping(mapping);
	size_t miss = access_measure(mapping.base_addr + 1024, mapping.base_addr + PAGE_SIZE + 512 + 1024, repeat, miss_median);

	// set threshold
	thresh = (hit + miss) / 2;
	L::debug("Threshold: %zu (%zu - %zu)\n", thresh, hit, miss);
	return thresh;
}

/**
 * Determines whether to use the use_nanosleep flag or not.
 *
 * @param      mapping         The mapping to work in
 * @param[in]  no_repetitions  The number of repetitions to perform
 * @param[in]  fr_thresh       The Flush+Reload threshold to use
 * @param[in]  noise_thresh    The noise threshold to use
 *
 * @return     The recommended value for the use_nanosleep flag
 */
static bool calibrate_sleep(Mapping const& mapping, size_t no_repetitions, size_t fr_thresh, size_t noise_thresh) {
	ssize_t stride = 3 * CACHE_LINE_SIZE;
	size_t step = 12;
	size_t prefetch_count_pos[2];
	bool use_nanosleep = true;

	// compare results of stride prefetcher with and without sleep to decide its need.
	for (int i = 0; i < 2; i++) {
		StrideExperiment calib_noise { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
		vector<size_t> cache_histogram_pos = calib_noise.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		vector<bool> prefetch_vector_diff = calib_noise.evaluate_cache_histogram(cache_histogram_pos, no_repetitions);
		random_activity(mapping);
		flush_mapping(mapping);

		prefetch_count_pos[i] = std::count(prefetch_vector_diff.begin(), prefetch_vector_diff.end(), true);
		use_nanosleep ^= true;
	}
	L::debug("\n prefetch count sleep(true): %zu, sleep(false): %zu\n", prefetch_count_pos[0], prefetch_count_pos[1]);
	if (prefetch_count_pos[0] >= prefetch_count_pos[1])
		use_nanosleep = true;
	else
		use_nanosleep = false;
	return use_nanosleep;

}

/**
 * Calibrates the noise threshold.
 *
 * @param      mapping         The mapping to work in
 * @param[in]  no_repetitions  The number of repetitions to perform
 * @param[in]  use_nanosleep   Whether to use nanosleep or not
 * @param[in]  fr_thresh       The Flush+Reload threshold to use
 *
 * @return     The recommneded noise threshold.
 */
static size_t calibrate_noise_thresh(Mapping const& mapping, size_t no_repetitions, bool use_nanosleep, size_t fr_thresh){
	// use dummy values to pass the assertion checks in collect_cache_histogram
	ssize_t stride = 40 * CACHE_LINE_SIZE;
	size_t step = 2;
	size_t thresh = 0;
	StrideExperiment calib_noise { stride, step, 0, use_nanosleep, fr_thresh, 0 };

	// pass NULL as workload to probe all the CL to compute average noise
	vector<size_t> cache_histogram_pos = calib_noise.collect_cache_histogram(mapping, no_repetitions, nullptr, nullptr);
	// proble all the cache lines to find max noise.
	for (auto it = cache_histogram_pos.begin() + 1; it < cache_histogram_pos.end(); it++) {
		if (thresh < *it) {
			thresh = *it;
		}
	}
	L::debug("\nnoise: %zu %zu \n", thresh*2, (thresh*2) * 1000/(no_repetitions/cache_histogram_pos.size()));
	return (thresh*2) * 1000/(no_repetitions/cache_histogram_pos.size());
}

/**
 * Calibrates all parameters. Returns the results via the references given
 * as function parameters.
 *
 * @param      fr_thresh      The Flush+Reload threshold
 * @param      noise_thresh   The noise threshold
 * @param      use_nanosleep  The use_nanosleep flag
 */
void calibrate(size_t& fr_thresh, size_t& noise_thresh, int& use_nanosleep) {
	Mapping mapping = allocate_mapping(2 * PAGE_SIZE);
	size_t no_repetitions = 40000;
	
	// Calibrate FR threshold
	if (fr_thresh == 0) {
		fr_thresh = calibrate_thresh(mapping);
		random_activity(mapping);
		flush_mapping(mapping);
	}

	// Calibrate noise level
	if (noise_thresh == std::numeric_limits<size_t>::max()) {
		noise_thresh = calibrate_noise_thresh(mapping, 10 * no_repetitions, use_nanosleep, fr_thresh);
		random_activity(mapping);
		flush_mapping(mapping);
	}

	// Is sleep required
	if (use_nanosleep == -1) {
		use_nanosleep = true;
		use_nanosleep = calibrate_sleep(mapping, no_repetitions, fr_thresh, noise_thresh);
		random_activity(mapping);
		flush_mapping(mapping);
	}

	unmap_mapping(mapping);
}
