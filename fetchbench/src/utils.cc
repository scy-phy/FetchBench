#include <algorithm>

#include "utils.hh"
#include "testcase_stride_strideexperiment.hh"

using std::string;
using std::map;
using std::vector;

#ifdef __x86_64__
/**
 * On x86_64 CPUs: get the Vendor ID string. Allows us to distinguish
 * between Intel and AMD CPUs.
 *
 * @return     Vendor ID. "GenuineIntel" on Intel, "AuthenticAMD" on AMD.
 */
string cpuid_get_vendor_id() {
	char vendor_id[13];
	__asm__ __volatile__(
		"movl $0, %%eax\n"
		"cpuid\n"
		"movl %%ebx, %0\n\t"
		"movl %%edx, %1\n\t"
		"movl %%ecx, %2\n\t"
		: "=m"(vendor_id), "=m"(vendor_id[4]), "=m"(vendor_id[8])
		:
		: "%eax", "%ebx", "%edx", "%ecx", "memory"
	);
	vendor_id[12] = 0;
	return {vendor_id};
}
#endif

/**
 * Determines the architecture of the current CPU.
 *
 * @return     The architecture as an element of the architecture_t enum.
 */
architecture_t get_arch() {
	#ifdef __x86_64__
		std::string vendor_id = cpuid_get_vendor_id();
		if (vendor_id == "GenuineIntel") {
			return ARCH_INTEL;
		} else if (vendor_id == "AuthenticAMD") {
			return ARCH_AMD;
		} else {
			return ARCH_UNKNOWN;
		}
	#elif defined(__aarch64__) && ! defined(__APPLE__)
		return ARCH_ARM;
	#elif defined(__aarch64__) && defined(__APPLE__)
		return ARCH_ARM_APPLE;
	#else
		return ARCH_UNKNOWN;
	#endif
}

/**
 * Calls an external plot script to plot JSON dumps of a stride experiment
 * and the corresponding cache histograms. Each JSON file is represented by
 * one line in the final plot.
 *
 * @param      name                   The name (will be used in the output
 *                                    file name and the plot title)
 * @param      json_dumps_file_paths  The json dumps file paths
 */
void _plot_call(string const& script_filepath, string const& name, vector<string> const& json_dumps_file_paths) {
	L::info("Plotting data...\n");
	fflush(stdout);
	pid_t pid = fork();
	switch(pid) {
		case -1:
			perror("fork() failed.");
			exit(EXIT_FAILURE);
		case 0: { // child
			vector<char const *> argv {"python3", script_filepath.c_str(), "-n"};
			argv.push_back(name.data());
			argv.push_back("-i");
			for (string const& path : json_dumps_file_paths) {
				argv.push_back(path.data());
			}
			argv.push_back(NULL);
			execvp(const_cast<char* const>(argv.data()[0]), const_cast<char* const*>(argv.data()));
			break;
		}
		default: // parent
			L::info("Waiting for plot script to finish...\n");
			waitpid(pid, NULL, 0);
			break;
	}
	L::info("Plot finished.\n");
}

void plot_stride(string const& name, vector<string> const& json_dumps_file_paths) {
	_plot_call("plot_stride.py", "stride_" + name, json_dumps_file_paths);
}

void plot_stride_minmax(string const& name, vector<string> const& json_dumps_file_paths) {
	_plot_call("plot_stride_minmax.py", "stride_" + name, json_dumps_file_paths);
}

void plot_sms(string const& name, vector<string> const& json_dumps_file_paths) {
	_plot_call("plot_sms_stream.py", "sms_" + name, json_dumps_file_paths);
}

void plot_stream(string const& name, vector<string> const& json_dumps_file_paths) {
	_plot_call("plot_sms_stream.py", "stream_" + name, json_dumps_file_paths);
}

void _plot_call_ext(string const& script_filepath, string const& name, string const& program_output_file_path) {
	L::info("Plotting data...\n");
	fflush(stdout);
	pid_t pid = fork();
	switch(pid) {
		case -1:
			perror("fork() failed.");
			exit(EXIT_FAILURE);
		case 0: { // child
			vector<char const *> argv {
				"python3", script_filepath.data(),
				program_output_file_path.data(), name.data(), NULL
			};
			execvp(const_cast<char* const>(argv.data()[0]), const_cast<char* const*>(argv.data()));
			break;
		}
		default: // parent
			L::info("Waiting for plot script to finish...\n");
			waitpid(pid, NULL, 0);
			break;
	}
	L::info("Plot finished.\n");
}


void plot_parr(string const& name, string const& program_output_file_path) {
	_plot_call_ext("plot_parr.py", name, program_output_file_path);
}

void plot_pchase(string const& name, string const& program_output_file_path) {
	_plot_call_ext("plot_pchase.py", name, program_output_file_path);
}

/**
 * Formats ("pretty-prints") an JSON string. This is a very primitive and
 * probably not the most performant implementation, but for the purpose of
 * our program it should be enough. Uses two spaces for indentation.
 *
 * @param[in]  json_in  The JSON string to format
 *
 * @return     Pretty-printed JSON string
 */
string json_pretty_print(string const& json_in) {
	// string to store the resulting (pretty) json string
	string result;
	// store current indentation level
	unsigned indent_level = 0;
	// store whether we are inside a quoted string or not
	bool quoted = false;
	// lambda function to add a newline (+ indentation for the next line)
	// to the result string
	auto add_newline = [&] () {
		// only add newline if the character did not appear within a quoted string
		if (!quoted) {
	        result += "\n";
			for (unsigned idt = 0; idt < indent_level; idt++) {
				result += "  ";
			}
		}
    };
	for (size_t i = 0; i < json_in.size(); i++) {
		char c = json_in[i];
		switch (c) {
			case '{':
			case '[':
				// add newline after the character and increase indentation level
				result += c;
				indent_level++;
				add_newline();
				break;
			case '}':
			case ']':
				// decrease indentation level and add newline before the character
				indent_level--;
				add_newline();
				result += c;
				break;
			case ',':
				// add newline after the character, keep indentation level
				result += c;
				add_newline();
				break;
			case '\"':
				// handle double quotes
				// copy them to the resulting string
				result += c;
				// if '"' is not part of sequence '\"' within a quoted string
				if (!(quoted && (i - 1) > 0 && json_in[i - 1] == '\\')) {
					// toggle the quoted flag
					quoted = !quoted;
				}
				break;
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				// remove whitespace, except in quoted strings
				if (quoted) {
					result += c;
				}
				break;
			case ':':
				// add space character after colon, except in quoted strings
				if (!quoted) {
					result += c;
					result += ' ';
				}
				break;
			default:
				// for all other characters, just copy them into the result string
				result += c;
				break;
		}
	}
	return result;
}

/**
 * Write a JSON structure to a file.
 *
 * @param      j         The JSON structure
 * @param      filepath  The filepath
 */
void json_dump_to_file(Json const& j, string const& filepath) {
	// dump the Json structure into a file
	std::ofstream file;
	file.open(filepath);
	file << j.dump() << "\n";
	file.close();
}

/**
 * Returns pointer to the random number generator. The pointer points to a
 * singleton instance (local static variable in this function.)
 *
 * @return     Pointer to the random number generator.
 */
std::shared_ptr<std::mt19937> get_rng() {
	static std::shared_ptr<std::mt19937> rng {nullptr};
	if (!rng) {
		// DEBUG: set seed to constant value to make execution runs reproducible.
		size_t seed = 0;
		rng = std::make_shared<std::mt19937>(seed);
	}
	return rng;
}

/**
 * Returns a random number in the interval `[lower, upper]`, `lower` and
 * `upper` both included.
 *
 * @param[in]  lower  The lower bound (inclusive)
 * @param[in]  upper  The upper bound (inclusive)
 *
 * @return     Random value between `lower` and `upper` (inclusive).
 */
std::mt19937::result_type random_uint32(std::mt19937::result_type lower, std::mt19937::result_type upper) {
	std::uniform_int_distribution<std::mt19937::result_type> dist {lower, upper};
	return dist(*get_rng());
}

/**
 * Convert a number (int64_t) to a string. The number will be padded with
 * 0's such that it is at least min_digits long. If the number is negative,
 * the sign will be preserved. It does not count as a digit.
 *
 * @param[in]  no          The number to pad
 * @param[in]  min_digits  The minimum number of digits
 *
 * @return     zero-padded number as string
 */
string zero_pad(int64_t no, size_t min_digits) {
	string str_sign = (no < 0) ? "-" : "";
	string str_no = std::to_string(std::abs(no));
	string str_padded_no = string(min_digits - std::min(min_digits, str_no.length()), '0') + str_no;
	return str_sign + str_padded_no;
}
