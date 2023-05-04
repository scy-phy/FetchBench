#pragma once

#include <unistd.h>
#include <inttypes.h>

#include <array>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <bitset>
#include <cstring>
#include <cassert>
#include <iomanip>
#include <fstream>
#include <sstream>

using std::array;
using std::vector;
using std::map;
using std::tuple;
using std::pair;
using std::bitset;
using std::string;
using std::stringstream;

typedef int32_t dist_t;
typedef int32_t offset_t;
typedef uint64_t counter_t;
typedef uint8_t aes_byte_t;
typedef array<dist_t, 4> dist4_t;
typedef array<offset_t, 4> offset4_t;
typedef array<aes_byte_t, 16> plaintext_t;
typedef map<dist4_t,counter_t> hit_map_t;
typedef map<offset4_t,double> offset_hit_map_t;

#define DIST4_T_DUMMY (999)

// shorthands to retrieve the components of a hit map tuple (aka.
// tuple<size_t,plaintext_t,hit_map_t>)
#define HMT_GET_BYTE_POS(X) (std::get<0>(X))
#define HMT_GET_PLAINTEXT(X) (std::get<1>(X))
#define HMT_GET_HIT_MAP(X) (std::get<2>(X))

typedef enum {BIT_0 = 0, BIT_1 = 1, BIT_UNKNOWN = 2} bit_t;
typedef enum {HIT_MAP_ANCHOR, HIT_MAP_DEPENDENT} hit_map_class_t;

// Keeps track of the recorded traces and byte hypotheses within the same
// lookup table.
class LUTHypothesis {
private:
	// data type to measure hit map similarity.
	typedef int score_t;

	// Contains the key/plaintext byte positions (\in [0,15]) that
	// influence the current LUT. e.g. for the first LUT: {0, 4, 8, 12}.
	// The first position indicates the position of the anchor byte. The
	// anchor byte is the byte that is loaded by the first load instruction
	// targeting this lookup table.
	array<size_t, 4> const positions;
	size_t const& anchor_byte_pos = positions[0];
	
	// Keeps the recorded traces for this LUT, together with the parameters
	// describing the experimental setup during the recording. Tuple:
	// position of moved/modified byte, plaintext, cache hit map The
	// position is the position itself, not an index into the positions
	// array.
	// 1. Hit maps recorded during anchor byte analysis
	vector<tuple<size_t, plaintext_t, hit_map_t>> hit_maps_anchor;
	// 2. Hit maps recorded during dependent byte analysis
	vector<tuple<size_t, plaintext_t, hit_map_t>> hit_maps_dependent;

	// For the anchor byte in this lookup table, for each of the 8
	// bits, keep 3 counters: votes for values 0, 1, or 'unknown value'.
	counter_t anchor_key_byte_hypothesis[8][3];
	// For each of the three dependent key byte positions, keep a
	// histogram: for each byte value (= key byte hypothesis), how often
	// did it occur?
	map<aes_byte_t, counter_t> dependent_key_byte_hypothesis[3];
public:
	LUTHypothesis(array<size_t, 4> const positions);

	size_t add_map(hit_map_class_t hit_map_class, size_t moved_byte_pos, plaintext_t plaintext, hit_map_t hit_map);
	void dump_hit_maps_to_file(hit_map_class_t hit_map_class, string filepath) const;
	void restore_hit_maps_from_file(hit_map_class_t hit_map_class, string filepath);

	pair<hit_map_t, hit_map_t> expected_maps_anchor(
		tuple<size_t, plaintext_t, hit_map_t> const& outer_map_tuple, aes_byte_t inner_pt_byte, aes_byte_t mask
	);
	void evaluate_maps_anchor();

	vector<dist4_t> distances_to_offsets(tuple<size_t, plaintext_t, hit_map_t> const& hit_map_tuple) const;
	void evaluate_maps_dependent();

	aes_byte_t get_current_hypothesis_at_pos_idx(size_t pos_idx) const;
	size_t pos_to_pos_idx(size_t pos) const;
	offset_t distance_to_offset(tuple<size_t, plaintext_t, hit_map_t> const& hit_map_tuple, dist_t distance) const;
	
	void print_current_hyp(hit_map_class_t hit_map_classaes_byte_t, aes_byte_t const* correct_key) const;
	void print_current_hyp_tabular(hit_map_class_t hit_map_class) const;
	void print_map(hit_map_class_t hit_map_class, size_t idx) const;
	
	/**
	 * @brief      Checks whether a key `k` is present in a `map<K,V>` `m`
	 *             or not.
	 *
	 * @param[in]  m     The map
	 * @param[in]  key   The key
	 *
	 * @tparam     K     map key type
	 * @tparam     V     map value type
	 *
	 * @return     True if key `k` is present in map `m`, false otherwise.
	 */
	template <typename K, typename V>
	static bool map_has_key(map<K,V> m, K key) {
		typename map<K,V>::const_iterator exists_it = m.find(key);
		return exists_it != m.end();
	}

	/**
	 * @brief      Converts a `map<K,V>` into a `vector<pair<K,V>>` and
	 *             sorts the vector based on the values in the map
	 *             (`pair.second`) in descending order. Useful if the map
	 *             represents a histogram and you are interested in the
	 *             most frequent element(s).
	 *
	 * @param      m     The original map to convert into a vector.
	 *
	 * @tparam     K     map key type
	 * @tparam     V     map value type
	 *
	 * @return     m converted into a vector<pair<K,V>> and sorted in
	 *             descending order by pair's second element (the former
	 *             values).
	 */
	template <typename K, typename V>
	static vector<pair<K,V>> copy_map_to_vector_and_sort_by_value_desc(map<K,V> const& m) {
		vector<pair<K,V>> result;
		for (pair<K,V> p : m) {
			result.push_back(p);
		}
		std::sort(
			result.begin(), result.end(),
			[](pair<K,V> const& a, pair<K,V> const& b) {
				return ! (a.second < b.second);
			}
		);
		return result;
	}

	/**
	 * @brief      Checks whether the key `k` exists in map `m`. If this is
	 *             the case, the corresponding value is retrieved and
	 *             incremented by `v_increment`. Otherwise (if `k` does not
	 *             exist in `m`), a new element with key `k` is added to
	 *             the map `m` and set to `v_increment`.
	 *
	 * @param      m            The map
	 * @param      k            The key (w.r.t. the map m)
	 * @param      v_increment  The increment
	 *
	 * @tparam     K            map key type
	 * @tparam     V            map value type (support for the += operator
	 *                          is required)
	 */
	template <typename K, typename V>
	static void set_or_increment(map<K,V>& m, K const& k, V const& v_increment) {
		typename map<K,V>::iterator it = m.find(k);
		if (it != m.end()) {
			it->second += v_increment;
		} else {
			m[k] = v_increment;
		}
	}

	/**
	 * @brief      Calculates a score indicating to which degree two
	 *             hit_maps match. Usually, one of the hit_maps was
	 *             actually recorded, the other one is computed based on a
	 *             key hypothesis (and a prior hitmap recording).
	 *
	 * @param      hit_map_actual    The actual (recorded) hit map
	 * @param      hit_map_expected  The expected hit map
	 *
	 * @return     The map similarity score.
	 */
	static score_t compute_map_similarity_score(hit_map_t const& hit_map_actual, hit_map_t const& hit_map_expected) {
		score_t score = 0;
		// +1P if number of expected distances matches number of observed distances
		if (hit_map_actual.size() == hit_map_expected.size()) {
			printf("score += 1 for matching length.\n");
			score += 1;
		}

		// compute metrics on both hit_maps used during comparison
		counter_t hit_map_actual_max = std::max_element(
			hit_map_actual.begin(), hit_map_actual.end(),
			[](pair<const dist4_t, counter_t> const& a, pair<const dist4_t, counter_t> const& b) {
				return a.second < b.second;
			}
		)->second;
		counter_t hit_map_expected_max = std::max_element(
			hit_map_expected.begin(), hit_map_expected.end(),
			[](pair<const dist4_t, counter_t> const& a, pair<const dist4_t, counter_t> const& b) {
				return a.second < b.second;
			}
		)->second;
		printf("Max. counter in hit_map_actual is %lu.\n", hit_map_actual_max);
		printf("Max. counter in hit_map_expected is %lu.\n", hit_map_expected_max);

		printf("Map (actual):\n");
		print_map(hit_map_actual);
		printf("Map (expected):\n");
		print_map(hit_map_expected);

		for (pair<const dist4_t, counter_t> const& hit_pair : hit_map_actual) {
			dist4_t const& distance4_actual = hit_pair.first;
			counter_t const& counter_actual = hit_pair.second;

			// check if actual distance matches one of the expexted distances
			// (i.e., one of the keys in hit_map_expected)
			if (map_has_key(hit_map_expected, distance4_actual)) {
				// +2P for each observed distance that matches an expected distance
				printf("score += 20 for matching expected distance %s.\n", x4_to_str_human(distance4_actual).c_str());
				score += 20;
				
				// +10P if the actual counter value has a similar ratio to the max. counter value in the
				// hit_map_actual as the counter has in the hit_map_expected.
				counter_t const& counter_expected = hit_map_expected.at(distance4_actual);
				double ratio_expected = (double)counter_expected / hit_map_expected_max;
				double ratio_actual = (double)counter_actual / hit_map_actual_max;
				printf(" Ratio expected: %lf\n", ratio_expected);
				printf(" Ratio actual: %lf\n", ratio_actual);

				double ratio_delta = std::abs(ratio_expected - ratio_actual);
				printf(" Abs. Ratio difference: %lf\n", ratio_delta);

				if (ratio_delta < 0.1) {
					printf(" score += 10 for ratio difference < 0.1\n");
					score += 10;
				}
			} else {
				// -1P for each observed distance that is not expected
				score -= 1;
			}
		}
		return score;
	}

	/**
	 * @brief      Determines whether the specified distance is a plausible
	 *             distance, based on the assumption that the prefetcher
	 *             only leaks distances in the range [-9, 12].
	 *
	 * @param[in]  distance  The distance
	 *
	 * @return     True if the specified distance is plausible distance, False otherwise.
	 */
	static bool is_plausible_distance(dist_t distance) {
		return (
			(distance == DIST4_T_DUMMY)
			|| (
				(distance >= -9)  // lower bound of prefetch distance
				&& (distance <= 12) // upper bound of prefetch distance
			)
		);
	}


	/**
	 * @brief      Determines whether the specified offest is a plausible
	 *             offset, based on the assumption that a lookup table
	 *             spans across 16 cache lines.
	 *
	 * @param[in]  offset  The offset
	 *
	 * @return     True if the specified offset is plausible offset,
	 *             False otherwise.
	 */
	static bool is_plausible_offset(offset_t offset) {
		return (offset >=  0)  // lower bound of LUT
			&& (offset <= 15); // upper bound of LUT
	}

	/**
	 * @brief      De-noises a map. Especially removes all hits to
	 *             implausible distances.
	 *
	 * @param      hit_map  The hit map
	 */
	static void filter_map(hit_map_t& hit_map) {
		hit_map_t::iterator it = hit_map.begin();
		while(it != hit_map.end()) {
			dist4_t const& distance4 = it->first;
			bool implausible = false;
			for (dist_t const& distance : distance4) {
				if( ! is_plausible_distance(distance)) {
					// remove hits for implausible distances
					implausible = true;
					break;
				}
			}
			if (implausible) {
				it = hit_map.erase(it);
			} else {
				++it;
			}
		}
	}
	
	template <typename x4_t>
	static string x4_to_str_human(x4_t const& distances) {
		typename x4_t::const_iterator it = distances.begin();
		stringstream ss;
		ss << std::setw(3);
		ss << "[";
		ss << *it;
		for (++it; it != distances.end(); ++it) {
			ss << ", " << *it;
		}
		ss << "]";
		return ss.str();
	}

	template <typename x4_t>
	static string x4_to_str_spaceless(x4_t const& distances) {
		typename x4_t::const_iterator it = distances.begin();
		stringstream ss;
		ss << *it;
		for (++it; it != distances.end(); ++it) {
			ss << "|" << *it;
		}
		return ss.str();
	}

	
	static dist4_t spaceless_str_to_dist4(string const& str) {
		size_t idx = 0;
		stringstream ss(str);
		string token;
		dist4_t result;
		assert(result.size() == 4);
		
		while(getline(ss, token, '|')) {
			assert(idx < 4);
			result[idx] = std::stoi(token);
			idx++;
		}

		return result;
	}

	/**
	 * @brief      Prints a hit map.
	 *
	 * @param      hit_map  The hit map to print.
	 */
	static void print_map(hit_map_t const& hit_map) {
		for (pair<const dist4_t, counter_t> const& hit : hit_map) {
			dist4_t const& distances = hit.first;
			counter_t count = hit.second;
			printf("map: %s, %3lu\n", x4_to_str_human(distances).c_str(), count);
		}
		puts("");
	}

	/**
	 * @brief      Prints an offset hit map.
	 *
	 * @param      hit_map  The hit map to print.
	 */
	static void print_offset_map(offset_hit_map_t const& offset_hit_map) {
		for (pair<const offset4_t, double> const& hit : offset_hit_map) {
			offset4_t const& offsets = hit.first;
			double ratio = hit.second;
			printf("map: %s, %4.3lf\n", x4_to_str_human(offsets).c_str(), ratio);
		}
		puts("");
	}
};