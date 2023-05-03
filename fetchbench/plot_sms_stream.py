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
	description = 'Plots cache histograms for SMS experiments.'
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
sms_jsons = []
for input_filename in args.input:
	with open(input_filename) as file:
		sms_json = json.loads(file.read())
		sms_json["_filename"] = input_filename
		sms_jsons.append(sms_json)

fig = plt.gcf()
ax = plt.gca()

# set figure dimensions
fig.set_size_inches(15 * (len(sms_jsons[0]["cache_histogram"]) / 64), 1+len(sms_jsons))
plt.tight_layout()

# plot main heatmap
imdata = [sms_json["cache_histogram"] for sms_json in sms_jsons]
plt.imshow(imdata)

# plot markers
for y, sms_json in enumerate(sms_jsons):
	cache_line_size = sms_json["cache_line_size"]
	training_offsets = sms_json["training_offsets"]
	trigger_offsets = sms_json["trigger_offsets"]
	
	# circles for relative prefetch locations
	if len(training_offsets) > 0 and len(trigger_offsets) > 0:
		# compute relative distances relative to the first training element
		distances = []
		for i in range(1, len(training_offsets)):
			cl_distance = (training_offsets[i]//cache_line_size) - (training_offsets[0]//cache_line_size)
			distances.append(cl_distance)

		# plot patches
		for trigger_offset in trigger_offsets:
			for x in [(trigger_offset//cache_line_size) + dist for dist in distances]:
				if sms_json["cache_histogram"][x] > 0:
					circ = patches.Circle((x, y), 0.1, linewidth=1, edgecolor='blue', facecolor='blue')
					ax.add_patch(circ)

	# boxes for trigger offsets
	for i, trigger_offset in enumerate(trigger_offsets):
		x = trigger_offset // cache_line_size
		rect = patches.Rectangle((x-0.375, y-0.375), 0.75, 0.75, linewidth=1, edgecolor='r', facecolor='none')
		ax.add_patch(rect)

	# red circles for training accesses (-> absolute prefetch locations)
	for training_offset in training_offsets:
		x = training_offset // cache_line_size
		if sms_json["cache_histogram"][x] > 0:
			circ = patches.Circle((x, y), 0.25, linewidth=1, edgecolor='red', facecolor='none')
			ax.add_patch(circ)


# axis labels
ax.set_xticks([i for i in range(len(sms_jsons[0]["cache_histogram"]))])
ax.set_yticks([i for i in range(len(sms_jsons))])
ax.set_ylabel("trace")
ax.set_yticklabels([sms_json["_filename"] for sms_json in sms_jsons])
ax.set_xlabel("CL")

# legend
# legend_patches = [
# 	matplotlib.lines.Line2D([], [], color="none", marker='s', markerfacecolor="none", markeredgecolor="red", label="Trigger locations"),
# 	matplotlib.lines.Line2D([], [], color="none", marker='o', markerfacecolor="none", markeredgecolor="red", label="Original access locations (absolute prefetch locations)"),
# 	matplotlib.lines.Line2D([], [], color="none", marker='o', markerfacecolor="blue", label="Relative prefetch locations"),
# ]
# plt.legend(handles=legend_patches, loc="upper left", ncol=3)

# title
plt.title(args.name)

# save result to file
output_filename = "plot_" + str(int(datetime.datetime.now().timestamp() * 1000)) + "_" + args.name + ".svg"
print("Plot: " + output_filename)
plt.savefig(output_filename, bbox_inches='tight')
