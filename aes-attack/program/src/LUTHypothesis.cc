#include "LUTHypothesis.hh"

LUTHypothesis::LUTHypothesis(array<size_t, 4> positions)
: positions {positions}
{
	std::memset(anchor_key_byte_hypothesis, 0, sizeof(anchor_key_byte_hypothesis));
}

/**
 * @brief      Stores a new trace (hit map) for this lookup table.
 *
 * @param[in]  hit_map_class   Which kind of hit map? (Hit map for anchor
 *                             byte or dependent byte(s))?
 * @param[in]  moved_byte_pos  The position of the moved byte
 * @param[in]  plaintext       The plaintext
 * @param[in]  hit_map         The hit map
 *
 * @return     Index of the hit_maps vector where the newly added map is
 *             now stored.
 */
size_t LUTHypothesis::add_map(hit_map_class_t hit_map_class, size_t moved_byte_pos, plaintext_t plaintext, hit_map_t hit_map) {
	vector<tuple<size_t,plaintext_t,hit_map_t>>& hit_maps =
		(hit_map_class == HIT_MAP_ANCHOR)
			? hit_maps_anchor
			: hit_maps_dependent;

	filter_map(hit_map);
	fflush(stdout);
	print_map(hit_map);
	hit_maps.push_back({moved_byte_pos, plaintext, hit_map});
	return hit_maps.size() - 1;
}


/**
 * @brief      Dumps the hit_maps_anchor/hit_maps_dependent data structure
 *             into a file.
 *
 * @param[in]  hit_map_class  Which kind of hit map? (Hit map for anchor
 *                            byte or dependent bytes)?
 * @param[in]  filepath       The file path to store the maps at.
 */
void LUTHypothesis::dump_hit_maps_to_file(hit_map_class_t hit_map_class, string filepath) const {
	vector<tuple<size_t,plaintext_t,hit_map_t>> const& hit_maps =
		(hit_map_class == HIT_MAP_ANCHOR)
			? hit_maps_anchor
			: hit_maps_dependent;

	std::ofstream file;
	file.open(filepath);
	for (tuple<size_t, plaintext_t, hit_map_t> const& hit_map_tuple : hit_maps) {
		file << HMT_GET_BYTE_POS(hit_map_tuple) << " ";
		for (aes_byte_t const& byte : HMT_GET_PLAINTEXT(hit_map_tuple)) {
			file << (int)byte << " ";
		}
		for (pair<const dist4_t,counter_t> hist_pair : HMT_GET_HIT_MAP(hit_map_tuple)) {
			file << x4_to_str_spaceless(hist_pair.first) << " " << hist_pair.second << " ";
		}
		file << "\n";
	}
	file.close();
}

/**
 * @brief      Restores the hit_maps_anchor/hit_maps_dependent data
 *             structure form a file.
 *
 * @param[in]  hit_map_class  Which kind of hit map? (Hit map for anchor
 *                            byte or dependent bytes)?
 * @param[in]  filepath       The file path to read the maps from
 */
void LUTHypothesis::restore_hit_maps_from_file(hit_map_class_t hit_map_class, string filepath) {
	vector<tuple<size_t,plaintext_t,hit_map_t>>& hit_maps =
		(hit_map_class == HIT_MAP_ANCHOR)
			? hit_maps_anchor
			: hit_maps_dependent;
	hit_maps.clear();

	std::ifstream file;
	file.open(filepath);
	string line;
	
	if ( ! file.is_open()) {
		printf("Failed to open file %s.\n", filepath.c_str());
		return;
	}
	
	while (getline(file, line)) {
		size_t moved_byte_pos;
		plaintext_t plaintext;
		hit_map_t hit_map;

		std::stringstream line_stream(line);
	    string token;

	    // first token: size_t moved_byte_pos
	    line_stream >> token;
	    moved_byte_pos = std::stoul(token);

	    // next 16 tokens: plaintext_t
	    for (size_t i = 0; i < 16; i++) {
	    	line_stream >> token;
	    	plaintext[i] = std::stoi(token);
	    }

	    // remaining tokens: hit map entries (alternating dist/counter)
	    while (line_stream >> token) {
	    	dist4_t dist = spaceless_str_to_dist4(token);
	    	line_stream >> token;
	    	counter_t counter = std::stoul(token);
	    	hit_map[dist] = counter;
	    }

		hit_maps.push_back({moved_byte_pos, plaintext, hit_map});
	}
	file.close();
}

/**
 * @brief      Given a hit_map for the anchor byte and two plaintexts that
 *             differ in exactly 1 bit: compute the expected hit maps for
 *             the cases that the key bit where the plaintext differs is
 *             either (a) zero or (b) one.
 *
 * @param      outer_map_tuple  The outer map tuple, containing the hit_map
 *                              for the anchor byte and the plaintext that
 *                              was used to record this hit_map
 * @param[in]  inner_pt_byte    The inner plaintext byte, that differs from
 *                              the corresponding plaintext in the first
 *                              parameter by exactly 1 bit
 * @param[in]  mask             A mask indicating the (single) bit position
 *                              where the two plaintexts differ
 *
 * @return     Two hit maps (wrapped in a pair), describing the expected
 *             distances for the case that the corresponding key bit is 0
 *             (.first) or 1 (.second).
 */
pair<hit_map_t,hit_map_t> LUTHypothesis::expected_maps_anchor(
	tuple<size_t, plaintext_t, hit_map_t> const& outer_map_tuple, aes_byte_t inner_pt_byte, aes_byte_t mask
) {
	size_t const& moved_byte_pos = HMT_GET_BYTE_POS(outer_map_tuple);
	aes_byte_t const& outer_pt_byte = HMT_GET_PLAINTEXT(outer_map_tuple)[moved_byte_pos];
	hit_map_t const& outer_map = HMT_GET_HIT_MAP(outer_map_tuple);

	// first = expected distances if key bit is 0, second = expected distances if key bit is 1
	hit_map_t hit_map_0, hit_map_1;

	// for each distance in the base map, compute corresponding expected
	// distances in the other map.
	for (pair<const dist4_t, counter_t> const& outer_map_entry : outer_map) {
		dist4_t const& outer_distance4 = outer_map_entry.first;
		
		dist4_t outer_distance4_shifted_plus;
		dist4_t outer_distance4_shifted_minus;
		for (size_t i = 0; i < outer_distance4.size(); i++) {
			// 0 needs to remain in the resulting array, it's it's the
			// anchor point it should be present in all dist4_t's.
			if (outer_distance4[i] == 0) {
				outer_distance4_shifted_plus[i]	= 0;
				outer_distance4_shifted_minus[i] = 0;
			} else if (outer_distance4[i] == DIST4_T_DUMMY) {
				// assuming the collision is at the anchor position
				outer_distance4_shifted_plus[i] = (mask >> 4);
				outer_distance4_shifted_minus[i] = -(mask >> 4);
				// assuming the collision is at one of the other positions
				outer_distance4_shifted_plus[i] = DIST4_T_DUMMY;
				outer_distance4_shifted_minus[i] = DIST4_T_DUMMY;
			} else {
				outer_distance4_shifted_plus[i]  = outer_distance4[i] + (mask >> 4);
				outer_distance4_shifted_minus[i] = outer_distance4[i] - (mask >> 4);
			}
		}

		// 0 or values generated from DIST4_T_DUMMY might be at the wrong
		// positions, move them to the correct place by sorting the arrays
		std::sort(outer_distance4_shifted_plus.begin(), outer_distance4_shifted_plus.end());
		std::sort(outer_distance4_shifted_minus.begin(), outer_distance4_shifted_minus.end());

		// If we expect a collision (i.e., two entries hit the same cache
		// line), insert a dummy
		for (size_t i = 1; i < outer_distance4_shifted_plus.size(); i++) {
			if (outer_distance4_shifted_plus[i-1] == outer_distance4_shifted_plus[i]) {
				outer_distance4_shifted_plus[i] = DIST4_T_DUMMY;
			}
			if (outer_distance4_shifted_minus[i-1] == outer_distance4_shifted_minus[i]) {
				outer_distance4_shifted_minus[i] = DIST4_T_DUMMY;
			}
		}

		// sort again to move the dummy (if any) to the end
		std::sort(outer_distance4_shifted_plus.begin(), outer_distance4_shifted_plus.end());
		std::sort(outer_distance4_shifted_minus.begin(), outer_distance4_shifted_minus.end());

		aes_byte_t relevant_outer_pt_bit = outer_pt_byte & mask;
		aes_byte_t relevant_inner_pt_bit = inner_pt_byte & mask;

		// compute new expected distances based on the direction of the bit flip
		if (relevant_outer_pt_bit == 0 && relevant_inner_pt_bit > 0) {
			// Plaintext bit flip from 0 to 1:
			// if key bit was 0, expect the distance to decrease by mask/16
			hit_map_0[outer_distance4_shifted_minus] = outer_map.at(outer_distance4);
			// if key bit was 1, expext the distance to increase by mask/16
			hit_map_1[outer_distance4_shifted_plus] = outer_map.at(outer_distance4);
		} else if (relevant_outer_pt_bit > 0 && relevant_inner_pt_bit == 0) {
			// Plaintext bit flip from 1 to 0:
			// if key bit was 0, expect the distance to increase by mask/16
			hit_map_0[outer_distance4_shifted_plus] = outer_map.at(outer_distance4);
			// if key bit was 1, expext the distance to decrease by mask/16
			hit_map_1[outer_distance4_shifted_minus] = outer_map.at(outer_distance4);
		} else {
			// should never happen, given that the two plaintexts only
			// differ at exactly one position (specified by mask)
			assert(false);
		}
	}

	// filter the resulting vectors: exclude implausible distances
	hit_map_t::iterator hit_map_0_it = hit_map_0.begin();
	while (hit_map_0_it != hit_map_0.end()) {
		bool implausible = false;
		for (dist_t distance : hit_map_0_it->first) {
			if( ! is_plausible_distance(distance)) {
				implausible = true;
				break;
			}
		}
		if (implausible) {
			hit_map_0_it = hit_map_0.erase(hit_map_0_it);
		} else {
			++hit_map_0_it;
		}
	}
	hit_map_t::iterator hit_map_1_it = hit_map_1.begin();
	while (hit_map_1_it != hit_map_1.end()) {
		bool implausible = false;
		for (dist_t distance : hit_map_1_it->first) {
			if( ! is_plausible_distance(distance)) {
				implausible = true;
				break;
			}
		}
		if (implausible) {
			hit_map_1_it = hit_map_1.erase(hit_map_1_it);
			break;
		} else {
			++hit_map_1_it;
		}
	}

	return {hit_map_0, hit_map_1};
}

/**
 * @brief      Evaluates all hit maps currently stored in hit_maps_anchor
 *             to find as many bits of the anchor byte as possible. The
 *             anchor_key_byte_hypothesis array is updated accordingly.
 */
void LUTHypothesis::evaluate_maps_anchor() {
	assert(hit_maps_anchor.size() > 1);

	// compare all the maps with equal plaintext byte MSB among each
	// other (but not with itself).
	for (size_t outer_map_i = 0; outer_map_i < hit_maps_anchor.size() - 1; outer_map_i++) {
		aes_byte_t const& outer_pt_byte = HMT_GET_PLAINTEXT(hit_maps_anchor[outer_map_i])[anchor_byte_pos];
		hit_map_t const& outer_map = HMT_GET_HIT_MAP(hit_maps_anchor[outer_map_i]);
		
		printf("Outer map for byte %.2x with %zu entries:\n", outer_pt_byte, outer_map.size());
		print_map(outer_map);
		
		for (size_t inner_map_i = outer_map_i + 1; inner_map_i < hit_maps_anchor.size(); inner_map_i++) {
			aes_byte_t& inner_pt_byte = HMT_GET_PLAINTEXT(hit_maps_anchor[inner_map_i])[anchor_byte_pos];
			hit_map_t& inner_map = HMT_GET_HIT_MAP(hit_maps_anchor[inner_map_i]);

			// XOR outer plaintext with inner plaintext
			aes_byte_t opt_xor_ipt = outer_pt_byte ^ inner_pt_byte;

			// skip hit_maps if the plaintext bytes differ by more/less than 1 bit
			if (bitset<8>(opt_xor_ipt).count() != 1) {
				printf("Skipping inner map for byte %.2x (plaintext byte hamming distance != 1).\n", inner_pt_byte);
				continue;
			}

			printf("Comparing maps %.2x (outer) / %.2x (inner)\n", outer_pt_byte, inner_pt_byte);

			// identify the position where the bit representation differs
			size_t diffpos;
			for (diffpos = 0; (opt_xor_ipt & (1 << diffpos)) == 0; ++diffpos);

			// compute the expected (shifted) distances for both possible key bit hypotheses.
			pair<hit_map_t, hit_map_t> expected = expected_maps_anchor(hit_maps_anchor[outer_map_i], inner_pt_byte, opt_xor_ipt);

			printf("expected_distances for key=0: ");
			for (pair<const dist4_t, counter_t> const& hit_pair : expected.first) {
				printf(" %s (%lu)", x4_to_str_human(hit_pair.first).c_str(), hit_pair.second);
			}
			printf("\nexpected_distances for key=1: ");
			for (pair<const dist4_t, counter_t> const& hit_pair : expected.second) {
				printf(" %s (%lu)", x4_to_str_human(hit_pair.first).c_str(), hit_pair.second);
			}
			printf("\ngot: ");
			for (pair<const dist4_t, counter_t> const& hit_pair : inner_map) {
				printf(" %s (%lu)", x4_to_str_human(hit_pair.first).c_str(), hit_pair.second);
			}
			puts("");

			// compute match metric for both potential expected distances
			score_t match0 = compute_map_similarity_score(inner_map, expected.first);
			score_t match1 = compute_map_similarity_score(inner_map, expected.second);
			if (match0 > match1 && match0 >= 10) {
				anchor_key_byte_hypothesis[diffpos][BIT_0]++;
			} else if (match0 < match1 && match1 >= 10) {
				anchor_key_byte_hypothesis[diffpos][BIT_1]++;
			} else {
				anchor_key_byte_hypothesis[diffpos][BIT_UNKNOWN]++;
			}
			printf("Scores for bit %zu:\nmatch0 score: %3d\nmatch1 score: %3d\n", diffpos, match0, match1);
		}
	}
}

/**
 * @brief      Converts distances (which are relative to the anchor byte)
 *             into offsets (which are relative to the beginning of the
 *             LUT). If a resulting tuple of four offsets would contain an
 *             implausible offset (negative or >= 16), this tuple is
 *             filtered out.
 *
 * @param      hit_map_tuple  The hit map tuple
 *
 * @return     A vector containing the offsets that correspond to the
 *             distances (which are provided as keys of the hit_map in
 *             hit_map_tuple). Note that tuples containing implausible
 *             offsets are filtered out, so the size of the returned vector
 *             is less than or equal the number of elements in the original
 *             hit map (and it may even be empty).
 */
vector<offset4_t> LUTHypothesis::distances_to_offsets(tuple<size_t, plaintext_t, hit_map_t> const& hit_map_tuple) const {
	aes_byte_t const& pt_byte = std::get<1>(hit_map_tuple)[anchor_byte_pos];
	hit_map_t const& hit_map = std::get<2>(hit_map_tuple);

	// compute offset of the current anchor byte hypothesis,
	// relative to beginning of the LUT
	aes_byte_t anchor_hyp = get_current_hypothesis_at_pos_idx(0);
	offset_t anchor_offset = ((anchor_hyp ^ pt_byte) >> 4);

	// compute the other offsets
	vector<offset4_t> offsets;
	for (pair<const dist4_t, counter_t> const& hit_pair : hit_map) {
		dist4_t const& distance4 = hit_pair.first;
		offset4_t inner_offsets;
		assert(inner_offsets.size() == distance4.size());
		bool implausible = false;
		for (size_t i = 0; i < distance4.size(); i++) {
			offset_t offset = anchor_offset + distance4[i];
			if ( ! is_plausible_offset(offset)) {
				implausible = true;
				break;
			}
			inner_offsets[i] = offset;
		}
		if (implausible) {
			continue;
		}
		offsets.push_back(inner_offsets);
	}

	return offsets;
}

/**
 * @brief      Evaluates a hit map for a specific dependent (= non-anchor)
 *             position to find the most likely key byte hypothesis.
 *             dependent_key_byte_hypothesis is updated accordingly.
 */
void LUTHypothesis::evaluate_maps_dependent() {
	for (tuple<size_t, plaintext_t, hit_map_t> const& dependent_map_tuple : hit_maps_dependent) {
		size_t const& dependent_byte_pos = HMT_GET_BYTE_POS(dependent_map_tuple);
		plaintext_t const& dependent_plaintext = HMT_GET_PLAINTEXT(dependent_map_tuple);
		hit_map_t const& dependent_byte_map = HMT_GET_HIT_MAP(dependent_map_tuple);
		
		// compute the absolute offsets (which are relative to the
		// beginning of the LUT, not to the anchor byte) for the bytes in
		// the given trace
		vector<offset4_t> dependent_offset4s = distances_to_offsets(dependent_map_tuple);

		// compare the map of the dependent byte with all maps for the anchor byte
		for (tuple<size_t, plaintext_t, hit_map_t> const& anchor_map_tuple : hit_maps_anchor) {
			size_t const& anchor_byte_pos_hmt = HMT_GET_BYTE_POS(anchor_map_tuple);
			plaintext_t const& anchor_plaintext = HMT_GET_PLAINTEXT(anchor_map_tuple);
			hit_map_t const& anchor_map = HMT_GET_HIT_MAP(anchor_map_tuple);

			// ensure the hit map has the correct anchor position
			assert(anchor_byte_pos_hmt == anchor_byte_pos);

			// compute the absolute offsets for the anchor map
			vector<offset4_t> anchor_offset4s = distances_to_offsets(anchor_map_tuple);

			// Debug Output
			printf("anchor PT byte: %.2x\n", anchor_plaintext[anchor_byte_pos]);
			printf("anchor map pos %zd: \n", anchor_byte_pos);
			print_map(anchor_map);
			printf("anchor offsets pos %zd: ", anchor_byte_pos);
			for (offset4_t const& offset4 : anchor_offset4s) {
				printf("%s ", x4_to_str_human(offset4).c_str());
			}
			printf("\ndepnd PT byte: %.2x\n", dependent_plaintext[dependent_byte_pos]);
			printf("\ndepnd map pos %zd: \n", dependent_byte_pos);
			print_map(dependent_byte_map);
			printf("depnd offsets pos %zd: ", dependent_byte_pos);
			for (offset4_t const& offset4 : dependent_offset4s) {
				printf("%s ", x4_to_str_human(offset4).c_str());
			}
			puts("");

			// identify the flipped bit
			aes_byte_t bytediff = (anchor_plaintext[dependent_byte_pos] ^ dependent_plaintext[dependent_byte_pos]) & 0b11110000;
			size_t flipped_bit = SIZE_MAX;
			for (size_t bitpos = 4; bitpos < 8; bitpos++) {
				if (((bytediff >> bitpos) & 1) == 1) {
					flipped_bit = bitpos;
					break;
				}
			}

			// ensure we identified a flipped bit and that the flipped bit
			// we identified is the only bit that changed
			assert(flipped_bit != SIZE_MAX);
			assert((bytediff & ~(1 << flipped_bit)) == 0);
			printf("flipped bit: %zu\n", flipped_bit);

			// Now go through all offset4's for the anchor map and compute
			// the possible dependent offset4_t's that could occur when
			// flipped_bit is flipped. Remember that flipping flipped_bit
			// should move the corresponding hit by (1<<(flipped_bit-4))
			// (either up or down).
			offset_t expected_dist_abs = (1 << (flipped_bit-4));
			for (offset4_t const& anchor_offset4 : anchor_offset4s) {
				// each of the 4 positions could be the one that moved due to
				// the flip. Additionally, each position can either move in
				// "plus" or "minus" direction. Generate the corresponding
				// hypotheses and check if one of the recorded dependent maps
				// matches.
				for (size_t moved_idx_hyp = 0; moved_idx_hyp < anchor_offset4.size(); moved_idx_hyp++) {
					// generate plus and minus hypotheses
					offset4_t expected_offset4_plus;
					offset4_t expected_offset4_minus;
					for (size_t i = 0; i < anchor_offset4.size(); i++) {
						if (i == moved_idx_hyp) {
							expected_offset4_plus[i]  = anchor_offset4[i] + expected_dist_abs;
							expected_offset4_minus[i] = anchor_offset4[i] - expected_dist_abs;
						} else {
							expected_offset4_plus[i]  = anchor_offset4[i];
							expected_offset4_minus[i] = anchor_offset4[i];
						}
					}

					// If we expect a collision (i.e., two entries hit the same cache
					// line), insert a dummy
					for (size_t i = 1; i < expected_offset4_plus.size(); i++) {
						if (expected_offset4_plus[i-1] == expected_offset4_plus[i]) {
							expected_offset4_plus[i] = DIST4_T_DUMMY;
						}
						if (expected_offset4_minus[i-1] == expected_offset4_minus[i]) {
							expected_offset4_minus[i] = DIST4_T_DUMMY;
						}
					}
					// sort hypotheses so any dummies are moved to the end
					std::sort(expected_offset4_plus.begin(), expected_offset4_plus.end());
					std::sort(expected_offset4_minus.begin(), expected_offset4_minus.end());

					printf("plus:  would expect move %s -> %s\n", x4_to_str_human(anchor_offset4).c_str(), x4_to_str_human(expected_offset4_plus).c_str());
					printf("minus: would expect move %s -> %s\n", x4_to_str_human(anchor_offset4).c_str(), x4_to_str_human(expected_offset4_minus).c_str());

					// Check if the hypothesis occurs in the dependent map.
					// If so, increment a counter.
					for (dist4_t const& dependent_offset4 : dependent_offset4s) {
						if (
							(dependent_offset4 == expected_offset4_plus)
							|| (dependent_offset4 == expected_offset4_minus)
						) {
							aes_byte_t hyp = (anchor_plaintext[dependent_byte_pos] ^ (anchor_offset4[moved_idx_hyp] << 4)) & 0b11110000;
							printf("found hypothesis for pos %zu: %.2x\n", dependent_byte_pos, hyp);
							// increase the byte hypothesis counter for this key byte hypothesis at this position
							set_or_increment(dependent_key_byte_hypothesis[pos_to_pos_idx(dependent_byte_pos) - 1], hyp, (counter_t)1);
						}
					}
				}
			}
		}
	}
}

/**
 * @brief      Gets the current key byte hypothesis for a specific
 *             position.
 *
 * @param[in]  pos_idx  The position index (into the positions array)
 *                      \in [0, 3]
 *
 * @return     The current key byte hypothesis.
 */
aes_byte_t LUTHypothesis::get_current_hypothesis_at_pos_idx(size_t pos_idx) const {
	if (pos_idx == 0) { // For anchor byte hypothesis
		aes_byte_t hyp = 0;
		for (size_t i = 0; i < 8; i++) {
			if (anchor_key_byte_hypothesis[i][BIT_0] > anchor_key_byte_hypothesis[i][BIT_1]) {
				continue;
			} else if (anchor_key_byte_hypothesis[i][BIT_0] < anchor_key_byte_hypothesis[i][BIT_1]) {
				hyp |= (1 << i);
			} else { // equal counter values
				// fall back to 0
				continue;
			}
		}
		return hyp;
	} else { // For dependent byte hypotheses
		vector<pair<aes_byte_t, counter_t>> dependent_key_byte_hypothesis_sorted = \
			copy_map_to_vector_and_sort_by_value_desc(dependent_key_byte_hypothesis[pos_idx - 1]);
		
		if (dependent_key_byte_hypothesis_sorted.size() > 0) {
			return dependent_key_byte_hypothesis_sorted[0].first;
		} else {
			// Fallback: return 0
			return 0x00;
		}
	}
}


/**
 * @brief      Converts a key byte position (\in [0, 15]) into an array
 *             index into the positions array (\in [0, 3]), where the
 *             given position number is stored.
 *
 * @param[in]  pos   The position \in [0, 15]
 *
 * @return     The position index \in [0, 3]
 */
size_t LUTHypothesis::pos_to_pos_idx(size_t pos) const {
	size_t pos_idx;
	for (
		pos_idx = 0;
		pos_idx < positions.size() && positions[pos_idx] != pos;
		pos_idx++
	);
	assert(positions[pos_idx] == pos);
	return pos_idx;
}

/**
 * @brief      Converts a relative distance (number of cache lines between
 *             anchor position and hit position) into an absolute and
 *             anchor-independent offset (number of cache lines from the
 *             beginning of the LUT). Note that the resulting offset
 *             correct only if the current key hypothesis for the anchor
 *             byte (stored in bytehyp) is correct. (This implies that
 *             distance_to_offset can't be used in algorithms that aim to
 *             find the anchor byte, since no valid anchor byte hypothesis
 *             is available at that stage.)
 *
 * @param      hit_map_tuple  The hit map tuple of the hit map where this
 *                            dist
 * @param[in]  distance       The distance to convert to an offset.
 *
 * @return     The offset
 */
offset_t LUTHypothesis::distance_to_offset(tuple<size_t, plaintext_t, hit_map_t> const& hit_map_tuple, dist_t distance) const {
	aes_byte_t const& pt_byte = HMT_GET_PLAINTEXT(hit_map_tuple)[anchor_byte_pos];
	
	// compute offset of the current anchor byte hypothesis,
	// relative to beginning of the LUT
	aes_byte_t anchor_hyp = get_current_hypothesis_at_pos_idx(0);
	offset_t anchor_offset = ((anchor_hyp ^ pt_byte) >> 4);

	// compute the dependent offset
	return anchor_offset + distance;
}

/**
 * @brief      Prints the current key byte hypotheses (1 line per byte).
 *
 * @param[in]  hit_map_class  The hit map class to print
 * @param      correct_key    The correct key to use as a reference
 */
void LUTHypothesis::print_current_hyp(hit_map_class_t hit_map_class, aes_byte_t const* correct_key) const {
	for (size_t byte_i = 0; byte_i < 4; byte_i++) {
		aes_byte_t const& correct_key_byte = correct_key[positions[byte_i]];
		printf("Key byte hypothesis %2zu: ", positions[byte_i]);
		aes_byte_t byte = 0;
		
		if (hit_map_class == HIT_MAP_ANCHOR && byte_i == 0) { // Anchor byte
			for (ssize_t bit_i = 7; bit_i >= 0; bit_i--) {
				counter_t const* const& current_bithyp = anchor_key_byte_hypothesis[bit_i];
				if (current_bithyp[BIT_0] > current_bithyp[BIT_1]) {
					printf("0");
				} else if (current_bithyp[BIT_1] > current_bithyp[BIT_0]) {
					byte |= (1 << bit_i);
					printf("1");
				} else {
					printf("_");
				}
			}
		} else if (hit_map_class == HIT_MAP_DEPENDENT) { // Dependent byte
			aes_byte_t const& most_probable_keyhyp = get_current_hypothesis_at_pos_idx(byte_i);
			for (ssize_t bit_i = 7; bit_i >= 0; bit_i--) {
				if (bit_i < 4) {
					printf("_");
				} else if (((most_probable_keyhyp >> bit_i) & 1) == 0) {
					printf("0");
				} else {
					byte |= (1 << bit_i);
					printf("1");
				}
			}
		}

		printf(
			" (%.2x), correct: %.2x (%s%s)\n",
			byte, correct_key_byte,
			(byte & 0b11110000) == (correct_key_byte & 0b11110000) ? "E" : "_",
			(byte & 0b00001111) == (correct_key_byte & 0b00001111) ? "E" : "_"
		);
	}
}

/**
 * @brief      Prints the current key byte hypotheses (1 line per bit).
 *
 * @param[in]  hit_map_class  The hit map class to print
 */
void LUTHypothesis::print_current_hyp_tabular(hit_map_class_t hit_map_class) const {
	for (size_t byte_i = 0; byte_i < 4; byte_i++) {
		if (hit_map_class == HIT_MAP_ANCHOR && byte_i == 0) { // Anchor byte
			for (size_t bit_i = 0; bit_i < 8; bit_i++) {
				counter_t const* const& current_bithyp = anchor_key_byte_hypothesis[bit_i];
				printf(
					"Key byte %2zu, bit %zu: votes ZERO: %3lu | votes ONE: %3lu | votes UNKNOWN: %3lu --> ",
					positions[byte_i], bit_i, current_bithyp[BIT_0], current_bithyp[BIT_1], current_bithyp[BIT_UNKNOWN]
				);
				if (current_bithyp[BIT_0] > current_bithyp[BIT_1]) {
					printf("0\n");
				} else if (current_bithyp[BIT_1] > current_bithyp[BIT_0]) {
					printf("1\n");
				} else {
					printf("_\n");
				}
			}
		} else if (hit_map_class == HIT_MAP_DEPENDENT){ // Dependent byte
			aes_byte_t const& most_probable_keyhyp = get_current_hypothesis_at_pos_idx(byte_i);
			printf("Key byte %2zu --> %.2x\n", positions[byte_i], most_probable_keyhyp);
		}
	}
}

/**
 * @brief      Prints a hit map.
 *
 * @param[in]  hit_map_class  The hit map class (anchor or dependent)
 * @param[in]  idx            The index into the hit_maps_anchor or
 *                            hit_maps_dependent array
 */
void LUTHypothesis::print_map(hit_map_class_t hit_map_class, size_t idx) const {
	hit_map_t const* hit_map_ptr = nullptr;
	switch (hit_map_class) {
		case HIT_MAP_ANCHOR:
			if (idx < hit_maps_anchor.size()) {
				hit_map_ptr = &HMT_GET_HIT_MAP(hit_maps_anchor[idx]);
			} else {
				goto error;
			}
			break;
		case HIT_MAP_DEPENDENT:
			if (idx < hit_maps_dependent.size()) {
				hit_map_ptr = &HMT_GET_HIT_MAP(hit_maps_dependent[idx]);
			} else {
				goto error;
			}
			break;
		default:
			assert(false);
	}
	assert(hit_map_ptr != nullptr);

	print_map(*hit_map_ptr);
	return;

error:
	printf("print_map: No such hitmap.\n");
	return;
}