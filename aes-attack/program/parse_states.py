import re

class State:
	def __init__(self, state_vec):
		self.state_vec = state_vec

	def len(self):
		return bin(self.state_vec).count('1')

	def to_bin(self):
		f"{self.state_vec:b}" 

	def to_arr(self):
		tmp = self.state_vec
		arr = []
		i = 0
		while tmp != 0:
			if (tmp & 1) != 0:
				arr.append(i - 16)
			i+=1
			tmp >>= 1
		return arr

states = []

with open("out.log") as file:
	for line in file:
		rx = re.match(r"^state: (\d+)$", line)
		if rx:
			state_vec = int(rx.group(1))
			states.append(State(state_vec))

for state in states:
	# if state.len() == 4:
	print(state.to_arr())