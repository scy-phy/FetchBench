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
fig.set_size_inches(15 * (len(stride_jsons[0]["cache_histogram"]) / 64), 1+len(stride_jsons))
plt.tight_layout()

# plot main heatmap
imdata = [stride_json["cache_histogram"] for stride_json in stride_jsons]
plt.imshow(imdata)
# plot boxes
for y, stride_json in enumerate(stride_jsons):
	cache_line_size = stride_json["cache_line_size"]
	first_access_offset = stride_json["first_access_offset"]
	first_access_offset_cl = stride_json["first_access_offset"] // cache_line_size
	stride = stride_json["stride"]
	step = stride_json["step"]
	stride_cls = int(stride / cache_line_size)

	# don't plot boxes if stride < CACHE_LINE_SIZE
	if stride_cls == 0:
		continue

	# boxes for earlier multiples of the stride
	for x in range(
		first_access_offset_cl-stride_cls,
		-1,
		-stride_cls
	) if stride >= 0 else range(
		first_access_offset_cl+stride_cls,
		len(stride_json["cache_histogram"]),
		-stride_cls
	):
		# if stride_json["cache_histogram"][x] > 0:
		# 	rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none', linestyle=":")
		# 	ax.add_patch(rect)
		if stride_json["prefetch_vector"][x] == True:
			rect = patches.Rectangle((x-0.45, y-0.45), 0.9, 0.9, linewidth=1, edgecolor="magenta", facecolor="none")
			ax.add_patch(rect)

	# boxes for later multiples of the stride
	for i, x in enumerate(range(
			first_access_offset_cl+stride_cls*step,
			len(stride_json["cache_histogram"]),
			stride_cls
		) if stride >= 0 else range(
			first_access_offset_cl+stride_cls*step,
			-1,
			stride_cls
		)
	):
		if stride_json["cache_histogram"][x] > 0:
			# rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none', linestyle=":")
			# ax.add_patch(rect)
			plt.text(x, y, "+" + str(i+1), ha="center", va="center", color="r", fontsize=8)
		if stride_json["prefetch_vector"][x] == True:
			rect = patches.Rectangle((x-0.45, y-0.45), 0.9, 0.9, linewidth=1, edgecolor="magenta", facecolor="none")
			ax.add_patch(rect)

	# boxes for architectural loads
	for x in range(
		first_access_offset_cl,
		first_access_offset_cl+stride_cls*step,
		stride_cls
	):
		rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none')
		ax.add_patch(rect)

# axis labels
ax.set_xticks([i for i in range(len(stride_jsons[0]["cache_histogram"]))])
ax.set_yticks([i for i in range(len(stride_jsons))])
ax.set_ylabel("trace")
ax.set_yticklabels([stride_json["_filename"] for stride_json in stride_jsons])
ax.set_xlabel("CL")

# title
plt.title("Stride: " + args.name)

# save result to file
output_filename = "plot_" + str(int(datetime.datetime.now().timestamp()) * 1000) + "_" + args.name + ".svg"
print("Plot: " + output_filename)
plt.savefig(output_filename, bbox_inches='tight')
