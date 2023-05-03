#include "testcase_stride_strideexperiment.hh"

using json11::Json;
using std::vector;

// Evaluation helper functions: were specific offsets (in bytes from
// the beginning of the mapping) or specific cachelines (also counted
// from the beginning of a mapping) accessed when the experiment was
// run? Are they potential prefetch candidates?

bool StrideExperiment::offset_accessed(size_t offset) const {
	// "normalize": subtract initial offset
	ssize_t offset_normalized = offset - first_access_offset;
	// accesses only happen at multiples of the stride
	bool is_multiple_of_stride = (offset_normalized % stride == 0);
	// how many steps from first access?
	ssize_t offset_step = offset_normalized / stride;

	if (is_multiple_of_stride && offset_step >= 0 && offset_step < (ssize_t)step) {
		return true;
	}
	return false;
}

bool StrideExperiment::offset_potential_prefetch(size_t offset) const {
	// "normalize": subtract initial offset
	ssize_t offset_normalized = offset - first_access_offset;
	// accesses only happen at multiples of the stride
	bool is_multiple_of_stride = (offset_normalized % stride == 0);
	// how many steps from first access?
	ssize_t offset_step = offset_normalized / stride;
	
	if (is_multiple_of_stride && offset_step >= 0 && offset_step >= (ssize_t)step) {
		return true;
	}
	return false;	
}

bool StrideExperiment::cl_accessed(size_t cl_idx) const {
	for (size_t offset = cl_idx * CACHE_LINE_SIZE; offset < (cl_idx+1) * CACHE_LINE_SIZE; offset++) {
		if (offset_accessed(offset)) {
			return true;
		}
	}
	return false;
}

bool StrideExperiment::cl_potential_prefetch(size_t cl_idx) const {
	for (size_t offset = cl_idx * CACHE_LINE_SIZE; offset < (cl_idx+1) * CACHE_LINE_SIZE; offset++) {
		if (offset_potential_prefetch(offset) && (stride >= CACHE_LINE_SIZE || !cl_accessed(cl_idx))) {
			return true;
		}
	}
	return false;
}

/**
 * Collects a cache histogram. To this end, this function runs the
 * provided `workload` `no_repetition` times in the memory area
 * specified by `mapping` and probes the cache afterwards. To be able
 * to use Flush+Reload for probing without introducing side-effects
 * through the probing, we only probe a single cache line after each
 * execution of the workload. The probed cache line moves by +1 in each
 * iteration, and wraps around once the end of the memory area is
 * reached. The cache histogram represents the cache state after the
 * experiment. It is a vector<size_t>, where each entry represents one
 * of the cache lines. The value indicates the number of hits seen in
 * this cache line.
 *
 * @param      mapping         The mapping to execute the workload on
 * @param[in]  no_repetitions  Number of repetitions
 * @param[in]  workload        The workload to run
 *
 * @return     Cache histogram (absolute counters per cache line)
 */
vector<size_t> StrideExperiment::collect_cache_histogram(Mapping const& mapping, size_t no_repetitions, void (*workload)(StrideExperiment const&, Mapping const&, void*), void* additional_info) {
	// ensure the first and last access are in bounds of the mapping
	uint8_t* ptr_begin = get_ptr_begin(mapping);
	uint8_t* ptr_last = get_ptr_end(mapping) - stride;
	assert(ptr_begin >= mapping.base_addr && ptr_begin < mapping.base_addr + mapping.size);
	assert(ptr_last >= mapping.base_addr && ptr_last < mapping.base_addr + mapping.size);
	
	vector<size_t> cache_histogram (mapping.size / CACHE_LINE_SIZE, 0);
	for (size_t repetition = 0; repetition < no_repetitions; repetition++) {
		// flush mappings
		flush_mapping(mapping);
		
		// induce pattern
		if (workload != nullptr) {
			workload(*this, mapping, additional_info);
		}
		mfence();
		
		// sleep a while to give the prefetcher some time to work
		if (use_nanosleep) {
			nanosleep(&t_req, &t_rem);
		}

		// probe probe array
		size_t probe_idx = repetition % (cache_histogram.size());
		probe_single(cache_histogram, probe_idx, mapping.base_addr + (probe_idx * CACHE_LINE_SIZE));
	}
	// normalize cache histogram
	for (size_t& hist_value : cache_histogram) {
		hist_value = hist_value * 1000 / (no_repetitions / cache_histogram.size());
	}
	return cache_histogram;
}

/**
 * Same as the other collect_cache_histogram() function, but for
 * workloads that require 2 mappings to work in. Only mapping2 will be
 * probed though.
 *
 * @param      mapping1        The mapping 1 (will not be probed)
 * @param      mapping2        The mapping 2 (will be probed)
 * @param[in]  no_repetitions  Number of repetitions
 * @param[in]  workload        The workload to run
 *
 * @return     Cache histogram (absolute counters per cache line)
 */
vector<size_t> StrideExperiment::collect_cache_histogram(Mapping const& mapping1, Mapping const& mapping2, size_t no_repetitions, void (*workload)(StrideExperiment const&, Mapping const&, Mapping const&, void*), void* additional_info) {
	// ensure the first and last access are in bounds of the mapping
	uint8_t* ptr_begin_1 = get_ptr_begin(mapping1);
	uint8_t* ptr_last_1 = get_ptr_end(mapping1) - stride;
	uint8_t* ptr_begin_2 = get_ptr_begin(mapping2);
	uint8_t* ptr_last_2 = get_ptr_end(mapping2) - stride;
	assert(ptr_begin_1 >= mapping1.base_addr && ptr_begin_1 < mapping1.base_addr + mapping1.size);
	assert(ptr_last_1 >= mapping1.base_addr && ptr_last_1 < mapping1.base_addr + mapping1.size);
	assert(ptr_begin_2 >= mapping2.base_addr && ptr_begin_2 < mapping2.base_addr + mapping2.size);
	assert(ptr_last_2 >= mapping2.base_addr && ptr_last_2 < mapping2.base_addr + mapping2.size);

	vector<size_t> cache_histogram (mapping2.size / CACHE_LINE_SIZE, 0);
	for (size_t repetition = 0; repetition < no_repetitions; repetition++) {
		// flush mappings
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		
		// induce pattern
		if (workload != nullptr) {
			workload(*this, mapping1, mapping2, additional_info);
		}
		mfence();
		
		// sleep a while to give the prefetcher some time to work
		if (use_nanosleep) {
			nanosleep(&t_req, &t_rem);
		}

		// probe probe array
		size_t probe_idx = repetition % (cache_histogram.size());
		probe_single(cache_histogram, probe_idx, mapping2.base_addr + (probe_idx * CACHE_LINE_SIZE));
	}
	// normalize cache histogram
	for (size_t& hist_value : cache_histogram) {
		hist_value = hist_value * 1000 / (no_repetitions / cache_histogram.size());
	}
	return cache_histogram;
}

/**
 * Variant of collect_cache_histogram for large mappings. For large
 * mappings, there are a lot of locations where misses are expected,
 * and only very few "interesting" locations (where we expect
 * prefetching). So we would waste a lot of iterations (and therefore
 * time) on probing cache lines where we expect misses. In this
 * variant, we only probe the locations where prefetching is expected,
 * and ignore all other locations (will be reported as 0). Further,
 * the resulting histogram reports relative values (from 0..1000) instead of the
 * absolute number of hits for each cache line.
 *
 * @param      mapping         The mapping to execute the workload on
 * @param[in]  no_repetitions  Number of repetitions
 * @param[in]  workload        The workload to run
 *
 * @return     Cache histogram (relative counters \in [0, 1000] per cache line)
 */
vector<size_t> StrideExperiment::collect_cache_histogram_lazy(Mapping const& mapping, size_t no_repetitions, void (*workload)(StrideExperiment const&, Mapping const&, void*), void* additional_info) {
	// ensure the first and last access are in bounds of the mapping
	uint8_t* ptr_begin = get_ptr_begin(mapping);
	uint8_t* ptr_last = get_ptr_end(mapping) - stride;
	assert(ptr_begin >= mapping.base_addr && ptr_begin < mapping.base_addr + mapping.size);
	assert(ptr_last >= mapping.base_addr && ptr_last < mapping.base_addr + mapping.size);
	
	// only probe indices that are multiples of the stride
	vector<size_t> indices_to_probe;
	for (
		ssize_t offset = first_access_offset;
		(stride > 0) ? (offset < (ssize_t)mapping.size) : (offset >= 0);
		offset += stride
	) {
		indices_to_probe.push_back(offset / CACHE_LINE_SIZE);
	}
	for (
		ssize_t offset = first_access_offset - stride;
		(stride > 0) ? (offset >= 0) : (offset < (ssize_t)mapping.size);
		offset += -stride
	) {
		indices_to_probe.push_back(offset / CACHE_LINE_SIZE);
	}
	vector<size_t> cache_histogram (mapping.size / CACHE_LINE_SIZE, 0);
	for (size_t repetition = 0; repetition < no_repetitions; repetition++) {
		// flush mapping
		for (size_t i = 0; i < indices_to_probe.size(); i++) {
			flush(mapping.base_addr + CACHE_LINE_SIZE * indices_to_probe[i]);
		}
		mfence();
		
		// induce pattern
		if (workload != nullptr) {
			workload(*this, mapping, additional_info);
		}
		mfence();
		
		// sleep a while to give the prefetcher some time to work
		if (use_nanosleep) {
			nanosleep(&t_req, &t_rem);
		}

		// probe probe array
		size_t probe_idx = indices_to_probe[repetition % indices_to_probe.size()];
		probe_single(cache_histogram, probe_idx, mapping.base_addr + (probe_idx * CACHE_LINE_SIZE));
	}
	// normalize cache histogram
	for (size_t const& idx : indices_to_probe) {
		cache_histogram[idx] = cache_histogram[idx] * 1000 / (no_repetitions / indices_to_probe.size());
	}
	return cache_histogram;
}

/**
 * Reduces a cache histogram, i.e., vector<size_t>, to a vector<bool>
 * of same size. Cache lines where prefetches were both _expected_ AND
 * _observed_ are marked as `true` in the returned "prefetch vector".
 * All other lines are marked `false`.
 *
 * @param      cache_histogram       The cache histogram
 * @param[in]  threshold_multiplier  The threshold multiplier
 *
 * @return     prefetch vector.
 */
vector<bool> StrideExperiment::evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions, double threshold_multiplier) const {
	// compute averages for (a) all locations where we expect hits,
	// (b) all locations where we expect misses
	size_t hit_avg = 0, hit_n = 0;
	size_t miss_avg = 0, miss_n = 0;
	for (size_t cl_idx = 0; cl_idx < cache_histogram.size(); cl_idx++) {
		if (cl_accessed(cl_idx)) {
			// architectural hit
			L::debug("expecting hit at %2zu:               %6zu\n", cl_idx, cache_histogram[cl_idx]);
			hit_avg += cache_histogram[cl_idx];
			hit_n++;
		} else if (cl_potential_prefetch(cl_idx)) {
			// prefetch (ignore for now)
			L::debug("potential prefetch location at %2zu: %6zu\n", cl_idx, cache_histogram[cl_idx]);
		} else {
			// miss location
			L::debug("miss location at %2zu:          %6zu\n", cl_idx, cache_histogram[cl_idx]);
			miss_avg += cache_histogram[cl_idx];
			miss_n++;
		}
	}
	if (hit_n > 0) {
		hit_avg /= hit_n;
	}
	if (miss_n > 0) {
		miss_avg /= miss_n;
	}

	L::debug("average hit:       %3zu\n", hit_avg);
	L::debug("average miss:      %3zu\n", miss_avg);
	size_t prefetch_thresh = miss_avg + (size_t)(threshold_multiplier * (double)(hit_avg - miss_avg));
	if (prefetch_thresh <= 0) { prefetch_thresh = 1; }
	L::debug("prefetch thresh:   %3zu\n", prefetch_thresh);

	// iterate over the possible prefetch locations and use the prefetch_threshold
	// to decide whether this is a prefetch or not.
	vector<bool> prefetch_vector (cache_histogram.size(), false);
	for (size_t cl_idx = 0; cl_idx < cache_histogram.size(); cl_idx++) {
		// check whether this is a prefetch location or not
		if (cl_potential_prefetch(cl_idx)) {
			L::debug("potential prefetch location at %2zu: %6zu\n", cl_idx, cache_histogram[cl_idx]);
			// check whether the value exceeds the noise threshold
			if (cache_histogram[cl_idx] > noise_thresh) {
				L::debug(" *** Exceeds noise threshold (%zu > %zu)\n", cache_histogram[cl_idx], noise_thresh);
				// check whether the value exceeds the prefetch threshold
				if (cache_histogram[cl_idx] >= prefetch_thresh) {
					L::debug(" *** I think this is a prefetch (%zu >= %zu). ***\n", cache_histogram[cl_idx], prefetch_thresh);
					prefetch_vector[cl_idx] = true;
				}
			}
		}
	}
	return prefetch_vector;
}


/**
 * Shortcut to call evaluate_cache_histogram with a default
 * threshold_multiplier of 1/64.
 *
 * @param      cache_histogram  The cache histogram
 *
 * @return     prefetch vector.
 */
vector<bool> StrideExperiment::evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions) const {
	return evaluate_cache_histogram(cache_histogram, no_repetitions, 1.0/64);
}

/**
 * Dumps a Stride Experiment and a cache histogram to a JSON file.
 *
 * @param      cache_histogram  The cache histogram
 * @param[in]  prefetch_vector  The prefetch vector
 * @param      filepath         The file path to the JSON file
 */
void StrideExperiment::dump(vector<size_t> const& cache_histogram, vector<bool> prefetch_vector, string const& filepath) const {
	// Build a JSON array from the cache histogram numbers
	Json::array cache_histogram_values {};
	for (size_t i = 0; i < cache_histogram.size(); i++) {
		cache_histogram_values.push_back((int)cache_histogram[i]);
	}
	Json::array prefetch_vector_values {};
	for (size_t i = 0; i < prefetch_vector.size(); i++) {
		prefetch_vector_values.push_back((bool)prefetch_vector[i]);
	}

	Json j = Json::object {
		{ "stride", (int)stride },
		{ "step", (int)step },
		{ "first_access_offset", (int)first_access_offset },
		{ "use_nanosleep", use_nanosleep },
		{ "fr_thresh", (int)fr_thresh },
		{ "noise_thresh", (int)noise_thresh },
		{ "cache_histogram", cache_histogram_values },
		{ "prefetch_vector", prefetch_vector },
		{ "cache_line_size", CACHE_LINE_SIZE },
	};
	
	// write JSON to file
	json_dump_to_file(j, filepath);
}

/**
 * Resotres a cache histogram from a JSON file.
 *
 * @param      filepath  The file path to the JSON file
 *
 * @return     Pair of stride experiment object and cache histogram.
 */
pair<StrideExperiment, vector<size_t>> StrideExperiment::restore(string const& filepath) {
	std::ifstream file;
	file.open(filepath);
	string line;

	if ( ! file.is_open()) {
		printf("Failed to open file %s.\n", filepath.c_str());
		exit(1);
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	string json_str = buffer.str();
	file.close();

	string json_err;
	Json json = Json::parse(json_str, json_err);
	StrideExperiment experiment {
		(ssize_t)json["stride"].int_value(),
		(size_t)json["step"].int_value(),
		(size_t)json["first_access_offset"].int_value(),
		json["use_nanosleep"].bool_value(),
		(size_t)json["fr_thresh"].int_value(),
		(size_t)json["noise_thresh"].int_value(),
	};

	vector<size_t> cache_histogram;
	for (Json value : json["cache_histogram"].array_items()) {
		cache_histogram.push_back((size_t)value.int_value());
	}
	
	return {experiment, cache_histogram};
}
