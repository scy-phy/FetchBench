import math
import matplotlib.pyplot as plt
import matplotlib.patches as patches

# the lookup tables are potentially not aligned to a 1KiB boundary.
# INITIAL_OFFSET specifies the offset from the 1KiB boundary (to find this
# offset, look at the address of FT0 in mbedtls/library/aes.c)
INITIAL_OFFSET = 0x0

PLAINTEXT = [
	0, #  0: FT0_0
	0, #  1: FT1_3
	0, #  2: FT2_2
	0, #  3: FT3_1
	
	0, #  4: FT0_1
	0, #  5: FT1_0
	0, #  6: FT2_3
	0, #  7: FT3_2
	
	0, #  8: FT0_2
	0, #  9: FT1_1
	0, # 10: FT2_0
	0, # 11: FT3_3
	
	0, # 12: FT0_3
	0, # 13: FT1_2
	0, # 14: FT2_1
	0, # 15: FT3_0
]

KEY = [
	0xde, #  0: FT0_0
	0x3d, #  1: FT1_3
	0x5a, #  2: FT2_2
	0xf2, #  3: FT3_1
	
	0xf0, #  4: FT0_1
	0x1c, #  5: FT1_0
	0x76, #  6: FT2_3
	0x58, #  7: FT3_2
	
	0x62, #  8: FT0_2
	0x4d, #  9: FT1_1
	0xaf, # 10: FT2_0
	0x8f, # 11: FT3_3
	
	0x92, # 12: FT0_3
	0xad, # 13: FT1_2
	0x91, # 14: FT2_1
	0xef, # 15: FT3_0
]

###########################################################################

class CL:
	WIDTH = 64
	def __init__(self):
		self.hits = [0] * CL.WIDTH
		self.content = ""
	def access(self, i, content=""):
		self.hits[i] += 1
		self.content = content
	def sum_hits(self):
		return sum(self.hits)
	def get_hits(self):
		return self.hits

class MemArea:
	def __init__(self, size):
		self.size = size
		self.cls = []
		for i in range(int(math.ceil(size/64))):
			self.cls.append(CL())

	def access(self, off, content=""):
		clno = int(off / 64)
		off_in_cl = int(off % 64)
		self.cls[clno].access(off_in_cl, content)

	def plot(self, arrs):
		cutoff = 1024
		cls_per_row = int(cutoff / CL.WIDTH)
		y_min = 0
		y_max = int(self.size / cutoff)

		hmdata = []
		hmlabeldata = []
		for rowidx in range(int(len(self.cls) / cls_per_row)):
			rowdata = []
			rowlabeldata = []
			for cellidx in range(cls_per_row):
				clidx = self.cls[rowidx * cls_per_row + cellidx]
				if cls_per_row < len(self.cls):
					cl = self.cls[rowidx * cls_per_row + cellidx]
					rowdata.append(cl.sum_hits())
					rowlabeldata.append(cl.content)
				else:
					rowdata.append(None)
					rowlabeldata.append("")
			hmdata.append(rowdata)
			hmlabeldata.append(rowlabeldata)
		plt.imshow(hmdata)
		ax = plt.gca()
		ax.set_xticks([i for i in range(cls_per_row)])
		ax.set_yticks([i for i in range(y_max)])
		ax.set_yticklabels([f"{i*1024}B - {((i+1)*1024)-1}B" for i in range(y_max)])
		
		for i in range(len(hmlabeldata)):
			for j in range(len(hmlabeldata[0])):
				text = ax.text(j, i, hmlabeldata[i][j],
					ha="center", va="center", color="r", fontsize=7, family="monospace")

		colors = ["r", "b", "g", "c"]
		for arridx in range(len(arrs)):
			arr = arrs[arridx]
			
			# draw rectangles
			color = colors[int(arridx % len(colors))]
			remaining_size = arr.size
			curr_x = int(arr.off_in_mem_area % cutoff)
			curr_y = int(arr.off_in_mem_area / cutoff)
			height = 0.8
			while remaining_size > 0:
				width = min(cutoff - curr_x, remaining_size)
				remaining_size -= width
				ax.add_patch(
					patches.Rectangle(
						((curr_x / CL.WIDTH)-0.5, curr_y-0.4),
						width/CL.WIDTH, height,					
						linewidth=1, edgecolor=color, facecolor='none'
					)
				)
				curr_x = 0
				curr_y += 1

		plt.show()

class Array:
	def __init__(self, mem_area, off_in_mem_area, size, typewidth):
		self.mem_area = mem_area
		self.off_in_mem_area = off_in_mem_area
		self.size = size
		self.typewidth = typewidth

	def access(self, i, content=""):
		self.mem_area.access(self.off_in_mem_area + i * self.typewidth, content)

if __name__ == '__main__':
	mem_area = MemArea(5 * 1024)

	FT0 = Array(mem_area, INITIAL_OFFSET + 0 * 1024, 1024, 4)
	FT1 = Array(mem_area, INITIAL_OFFSET + 1 * 1024, 1024, 4)
	FT2 = Array(mem_area, INITIAL_OFFSET + 2 * 1024, 1024, 4)
	FT3 = Array(mem_area, INITIAL_OFFSET + 3 * 1024, 1024, 4)

	FT0.access(
		KEY[ 0] ^ PLAINTEXT[ 0],
		f"FT0_0\nidx:  0\n{KEY[ 0]:02x}^{PLAINTEXT[ 0]:02x}={KEY[ 0]^PLAINTEXT[ 0]:02x}"
	)
	FT1.access(
		KEY[ 5] ^ PLAINTEXT[ 5],
		f"FT1_0\nidx:  5\n{KEY[ 5]:02x}^{PLAINTEXT[ 5]:02x}={KEY[ 5]^PLAINTEXT[ 5]:02x}"
	)
	FT2.access(
		KEY[10] ^ PLAINTEXT[10],
		f"FT2_0\nidx: 10\n{KEY[10]:02x}^{PLAINTEXT[10]:02x}={KEY[10]^PLAINTEXT[10]:02x}"
	)
	FT3.access(
		KEY[15] ^ PLAINTEXT[15],
		f"FT3_0\nidx: 15\n{KEY[15]:02x}^{PLAINTEXT[15]:02x}={KEY[15]^PLAINTEXT[15]:02x}"
	)

	FT0.access(
		KEY[ 4] ^ PLAINTEXT[ 4],
		f"FT0_1\nidx:  4\n{KEY[ 4]:02x}^{PLAINTEXT[ 4]:02x}={KEY[ 4]^PLAINTEXT[ 4]:02x}"
	)
	FT1.access(
		KEY[ 9] ^ PLAINTEXT[ 9],
		f"FT1_1\nidx:  9\n{KEY[ 9]:02x}^{PLAINTEXT[ 9]:02x}={KEY[ 9]^PLAINTEXT[ 9]:02x}"
	)
	FT2.access(
		KEY[14] ^ PLAINTEXT[14],
		f"FT2_1\nidx: 14\n{KEY[14]:02x}^{PLAINTEXT[14]:02x}={KEY[14]^PLAINTEXT[14]:02x}"
	)
	FT3.access(
		KEY[ 3] ^ PLAINTEXT[ 3],
		f"FT3_1\nidx:  3\n{KEY[ 3]:02x}^{PLAINTEXT[ 3]:02x}={KEY[ 3]^PLAINTEXT[ 3]:02x}"
	)

	FT0.access(
		KEY[ 8] ^ PLAINTEXT[ 8],
		f"FT0_2\nidx:  8\n{KEY[ 8]:02x}^{PLAINTEXT[ 8]:02x}={KEY[ 8]^PLAINTEXT[ 8]:02x}"
	)
	FT1.access(
		KEY[13] ^ PLAINTEXT[13],
		f"FT1_2\nidx: 13\n{KEY[13]:02x}^{PLAINTEXT[13]:02x}={KEY[13]^PLAINTEXT[13]:02x}"
	)
	FT2.access(
		KEY[ 2] ^ PLAINTEXT[ 2],
		f"FT2_2\nidx:  2\n{KEY[ 2]:02x}^{PLAINTEXT[ 2]:02x}={KEY[ 2]^PLAINTEXT[ 2]:02x}"
	)
	FT3.access(
		KEY[ 7] ^ PLAINTEXT[ 7],
		f"FT3_2\nidx:  7\n{KEY[ 7]:02x}^{PLAINTEXT[ 7]:02x}={KEY[ 7]^PLAINTEXT[ 7]:02x}"
	)

	FT0.access(
		KEY[12] ^ PLAINTEXT[12],
		f"FT0_3\nidx: 12\n{KEY[12]:02x}^{PLAINTEXT[12]:02x}={KEY[12]^PLAINTEXT[12]:02x}"
	)
	FT1.access(
		KEY[ 1] ^ PLAINTEXT[ 1],
		f"FT1_3\nidx:  1\n{KEY[ 1]:02x}^{PLAINTEXT[ 1]:02x}={KEY[ 1]^PLAINTEXT[ 1]:02x}"
	)
	FT2.access(
		KEY[ 6] ^ PLAINTEXT[ 6],
		f"FT2_3\nidx:  6\n{KEY[ 6]:02x}^{PLAINTEXT[ 6]:02x}={KEY[ 6]^PLAINTEXT[ 6]:02x}"
	)
	FT3.access(
		KEY[11] ^ PLAINTEXT[11],
		f"FT3_3\nidx: 11\n{KEY[11]:02x}^{PLAINTEXT[11]:02x}={KEY[11]^PLAINTEXT[11]:02x}"
	)

	FT = [FT0, FT1, FT2, FT3]
	mem_area.plot(FT)
