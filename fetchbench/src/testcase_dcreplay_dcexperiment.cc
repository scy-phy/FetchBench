#include "testcase_dcreplay_dcexperiment.hh"

using json11::Json;
using std::vector;

DCReplayExperiment::DCReplayExperiment(vector<size_t> training_offsets, vector<size_t> trigger_offsets, bool use_nanosleep, size_t fr_thresh, size_t noise_thresh)
: training_offsets {training_offsets}
, trigger_offsets {trigger_offsets}
, use_nanosleep {use_nanosleep}
, fr_thresh {fr_thresh}
, noise_thresh {noise_thresh}
, t_req { .tv_sec = 0, .tv_nsec = 1000 /* 1µs */ }
{
	// compute distances
	for (size_t i = 1; i < training_offsets.size(); i++) {
		ssize_t distance = training_offsets[i] - training_offsets[0];
		distances.push_back(distance);
		L::debug("Computed distance: %zd\n", distance);
	}
}

// Evaluation helper functions: were specific offsets (in bytes from
// the beginning of the mapping) or specific cachelines (also counted
// from the beginning of a mapping) accessed when the experiment was
// run? Are they potential prefetch candidates?

bool DCReplayExperiment::offset_accessed(size_t offset) const {
	if (std::find(trigger_offsets.begin(), trigger_offsets.end(), offset) != trigger_offsets.end()) {
		return true;
	}
	return false;
}

bool DCReplayExperiment::offset_potential_prefetch(size_t offset) const {
	// assume absolute prefetching
	if (std::find(training_offsets.begin(), training_offsets.end(), offset) != training_offsets.end()) {
		return true;
	}
	
	return false;
}

bool DCReplayExperiment::cl_accessed(size_t cl_idx) const {
	for (size_t const& offset : trigger_offsets) {
		if (offset / CACHE_LINE_SIZE == cl_idx) {
			return true;
		}
	}
	return false;
}

bool DCReplayExperiment::cl_potential_prefetch(size_t cl_idx) const {
	// assume absolute prefetching
	for (size_t const& offset : training_offsets) {
		// if cache line contains at least one offset that's not a trigger
		// location
		if (
			offset / CACHE_LINE_SIZE == cl_idx
			&& std::find(trigger_offsets.begin(), trigger_offsets.end(), offset) == trigger_offsets.end()
		) {
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
vector<size_t> DCReplayExperiment::collect_cache_histogram(Mapping const& mapping, size_t no_repetitions, void (*workload)(DCReplayExperiment const&, Mapping const&, void*), void* additional_info) {
	// ensure the maximum offset is in bounds of the mapping
	assert(training_offsets.size() > 0);
	vector<size_t>::const_iterator max_it = std::max_element(training_offsets.begin(), training_offsets.end());
	assert(max_it != training_offsets.end());
	assert(mapping.base_addr + *max_it < mapping.base_addr + mapping.size);
	
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
vector<size_t> DCReplayExperiment::collect_cache_histogram(Mapping const& mapping1, Mapping const& mapping2, size_t no_repetitions, void (*workload)(DCReplayExperiment const&, Mapping const&, Mapping const&, void*), void* additional_info) {
	// ensure the maximum offset is in bounds of both mappings
	assert(training_offsets.size() > 0);
	vector<size_t>::const_iterator max_it = std::max_element(training_offsets.begin(), training_offsets.end());
	assert(mapping1.base_addr + *max_it < mapping1.base_addr + mapping1.size);
	assert(mapping2.base_addr + *max_it < mapping2.base_addr + mapping2.size);

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
vector<bool> DCReplayExperiment::evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions, double threshold_multiplier) const {
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
vector<bool> DCReplayExperiment::evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions) const {
	return evaluate_cache_histogram(cache_histogram, no_repetitions, 1.0/64);
}

/**
 * Dumps an experiment and a cache histogram to a JSON file.
 *
 * @param      cache_histogram  The cache histogram
 * @param[in]  prefetch_vector  The prefetch vector
 * @param      filepath         The file path to the JSON file
 */
void DCReplayExperiment::dump(vector<size_t> const& cache_histogram, vector<bool> prefetch_vector, string const& filepath) const {
	// Build a JSON array from the cache histogram numbers
	Json::array cache_histogram_values {};
	for (size_t i = 0; i < cache_histogram.size(); i++) {
		cache_histogram_values.push_back((int)cache_histogram[i]);
	}
	Json::array prefetch_vector_values {};
	for (size_t i = 0; i < prefetch_vector.size(); i++) {
		prefetch_vector_values.push_back((bool)prefetch_vector[i]);
	}
	Json::array training_offsets_values {};
	for (size_t i = 0; i < training_offsets.size(); i++) {
		training_offsets_values.push_back((int)training_offsets[i]);
	}
	Json::array trigger_offsets_values {};
	for (size_t i = 0; i < trigger_offsets.size(); i++) {
		trigger_offsets_values.push_back((int)trigger_offsets[i]);
	}

	Json j = Json::object {
		{ "training_offsets", training_offsets_values },
		{ "trigger_offsets", trigger_offsets_values },
		{ "use_nanosleep", use_nanosleep },
		{ "fr_thresh", (int)fr_thresh },
		{ "noise_thresh", (int)noise_thresh },
		{ "cache_histogram", cache_histogram_values },
		{ "prefetch_vector", prefetch_vector },
		{ "cache_line_size", CACHE_LINE_SIZE },
	};
	
	// write JSON to file
	std::ofstream file;
	file.open(filepath);
	file << j.dump() << "\n";
	file.close();
}

/**
 * Resotres a cache histogram from a JSON file.
 *
 * @param      filepath  The file path to the JSON file
 *
 * @return     Pair of stride experiment object and cache histogram.
 */
pair<DCReplayExperiment, vector<size_t>> DCReplayExperiment::restore(string const& filepath) {
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
	vector<size_t> training_offsets;
	for (Json value : json["training_offsets"].array_items()) {
		training_offsets.push_back((size_t)value.int_value());
	}
	vector<size_t> trigger_offsets;
	for (Json value : json["trigger_offsets"].array_items()) {
		trigger_offsets.push_back((size_t)value.int_value());
	}

	DCReplayExperiment experiment {
		training_offsets,
		trigger_offsets,
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
