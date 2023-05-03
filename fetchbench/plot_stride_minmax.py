import json
import sys
import datetime
import argparse

# use a matplotlib backend that does not require an X server
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches

parser = argparse.ArgumentParser(
	prog = 'plot_stride',
	description = 'Plots cache histograms for stride experiments.'
)
parser.add_argument(
	"-n", "--name", required=True,
	help="Experiment name (used as title and part of the filename)."
)
parser.add_argument(
	"-i", "--input", required=True, nargs="+",
	help="List of json files to plot."
)

args = parser.parse_args()
stride_jsons = []
for input_filename in args.input:
	with open(input_filename) as file:
		stride_json = json.loads(file.read())
		stride_json["_filename"] = input_filename
		stride_jsons.append(stride_json)

fig = plt.gcf()
ax = plt.gca()

# set figure dimensions
fig.set_size_inches(30, len(stride_jsons) // 4)
plt.tight_layout()

max_steps = -1
for stride_json in stride_jsons:
	steps = len(stride_json["cache_histogram"]) // (stride_json["stride"] // stride_json["cache_line_size"])
	if steps > max_steps:
		max_steps = steps

# aggregate heatmap data
heatmap_data = []
prefetch_vector_data = []
for stride_json in stride_jsons:
	cache_line_size = stride_json["cache_line_size"]
	cache_histogram = stride_json["cache_histogram"]
	prefetch_vector = stride_json["prefetch_vector"]
	first_access_offset = stride_json["first_access_offset"]
	first_access_offset_cl = stride_json["first_access_offset"] // cache_line_size
	stride = stride_json["stride"]
	stride_cls = stride // cache_line_size

	# aggregate relevant data for this trace (i.e. for this horizontal line in the map)
	trace_value = []
	trace_prefetch_vector = []
	x = first_access_offset_cl
	while x >= 0 and x < len(cache_histogram):
		trace_value.append(cache_histogram[x])
		trace_prefetch_vector.append(prefetch_vector[x])
		x += stride_cls
	
	# pad trace with 0 at the end so that all traces have equal length
	for i in range(len(trace_value), max_steps):
		trace_value.append(0)
		trace_prefetch_vector.append(False)
	heatmap_data.append(trace_value)
	prefetch_vector_data.append(trace_prefetch_vector)

# plot main heatmap
plt.imshow(heatmap_data)

# plot boxes
for y, stride_json in enumerate(stride_jsons):
	step = stride_json["step"]

	# boxes for later multiples of the stride
	for i, x in enumerate(range(step, max_steps)):
		if heatmap_data[y][x] > 0:
			# rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none', linestyle=":")
			# ax.add_patch(rect)
			plt.text(x, y, "+" + str(i+1), ha="center", va="center", color="r", fontsize=8)
		if prefetch_vector_data[y][x] == True:
			rect = patches.Rectangle((x-0.45, y-0.45), 0.9, 0.9, linewidth=1, edgecolor="magenta", facecolor="none")
			ax.add_patch(rect)

	# boxes for architectural loads
	for x in range(step):
		rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none')
		ax.add_patch(rect)

# axis labels
ax.set_xticks([i for i in range(0, max_steps)])
ax.set_yticks([i for i in range(len(stride_jsons))])
ax.set_ylabel("trace")
ax.set_xticklabels([i for i in range(1, max_steps+1)])
ax.set_yticklabels([stride_json["_filename"] for stride_json in stride_jsons])
ax.set_xlabel("Multiple of stride")

# title
plt.title("Min/Max Stride: " + args.name)

# save result to file
output_filename = "plot_" + str(int(datetime.datetime.now().timestamp() * 1000)) + "_" + args.name + ".svg"
print("Plot: " + output_filename)
plt.savefig(output_filename, bbox_inches='tight')
