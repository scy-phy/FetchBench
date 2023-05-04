import re
import functools
import gzip
import sys
from typing import List, Dict, Optional

# TODO:
# - Filtering dupes should no longer be necessary
# - Tracking the flipped bits in dependent plaintext bytes should be easier

def popcnt(x: int) -> int:
    return bin(x).count('1')

class AES:
	def __init__(self, key: List[int], initial_plaintext: List[int]):
		self._key = key
		self._initial_plaintext = initial_plaintext
		
		# map the four lookup tables to the four key/plaintext positions
		# that target that table (in execution order). the first position
		# in each array is the "anchor" position, the other 3 positions are
		# "dependent" positions.
		self._aes_positions: List[List[int]] = [
			[ 0,  4,  8, 12], # FT0
			[ 5,  9, 13,  1], # FT1
			[10, 14,  2,  6], # FT2
			[15,  3,  7, 11]  # FT3
		]
		self.reset()

	def reset(self):
		self.plaintext = self._initial_plaintext[:]

	def xor(self, i: int) -> int:
		return self._key[i] ^ self.plaintext[i]

	def expected_state_bitvector(self, lut_i: int) -> int:
		positions = self._aes_positions[lut_i]

		# compute the indices into the lookup table
		anchor_offset = self.xor(positions[0]) >> 4
		dependent_offsets = [(self.xor(i) >> 4) for i in positions[1:]]
		
		# convert offsets (= indices relative to the beginning of the
		# table) into distances (= distance between dependent offset and
		# anchor offset)
		distances = sorted(list(set([0] + [dependent_offset - anchor_offset for dependent_offset in dependent_offsets])))
		
		# remove implausible distances
		distances_filtered = list(filter(lambda x: x <= 12 and x >= -9, distances))

		# print(distances_filtered)

		# convert distances into state bitvector
		bitvector = (1 << 16)
		for distance in distances_filtered:
			bitvector |= (1 << (16 + distance))

		return bitvector

	def print_pt(self):
		out = ""
		for byte in self.plaintext:
			out += f"{byte:02x} "
		print(out[:-1])

class State:
	def __init__(self, is_anchor: bool, lut_i: int, pt_pos: int, pt_value: int, flipped_bit: Optional[int], bitvector: int, match: bool):
		self.is_anchor = is_anchor
		self.lut_i = lut_i
		self.pt_pos = pt_pos
		self.pt_value = pt_value
		self.flipped_bit = flipped_bit
		self.bitvector = bitvector
		self.match = match

	def __repr__(self) -> str:
		return f"State({self.is_anchor}, {self.lut_i}, {self.pt_pos}, 0x{self.pt_value:02x}, {self.flipped_bit}, 0b{self.bitvector:016b})"

aes = AES(
	key=[
		0xde, 0x3d, 0x5a, 0xf2, 0xf0, 0x1c, 0x76, 0x58,
		0x62, 0x4d, 0xaf, 0x8f, 0x92, 0xad, 0x91, 0xef
	],
	initial_plaintext=[
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	]
)

def state_bitvector_to_arr(bitvector):
	arr = []
	shift = 0
	while bitvector != 0:
		if (bitvector & 1) != 0:
			arr.append(shift - 16)
		bitvector >>= 1
		shift += 1
	return arr

################################################

if len(sys.argv) > 1:
	INFILE = sys.argv[1]
else:
	INFILE = "out.log"

timestamps: Dict[str, int] = {}
states_per_byte: List[List[State]] = []
current_states_list_i = -1

if INFILE.endswith(".gz"):
	file = gzip.open(INFILE, "rt")
else:
	file = open(INFILE)

state_anchor_pt_value = -1
state_dependent_flipped_bit = -1
for line in file:
	# extract timestamps
	rx_timestamp = re.match(r"^Timestamp \((.*)\): (\d+)", line)
	if rx_timestamp:
		print(line.strip())
		label = rx_timestamp.group(1)
		value = int(rx_timestamp.group(2))
		timestamps[label] = value
		# if label == "LUT 1 start":
		# 	break
		continue

	# extract state changes
	rx_anchor_begin = re.match(r"^Collecting anchor traces for LUT (\d), PT pos (\d+), value ([0-9a-f]{2})", line)
	if rx_anchor_begin:
		# filter out duplicates
		state_anchor_pt_value_new = int(rx_anchor_begin.group(3), 16)
		if state_anchor_pt_value != state_anchor_pt_value_new:
			print(line.strip())
			state_is_anchor = True
			state_lut_i = int(rx_anchor_begin.group(1))
			state_anchor_pt_pos = int(rx_anchor_begin.group(2))
			state_anchor_pt_value = state_anchor_pt_value_new
			aes.reset()
			aes.plaintext[state_anchor_pt_pos] = state_anchor_pt_value
			state_expected = aes.expected_state_bitvector(state_lut_i)
			print(f"Now expected:   {state_expected:032b} {str(state_bitvector_to_arr(state_expected))}")
			states_per_byte.append([])
			current_states_list_i += 1
		continue

	rx_dependent_begin = re.match(r"^Collecting traces for LUT (\d), dependent byte pos (\d+), flipped bit (\d+) -> [0-9a-f]{2}, anchor PT value ([0-9a-f]{2})", line)
	if rx_dependent_begin:
		# filter out duplicates
		state_anchor_pt_value_new = int(rx_dependent_begin.group(4), 16)
		state_dependent_flipped_bit_new = int(rx_dependent_begin.group(3))
		if state_anchor_pt_value != state_anchor_pt_value_new:
			print(line.strip())
			state_is_anchor = False
			state_lut_i = int(rx_dependent_begin.group(1))
			state_dependent_pt_pos = int(rx_dependent_begin.group(2))
			state_anchor_pt_value = state_anchor_pt_value_new
			state_dependent_flipped_bit = state_dependent_flipped_bit_new
			aes.reset()
			aes.plaintext[state_anchor_pt_pos] = state_anchor_pt_value
			aes.plaintext[state_dependent_pt_pos] ^= (1 << state_dependent_flipped_bit)
			state_expected = aes.expected_state_bitvector(state_lut_i)
			print(f"Now expected:   {state_expected:032b} {str(state_bitvector_to_arr(state_expected))}")
			states_per_byte.append([])
			current_states_list_i += 1
			# aes.print_pt()
		continue

	# extract state
	rx_state = re.match(r"^state: (\d+)", line)
	if rx_state:
		state_bitvector = int(rx_state.group(1))
		match = (state_bitvector == state_expected)
		if match:
			print(f"Match: {state_bitvector:8d} {state_bitvector:032b} {str(state_bitvector_to_arr(state_bitvector))}")

		if state_is_anchor:
			state = State(
				state_is_anchor, state_lut_i, state_anchor_pt_pos, state_anchor_pt_value,
				None, state_bitvector, match
			)
		else:
			state = State(
				state_is_anchor, state_lut_i, state_dependent_pt_pos, state_anchor_pt_value,
				state_dependent_flipped_bit, state_bitvector, match
			)
		states_per_byte[current_states_list_i].append(state)
		continue
file.close()

# compute runtime metrics
# runtime = {
# 	"overall": timestamps["program end"] - timestamps["program start"],
# 	"LUT0": timestamps["LUT 1 start"] - timestamps["LUT 0 start"],
# 	"LUT1": timestamps["LUT 2 start"] - timestamps["LUT 1 start"],
# 	"LUT2": timestamps["LUT 3 start"] - timestamps["LUT 2 start"],
# 	"LUT3": timestamps["program end"] - timestamps["LUT 3 start"]
# }
# print(runtime)

print()
for states in states_per_byte:
	no_states_total = len(states)
	no_states_match = functools.reduce(lambda count, state: count + (state.match), states, 0)
	match_ratio = no_states_match / no_states_total
	print(states[0])
	print(f"states total: {no_states_total:6d} match: {no_states_match:6d} ratio: {match_ratio}")
	print()
