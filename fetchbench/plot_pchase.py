import matplotlib.pyplot as plt
import sys
import statistics
import os

lines = None
args = None


def get_experiment_results(experiment_name):
    started = False
    results = []
    for i in range(len(lines)):
        if lines[i] == "":
            continue
        if lines[i][0] == ";":
            print(experiment_name, lines[i])
            continue
        it, mes, ref = lines[i].split(" ")
        
        results.append((int(it[:-1]), int(mes), int(ref)))
    return results

def plot_experiment(x_values, y_measure, y_ref, x_text, y_text, title, file_name, func = plt.plot, file_format="csv", show=False):
    global args
    func(x_values, y_measure, label="measurement")
    func(x_values, y_ref, label="reference")
    plt.xlabel(x_text)
    plt.ylabel(y_text)
    plt.title(title)
    plt.legend()
    if show:
        plt.show()
    os.makedirs(f"plots/{args[2]}", exist_ok=True)
    plt.savefig(f"plots/{args[2]}/{file_name}.{file_format}")
    plt.clf()

def plot_parameterized(experiment_name, x_text, y_text, title, file_name, aggregate, func = plt.plot, file_format="svg", show=False):
    results = get_experiment_results(experiment_name)
    params = dict()
    for i, measurement, reference in results:
        if i not in params:
            # measurement, reference
            params[i] = ([], [])
        l_measurement, l_reference = params[i]
        l_measurement.append(measurement)
        l_reference.append(reference)
    
    x_vals = sorted(list(params.keys()))
    y_measure = []
    y_ref = []
    for x in x_vals:
        l_measurement, l_reference = params[x]
        y_measure.append(aggregate(l_measurement))
        y_ref.append(aggregate(l_reference))
    plot_experiment(x_vals, y_measure, y_ref, x_text, y_text, title, file_name, func=func, file_format=file_format, show=show)

func = statistics.median

if len(sys.argv) > 1:
    runs = [sys.argv]
else:
    runs = []
    for fl in os.listdir("measurements"):
        runs.append(["blub", f"measurements/{fl}", f"{fl.split('.')[0]}"])

for run in runs:
    args = run
    lines = open(args[1]).read().split("\n")
    plot_parameterized("pointer chasing test", "pointer chasing test", "time (median)", "pointer chasing test", "pointer_chase", func, show=False)
