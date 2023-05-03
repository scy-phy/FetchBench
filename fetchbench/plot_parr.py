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
        if lines[i] == f"#[Experiment][{experiment_name}]: start":
            started = True
            continue
        if lines[i] == f"#[Experiment][{experiment_name}]: end":
            return results
        if lines[i][0] == ";":
            print(experiment_name, lines[i])
            continue
        if started:
            results.append(tuple(list(map(int, lines[i].split(" ")))))
    if started:
        raise Exception(f"experiment {experiment_name} is not terminated!")
    else:
        raise Exception(f"experiment {experiment_name} not found!")
       

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


def plot_simple(experiment_name, x_text, y_text, title, file_name, func = plt.plot, file_format="svg", show=False):
    results = get_experiment_results(experiment_name)
    x_vals = range(len(results))
    y_measure = []
    y_ref = []
    for measurement, reference in results:
        y_measure.append(measurement)
        y_ref.append(reference)
    plot_experiment(x_vals, y_measure, y_ref, x_text, y_text, title, file_name, func=func, file_format=file_format, show=show)

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
 
if len(sys.argv) > 1:
    runs = [sys.argv]
else:
    runs = []
    for fl in os.listdir("measurements"):
        runs.append(["blub", f"measurements/{fl}", f"{fl.split('.')[0]}"])

def hits(x):
    return sum(1 for a in x if a < 180)

for run in runs:
    args = run
    lines = open(args[1]).read().split("\n")
    
    plot_simple("existence", "measurement #", "time", "pointer array existence test", "existence", show=False)
    plot_simple("existence-ref", "measurement #", "time", "pointer array existence test (reference)", "existence_ref", show=False)
    plot_simple("backwards", "measurement #", "time", "pointer array backwards detection test", "backwards", show=False)
    plot_simple("backwards-ref", "measurement #", "time", "pointer array backwards detection test (reference)", "backwards-ref", show=False)
    plot_simple("pc-dependence", "measurement #", "time", "pointer array pc-dependence test", "pc_dependence", show=False)
    plot_simple("pc-dependence-ref", "measurement #", "time", "pointer array pc-dependence test (reference)", "pc_dependence_ref", show=False)
    plot_simple("pc-dependence-align", "measurement #", "time", "pointer array pc-dependence aligned test", "pc_dependence_align", show=False)
    plot_simple("pc-dependence-align-ref", "measurement #", "time", "pointer array pc-dependence aligned test (reference)", "pc_dependence_ref_align", show=False)
   
    plot_parameterized("training-pointers", "training pointers", "time (median)", "training pointers test", "training_pointers", statistics.median, show=False)
    plot_parameterized("target-offset", "offset from entry", "time (median)", "target array offset test", "target_offset", statistics.median, show=False)
    plot_parameterized("pointer-array-offset", "offset from last training pointer", "time (median)", "measurement offset test", "pointer_array_offset", hits, show=False)
    plot_parameterized("pointer-array-space", "space between pointers (1 = 8 bytes)", "time (median)", "pointer array space test", "pointer_array_space", hits, show=False)
